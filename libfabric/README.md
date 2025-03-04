# Libfabric things

> Let's ask GPT to write us a benchmark for libfabric collectives.

After several rounds of prompting, we have a benchmark that... segfaults.

A lot of the major pieces seem to be here:

- timing harness
- address exchange via MPI
- some error reporting
- benchmarking `fi_barrier` and `fi_broadcast` routines.
