import os

#readout:
# grep -ir "h\." runs/ | sort -n | awk '{print $7}'

prog = "./sim_cache"
trace = "example_trace.txt"
bs = [16, 32, 64, 128]
cs = [10, 11, 12, 13, 14, 15]
a = 4

def FormatCallString(size, bsx):
    line = prog
    line = line + " " + str(bsx)
    line = line + " " + str(size)
    line = line + " " + str(a)
    line = line + " 0 0 0 " + trace
    line = line + " >>"
    line = line + "runs/" + str(size).zfill(7) + "/" + str(bsx).zfill(4) + ".txt" 
    return line

def GetFaAssocVal(size):
    return (size // bs)

os.system("rm -rf runs/")
os.system("mkdir runs")
numrun = 0
for x in cs:
    cachesize = 1 << x
    os.system("mkdir runs/" + str(cachesize).zfill(7))
    for s in bs:
        numrun = numrun + 1
        t = FormatCallString(cachesize, s)
        print(t)
        os.system(t)

        

print("total runs:", numrun)
