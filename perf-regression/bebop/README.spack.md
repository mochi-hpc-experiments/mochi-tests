To compile the full stack on bebop as of August 2018 using spack:

# load numactl in environment; it looks like this is an implied dependency
# of one of the packages

```
module load numactl
```

# get spack repos:
```
git clone https://github.com/spack/spack.git
cd spack
. share/spack/setup-env.sh

git clone https://github.com/mochi-hpc/mochi-spack-packages
cd mochi-spack-packages
spack repo add .
```
This directory contains a packages.yaml that should be copied to one of the spack
configuration locations documented here: https://spack.readthedocs.io/en/stable/configuration.html .  This could be your ~/.spack/linux/ directory, for example.
The automation scripts in this directory use $SPACK_ROOT/etc/spack to avoid
perturbing the account on which the regression tests are executed.

```
# compile everything and load module for ssg
spack install ssg
spack load -r ssg
```
