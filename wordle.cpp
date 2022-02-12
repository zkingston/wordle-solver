#include <fcntl.h>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

// color codes
#define BLK "\e[0;30m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define RST "\e[0m"

// possible wordle answers
enum Results
{
    BLACK = 0,
    YELLOW = 1,
    GREEN = 2,
};

// result to color code
static const char *COLOR[3] = {BLK, YEL, GRN};

#define L 5u               // number of letters
#define N_VALID 2315u      // number of valid answers
#define N_WORDS 12972u     // number of valid guesses
#define N_THREADS 8u       // number of threads to use
#define ALL_26 0x03FFFFFF  // bitmask for 26
#define U32MAX std::numeric_limits<uint32_t>::max()
#define BLOCK (uint16_t)(N_WORDS / N_THREADS)
#define MINMAX true

static uint32_t MASKS[255];                 // cached masks for each character
#define MASK(c) (uint32_t)(1 << (c - 'a'))  // get bitmask for char
#define GET_MASK(c) MASKS[(uint8_t)c]       // get the mask from the cache

static char *VALID_WORDS;  // valid answers
static char *WORDS;        // valid guesses

static uint32_t MASKED_ANSWERS[N_VALID][L];  // premasked answers
static std::vector<uint16_t> ANSWERS;        // set of valid answers

// memory map a file
static char *map(const char *file)
{
    int fd = open(file, O_RDONLY);

    struct stat s;
    fstat(fd, &s);

    return (char *)mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
}

// get the answer at word idx
inline const char *get_valid_word(uint16_t idx)
{
    return &VALID_WORDS[6 * idx];
}

// get the guess at word idx
inline const char *get_word(uint16_t idx)
{
    return &WORDS[6 * idx];
}

// state which encodes known information
// uses bitmasks to encode letter information - e.g., bit #3 corresponds to `c`
struct State
{
    // bitmasks for valid characters at each position
    std::array<uint32_t, L> valid = {ALL_26, ALL_26, ALL_26, ALL_26, ALL_26};
    uint32_t include = 0;  // letters that must be in the word

    bool operator==(const State &other) const
    {
        return include == other.include and valid == other.valid;
    }

    void apply(const Results result[L], const char *guess)
    {
        uint32_t temp = 0;
        for (uint8_t i = 0; i < L; ++i)
        {
            const uint32_t m = GET_MASK(guess[i]);
            if (result[i] == GREEN)  // must be this value
                valid[i] = m;

            else if (result[i] == YELLOW)  // this position cannot be this value
            {
                valid[i] &= ~m;
                include |= m;  // but this value must be elsewhere
                temp |= m;     // keep track of prior yellow, as,
            }
            else
            {
                if (not(temp & m))  // if yellow of this letter not seen,
                {
                    for (uint8_t j = 0; j < L; ++j)  // no position can be this value
                        if (valid[j] - m)            // there are other values, not just this one
                            valid[j] &= ~m;
                }
                else  // else just this position is not this letter
                    valid[i] &= ~m;
            }
        }
    }

    void print_mask(uint32_t mask) const
    {
        for (uint8_t j = 0; j < 26; ++j)
            std::cout << ((mask & (1 << j)) ? (char)('a' + j) : '_');
        std::cout << std::endl;
    }

    void print() const
    {
        std::cout << N_WORDS << " / " << ANSWERS.size() << std::endl;
        print_mask(include);
        for (uint8_t i = 0; i < L; ++i)
            print_mask(valid[i]);
    }

    inline bool is_valid(uint16_t index) const
    {
        bool r = true;
        uint32_t required = include;
        const auto &mask = MASKED_ANSWERS[index];
        for (uint8_t i = 0; i < L; ++i)
        {
            r &= (bool)(valid[i] & mask[i]);
            if (not r)
                return false;
            required -= required & mask[i];
        }

        return r and not required;
    }

    void valid_answers()
    {
        uint16_t k = 0;
        for (const auto &answer : ANSWERS)
            if (is_valid(answer))
                ANSWERS[k++] = answer;

        ANSWERS.resize(k);
    }

    uint16_t num_answers() const
    {
        int k = 0;
        for (const auto &answer : ANSWERS)
            k += is_valid(answer);

        return (uint16_t)k;
    }
};

bool play(Results result[L], const char *guess, const char *hidden)
{
    bool r = true;
    bool done[L];

    for (uint8_t i = 0; i < L; ++i)
    {
        r &= (done[i] = guess[i] == hidden[i]);
        result[i] = (done[i]) ? GREEN : BLACK;
    }

    if (not r)
        for (uint8_t i = 0; i < L; ++i)
            if (result[i] != GREEN)
                for (uint8_t j = 0; j < L; ++j)
                    if (guess[i] == hidden[j] and not done[j] and i != j)
                    {
                        done[j] = true;
                        result[i] = YELLOW;
                        break;
                    }

    return r;
}

void print_move(Results result[L], const char *guess, const char *hidden = nullptr)
{
    for (uint8_t i = 0; i < L; ++i)
        std::cout << COLOR[result[i]] << guess[i];

    std::cout << RST;
    if (hidden)
    {
        std::cout << " / ";
        for (uint8_t i = 0; i < L; ++i)
            std::cout << hidden[i];
    }
}

class StateHash
{
public:
    std::size_t operator()(const State &state) const
    {
        std::size_t r = state.include;
        for (uint8_t i = 0; i < L; ++i)
            r += state.valid[i];
        return r;
    }
};

struct Guess
{
    uint32_t max{U32MAX};
    uint32_t avg{U32MAX};
    uint32_t best{U32MAX};
    uint16_t word{0};

    bool operator<(const Guess &other) const
    {
        if (MINMAX and max < other.max)
            return true;
        if (MINMAX and max > other.max)
            return false;

        if (avg < other.avg)
            return true;
        if (avg > other.avg)
            return false;

        if (MINMAX and best < other.best)
            return true;
        if (MINMAX and best > other.best)
            return false;

        return word < other.word;
    }

    void add_score(uint32_t score)
    {
        avg += score;
        if (MINMAX)
        {
            max = std::max(score, max);
            best = std::min(score, best);
        }
    }
};

const char *find_guess(const State &state)
{
    // lookup result from cache if the best play for this state was already computed
    static std::unordered_map<const State, uint16_t, StateHash> guess_cache;
    const auto &it = guess_cache.find(state);
    if (it != guess_cache.end())
        return get_word(it->second);

    if (ANSWERS.size() == 1)  // only one word left
        return get_word(ANSWERS[0]);

    Guess guesses[N_THREADS];
    std::thread threads[N_THREADS];

    for (uint8_t id = 0; id < N_THREADS; ++id)
        threads[id] = std::thread([&, id]() {
            Results result[L];
            const uint16_t start = (uint16_t)(BLOCK * id);
            const uint16_t end =
                (uint16_t)(start + BLOCK + ((id == N_THREADS - 1) ? N_WORDS % N_THREADS : 0));
            for (uint16_t i = start; i < end; ++i)
            {
                Guess score = {0, 0, U32MAX, i};
                const char *guess = get_word(i);
                for (const auto &answer : ANSWERS)
                {
                    play(result, guess, get_valid_word(answer));

                    State next = state;
                    next.apply(result, guess);
                    score.add_score(next.num_answers());

                    if (score.max > guesses[id].max)
                        break;
                }

                if (score < guesses[id])
                    guesses[id] = score;
            }
        });

    for (auto &thread : threads)
        thread.join();

    for (uint8_t i = 1; i < N_THREADS; ++i)
        if (guesses[i] < guesses[0])
            guesses[0] = guesses[i];

    guess_cache.emplace(state, guesses[0].word);
    return get_word(guesses[0].word);
}

void evaluate()
{
    float avg = 0;
    Results result[L];
    uint16_t totals[7] = {};

    for (uint16_t i = 0; i < N_VALID; ++i)
    {
        State state;
        const char *hidden = get_valid_word(i);

        int j = 0;
        bool r = true;
        for (; j < 6 and r; ++j)
        {
            const char *guess = find_guess(state);
            r = not play(result, guess, hidden);
            print_move(result, guess);
            std::cout << " ";

            state.apply(result, guess);
            state.valid_answers();
        }

        j += r;
        totals[j]++;

        if (not r)
            std::cout << j << std::endl;
        else
            printf("%.5s\n", hidden);

        avg += (float)j;

        ANSWERS.resize(N_VALID);
        std::iota(ANSWERS.begin(), ANSWERS.end(), 0);
    }

    for (uint8_t i = 0; i < 7; ++i)
        std::cout << totals[i] << " ";
    std::cout << "Avg: " << avg / N_VALID << std::endl;
}

void check_word(const char *hidden)
{
    State state;
    Results result[L];

    bool found = false;
    for (uint16_t i = 0; i < N_VALID and not found; ++i)
        found |= strncmp(hidden, get_valid_word(i), L) == 0;

    if (not found)
    {
        std::cout << "Invalid word. Not in hidden list." << std::endl;
        return;
    }

    bool r = true;
    for (uint8_t i = 0; i < 6 and r; ++i)
    {
        const char *guess = find_guess(state);
        r = not play(result, guess, hidden);

        print_move(result, guess, hidden);
        std::cout << std::endl;

        state.apply(result, guess);
        state.print();
        state.valid_answers();

        std::cout << "Press Enter" << std::endl;
        std::cin.ignore();
    }
}

void interactive()
{
    State state;
    Results result[L];

    std::cout << "After each guess, enter in result (string of 5 of {b,y,g}, e.g., bbygb)" << std::endl;

    for (uint8_t i = 0; i < 6 and ANSWERS.size() > 1; ++i)
    {
        const char *guess = find_guess(state);
        printf("Guess : %.5s\nResult: ", guess);

        while (true)
        {
            std::string result_string;
            std::getline(std::cin, result_string);

            if (result_string.size() == L)
            {
                uint8_t i = 0;
                for (; i < L; ++i)
                    if (result_string[i] == 'b')
                        result[i] = BLACK;
                    else if (result_string[i] == 'y')
                        result[i] = YELLOW;
                    else if (result_string[i] == 'g')
                        result[i] = GREEN;
                    else
                        break;

                if (i == L)
                    break;
            }

            std::cout << "Invalid Result. Try again." << std::endl;
        }

        state.apply(result, guess);
        state.valid_answers();
        state.print();
        std::cout << std::endl;
    }

    printf("Answer: %.5s\n", get_word(ANSWERS[0]));
}

int main(int argc, char **argv)
{
    // Iniitalize word files
    VALID_WORDS = map("words_hidden");
    WORDS = map("words_all");

    ANSWERS.resize(N_VALID);
    std::iota(ANSWERS.begin(), ANSWERS.end(), 0);

    // Initialize mask cache
    for (char c = 'a'; c <= 'z'; ++c)
        GET_MASK(c) = MASK(c);

    // Initialize answer mask cache
    for (uint16_t i = 0; i < N_VALID; ++i)
    {
        const char *word = get_valid_word(i);
        for (uint8_t j = 0; j < L; ++j)
            MASKED_ANSWERS[i][j] = GET_MASK(word[j]);
    }

    if (argc == 2)
        if (strncmp(argv[1], "?", 1) == 0)
            interactive();
        else
            check_word(argv[1]);

    else
        evaluate();

    return 0;
}
