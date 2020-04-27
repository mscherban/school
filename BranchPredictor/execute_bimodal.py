#!/usr/bin/python3

import os

traces = ["gcc", "jpeg", "perl"]
suffix = "_trace.txt"
mvals = [7, 8, 9, 10, 11, 12]

os.system("rm -rf bimodal/")
os.system("mkdir bimodal")
for trace in traces:
    os.system("mkdir bimodal/" + trace)
    for m in mvals:
        run = "./sim bimodal " + str(m) + " " + trace + suffix + " >>bimodal/" + trace + "/" + str(m).zfill(2) + ".txt"
        print(run)
        os.system(run)
    
print("done")
