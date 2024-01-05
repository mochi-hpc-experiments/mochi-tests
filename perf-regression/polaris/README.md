# Overview

This directory contains scripts and files for running various Mochi
tests on Argonne's Polaris system.

A basic collection of Margo performance regression tests can be compiled and
executed via the run-regression.sh script from a Polaris login node.

The same collection of Margo performance regression tests is also executed
nightly using a scheduled pipeline on gitlab-ci.alcf.anl.gov.

# Additional manual test cases

Additional information about additional individual test cases can be found
below:

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


`run_gpu_margo_p2p_bw.qsub` is a simple script used for running the
GPU-aware Mochi Margo point-to-point bandwidth test on Polaris. This
script should be edited so that the `MOCHI_TESTS_INSTALL` variable
points to the directory where the Mochi Margo tests have been installed
and wherein the `gpu-margo-p2p-bw` program is available under the `bin`
directory. This script may also need to be [edited](run_gpu_margo_p2p_bw.qsub#L25-L28)
if Spack is installed in a different location or a differently named
Spack environment is used (or if a Spack environment isn't used).
The script assumes that:

 * The user has Spack installed at `$HOME/spack`
 * The user has setup a Spack environment called "mochi-env" by  
   using the existing Mochi [platform configuration file for Polaris](https://github.com/mochi-hpc-experiments/platform-configurations/blob/main/ANL/Polaris/spack.yaml)
 * The user has installed Mochi Margo with SSG and CUDA-enabled libfabric
   support. Once the user has a Spack environment setup and activated
   (`spack env activate mochi-env`) from the previous point, installing
   Mochi Margo this way can be done with Spack by running
   `spack add mochi-margo@develop ^libfabric+cuda`, `spack add mochi-ssg`,
   `spack install`
 * The user has built and installed the Mochi Margo tests from source
   and installed them on the system somewhere. The user may need to run
   `spack load mochi-margo` before trying to install the Mochi Margo
   tests. The user may also need to run `module load cray-mpich` and set
   `CC=cc` before running `configure` when building the tests in order for
   MPI support to be properly detected.

**NOTE:** The Polaris platform configuration file currently uses GCC
for compilation, whereas the system's default compilers are the `nvhpc`
compilers. To ensure that everything is built with the same compiler,
this means that the user should probably switch to the GNU programming
environment on Polaris before building the Mochi Margo tests. This can
be done by running:

    module swap PrgEnv-nvhpc PrgEnv-gnu
    module load nvhpc-mixed (optional to have nvhpc compilers still available)

Further, the default `nvcc` compiler that is loaded into the system
path appears to be too new to correctly compile the CUDA code in the
Mochi Margo point-to-point bandwidth test so that it can run on the
GPUs attached to the compute nodes. If running the
`run_gpu_margo_p2p_bw.qsub` script gives output similar to the following:

    Error: unable to cudaMalloc 524288000 byte buffer

this is a sign that the Mochi Margo test code was compiled with the
newer `nvcc`. To fix this issue, the user should run:

    module load cudatoolkit-standalone/11.4.4

and then recompile the test code.
