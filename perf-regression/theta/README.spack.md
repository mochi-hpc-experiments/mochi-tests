To compile the full stack on theta as of August 2018 using spack:
```

# get spack repos:
```
git clone https://github.com/spack/spack.git
cd spack
. share/spack/setup-env.sh

git clone git@xgitlab.cels.anl.gov:sds/sds-repo
spack repo add .
```

This directory contains a packages.yaml that should be copied to one of the spack
configuration locations documented here: https://spack.readthedocs.io/en/stable/configuration.html .  This could be your ~/.spack/cray/ directory, for example. 
The automation scripts in this directory use $SPACK_ROOT/etc/spack to avoid 
perturbing the account on which the regression tests are executed.

```
# compile everything and load module for ssg
spack install ssg
# you may need to re-run setup-env.sh before loading to avoid some problems with finding modules
spack load -r ssg
```
