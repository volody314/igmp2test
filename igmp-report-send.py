#!python3
import scapy.all as scapy

pack = (scapy.IP(dst="127.0.0.1",src="192.168.88.1",proto=2)/'\x16\x00\x00\x00\xE0\x00\x00\x63')
del pack[scapy.IP].chksum
pack.show2()
scapy.sr1(pack)


#scapy.Ether()/
