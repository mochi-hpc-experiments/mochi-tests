To compile the full stack on cooley as of May 2019 using spack:

1. set up softenv by following the "soft add" commands in run-regression.sh, or add them to your .soft.cooley file
2. get spack repos:
```
git clone https://github.com/spack/spack.git
git clone git@xgitlab.cels.anl.gov:sds/sds-repo
cd spack
. share/spack/setup-env.sh
spack bootstrap
cd ../sds-repo
spack repo add .
```
3. copy packages.yaml from this directory to your personal configuration location (usually ~/.spack/linux/)
4. compile and load the packages you want with `spack install` and `spack load`
