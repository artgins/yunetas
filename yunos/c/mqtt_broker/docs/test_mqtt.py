#!/usr/bin/python
############################################
# Utilidad para probar los comandos
############################################
import json
import socket
from time import sleep

# imei 352625699888044

data_connect = """\
0000: 10 4F 00 04 4D 51 54 54 04 EE 00 1E 00 0B 44 56   .O..MQTT......DV
0010: 45 53 5F 34 30 41 43 36 36 00 17 74 65 6C 65 2F   ES_40AC66..tele/
0020: 74 61 73 6D 6F 74 61 5F 34 30 41 43 36 36 2F 4C   tasmota_40AC66/L
0030: 57 54 00 07 4F 66 66 6C 69 6E 65 00 09 44 56 45   WT..Offline..DVE
0040: 53 5F 55 53 45 52 00 09 44 56 45 53 5F 50 41 53   S_USER..DVES_PAS
0050: 53                                                S
"""

data_subscribe = """\
0000: 31 1F 00 17 74 65 6C 65 2F 74 61 73 6D 6F 74 61   1...tele/tasmota
0010: 5F 34 30 41 43 36 36 2F 4C 57 54                  _40AC66/LWT
"""

data_subscribe2 = """\
0000: 4F 6E 6C 69 6E 65 30 1B 00 19 63 6D 6E 64 2F 74   Online0...cmnd/t
0010: 61 73 6D 6F 74 61 5F 34 30 41 43 36 36 2F 50 4F   asmota_40AC66/PO
0020: 57 45 52 82 1A 00 02 00 15 63 6D 6E 64 2F 74 61   WER......cmnd/ta
0030: 73 6D 6F 74 61 5F 34 30 41 43 36 36 2F 23 00 82   smota_40AC66/#..
0040: 14 00 03 00 0F 63 6D 6E 64 2F 74 61 73 6D 6F 74   .....cmnd/tasmot
0050: 61 73 2F 23 00 82 1A 00 04 00 15 63 6D 6E 64 2F   as/#.......cmnd/
0060: 44 56 45 53 5F 34 30 41 43 36 36 5F 66 62 2F 23   DVES_40AC66_fb/#
0070: 00                                                .
"""

data_publish = """\
0000: 31 82 07 00 25 74 61 73 6D 6F 74 61 2F 64 69 73   1....tasmota/dis
0010: 63 6F 76 65 72 79 2F 30 38 46 39 45 30 34 30 41   covery/08F9E040A
0020: 43 36 36 2F 63 6F 6E 66 69 67                     C66/config
"""

data_publish2 = """\
0000: 7B 22 69 70 22 3A 22 31 39 32 2E 31 36 38 2E 31   {"ip":"192.168.1
0010: 2E 34 31 22 2C 22 64 6E 22 3A 22 54 61 73 6D 6F   .41","dn":"Tasmo
0020: 74 61 22 2C 22 66 6E 22 3A 5B 22 54 61 73 6D 6F   ta","fn":["Tasmo
0030: 74 61 22 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75   ta",null,null,nu
0040: 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C   ll,null,null,nul
0050: 6C 2C 6E 75 6C 6C 5D 2C 22 68 6E 22 3A 22 74 61   l,null],"hn":"ta
0060: 73 6D 6F 74 61 2D 34 30 41 43 36 36 2D 33 31 37   smota-40AC66-317
0070: 34 22 2C 22 6D 61 63 22 3A 22 30 38 46 39 45 30   4","mac":"08F9E0
0080: 34 30 41 43 36 36 22 2C 22 6D 64 22 3A 22 52 65   40AC66","md":"Re
0090: 66 6F 73 73 2D 50 31 31 22 2C 22 74 79 22 3A 30   foss-P11","ty":0
00A0: 2C 22 69 66 22 3A 30 2C 22 6F 66 6C 6E 22 3A 22   ,"if":0,"ofln":"
00B0: 4F 66 66 6C 69 6E 65 22 2C 22 6F 6E 6C 6E 22 3A   Offline","onln":
00C0: 22 4F 6E 6C 69 6E 65 22 2C 22 73 74 61 74 65 22   "Online","state"
00D0: 3A 5B 22 4F 46 46 22 2C 22 4F 4E 22 2C 22 54 4F   :["OFF","ON","TO
00E0: 47 47 4C 45 22 2C 22 48 4F 4C 44 22 5D 2C 22 73   GGLE","HOLD"],"s
00F0: 77 22 3A 22 31 32 2E 34 2E 30 22 2C 22 74 22 3A   w":"12.4.0","t":
0100: 22 74 61 73 6D 6F 74 61 5F 34 30 41 43 36 36 22   "tasmota_40AC66"
0110: 2C 22 66 74 22 3A 22 25 70 72 65 66 69 78 25 2F   ,"ft":".prefix./
0120: 25 74 6F 70 69 63 25 2F 22 2C 22 74 70 22 3A 5B   .topic./","tp":[
0130: 22 63 6D 6E 64 22 2C 22 73 74 61 74 22 2C 22 74   "cmnd","stat","t
0140: 65 6C 65 22 5D 2C 22 72 6C 22 3A 5B 31 2C 30 2C   ele"],"rl":[1,0,
0150: 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C   0,0,0,0,0,0,0,0,
0160: 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C   0,0,0,0,0,0,0,0,
0170: 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C   0,0,0,0,0,0,0,0,
0180: 30 2C 30 5D 2C 22 73 77 63 22 3A 5B 2D 31 2C 2D   0,0],"swc":[-1,-
0190: 31 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31   1,-1,-1,-1,-1,-1
01A0: 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31 2C   ,-1,-1,-1,-1,-1,
01B0: 2D 31 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D   -1,-1,-1,-1,-1,-
01C0: 31 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31   1,-1,-1,-1,-1,-1
01D0: 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31 2C 2D 31 5D   ,-1,-1,-1,-1,-1]
01E0: 2C 22 73 77 6E 22 3A 5B 6E 75 6C 6C 2C 6E 75 6C   ,"swn":[null,nul
01F0: 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C   l,null,null,null
0200: 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C   ,null,null,null,
0210: 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E   null,null,null,n
0220: 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75   ull,null,null,nu
0230: 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C   ll,null,null,nul
0240: 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C   l,null,null,null
0250: 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C   ,null,null,null,
0260: 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E 75 6C 6C 2C 6E   null,null,null,n
0270: 75 6C 6C 5D 2C 22 62 74 6E 22 3A 5B 30 2C 30 2C   ull],"btn":[0,0,
0280: 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C   0,0,0,0,0,0,0,0,
0290: 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C   0,0,0,0,0,0,0,0,
02A0: 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C 30 2C   0,0,0,0,0,0,0,0,
02B0: 30 2C 30 5D 2C 22 73 6F 22 3A 7B 22 34 22 3A 30   0,0],"so":{"4":0
02C0: 2C 22 31 31 22 3A 30 2C 22 31 33 22 3A 30 2C 22   ,"11":0,"13":0,"
02D0: 31 37 22 3A 30 2C 22 32 30 22 3A 30 2C 22 33 30   17":0,"20":0,"30
02E0: 22 3A 30 2C 22 36 38 22 3A 30 2C 22 37 33 22 3A   ":0,"68":0,"73":
02F0: 30 2C 22 38 32 22 3A 30 2C 22 31 31 34 22 3A 30   0,"82":0,"114":0
0300: 2C 22 31 31 37 22 3A 30 7D 2C 22 6C 6B 22 3A 30   ,"117":0},"lk":0
0310: 2C 22 6C 74 5F 73 74 22 3A 30 2C 22 73 68 6F 22   ,"lt_st":0,"sho"
0320: 3A 5B 30 2C 30 2C 30 2C 30 5D 2C 22 73 68 74 22   :[0,0,0,0],"sht"
0330: 3A 5B 5B 30 2C 30 2C 30 5D 2C 5B 30 2C 30 2C 30   :[[0,0,0],[0,0,0
0340: 5D 2C 5B 30 2C 30 2C 30 5D 2C 5B 30 2C 30 2C 30   ],[0,0,0],[0,0,0
0350: 5D 5D 2C 22 76 65 72 22 3A 31 7D 31 BC 02 00 26   ]],"ver":1}1...&
0360: 74 61 73 6D 6F 74 61 2F 64 69 73 63 6F 76 65 72   tasmota/discover
0370: 79 2F 30 38 46 39 45 30 34 30 41 43 36 36 2F 73   y/08F9E040AC66/s
0380: 65 6E 73 6F 72 73 7B 22 73 6E 22 3A 7B 22 54 69   ensors{"sn":{"Ti
0390: 6D 65 22 3A 22 32 30 32 34 2D 30 31 2D 32 34 54   me":"2024-01-24T
03A0: 30 38 3A 35 38 3A 33 30 22 2C 22 41 4E 41 4C 4F   08:58:30","ANALO
03B0: 47 22 3A 7B 22 54 65 6D 70 65 72 61 74 75 72 65   G":{"Temperature
03C0: 22 3A 32 30 2E 38 7D 2C 22 45 4E 45 52 47 59 22   ":20.8},"ENERGY"
03D0: 3A 7B 22 54 6F 74 61 6C 53 74 61 72 74 54 69 6D   :{"TotalStartTim
03E0: 65 22 3A 22 32 30 32 34 2D 30 31 2D 32 30 54 32   e":"2024-01-20T2
03F0: 30 3A 33 31 3A 31 36 22 2C 22 54 6F 74 61 6C 22   0:31:16","Total"
0400: 3A 30 2E 31 30 31 2C 22 59 65 73 74 65 72 64 61   :0.101,"Yesterda
0410: 79 22 3A 30 2E 30 34 31 2C 22 54 6F 64 61 79 22   y":0.041,"Today"
0420: 3A 30 2E 30 31 35 2C 22 50 6F 77 65 72 22 3A 32   :0.015,"Power":2
0430: 2C 22 41 70 70 61 72 65 6E 74 50 6F 77 65 72 22   ,"ApparentPower"
0440: 3A 38 2C 22 52 65 61 63 74 69 76 65 50 6F 77 65   :8,"ReactivePowe
0450: 72 22 3A 38 2C 22 46 61 63 74 6F 72 22 3A 30 2E   r":8,"Factor":0.
0460: 32 32 2C 22 56 6F 6C 74 61 67 65 22 3A 32 32 30   22,"Voltage":220
0470: 2C 22 43 75 72 72 65 6E 74 22 3A 30 2E 30 33 35   ,"Current":0.035
0480: 7D 2C 22 54 65 6D 70 55 6E 69 74 22 3A 22 43 22   },"TempUnit":"C"
0490: 7D 2C 22 76 65 72 22 3A 31 7D                     },"ver":1}
"""


def main():
    #hexdata = extract_hexa_in_bytearray(data_con_presion)
    #print(binascii.hexlify(hexdata))


    ip = "127.0.0.1"
    port = int("1881")

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((ip, port))

    while 1:
        p = extract_hexa(data_connect)
        s.sendall(bytearray(p))
        print("Sending connect to %r:%r %r" % (ip, port, p))
        rx = s.recv(1024)
        print("Received len %d, %r" % (len(rx), rx))

        p = extract_hexa(data_subscribe)
        s.sendall(bytearray(p))
        print("Sending connect to %r:%r %r" % (ip, port, p))
        p = extract_hexa(data_subscribe2)
        s.sendall(bytearray(p))
        print("Sending connect to %r:%r %r" % (ip, port, p))
        rx = s.recv(1024)
        print("Received len %d, %r" % (len(rx), rx))

        p = extract_hexa(data_publish)
        s.sendall(bytearray(p))
        print("Sending connect to %r:%r %r" % (ip, port, p))
        p = extract_hexa(data_publish2)
        s.sendall(bytearray(p))
        print("Sending connect to %r:%r %r" % (ip, port, p))
        rx = s.recv(1024)
        print("Received len %d, %r" % (len(rx), rx))

        sleep(100000000)
        break

    s.close()


def extract_hexa(s):
    data = []
    lines = s.splitlines()
    fin = False
    for line in lines:
        xxx = line.split(" ")
        l = len(xxx)
        i = 1
        while i < l and i <=16:
            xx = xxx[i]
            if len(xx) == 0:
                fin = True
                break
            n = int(xx, 16)
            data.append(n)
            i += 1
        if fin:
            break
    return data

if __name__ == "__main__":
    main()
