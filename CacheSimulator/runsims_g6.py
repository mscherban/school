import os

#l1 miss:
# grep -ir "h\." . | sort -n | awk '{print $7}'

#l2 miss:
# grep -ir "n\." . | sort -n | awk '{print $6}'

#srr:
# grep -ir "f\." . | sort -n | awk '{print $6}'

prog = "./sim_cache"
trace = "example_trace.txt"
bs = 32
cs1 = [10, 11, 12, 13, 14, 15]
a1 = [1, 1, 1, 1, 1, 2, 4]
vcl = [0, 2, 4, 8, 16, 0, 0]
cs2 = 32768
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
    line = line + "runs/" + str(l1size).zfill(7) + "/a" + str(l1a).zfill(2) + "-v" + str(vcb).zfill(2) + ".txt" 
    return line

os.system("rm -rf runs/")
os.system("mkdir runs")
numrun = 0
for x in cs1:
    cachesize = 1 << x
    os.system("mkdir runs/" + str(cachesize).zfill(7))
    count = 0
    for a in a1:
        numrun = numrun + 1
        t = FormatCallString(bs, cachesize, a, vcl[count], cs2, a2)
        count = count + 1
        print(t)
        os.system(t)
    

        

print("total runs:", numrun)
