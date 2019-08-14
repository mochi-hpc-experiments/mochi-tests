These are example scripts for executing an automated regression test on the 
MCS workstations.  The entire process is handled by the 
"run-regression.sh" script, which is suitable for execution within a cron job.

Your $HOME/.soft file should have the following packages loaded:

+gcc-8.2.0
+git-2.10.1
+cmake-3.14.3
@default

(another version of gcc, such as 7.3.0, should work, as long as it supports
 C++14 by default).
