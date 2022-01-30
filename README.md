# Wordle Solver

Automatic solver for Wordle.

## Compiling
Simply compile with CMake:
```sh
mkdir build
cd build
cmake ..
make
```

## Running
While in the build directory, run:
```sh
./wordle
```
This will print out all the traces for every possible word, and finally the average number of guesses over the entire corpus.

A specific word can be analyzed with:
```sh
./wordle <word>
```
Which prints out the internal state and individual guesses.
This internal state consists of the total number of valid guesses and answers, as well as:
- Known yellow letters
- The valid letters for each position of the word.
