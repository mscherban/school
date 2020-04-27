# launch with command line:
# sudo mn --custom ~/mininet/custom/proj-topo.py --topo topo --mac --switch ovsk --controller remote
# OR to define the number of spines, leafs, and number of hosts per leaf, pass with topo:
# sudo mn --custom ~/mininet/custom/proj-topo.py --topo topo,3,3,2 --mac --switch ovsk --controller remote
# put proj-topo.py in folder ~/mininet/custom/

from mininet.topo import Topo

#numhosts = 6
#numspines = 3
#numleafs = 3
#numhosts_per_leaf = 2

class ProjTopo(Topo):
    def __init__(self, ns=3, nl=3, nh_p_l=2):
    	numspines = ns
	numleafs = nl
	numhosts_per_leaf = nh_p_l
        Topo.__init__(self)
        #initialize all hosts and switches
        Spines = []
        Leafs = []
        Hosts = []

	for i in range(0, numleafs * numhosts_per_leaf):
	  nm = 'h' + str(i+1)
	  Hosts.append(self.addHost(nm))

	for i in range(0,numspines):
	  nm = 's' + str(i+1)
	  Spines.append(self.addSwitch(nm))

	for i in range(numspines, numleafs+numspines):
	  nm = 's' + str(i+1)
	  Leafs.append(self.addSwitch(nm))

        #Spines.append(self.addSwitch('s1'))
        #Spines.append(self.addSwitch('s2'))
        #Spines.append(self.addSwitch('s3'))
        #Leafs.append(self.addSwitch('s4'))
        #Leafs.append(self.addSwitch('s5'))
        #Leafs.append(self.addSwitch('s6'))

        #connect topology
        for s in range(numspines):
            for l in range(numleafs):
                self.addLink(Spines[s], Leafs[l])
        for l in range(numleafs):
            for h in range(numhosts_per_leaf):
                self.addLink(Leafs[l], Hosts[l*numhosts_per_leaf+h])

topos = { 'topo': ProjTopo }
