import os

prog = "./sim_cache"
trace = "example_trace.txt"
bs = 32
cs = [10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]
a = [1, 2, 4, 8]

def FormatCallString(size, assoc):
    line = prog
    line = line + " " + str(bs)
    line = line + " " + str(size)
    line = line + " " + str(assoc)
    line = line + " 0 0 0 " + trace
    line = line + " >>"
    line = line + "runs/" + str(size) + "/" + str(assoc) + ".txt" 
    return line
    #return prog + " " + str(bs) + " " + str(size) + " " + str(assoc) + " 0 0 0 0 " + trace

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
    os.system("mkdir runs/" + str(cachesize))
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
        
        
        

print(numrun)
