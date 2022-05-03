#!python3
import scapy.all as scapy

pack = (scapy.IP(dst="224.0.0.1",proto=2)/'\x11\x00\x00\x00\xE0\x00\x00\x63')
del pack[scapy.IP].chksum
pack.show2()
scapy.sr1(pack)


#scapy.Ether()/
