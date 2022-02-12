# Wordle Solver

Automatic solver for Wordle (and Mathler).

## Compiling
Simply compile with CMake:
```sh
cmake .
make
```

## Running Wordle
Run:
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

The program can also be run interactively with:
```sh
./wordle ?
```
Which will query the user for the result of a play, which is encoded as a string of length 5.
For example, if the hidden word is `wooer`, and the guess is `roate`, the string will be `ygbby`.
Type the string in, press enter, and get the next guess.

## Running Mathler
Run:
```sh
./mathler
```
This will print out all the traces for every possible equation for values 0 to 100, and finally the average number of guesses over the entire set.

The results for a specific value can be analyzed with:
```sh
./mathler <n>
```
Which prints similar output to the above, but only for equations that equal `<n>`.

The program can also be run interactively with:
```sh
./mathler ? <n>
```
Which will query the user for the result of a play, which is encoded as a string of length 6.
Be sure to put in the value as `<n>`.
Type the string in, press enter, and get the next guess.

The equations for a specific value can be output with:
```sh
./mathler list <n>
```
Which prints out the list of all equations that equal `<n>`.

Finally, a specific equation can be inspected with:
```sh
./mathler <eq> <n>
```
Which prints out the internal state and individual guesses.
This internal state consists of the total number of valid guesses and answers, as well as:
- Known yellow symbols
- The valid numbers and symbols for each position of the word.
