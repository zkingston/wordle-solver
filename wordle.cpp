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

static uint32_t MASKS[255];                 // cached masks for each character
#define MASK(c) (uint32_t)(1 << (c - 'a'))  // get bitmask for char
#define GET_MASK(c) MASKS[(uint8_t)c]       // get the mask from the cache

static char *VALID_WORDS;  // valid answers
static char *WORDS;        // valid guesses

// memory map a file
static char *map(const std::string &file)
{
    int fd = open(file.c_str(), O_RDONLY);

    struct stat s;
    fstat(fd, &s);

    return (char *)mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
}

// get the answer at index idx
const char *get_valid_word(uint16_t idx)
{
    return &VALID_WORDS[6 * idx];
}

// get the guess at index idx
const char *get_word(uint16_t idx)
{
    return &WORDS[6 * idx];
}

// state which encodes known information
// uses bitmasks to encode letter information - e.g., bit #3 corresponds to `c`
struct State
{
    // bitmasks for valid characters at each position
    uint32_t valid[L] = {ALL_26, ALL_26, ALL_26, ALL_26, ALL_26};
    uint32_t include = 0;           // letters that must be in the word
    std::vector<uint16_t> words;    // set of valid guesses given state
    std::vector<uint16_t> answers;  // set of valid answers given state

    State()
    {
        words.resize(N_WORDS);
        std::iota(words.begin(), words.end(), 0);
        answers.resize(N_VALID);
        std::iota(answers.begin(), answers.end(), 0);
    }

    State(const State &other) : include(other.include), words(other.words), answers(other.answers)
    {
        for (uint8_t i = 0; i < L; ++i)
            valid[i] = other.valid[i];
    }

    bool operator==(const State &other) const
    {
        bool r = include == other.include;
        for (uint8_t i = 0; i < L and r; ++i)
            r &= valid[i] == other.valid[i];

        return r;
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
        std::cout << words.size() << " / " << answers.size() << std::endl;
        print_mask(include);
        for (uint8_t i = 0; i < L; ++i)
            print_mask(valid[i]);
    }

    bool is_valid(const char *word) const
    {
        bool r = true;
        uint32_t required = include;
        for (uint8_t i = 0; i < L and r; ++i)
        {
            const uint32_t m = GET_MASK(word[i]);
            r &= (bool)(valid[i] & m);
            required -= required & m;
        }

        return r and not required;
    }

    void valid_sets()
    {
        uint16_t j = 0;
        for (uint16_t i = 0; i < words.size(); ++i)
            if (is_valid(get_word(words[i])))
                words[j++] = words[i];

        words.resize(j);
    }

    void valid_answers()
    {
        uint16_t k = 0;
        for (uint16_t i = 0; i < answers.size(); ++i)
            if (is_valid(get_valid_word(answers[i])))
                answers[k++] = answers[i];

        answers.resize(k);
    }
};

bool play(Results result[L], const char *guess, const char *hidden)
{
    bool r = true;
    bool done[5];

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
    std::cout << RST;
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

const char *find_guess(const State &state)
{
    // lookup result from cache if the best play for this state was already computed
    static std::unordered_map<const State, uint16_t, StateHash> guess_cache;
    const auto &it = guess_cache.find(state);
    if (it != guess_cache.end())
        return get_word(it->second);

    if (state.answers.size() == 1)  // only one word left
        return get_word(state.answers[0]);

    const uint8_t n = (state.words.size() > N_THREADS) ? N_THREADS : 1;
    const uint16_t block = (uint16_t)(state.words.size() / n);

    float best_score[n] = {};
    uint16_t best_word[n];

    std::thread threads[n];
    for (uint8_t id = 0; id < n; ++id)
        threads[id] = std::thread([&, id]() {
            const uint16_t start = (uint16_t)(block * id);
            const uint16_t end = (uint16_t)(start + block + ((id == n - 1) ? state.words.size() % n : 0));
            for (uint16_t i = start; i < end; ++i)
            {
                float score = 0;
                const char *guess = get_word(state.words[i]);
                for (const auto &answer : state.answers)
                {
                    Results result[L];
                    play(result, guess, get_valid_word(answer));

                    State next = state;
                    next.apply(result, guess);
                    next.valid_answers();

                    score += (float)(state.answers.size() - next.answers.size());
                }

                if (score > best_score[id])
                {
                    best_score[id] = score;
                    best_word[id] = state.words[i];
                }
            }
        });

    for (auto &thread : threads)
        thread.join();

    for (uint8_t i = 1; i < n; ++i)
        if (best_score[i] > best_score[0])
        {
            best_score[0] = best_score[i];
            best_word[0] = best_word[i];
        }

    guess_cache.emplace(state, best_word[0]);
    return get_word(best_word[0]);
}

float evaluate(bool verbose)
{
    float avg = 0;

    Results result[L];
    for (uint16_t i = 0; i < N_VALID; ++i)
    {
        State state;
        const char *hidden = get_valid_word(i);

        int j = 0;
        bool r = true;
        for (; j < 6 and r; ++j)
        {
            // simple optimization as "roate" is the first word always found
            const char *guess = (j == 0) ? "roate" : find_guess(state);
            r = not play(result, guess, hidden);
            if (verbose)
            {
                print_move(result, guess);
                std::cout << " ";
            }

            state.apply(result, guess);
            state.valid_answers();
        }

        j += r;

        if (verbose and not r)
            std::cout << j << std::endl;
        else if (verbose)
            printf("%.5s\n", hidden);

        avg += (float)j;
    }

    return avg / N_VALID;
}

void check_word(const char *hidden)
{
    State state;
    Results result[L];

    bool found = false;
    for (uint16_t i = 0; i < N_VALID and not found; ++i)
        found |= strncmp(hidden, get_valid_word(i), 5) == 0;

    if (not found)
    {
        std::cout << "Invalid word. Not in hidden list." << std::endl;
        return;
    }

    bool r = true;
    for (uint8_t i = 0; i < 6 and r; ++i)
    {
        // simple optimization as "roate" is the first word always found
        const char *guess = (i == 0) ? "roate" : find_guess(state);
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

    for (uint8_t i = 0; i < 6 and state.answers.size() > 1; ++i)
    {
        // simple optimization as "roate" is the first word always found
        const char *guess = (i == 0) ? "roate" : find_guess(state);
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

    printf("Answer: %.5s\n", get_word(state.answers[0]));
}

int main(int argc, char **argv)
{
    // Iniitalize word files
    VALID_WORDS = map("words_hidden");
    WORDS = map("words_all");

    // Initialize mask cache
    for (char c = 'a'; c <= 'z'; ++c)
        GET_MASK(c) = MASK(c);

    if (argc == 2)
        if (strncmp(argv[1], "?", 1) == 0)
            interactive();
        else
            check_word(argv[1]);

    else
    {
        float r = evaluate(true);
        std::cout << "Average Guesses: " << r << std::endl;
    }

    return 0;
}
