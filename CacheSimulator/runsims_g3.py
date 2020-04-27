import os

#readout:
# grep -ir "n\." runs/ | sort -n | awk '{print $6}'

prog = "./sim_cache"
trace = "example_trace.txt"
bs = 32
cs = [10, 11, 12, 13, 14, 15, 16, 17, 18]
a = [1, 2, 4, 8]

def FormatCallString(size, assoc):
    line = prog
    line = line + " " + str(bs)
    line = line + " " + str(size)
    line = line + " " + str(assoc)
    line = line + " 0 524288 8 " + trace
    line = line + " >>"
    line = line + "runs/" + str(size).zfill(7) + "/" + str(assoc).zfill(4) + ".txt" 
    return line

def GetFaAssocVal(size):
    return (size // bs)

def IsAssocValid(size, assoc):
    v = ((size) // (bs * assoc))
    #print("size:", size, "assoc:", assoc, "v:", v)
    if v > 0:
        return True
    else:
        return False

os.system("rm -rf runs/")
os.system("mkdir runs")
numrun = 0
for x in cs:
    cachesize = 1 << x
    os.system("mkdir runs/" + str(cachesize).zfill(7))
    for s in a:
        numrun = numrun + 1
        t = FormatCallString(cachesize, s)
        print(t)
        os.system(t)
    else:
        numrun = numrun + 1
        t = FormatCallString(cachesize, GetFaAssocVal(cachesize))
        print(t)
        os.system(t)
        
        
        

print("total runs:", numrun)
