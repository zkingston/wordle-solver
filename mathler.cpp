#include <cmath>
#include <iostream>
#include <numeric>
#include <stdint.h>
#include <string.h>
#include <thread>
#include <unordered_map>
#include <vector>

// color codes
#define BLK "\e[0;30m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define RST "\e[0m"

enum Results
{
    BLACK = 0,
    YELLOW = 1,
    GREEN = 2,
};

// result to color code
static const char *COLOR[3] = {BLK, YEL, GRN};

#define L 6
#define N_THREADS 8

#define NUMBERS 0x3FF0
#define NO_ZERO 0x3FE0
#define ALL_SYM 0x3FFF
#define SYMMASK 0x000F

enum Symbol
{
    // 0 - 9 are simply (1 - 10) + 4 (1 << 4 onward)
    PLUS = 1 << 0,
    MINUS = 1 << 1,
    MULT = 1 << 2,
    DIV = 1 << 3,
};

static char SYMBOLS[9] = {0, '+', '-', 0, '*', 0, 0, 0, '/'};

inline uint16_t get_mask(char c)
{
    if ('0' <= c and c <= '9')
        return (uint16_t)(1 << (c - '0' + 4));
    else if (c == '+')
        return PLUS;
    else if (c == '-')
        return MINUS;
    else if (c == '*')
        return MULT;
    else if (c == '/')
        return DIV;
    return 0;
}

static char EXPS[2000][L];
static uint16_t NUM_EXPS;

struct State
{
    uint16_t valid[L] = {NO_ZERO, ALL_SYM, ALL_SYM, ALL_SYM, ALL_SYM, NUMBERS};
    uint32_t include = 0;  // values that must be included

    std::vector<uint16_t> answers;

    State()
    {
        answers.resize(NUM_EXPS);
        std::iota(answers.begin(), answers.end(), 0);
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
            const uint16_t m = get_mask(guess[i]);
            if (result[i] == GREEN)  // must be this value
                valid[i] = m;

            else if (result[i] == YELLOW)  // this position cannot be this value
            {
                valid[i] &= (uint16_t)~m;
                include |= m;  // but this value must be elsewhere
                temp |= m;     // keep track of prior yellow, as,
            }
            else
            {
                if (not(temp & m))  // if yellow of this letter not seen,
                {
                    for (uint8_t j = 0; j < L; ++j)  // no position can be this value
                        if (valid[j] - m)            // there are other values, not just this one
                            valid[j] &= (uint16_t)~m;
                }
                else  // else just this position is not this letter
                    valid[i] &= (uint16_t)~m;
            }
        }
    }

    void print_mask(uint32_t mask) const
    {
        for (uint8_t j = 0; j < 10; ++j)
            std::cout << ((mask & (1 << (j + 4))) ? (char)('0' + j) : '_');

        for (const auto &symbol : {PLUS, MINUS, MULT, DIV})
            std::cout << ((mask & symbol) ? SYMBOLS[symbol] : '_');

        std::cout << std::endl;
    }

    void print() const
    {
        std::cout << NUM_EXPS << " / " << answers.size() << std::endl;
        print_mask(include);
        for (uint8_t i = 0; i < L; ++i)
            print_mask(valid[i]);
    }

    inline bool is_valid(const char *word) const
    {
        bool r = true;
        uint32_t required = include;
        for (uint8_t i = 0; i < L and r; ++i)
        {
            const uint32_t m = get_mask(word[i]);
            r &= (bool)(valid[i] & m);
            required -= required & m;
        }

        return r and not required;
    }

    void valid_answers()
    {
        uint16_t j = 0;
        for (uint16_t i = 0; i < answers.size(); ++i)
            if (is_valid(EXPS[answers[i]]))
                answers[j++] = answers[i];

        answers.resize(j);
    }
};

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

inline uint8_t n_digits(uint16_t v)
{
    return (v >= 1000) ? 4 : ((v >= 100) ? 3 : ((v >= 10) ? 2 : 1));
}

inline void to_expression(char *s, uint16_t a, Symbol op1, uint16_t b)
{
    snprintf(s, L + 1, "%u%c%u", a, SYMBOLS[op1], b);
}

inline void to_expression(char *s, uint16_t a, Symbol op1, uint16_t b, Symbol op2, uint16_t c)
{
    snprintf(s, L + 1, "%u%c%u%c%u", a, SYMBOLS[op1], b, SYMBOLS[op2], c);
}

void populate_expressions(uint16_t value)
{
    NUM_EXPS = 0;

    static uint16_t maxes[5] = {0, 9, 99, 999, 9999};
    for (uint16_t v = 1; v <= maxes[4]; ++v)
    {
        const uint8_t vn = n_digits(v);
        const uint16_t w_max = maxes[5 - vn];

        for (uint16_t w = 1; w <= w_max; ++w)
        {
            const uint8_t wn = n_digits(w);
            const uint16_t x_max = maxes[5 - vn - wn];

            for (uint16_t x = 1; x <= x_max; ++x)
            {
                const uint8_t xn = n_digits(x);

                if (vn + wn + xn == 4)
                {
                    if ((uint16_t)(v + w + x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, PLUS, w, PLUS, x);
                    if ((uint16_t)(v + w - x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, PLUS, w, MINUS, x);
                    if ((uint16_t)(v + w * x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, PLUS, w, MULT, x);
                    if ((uint16_t)(v - w * x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, MINUS, w, MULT, x);
                    if ((uint16_t)(v * w + x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, MULT, w, PLUS, x);
                    if ((uint16_t)(v * w - x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, MULT, w, MINUS, x);
                    if ((uint16_t)(v * w * x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, MULT, w, MULT, x);

                    if (v > w)
                    {
                        if ((uint16_t)(v - w + x) == value)
                            to_expression(EXPS[NUM_EXPS++], v, MINUS, w, PLUS, x);
                        if ((uint16_t)(v - w - x) == value)
                            to_expression(EXPS[NUM_EXPS++], v, MINUS, w, MINUS, x);
                    }

                    const bool vwd = v % w == 0;
                    const bool wxd = w % x == 0;

                    if (vwd)
                    {
                        if ((uint16_t)(v / w + x) == value)
                            to_expression(EXPS[NUM_EXPS++], v, DIV, w, PLUS, x);
                        if ((uint16_t)(v / w - x) == value)
                            to_expression(EXPS[NUM_EXPS++], v, DIV, w, MINUS, x);
                        if ((uint16_t)(v / w * x) == value)
                            to_expression(EXPS[NUM_EXPS++], v, DIV, w, MULT, x);
                    }

                    if (wxd)
                    {
                        if ((uint16_t)(v + w / x) == value)
                            to_expression(EXPS[NUM_EXPS++], v, PLUS, w, DIV, x);
                        if ((uint16_t)(v - w / x) == value)
                            to_expression(EXPS[NUM_EXPS++], v, MINUS, w, DIV, x);
                    }

                    if ((v * w) % x == 0 and (uint16_t)(v * w / x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, MULT, w, DIV, x);
                    if (vwd and (v / w) % x == 0 and (uint16_t)(v / w / x) == value)
                        to_expression(EXPS[NUM_EXPS++], v, DIV, w, DIV, x);
                }
            }

            if (vn + wn == 5)
            {
                if ((uint16_t)(v + w) == value)
                    to_expression(EXPS[NUM_EXPS++], v, PLUS, w);
                if ((uint16_t)(v - w) == value)
                    to_expression(EXPS[NUM_EXPS++], v, MINUS, w);
                if ((uint16_t)(v * w) == value)
                    to_expression(EXPS[NUM_EXPS++], v, MULT, w);
                if (v % w == 0 and (uint16_t)(v / w) == value)
                    to_expression(EXPS[NUM_EXPS++], v, DIV, w);
            }
        }
    }
}

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
    std::cout << RST;
}

static std::unordered_map<const State, uint16_t, StateHash> guess_cache;
const char *find_guess(const State &state)
{
    // lookup result from cache if the best play for this state was already computed
    const auto &it = guess_cache.find(state);
    if (it != guess_cache.end())
        return EXPS[it->second];

    if (state.answers.size() == 1)  // only one word left
        return EXPS[state.answers[0]];

    const uint8_t n = (NUM_EXPS > N_THREADS) ? N_THREADS : 1;
    const uint16_t block = (uint16_t)(NUM_EXPS / n);

    float best_score[n] = {};
    uint16_t best_word[n];

    std::thread threads[n];
    for (uint8_t id = 0; id < n; ++id)
        threads[id] = std::thread([&, id]() {
            const uint16_t start = (uint16_t)(block * id);
            const uint16_t end = (uint16_t)(start + block + ((id == n - 1) ? NUM_EXPS % n : 0));
            for (uint16_t i = start; i < end; ++i)
            {
                float score = 0;
                const char *guess = EXPS[i];
                for (const auto &answer : state.answers)
                {
                    Results result[L];
                    play(result, guess, EXPS[answer]);

                    State next = state;
                    next.apply(result, guess);
                    next.valid_answers();

                    score += (float)(state.answers.size() - next.answers.size());
                }

                if (score > best_score[id])
                {
                    best_score[id] = score;
                    best_word[id] = i;
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
    return EXPS[best_word[0]];
}

void check_word(const char *hidden)
{
    State state;
    Results result[L];

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

    for (uint8_t i = 0; i < 6 and state.answers.size() > 1; ++i)
    {
        const char *guess = find_guess(state);
        printf("Guess : %.6s\nResult: ", guess);

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

    printf("Answer: %.6s\n", EXPS[state.answers[0]]);
}

float evaluate(uint16_t value, bool all = false, bool verbose = true)
{
    const uint16_t min_v = (all) ? 0 : value;
    const uint16_t max_v = (all) ? 100 : (uint16_t)(value + 1);

    float global_avg = 0;
    Results result[L];
    for (uint16_t v = min_v; v < max_v; ++v)
    {
        guess_cache.clear();
        populate_expressions(v);

        float avg = 0;
        for (uint16_t i = 0; i < NUM_EXPS; ++i)
        {
            State state;
            const char *hidden = EXPS[i];

            int j = 0;
            bool r = true;
            for (; j < 6 and r; ++j)
            {
                const char *guess = find_guess(state);
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
                printf("%.6s\n", hidden);

            avg += (float)j;
        }

        avg /= (float)NUM_EXPS;
        global_avg += avg;
    }

    global_avg /= (float)(max_v - min_v);
    return global_avg;
}

int main(int argc, char **argv)
{
    if (argc == 2)
    {
        uint16_t value = (uint16_t)atoi(argv[1]);
        float r = evaluate(value, false);
        std::cout << "Average Guesses: " << r << std::endl;
    }
    else if (argc == 3)
    {
        uint16_t value = (uint16_t)atoi(argv[2]);
        populate_expressions(value);

        if (strncmp(argv[1], "?", 1) == 0)
            interactive();

        else if (strncmp(argv[1], "list", 4) == 0)
        {
            for (uint16_t i = 0; i < NUM_EXPS; ++i)
                printf("%.6s\n", EXPS[i]);

            std::cout << "Total: " << NUM_EXPS << std::endl;
        }
        else
            check_word(argv[1]);
    }
    else
    {
        float r = evaluate(0, true);
        std::cout << "Average Guesses: " << r << std::endl;
    }

    return 0;
}
