from scapy.all import *
import scapy.contrib.igmp
p = IP(dst="192.168.88.216",src="192.168.88.1")/scapy.contrib.igmp.IGMP(gaddr='233.3.2.1')
p[scapy.contrib.igmp.IGMP].igmpize()
send(p)
