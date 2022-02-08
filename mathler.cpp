enum Results
{
    BLACK = 0,
    YELLOW = 1,
    GREEN = 2,
};

// result to color code
static const char *COLOR[3] = {BLK, YEL, GRN};

#define L 6u  // number of letters
#define NUMBERS = 0x03FF
#define ALL_SYM = 0x3FFF

enum Symbols
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
// 1 op 4       10 * 4 * 10000
// 2 op 3       100 * 4 * 1000
// 2 op 1 op 1  10 * 4 * 10 * 4 * 100
// 3 op 2       100 * 4 * 1000
// 4 op 1       10 * 4 * 10000 = 2080000 total - ablate with only valid options

struct Expression
{
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

void interactive(const char *arg)
{
}

int main(int argc, char **argv)
{
    if (argc == 2)
        if (strncmp(argv[1], "?", 1) == 0)
            interactive(argv[1]);

        else
        {
            // float r = evaluate(true);
            // std::cout << "Average Guesses: " << r << std::endl;
        }

    return 0;
}
