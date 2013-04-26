This is an optimizing compiler of
[Brainfuck](http://en.wikipedia.org/wiki/Brainfuck) programming language, that
I've written back in 2006. It can join sequences of simple instructions and even
unroll some loops.

Here are the features of the implemented BF dialect:

* The main cell type is `char`.
* The tape is growing to the right as the carriage reaches the right edge, so
  there's no strict limit to its length.
* There are no checks for moving the carriage to left of the first cell.
  Changing the cells there may result in segmentation fault.

Usage
-----

    ./bfc <input Bf file> <output C file>
    gcc <C file> -o <executable>
    ./<executable>
