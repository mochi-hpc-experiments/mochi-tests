# interoperability tests

This subdirectory contains demo programs that show how to use margo in
conjunction with other technologies.

* margo-plus-non-margo
  * margo sharing access to Mercury with code that does not use margo.  The
    non-margo code is found in lib-nm.[ch].  It deliberately sets up a
    separate context on both the client and server to prevent callbacks from
    mingling.
