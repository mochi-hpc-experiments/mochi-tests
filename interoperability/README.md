# interoperability tests

This subdirectory contains demo programs that show how to use margo in
conjunction with other technologies.

* margo-plus-non-margo
  * Example: margo sharing access to Mercury with code that does not use
    margo.  The non-margo code is found in lib-nm.[ch].  It sets up a
    separate context to prevent callbacks from mingling.
    * Note: as of Mercury 2.0rc1, this capability is not uniformly supported
      across all transports.  You can confirm by looking at the output to
      see if the noop_ult and nm_noop_rpc_cb are executed by the same tid or
      not.  The are intended to execute on differnet tids if the context
      functionality is working correctly.
* margo-calls-from-pthreads
  * Example: a margo program that also spawns pthreads that issue concurrent
    margo forward calls.
