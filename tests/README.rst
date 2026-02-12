README
======

C test is using Criterion. YET NO!

To run test, go to build directory and execute::

    ctest

List available tests::

    ctest -N

Runs only test #5::

    ctest -I 5,5

Run with verbose::

    ctest -I 5,5 -V
