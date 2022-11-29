This directory contains scripts and files for running various Mochi
tests on Argonne's Polaris system.

`mobject_bedrock.json` is a simple bedrock configuration file used
for configuring the various components of Mobject.

`run_ior.qsub` is used for running IOR with Mobject while utilizing
Mochi Bake's file backend to write to Polaris' node-local SSDs at
`/local/scratch`. The script assumes that:

 * The user has spack installed at `$HOME/spack`
 * The user has setup a spack environment called "mochi-env" by  
   using the existing Mochi [platform configuration file for Polaris](https://github.com/mochi-hpc-experiments/platform-configurations/blob/main/ANL/Polaris/spack.yaml)

This script may need to be [edited](run_ior.qsub#L25-L28) if spack
is installed in a different location or a differently named spack
environment is used (or if a spack environment isn't used).

This script also assumes that IOR has been installed with spack
according to the spec `ior+mobject ^mobject@main`. Instructions for
setting up a spack environment called "mochi-env" and installing
IOR within that environment can be found at [https://github.com/mochi-hpc/mobject/blob/main/doc/FAQ.md](https://github.com/mochi-hpc/mobject/blob/main/doc/FAQ.md).


`run_ior_hdf5_rados.qsub` is used for running IOR with Mobject through
the HDF5 RADOS VOL connector; it also is setup to write to Polaris'
node-local SSDs with Mochi Bake's file backend. The script assumes
that:

 * The user has spack installed at `$HOME/spack`
 * The user has setup a spack environment called "mochi-env" by  
   using the existing Mochi [platform configuration file for Polaris](https://github.com/mochi-hpc-experiments/platform-configurations/blob/main/ANL/Polaris/spack.yaml)

This script may need to be [edited](run_ior_hdf5_rados.qsub#L25-L28) if spack
is installed in a different location or a differently named spack
environment is used (or if a spack environment isn't used).

This script also assumes that the HDF5 RADOS VOL connector has been
installed according to the spack spec `hdf5-rados` and that IOR has
been installed with Spack according to the spec `ior@develop` or `ior+mobject@develop`.
Instructions for setting up a spack environment called "mochi-env"
and installed the VOL connector in conjunction with IOR can be found
at [https://github.com/mochi-hpc/mobject/blob/main/doc/FAQ.md](https://github.com/mochi-hpc/mobject/blob/main/doc/FAQ.md).
