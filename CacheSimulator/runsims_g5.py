import os

#readout:
# grep -ir "h\." runs/ | sort -n | awk '{print $7}'

prog = "./sim_cache"
trace = "example_trace.txt"
bs = 32
cs1 = [10, 11, 12, 13, 14, 15, 16, 17, 18]
cs2 = [15, 16, 17, 18, 19, 20]
a1 = 4
a2 = 8

def FormatCallString(bsx, l1size, l1a, vcb, l2size, l2a):
    line = prog
    line = line + " " + str(bsx)
    line = line + " " + str(l1size)
    line = line + " " + str(l1a)
    line = line + " " + str(vcb)
    line = line + " " + str(l2size)
    line = line + " " + str(l2a)
    line = line + " " + trace
    line = line + " >>"
    line = line + "runs/" + str(l1size).zfill(7) + "/" + str(l2size).zfill(7) + ".txt" 
    return line

os.system("rm -rf runs/")
os.system("mkdir runs")
numrun = 0
for x in cs1:
    cachesize = 1 << x
    os.system("mkdir runs/" + str(cachesize).zfill(7))
    for s in cs2:
        cachesize2 = 1 << s
        if (cachesize >= cachesize2):
            continue
        numrun = numrun + 1
        t = FormatCallString(bs, cachesize, a1, 0, cachesize2, a2)
        print(t)
        os.system(t)

        

print("total runs:", numrun)
