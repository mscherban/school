#!/usr/bin/python3

import os

traces = ["gcc", "jpeg", "perl"]
suffix = "_trace.txt"
mvals = [7, 8, 9, 10, 11, 12]
nvals = [2, 4, 6, 8, 10, 12]
maxm = 16

def makecmd(m, n, trace):
    run = "./sim gshare "
    run = run + str(m) + " "
    run = run + str(n) + " "
    run = run + trace + suffix + " >>gshare/"
    run = run + trace + "/n"
    run = run + str(n).zfill(2) + "-m" + str(m).zfill(2) + ".txt"
    return run

os.system("rm -rf gshare/")
os.system("mkdir gshare")
numrun = 0
for trace in traces:
    os.system("mkdir gshare/" + trace)
    for m in range(7, maxm+1):
        for n in range(2, m+1, 2):
			run = makecmd(m, n, trace)
			print(run)
			os.system(run)
			numrun = numrun + 1
    
print("done, numrun=" + str(numrun))
