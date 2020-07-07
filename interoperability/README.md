# interoperability tests

This subdirectory contains demo programs that show how to use margo in
conjunction with other technologies.

* margo-plus-non-margo
  * Example: margo sharing access to Mercury with code that does not use
    margo.  The non-margo code is found in lib-nm.[ch].  It sets up a
    separate context to prevent callbacks from mingling.
* margo-calls-from-pthreads
  * Example: a margo program that also spawns pthreads that issue concurrent
    margo forward calls.
