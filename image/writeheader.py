from sys import argv
from struct import pack
from datetime import datetime

fout = open(argv[2], 'wb')
fout.write(pack('<BH36sH11s', 0x1, 0xACE1, "wsx100", int(argv[1]), datetime.now().strftime('%Y-%m-%d')))
fout.flush()
fout.close()

