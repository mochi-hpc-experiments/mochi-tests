This directory contains scripts and files for running various Mochi
tests on Argonne's Polaris system.

`mobject_bedrock.json` is a simple bedrock configuration file used
for configuring the various components of Mobject.

`run_ior.qsub` is used for running IOR with Mobject while utilizing
Mochi Bake's file backend to write to Polaris' node-local SSDs at
`/local/scratch`. The script assumes that:

 * The user has Spack installed at `$HOME/spack`
 * The user has setup a Spack environment called "mochi-env" by  
   using the existing Mochi [platform configuration file for Polaris](https://github.com/mochi-hpc-experiments/platform-configurations/blob/main/ANL/Polaris/spack.yaml)

This script may need to be [edited](run_ior.qsub#L25-L28) if Spack
is installed in a different location or a differently named Spack
environment is used (or if a Spack environment isn't used).

This script also assumes that IOR has been installed with Spack
according to the spec `ior@develop+mobject ^mobject@develop`.
Instructions for setting up a Spack environment called "mochi-env"
and installing IOR within that environment can be found at
[https://github.com/mochi-hpc/mobject/blob/main/doc/FAQ.md](https://github.com/mochi-hpc/mobject/blob/main/doc/FAQ.md).


`run_ior_hdf5_rados.qsub` is used for running IOR with Mobject through
the HDF5 RADOS VOL connector; it also is setup to write to Polaris'
node-local SSDs with Mochi Bake's file backend. The script assumes
that:

 * The user has Spack installed at `$HOME/spack`
 * The user has setup a Spack environment called "mochi-env" by  
   using the existing Mochi [platform configuration file for Polaris](https://github.com/mochi-hpc-experiments/platform-configurations/blob/main/ANL/Polaris/spack.yaml)

This script may need to be [edited](run_ior_hdf5_rados.qsub#L25-L28)
if Spack is installed in a different location or a differently named
Spack environment is used (or if a Spack environment isn't used).

This script also assumes that the HDF5 RADOS VOL connector has been
installed according to the Spack spec `hdf5-rados ^mobject@develop` and that
IOR has been installed with Spack according to the spec `ior@develop+hdf5 ^hdf5@develop-1.13`.
Instructions for setting up a Spack environment called "mochi-env"
and installing the VOL connector in conjunction with IOR can be found
at [https://github.com/mochi-hpc/mobject/blob/main/doc/FAQ.md](https://github.com/mochi-hpc/mobject/blob/main/doc/FAQ.md).


`run_gpu_margo_p2p_bw.qsub` is used for running the GPU-aware Mochi
Margo point-to-point bandwidth test. The script assumes that:

 * The user has Spack installed at `$HOME/spack`
 * The user has setup a Spack environment called "mochi-env" by  
   using the existing Mochi [platform configuration file for Polaris](https://github.com/mochi-hpc-experiments/platform-configurations/blob/main/ANL/Polaris/spack.yaml)

This script may need to be [edited](run_gpu_margo_p2p_bw.qsub#L25-L28)
if Spack is installed in a different location or a differently named
Spack environment is used (or if a Spack environment isn't used).

This script also assumes that the user has already built and installed
the Mochi tests from source with a Mochi Margo installation that uses
a CUDA-enabled libfabric version. Margo can be installed in this way
with Spack according to the spec `mochi-margo@develop ^libfabric+cuda`.
The script simply has a variable called `MOCHI_TESTS_INSTALL` that should
be edited to the directory where the Mochi tests have been installed
and wherein the `gpu-margo-p2p-bw` program is available in the `bin`
directory.
