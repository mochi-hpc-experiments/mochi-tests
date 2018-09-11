To compile the full stack on cooley as of June 2018 using spack:

# set up .soft.cooley like this:
```
+gcc-7.1.0
+autotools-feb2016
+cmake-3.9.1
+mvapich2
@default
```

# get spack repos:
```
git clone https://github.com/spack/spack.git
# as of 2018-06-12, need to pull in PR changes to allow SPACK_SHELL override
cd spack
git remote add jrood-nrel https://github.com/jrood-nrel/spack.git
git fetch --all
git merge jrood-nrel/fix_spack_shell_bootstrap
. share/spack/setup-env.sh
spack bootstrap
git clone git@xgitlab.cels.anl.gov:sds/sds-repo
spack repo add .
```

This directory contains a packages.yaml that should be copied to one of the spack
configuration locations documented here: https://spack.readthedocs.io/en/stable/configuration.html .  This could be your ~/.spack/linux/ directory, for example.
The automation scripts in this directory use $SPACK_ROOT/etc/spack to avoid
perturbing the account on which the regression tests are executed.

```
# compile everything and load module for ssg
# note that the --dirty option is needed because gcc 7.1 on cooley only works if it can inherit LD_LIBRARY_PATH from softenv
spack install --dirty ssg
spack load -r ssg
```
