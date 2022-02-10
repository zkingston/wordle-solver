#include <cmath>
#include <iostream>
#include <stdint.h>
#include <string.h>
#include <thread>
#include <unordered_map>

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

#define L 6u  // number of letters
#define NUMBERS 0x03FF
#define ALL_SYM 0x3FFF

enum Symbol
{
    // 0 - 9 are simply (1 - 10)
    PLUS = 1 << 11,
    MINUS = 1 << 12,
    MULT = 1 << 13,
    DIV = 1 << 14,
};

// structures
// 1 op 1 op 2  10 * 4 * 10 * 4 * 100
// 1 op 2 op 1  10 * 4 * 10 * 4 * 100
// 2 op 1 op 1  10 * 4 * 10 * 4 * 100

// 1 op 4       10 * 4 * 10000
// 2 op 3       100 * 4 * 1000
// 3 op 2       100 * 4 * 1000
// 4 op 1       10 * 4 * 10000 = 2080000 total - ablate with only valid options

struct State
{
    uint16_t valid[L] = {NUMBERS, ALL_SYM, ALL_SYM, ALL_SYM, ALL_SYM, NUMBERS};
    uint32_t include = 0;  // values that must be included
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

struct Pair
{
    uint16_t a, b;
    Symbol op;
};

void interactive(uint32_t value)
{
    static uint16_t pow10[10] = {1, 10, 100, 1000, 10000};

    // first digit
    // for (uint16_t v = 1; v <= 9999; ++v)
    for (uint16_t v = 1; v <= 9999; ++v)
    {
        const uint8_t vn = (v >= 1000) ? 4 : ((v >= 100) ? 3 : ((v >= 10) ? 2 : 1));
        const uint16_t w_max = pow10[5 - vn] - 1;

        for (uint16_t w = 1; w <= w_max; ++w)
        {
            const uint8_t wn = (w >= 1000) ? 4 : ((w >= 100) ? 3 : ((w >= 10) ? 2 : 1));
            const uint16_t x_max = pow10[5 - vn - wn] - 1;

            for (uint16_t x = 1; x <= x_max; ++x)
            {
                const uint8_t xn = (x >= 100) ? 3 : ((x >= 10) ? 2 : 1);

                if (vn + wn + xn == 4)
                {
                    if (v + w + x == value)
                        std::cout << v << "+" << w << "+" << x << std::endl;
                    if (v + w - x == value)
                        std::cout << v << "+" << w << "-" << x << std::endl;
                    if (v + w * x == value)
                        std::cout << v << "+" << w << "*" << x << std::endl;

                    if (v - w + x == value)
                        std::cout << v << "-" << w << "+" << x << std::endl;
                    if (v - w - x == value)
                        std::cout << v << "-" << w << "-" << x << std::endl;
                    if (v - w * x == value)
                        std::cout << v << "-" << w << "*" << x << std::endl;

                    if (v * w + x == value)
                        std::cout << v << "*" << w << "+" << x << std::endl;
                    if (v * w - x == value)
                        std::cout << v << "*" << w << "-" << x << std::endl;
                    if (v * w * x == value)
                        std::cout << v << "*" << w << "*" << x << std::endl;

                    const bool vwd = v % w == 0;
                    const bool wxd = w % x == 0;

                    if (vwd)
                    {
                        if (v / w + x == value)
                            std::cout << v << "/" << w << "+" << x << std::endl;
                        if (v / w - x == value)
                            std::cout << v << "/" << w << "-" << x << std::endl;
                        if (v / w * x == value)
                            std::cout << v << "/" << w << "*" << x << std::endl;
                    }

                    if (wxd)
                    {
                        if (v + w / x == value)
                            std::cout << v << "+" << w << "/" << x << std::endl;
                        if (v - w / x == value)
                            std::cout << v << "-" << w << "/" << x << std::endl;
                    }

                    if ((v * w) % x == 0 and v * w / x == value)
                        std::cout << v << "*" << w << "/" << x << std::endl;

                    if (vwd and wxd and (v / w / x) == value)
                        std::cout << v << "/" << w << "/" << x << std::endl;
                }
            }

            if (vn + wn == 5)
            {
                if (v + w == value)
                    std::cout << v << "+" << w << std::endl;
                if (v - w == value)
                    std::cout << v << "-" << w << std::endl;
                if (v * w == value)
                    std::cout << v << "*" << w << std::endl;
                if (v % w == 0 and v / w == value)
                    std::cout << v << "/" << w << std::endl;
            }
        }
    }

    // 1 + 4 (value must be >= 1000 <= 10008) same as 4 + 1
    // 2 + 3 (value must be >= 110 and <= 1098) same as 3 + 2

    // 1 * 4 (value must be 0 or >= 1000 and <= 89991) same as 4 * 1
    // 2 * 3 (value must be >= 1000 and <= 98901) same as 3 * 2
    // 3 - 2 (value must be >= 1 and <= 989)
    // 3 / 2 (value must be >= 2 and <= 99)
    // 4 - 1 (value must be >= 991 and <= 9999)
    // 4 / 1 (value must be >= 111.11 and <= 9999)

    // a = (1 + 1) (1 * 1) (1 / 1) (1 - 1)
    // (a + 2) (a * 2) (a - 2)
    // (2 + a) (2 * a) (2 - a)

    // b = (1 + 2) (1 * 2)
    // (b + 1) (b - 1)
    // (2 + 1) (2 * 1) (2 / 1) (2 - 1)
}

int main(int argc, char **argv)
{
    if (argc == 2)
        interactive(atoi(argv[1]));

    return 0;
}
