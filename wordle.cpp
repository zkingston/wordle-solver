#include <iostream>
#include <numeric>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define BLK "\e[0;30m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define RST "\e[0m"

#define L 5u
#define N_VALID 2315u
#define N_WORDS 12972u

#define ALL_26 0x03FFFFFF
#define MASK(c) (uint32_t)(1 << (c - 'a'))

enum Results {
  BLACK = 0,
  YELLOW = 1,
  GREEN = 2,
};

static const char *COLOR[3] = {BLK, YEL, GRN};

static char *map(const std::string &file) {
  int fd = open(file.c_str(), O_RDONLY);

  struct stat s;
  fstat(fd, &s);

  return (char *)mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
}

static char *VALID_WORDS = map("../words_hidden");
static char *WORDS = map("../words_all");

const char *get_valid_word(uint16_t idx) { return &VALID_WORDS[6 * idx]; }
const char *get_word(uint16_t idx) { return &WORDS[6 * idx]; }

struct State {
  uint32_t valid[L] = {ALL_26, ALL_26, ALL_26, ALL_26, ALL_26};
  uint32_t include = 0;

  std::vector<uint16_t> words;
  std::vector<uint16_t> answers;

  State() {
    words.resize(N_WORDS);
    std::iota(words.begin(), words.end(), 0);
    answers.resize(N_VALID);
    std::iota(answers.begin(), answers.end(), 0);
  }

  State(const State &other) : include(other.include), words(other.words) {
    memcpy(valid, other.valid, sizeof(uint32_t) * L);
  }

  void apply(const Results result[L], const char *guess) {
    uint32_t temp = 0;
    for (uint8_t i = 0; i < L; ++i) {
      const uint32_t m = MASK(guess[i]);
      if (result[i] == GREEN) // Must be this value.
        valid[i] = m;
      else if (result[i] == YELLOW) { // This position cannot be this value.
        valid[i] &= ~m;
        include |= m; // But this value must be elsewhere
        temp |= m;
      } else if (not(temp & m))
        for (uint8_t j = 0; j < L; ++j) // No position can be this value.
          if (valid[j] - m)
            valid[j] &= ~m;
    }
  }

  void print_mask(std::ostream &out, uint32_t mask) const {
    for (uint8_t j = 0; j < 26; ++j)
      out << ((mask & (1 << j)) ? (char)('a' + j) : '_');
  }

  void print(std::ostream &out) const {
    print_mask(out, include);
    out << std::endl;
    for (uint8_t i = 0; i < L; ++i) {
      print_mask(out, valid[i]);
      out << std::endl;
    }
  }

  bool is_valid(const char *word) const {
    bool r = true;

    uint32_t required = include;
    for (uint8_t i = 0; i < L and r; ++i) {
      const uint32_t m = MASK(word[i]);
      required -= required & m;
      r &= (bool)(valid[i] & m);
    }

    return not required and r;
  }

  void valid_sets() {
    uint16_t j = 0;
    for (uint16_t i = 0; i < N_WORDS; ++i)
      if (is_valid(get_word(i)))
        words[j++] = i;

    words.resize(j);

    uint16_t k = 0;
    for (uint16_t i = 0; i < N_VALID; ++i)
      if (is_valid(get_word(i)))
        answers[k++] = i;

    answers.resize(k);
  }
};

bool play(Results result[L], const char *guess, const char *hidden) {
  bool r = true;
  bool done[5];

  for (uint8_t i = 0; i < L; ++i) {
    done[i] = guess[i] == hidden[i];
    r &= done[i];
    result[i] = (done[i]) ? GREEN : BLACK;
  }

  if (r)
    return true;

  for (uint8_t i = 0; i < L; ++i) {
    if (result[i] == GREEN)
      continue;

    for (uint8_t j = 1; j < L; ++j) {
      const unsigned int idx = (i + j) % L;

      if (not done[idx] and guess[i] == hidden[idx]) {
        done[idx] = true;
        result[i] = YELLOW;
        break;
      }
    }
  }

  return false;
}

void print_move(std::ostream &out, Results result[L], const char *guess,
                const char *hidden) {
  for (uint8_t i = 0; i < L; ++i)
    out << COLOR[result[i]] << guess[i] << RST;
  out << " / ";
  for (uint8_t i = 0; i < L; ++i)
    out << hidden[i];
}

uint32_t result_to_score(const Results result[L]) {
  uint32_t r = 0;
  for (uint8_t i = 0; i < L; ++i)
    r += result[i];
  return r;
}

const char *guess_random(const State &state) {
  return get_word(state.words[rand() % state.words.size()]);
}

const char *guess_brute(const State &state) {
  Results result[L];

  uint32_t best_score = 0;
  uint16_t best_word = 0;

  for (uint16_t i = 0; i < state.words.size(); ++i) {
    uint32_t score = 0;
    for (uint16_t j = 0; j < state.answers.size(); ++j) {
      play(result, get_word(state.words[i]), get_valid_word(state.answers[j]));
      score += result_to_score(result);
    }

    if (score > best_score) {
      best_score = score;
      best_word = i;
    }
  }

  return get_word(state.words[best_word]);
}

int main(int argc, char **argv) {
  for (;;) {
    State state;
    Results result[L];
    const char *hidden = get_valid_word(rand() % N_VALID);
    bool r = true;

    for (int i = 0; i < 6 and r; ++i) {
      const char *guess = guess_brute(state);

      r = not play(result, guess, hidden);

      print_move(std::cout, result, guess, hidden);
      std::cout << std::endl;

      state.apply(result, guess);
      state.print(std::cout);
      state.valid_sets();

      std::cin.ignore();
    }
  }

  return 0;
}
