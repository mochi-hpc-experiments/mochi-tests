These are example scripts for executing an automated regression test on the 
Cooley system at the ALCF.  The entire process is handled by the 
"run-regression.sh" script, which is suitable for execution within a cron job.

Crontab example:

15 6 * * * bash -l -c /home/carns/bin/margo-regression-test-cron.sh
