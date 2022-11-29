To run the margo gpu test: gpu-margo-p2p-bw.cu with the current libfabric release, 
libfabric needs to be configured with cuda support.  
See the local variant added to mochi-spack-packages/packages/libfabric/package.py.

There is an example script for executing an automated regression test on the
Thetagpu system at the ALCF.  The entire process is handled by the
"./gpu-qsub" script.  The script can be copied to a desired location where
the test may be run and submitted via "qsub ./gpu-qsub" on a
thetagpu service node.

To do it manually:
- Login to theta, then to a thetagpu service node
- You can request an interactive session on a single-gpu via:
    * qsub -A *project* -q single-gpu -n 1 -t *time* -I

- If the node you are on doesn't have outbound network connectivity,
  do the following: (see https://www.alcf.anl.gov/support/user-guides/theta-gpu/getting-started/index.html)
    * export HTTP_PROXY=http://theta-proxy.tmi.alcf.anl.gov:3128
    * export HTTPS_PROXY=http://theta-proxy.tmi.alcf.anl.gov:3128
    * export http_proxy=http://theta-proxy.tmi.alcf.anl.gov:3128
    * export https_proxy=http://theta-proxy.tmi.alcf.anl.gov:3128

- Use the latest spack from the develop branch:
    * git clone https://github.com/spack/spack
    * source spack/share/spack/setup-env.sh

- Build mochi-margo with the latest from the main branch so as to get a new enough margo and mercury that
  have GPU capabilities:
    * git clone https://github.com/mochi-hpc/mochi-spack-packages
    * git clone https://github.com/mochi-hpc-experiments/platform-configurations.git
    * spack repo add mochi-spack-packages
    * spack install mochi-margo@develop

- Use the system (preinstalled) MPI instead of letting spack build it:
    * spack external find mpi

- Build libfabric:
    * with fabrics=verbs,rxm
    * with the system's rdma-core instead of letting spack build its own rdma-core
        ```
        rdma-core:
            buildable: False
            externals:
            - spec: rdma-core@39.1
              prefix: /usr
        ```

- Build mochi-ssg:
    * Pass whatever MPI implementation ThetaGPU has as a dependency to this command.
    * spack install mochi-ssg@develop ^<mpich | openmpi | ...

- Build mochi-tests:
    * git clone -q https://github.com/mochi-hpc-experiments/mochi-tests.git
    * If you want to build the gpu margo test, make sure *nvcc* is on PATH
    * spack load mochi-margo, mochi-ssg, mercury, argobots
    * setup LD_LIBRARY_PATH
