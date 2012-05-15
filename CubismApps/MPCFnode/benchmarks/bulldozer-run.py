import sys
import re
import os

#this is my shit
import run

print 'usage: python benchmark-bulldozer.py {icc|gcc} <desired-time-in-minutes> <footprint-per-thread-in-megabytes> [strong]'

if len(sys.argv) > 5:
	print('Error: passing more arguments than allowed. Aborting now.')
	sys.exit("Aborting because of errors!")
	
print "Args are:"
for x in sys.argv: print x
	
stage = "icc gcc"
if len(sys.argv) > 1:
	if re.search("icc|gcc", sys.argv[1]) == None:
		print('No match for the first argument')
		sys.exit("Aborting because of errors!")
	else:
		stage = sys.argv[1]

tdesired_seconds = 5.*60
if len(sys.argv) >= 3:
	tdesired_seconds = float(sys.argv[2])*60;
	
footprint_megabytes = 128
if len(sys.argv) >= 4:
	footprint_megabytes = int(sys.argv[3]);
	
desired_bpd = -1
if len(sys.argv) >= 5:
	if sys.argv[4] == "strong" : desired_bpd = 16
	
print 'strong scaling is %s' % (desired_bpd != -1)
	
getnodedata = os.popen('hostname ; date \'+-%y-%m-%d-%H_%M_%S\'')
nodedata = ("result-" + getnodedata.readline() + getnodedata.readline()).replace('\n', '')
common = " "

threads = [8, 4, 2, 1];
if desired_bpd != -1 : threads = [8,2]
runargs = " -dumpperiod 100000000 -saveperiod 100000000 -ascii 0 -awk 0 -bubx 0.2 -buby 0.5 -bubz 0 -cfl 0.6 -g1 1.4 -g2 1.667 -gr 0 \
-mach 1.2 -mollfactor 2 -pc1 0 -pc2 0 -rad 0.05 -reinit 0 -reinitfreq 1000 -report 5 -restart 0 -shockpos 0.1 \
-sim sb -sten 1e-4 -sumrho 3 -tend 2.0 -verb 1 -vp 0 -ncores 4 "

threads_per_node = 8

if re.search("gcc", stage) != None:
	run.run(runargs, tdesired_seconds, "57.6", "20.", footprint_megabytes, common, "./bulldozer-exec-gcc461/", nodedata+"-gcc461", "", threads, threads_per_node)

#for the moment not supported	
if re.search("icc", stage) != None:
	run.run(runargs, tdesired_seconds, "57.6", "20.", footprint_megabytes, common + " source /opt/intel/composerxe/bin/compilervars.sh intel64; ", "./bulldozer-exec-icc12/", nodedata+"-icc12", "", threads, threads_per_node, desired_bpd)