# launch with:
# pox/pox.py --verbose proj-pox
# OR pick the number of spine switches with:
# pox/pox.py --verbose proj-pox --numspines=3
# place proj-pox.py in folder ~/pox/ext/

# TOPOLOGY:
#    [s1]     [s2]     [s3]
#     |    X    |   X    |
#    [s4]     [s5]     [s6]
#    /  \     /  \     /  \
#  [h1][h2] [h3][h4] [h5][h6]
#
# (s1-s6, s3-s4 are connect, difficult to ascii)

from pox.core import core
from pox.openflow import *
import pox.openflow.libopenflow_01 as of
from pox.lib.util import dpid_to_str
import pox.lib.packet as pkt

log = core.getLogger()
spine_switches = []
leaf_switches = []

# dictionary of switches and a reference to them
# likely to use dpid:connection, or make a switch class
switches = {}

class Switch:
  def __init__(self):
    self.mac_to_port = {}

class MyComponent(object):
  def __init__(self):
    core.openflow.addListeners(self)
    self.serv = 1
    log.debug("spine_switches: %s", spine_switches)

  def flow_packets_to_port(self, event, port_out):
    msg = of.ofp_flow_mod()
    msg.match = of.ofp_match.from_packet(event.parsed)
    msg.idle_timeout = 30
    msg.data = event.ofp
    msg.actions.append(of.ofp_action_output(port = port_out))
    event.connection.send(msg)

  def flood_to_hosts_and(self, event, extra=[]):
    msg = of.ofp_packet_out()
    msg.data = event.ofp
    total_ports = len(event.connection.ports)
    starting_port = spine_switches[-1] + 1
    ports = []
    ports.extend([i for i in range(starting_port,total_ports)])
    ports.extend(extra)
    for i in ports:
      if i != event.port:
        log.debug("  flooding to port %s", i)
        ap = of.ofp_action_output(port = i)
        msg.actions.append(ap)
    event.connection.send(msg)
  
  def is_host_port(self, port, event):
    total_ports = len(event.connection.ports)
    starting_port = spine_switches[-1] + 1
    if port in [i for i in range(starting_port,total_ports)]:
      return True
    else:
      return False

  def is_spine_port(self, port):
    if port in spine_switches:
      return True
    else:
      return False

  def pick_a_server(self):
    self.serv = (self.serv + 1) % len(spine_switches)
    self.serv = self.serv + 1
    return self.serv

  def spine_routine(self, sw, event):
    packet = event.parsed
    sw.mac_to_port[packet.src] = event.port
    if packet.dst in sw.mac_to_port:
      log.debug("  spine switch %s: dst found at port %s", event.dpid, sw.mac_to_port[packet.dst])
      self.flow_packets_to_port(event, sw.mac_to_port[packet.dst])
    else:
      #flood
      log.debug("  spine switch %s: dst not found, flooding instead", event.dpid)
      msg = of.ofp_packet_out()
      msg.data = event.ofp
      msg.actions.append(of.ofp_action_output(port = of.OFPP_FLOOD))
      event.connection.send(msg)

  def leaf_routine(self, sw, event):
    packet = event.parsed
    sw.mac_to_port[packet.src] = event.port
    #pkt recv from spine port or host port?
    if event.port not in spine_switches:
      #host port, install a flow for packets arriving to this port
      #log.debug("  leaf switch %s: installing flow for port %s", event.dpid, event.port)
      #can we figure out where this packet is going?
      if packet.dst in sw.mac_to_port:
        #it has been logged before, check if came from spine
	dst_port = sw.mac_to_port[packet.dst]
	log.debug("  pkt seen on port %s", dst_port)
	if self.is_spine_port(dst_port):
	  #it came from the spine, send it back that way
	  #choose a random spine
	  picked = self.pick_a_server()
	  log.debug("  came from spine, picked %s", picked)
	  #route future traffic to this spine
          self.flow_packets_to_port(event, picked) 
	else:
	  #it did not come from the spine, came from a fellow host. route them that way
	  log.debug("  came from host, installing flow to port %s", dst_port)
	  self.flow_packets_to_port(event, dst_port)
      else:
        #the destination has not been seen before, not sure if its from
	#spine or a fellow host on this switch
	#pick a spine, and flood to other hosts
	picked = self.pick_a_server()
	log.debug("  dst not seen, flooding to hosts and spine %s", picked)
	self.flood_to_hosts_and(event, [picked])
    else:
      #packet recieved by a spine switch, flood to hosts only
      #flooding to a spine will cause a loop
      if packet.dst in sw.mac_to_port:
        log.debug("  pkt recv from spine, dst found, installing flow to host")
        dst_port = sw.mac_to_port[packet.dst]
	self.flow_packets_to_port(event, dst_port)
      else:
        log.debug("  pkt recv from spine, dst not found, flooding to hosts")
        self.flood_to_hosts_and(event)

  def _handle_PacketIn(self, event):
    log.debug("++NEW IN PACKET on switch %s, port %s", event.dpid, event.port)
    packet = event.parsed
    if packet.type == packet.ARP_TYPE:
      log.debug("  ARP PACKET")
    elif packet.type == packet.IP_TYPE:
      log.debug("  IP PACKET")
    elif packet.type == packet.ICMP_TYPE:
      log.debug("  ICMP PACKET")
    log.debug("  Dst: %s, src: %s", packet.dst, packet.src)
    sw = switches[event.dpid]
    if event.dpid in spine_switches:
      self.spine_routine(sw, event)
    else:
      self.leaf_routine(sw, event)

  def _handle_ConnectionUp(self, event):
    log.debug("Switch %s Online", event.dpid)
    sw = Switch() #create new switch object for this connectioni
    sw.dpid = event.dpid
    sw.connection = event.connection
    switches[event.dpid] = sw

def launch(numspines=3):
  for i in range(1, int(numspines)+1):
    spine_switches.append(i)
  core.registerNew(MyComponent)
