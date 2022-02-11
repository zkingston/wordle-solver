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
#define ALL_SYM 0x3FFF

enum Symbol
{
    // 0 - 9 are simply (1 - 10) + 4 (1 << 4 onward)
    PLUS = 1 << 0,
    MINUS = 1 << 1,
    MULT = 1 << 2,
    DIV = 1 << 3,
};

static char SYMBOLS[9] = {0, '+', '-', 0, '*', 0, 0, 0, '/'};

struct Expression
{
    char s[L + 1];
    uint16_t v[3] = {0, 0, 0};
    Symbol op[2] = {PLUS, PLUS};

    Expression(uint16_t a, Symbol op1, uint16_t b)
    {
        v[0] = a;
        v[1] = b;
        op[0] = op1;
        snprintf(s, L + 1, "%u%c%u", a, SYMBOLS[op1], b);
    }

    Expression(uint16_t a, Symbol op1, uint16_t b, Symbol op2, uint16_t c)
    {
        v[0] = a;
        v[1] = b;
        v[2] = c;
        op[0] = op1;
        op[1] = op2;
        snprintf(s, L + 1, "%u%c%u%c%u", a, SYMBOLS[op1], b, SYMBOLS[op2], c);
    }

    void print() const
    {
        printf("%.6s\n", s);
    }
};

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

static std::vector<Expression> expressions;
struct State
{
    uint16_t valid[L] = {NUMBERS, ALL_SYM, ALL_SYM, ALL_SYM, ALL_SYM, NUMBERS};
    uint32_t include = 0;  // values that must be included

    std::vector<uint16_t> answers;

    State()
    {
        answers.resize(expressions.size());
        std::iota(answers.begin(), answers.end(), 0);
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
        std::cout << expressions.size() << " / " << answers.size() << std::endl;
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
            if (is_valid(expressions[answers[i]].s))
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

void populate_expressions(uint32_t value)
{
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
                    if ((uint32_t)(v + w + x) == value)
                        expressions.emplace_back(v, PLUS, w, PLUS, x);
                    if ((uint32_t)(v + w - x) == value)
                        expressions.emplace_back(v, PLUS, w, MINUS, x);
                    if ((uint32_t)(v + w * x) == value)
                        expressions.emplace_back(v, PLUS, w, MULT, x);
                    if ((uint32_t)(v - w * x) == value)
                        expressions.emplace_back(v, MINUS, w, MULT, x);
                    if ((uint32_t)(v * w + x) == value)
                        expressions.emplace_back(v, MULT, w, PLUS, x);
                    if ((uint32_t)(v * w - x) == value)
                        expressions.emplace_back(v, MULT, w, MINUS, x);
                    if ((uint32_t)(v * w * x) == value)
                        expressions.emplace_back(v, MULT, w, MULT, x);

                    if (v > w)
                    {
                        if ((uint32_t)(v - w + x) == value)
                            expressions.emplace_back(v, MINUS, w, PLUS, x);
                        if ((uint32_t)(v - w - x) == value)
                            expressions.emplace_back(v, MINUS, w, MINUS, x);
                    }

                    const bool vwd = v % w == 0;
                    const bool wxd = w % x == 0;
                    if (vwd)
                    {
                        if ((uint32_t)(v / w + x) == value)
                            expressions.emplace_back(v, DIV, w, PLUS, x);
                        if ((uint32_t)(v / w - x) == value)
                            expressions.emplace_back(v, DIV, w, MINUS, x);
                        if ((uint32_t)(v / w * x) == value)
                            expressions.emplace_back(v, DIV, w, MULT, x);
                    }

                    if (wxd)
                    {
                        if ((uint32_t)(v + w / x) == value)
                            expressions.emplace_back(v, PLUS, w, DIV, x);
                        if ((uint32_t)(v - w / x) == value)
                            expressions.emplace_back(v, MINUS, w, DIV, x);
                    }

                    if (vwd and wxd)
                    {
                        if ((uint32_t)(v * w / x) == value)
                            expressions.emplace_back(v, MULT, w, DIV, x);
                        if ((uint32_t)(v / w / x) == value)
                            expressions.emplace_back(v, DIV, w, DIV, x);
                    }
                }
            }

            if (vn + wn == 5)
            {
                if ((uint32_t)(v + w) == value)
                    expressions.emplace_back(v, PLUS, w);
                if ((uint32_t)(v - w) == value)
                    expressions.emplace_back(v, MINUS, w);
                if ((uint32_t)(v * w) == value)
                    expressions.emplace_back(v, MULT, w);
                if (v % w == 0 and (uint32_t)(v / w) == value)
                    expressions.emplace_back(v, DIV, w);
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

const char *find_guess(const State &state)
{
    if (state.answers.size() == 1)  // only one word left
        return expressions[state.answers[0]].s;

    const uint8_t n = (expressions.size() > N_THREADS) ? N_THREADS : 1;
    const uint16_t block = (uint16_t)(expressions.size() / n);

    float best_score[n] = {};
    uint16_t best_word[n];

    std::thread threads[n];
    for (uint8_t id = 0; id < n; ++id)
        threads[id] = std::thread([&, id]() {
            const uint16_t start = (uint16_t)(block * id);
            const uint16_t end = (uint16_t)(start + block + ((id == n - 1) ? expressions.size() % n : 0));
            for (uint16_t i = start; i < end; ++i)
            {
                float score = 0;
                const char *guess = expressions[i].s;
                for (const auto &answer : state.answers)
                {
                    Results result[L];
                    play(result, guess, expressions[answer].s);

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

    return expressions[best_word[0]].s;
}

void check_word(const char *hidden, uint32_t value)
{
    State state;
    Results result[L];

    bool r = true;
    for (uint8_t i = 0; i < 6 and r; ++i)
    {
        // const char *guess = expressions[rand() % expressions.size()].s;
        // const char *guess = expressions[state.answers[rand() % state.answers.size()]].s;
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

int main(int argc, char **argv)
{
    uint32_t value = atoi(argv[1]);
    populate_expressions(value);

    const char *hidden = expressions[rand() % expressions.size()].s;
    check_word(hidden, value);

    return 0;
}
