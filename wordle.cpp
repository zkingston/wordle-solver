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
enum Results {
  BLACK = 0,
  YELLOW = 1,
  GREEN = 2,
};

// result to color code
static const char *COLOR[3] = {BLK, YEL, GRN};

#define L 5u              // number of letters
#define N_VALID 2315u     // number of valid answers
#define N_WORDS 12972u    // number of valid guesses
#define N_THREADS 8u      // number of threads to use
#define ALL_26 0x03FFFFFF // bitmask for 26

// get bitmask for char
#define MASK(c) (uint32_t)(1 << (c - 'a'))

// cached masks for each character
static uint32_t MASKS[255];

// get the mask from the cache
#define GET_MASK(c) MASKS[(uint8_t)c]

// memory map a file
static char *map(const std::string &file) {
  int fd = open(file.c_str(), O_RDONLY);

  struct stat s;
  fstat(fd, &s);

  return (char *)mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
}

static char *VALID_WORDS = map("../words_hidden"); // valid answers
static char *WORDS = map("../words_all");          // valid guesses

// get the answer at index idx
const char *get_valid_word(uint16_t idx) { return &VALID_WORDS[6 * idx]; }

// get the guess at index idx
const char *get_word(uint16_t idx) { return &WORDS[6 * idx]; }

// state which encodes known information
// uses bitmasks to encode letter information - e.g., bit #3 corresponds to `c`
struct State {
  // bitmasks for valid characters at each position
  uint32_t valid[L] = {ALL_26, ALL_26, ALL_26, ALL_26, ALL_26};
  // letters that must be in the word
  uint32_t include = 0;

  std::vector<uint16_t> words;   // set of valid guesses given state
  std::vector<uint16_t> answers; // set of valid answers given state

  State() {
    words.resize(N_WORDS);
    std::iota(words.begin(), words.end(), 0);
    answers.resize(N_VALID);
    std::iota(answers.begin(), answers.end(), 0);
  }

  State(const State &other)
      : include(other.include), words(other.words), answers(other.answers) {
    memcpy(valid, other.valid, sizeof(uint32_t) * L);
  }

  bool operator==(const State &other) const {
    bool r = include == other.include;
    for (uint8_t i = 0; i < L and r; ++i)
      r &= valid[i] == other.valid[i];

    return r;
  }

  void apply(const Results result[L], const char *guess) {
    uint32_t temp = 0;
    for (uint8_t i = 0; i < L; ++i) {
      const uint32_t m = GET_MASK(guess[i]);
      if (result[i] == GREEN) // must be this value
        valid[i] = m;
      else if (result[i] == YELLOW) { // this position cannot be this value
        valid[i] &= ~m;
        include |= m; // but this value must be elsewhere
        temp |= m;    // keep track of prior yellow, as,
      } else {
        if (not(temp & m)) {              // if yellow of this letter not seen,
          for (uint8_t j = 0; j < L; ++j) // no position can be this value
            if (valid[j] - m)
              valid[j] &= ~m;
        } else // else just this position is not this letter
          valid[i] &= ~m;
      }
    }
  }

  void print_mask(uint32_t mask) const {
    for (uint8_t j = 0; j < 26; ++j)
      std::cout << ((mask & (1 << j)) ? (char)('a' + j) : '_');
    std::cout << std::endl;
  }

  void print() const {
    std::cout << words.size() << " / " << answers.size() << std::endl;
    print_mask(include);
    for (uint8_t i = 0; i < L; ++i)
      print_mask(valid[i]);
  }

  bool is_valid(const char *word) const {
    bool r = true;
    uint32_t required = include;
    for (uint8_t i = 0; i < L and r; ++i) {
      const uint32_t m = GET_MASK(word[i]);
      r &= (bool)(valid[i] & m);
      required -= required & m;
    }

    return r and not required;
  }

  void valid_sets() {
    uint16_t j = 0;
    for (uint16_t i = 0; i < words.size(); ++i)
      if (is_valid(get_word(words[i])))
        words[j++] = words[i];

    words.resize(j);
  }

  void valid_answers() {
    uint16_t k = 0;
    for (uint16_t i = 0; i < answers.size(); ++i)
      if (is_valid(get_word(answers[i])))
        answers[k++] = answers[i];

    answers.resize(k);
  }
};

bool play(Results result[L], const char *guess, const char *hidden) {
  bool r = true;
  bool done[5];

  for (uint8_t i = 0; i < L; ++i) {
    r &= (done[i] = guess[i] == hidden[i]);
    result[i] = (done[i]) ? GREEN : BLACK;
  }

  if (not r) {
    for (uint8_t i = 0; i < L; ++i) {
      if (result[i] != GREEN) {
        for (uint8_t j = 0; j < L; ++j) {
          if (guess[i] == hidden[j] and not done[j] and i != j) {
            done[j] = true;
            result[i] = YELLOW;
            break;
          }
        }
      }
    }
  }

  return r;
}

void print_move(Results result[L], const char *guess,
                const char *hidden = nullptr) {
  for (uint8_t i = 0; i < L; ++i)
    std::cout << COLOR[result[i]] << guess[i] << RST;
  if (hidden) {
    std::cout << " / ";
    for (uint8_t i = 0; i < L; ++i)
      std::cout << hidden[i];
  }
}

// uint32_t result_to_score(const Results result[L]) {
//     return result[0] + result[1] + result[2] + result[3] + result[4];
// }

uint32_t result_to_score(const Results result[L]) {
  uint32_t score = 1;
  for (uint8_t i = 0; i < L; ++i)
    score *= (result[i] + 1);

  return score;
}

class StateHash {
public:
  std::size_t operator()(const State &state) const {
    std::size_t r = state.include;
    for (uint8_t i = 0; i < L; ++i)
      r += state.valid[i];
    return r;
  }
};

const char *guess_random(const State &state) {
  return get_word(state.words[rand() % state.words.size()]);
}

const char *guess_brute(const State &state) {
  static std::unordered_map<const State, uint16_t, StateHash> guess_cache;
  const auto &it = guess_cache.find(state);
  if (it != guess_cache.end())
    return get_word(it->second);

  const uint8_t n = (state.words.size() > 100) ? N_THREADS : 1;
  const uint16_t block = (uint16_t)(state.words.size() / n);

  uint32_t best_score[n] = {};
  uint16_t best_word[n];

  std::thread threads[n];
  for (uint8_t id = 0; id < n; ++id)
    threads[id] = std::thread([&, id]() mutable {
      Results result[L];

      for (uint16_t i = (uint16_t)(block * id); i < block; ++i) {
        uint32_t score = 0;
        const char *guess = get_word(state.words[i]);

        for (uint16_t j = 0; j < state.answers.size(); ++j) {
          play(result, guess, get_valid_word(state.answers[j]));
          score += result_to_score(result);
        }

        score /= (uint32_t)state.answers.size();

        if (score > best_score[id]) {
          best_score[id] = score;
          best_word[id] = state.words[i];
        }
      }
    });

  for (auto &thread : threads)
    thread.join();

  for (uint8_t i = 1; i < n; ++i)
    if (best_score[i] > best_score[0]) {
      best_score[0] = best_score[i];
      best_word[0] = best_word[i];
    }

  guess_cache.emplace(state, best_word[0]);
  return get_word(best_word[0]);
}

double evaluate(const char *(*guesser)(const State &state), bool verbose) {
  double avg = 0;

  Results result[L];
  for (uint16_t i = 0; i < N_VALID; ++i) {
    State state;
    const char *hidden = get_valid_word(i);

    int j = 0;
    bool r = true;
    for (; j < 6 and r; ++j) {
      const char *guess = guesser(state);
      r = not play(result, guess, hidden);
      if (verbose) {
        print_move(result, guess);
        std::cout << " ";
      }

      state.apply(result, guess);
      state.valid_sets();
      state.valid_answers();
    }

    j += r;

    if (verbose and not r)
      std::cout << j << std::endl;
    else if (verbose)
      printf("%.5s\n", hidden);

    avg += j;
  }

  return avg / N_VALID;
}

void random_examples(const char *hidden) {
  State state;
  Results result[L];

  bool r = true;
  for (int i = 0; i < 6 and r; ++i) {
    const char *guess = guess_brute(state);
    r = not play(result, guess, hidden);

    print_move(result, guess, hidden);
    std::cout << std::endl;

    state.apply(result, guess);
    state.print();
    state.valid_sets();
    state.valid_answers();

    std::cout << "Press Enter" << std::endl;
    std::cin.ignore();
  }
}

int main(int argc, char **argv) {
  // Initialize mask cache
  for (char c = 'a'; c <= 'z'; ++c)
    GET_MASK(c) = MASK(c);

  if (argc == 2)
    random_examples(argv[1]);

  else {
    double avg = evaluate(guess_brute, true);
    std::cout << "Average Guesses: " << avg << std::endl;
  }

  return 0;
}
