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

void interactive(uint32_t value)
{
    static uint16_t maxes[10] = {0, 9, 99, 999, 9999};

    for (uint16_t v = 1; v <= 9999; ++v)
    {
        const uint8_t vn = (v >= 1000) ? 4 : ((v >= 100) ? 3 : ((v >= 10) ? 2 : 1));
        const uint16_t w_max = maxes[5 - vn];

        for (uint16_t w = 1; w <= w_max; ++w)
        {
            const uint8_t wn = (w >= 1000) ? 4 : ((w >= 100) ? 3 : ((w >= 10) ? 2 : 1));
            const uint16_t x_max = maxes[5 - vn - wn];

            for (uint16_t x = 1; x <= x_max; ++x)
            {
                const uint8_t xn = (x >= 100) ? 3 : ((x >= 10) ? 2 : 1);

                if (vn + wn + xn == 4)
                {
                    const bool vwd = v % w == 0;
                    const bool wxd = w % x == 0;

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
}

int main(int argc, char **argv)
{
    if (argc == 2)
        interactive(atoi(argv[1]));

    return 0;
}
