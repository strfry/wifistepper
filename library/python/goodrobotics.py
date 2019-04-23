import threading
import time
import struct
import socket
import json


class _lowtcp_std:
    __L_MAGIC_1 = (0xAE)
    __L_MAGIC_2 = (0x7B11)

    __TYPE_ERROR = (0x00)
    __TYPE_HELLO = (0x01)
    __TYPE_GOODBYE = (0x02)
    __TYPE_PING = (0x03)
    __TYPE_STD = (0x04)
    __TYPE_CRYPTO = (0x05)

    __OPCODE_ESTOP = (0x00)
    __OPCODE_SETCONFIG = (0x01)
    __OPCODE_GETCONFIG = (0x02)
    __OPCODE_LASTWILL = (0x03)

    __OPCODE_STOP = (0x11)
    __OPCODE_RUN = (0x12)
    __OPCODE_STEPCLOCK = (0x13)
    __OPCODE_MOVE = (0x14)
    __OPCODE_GOTO = (0x15)
    __OPCODE_GOUNTIL = (0x16)
    __OPCODE_RELEASESW = (0x17)
    __OPCODE_GOHOME = (0x18)
    __OPCODE_GOMARK = (0x19)
    __OPCODE_RESETPOS = (0x1A)
    __OPCODE_SETPOS = (0x1B)
    __OPCODE_SETMARK = (0x1C)

    __OPCODE_WAITBUSY = (0x21)
    __OPCODE_WAITRUNNING = (0x22)
    __OPCODE_WAITMS = (0x23)
    __OPCODE_WAITSWITCH = (0x24)

    __OPCODE_EMPTYQUEUE = (0x31)
    __OPCODE_SAVEQUEUE = (0x32)
    __OPCODE_LOADQUEUE = (0x33)
    __OPCODE_ADDQUEUE = (0x34)
    __OPCODE_COPYQUEUE = (0x35)
    __OPCODE_RUNQUEUE = (0x36)
    __OPCODE_GETQUEUE = (0x37)

    __SUBCODE_NACK = (0x00)
    __SUBCODE_ACK = (0x01)
    __SUBCODE_CMD = (0x02)

    __LTO_PING = (3000)

    def __preamble(self, type):
        return struct.pack('<BHB', self.__L_MAGIC_1, self.__L_MAGIC_2, type)

    def __header(self, opcode, subcode, target, queue, packetid, length):
        return struct.pack('<BBBBHH', opcode, subcode, target, queue, packetid, length)

    def __nextid(self):
        # TODO
        return 0

    def __init__(self, host, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.host = host
        self.port = port
        self.rx = threading.Thread(name='lowtcp rxworker', target=self._rxworker)
        self.rx.setDaemon(True)
        self.tx = threading.Thread(name='lowtcp txworker', target=self._txworker)
        self.tx.setDaemon(True)

    def connect(self):
        self.sock.connect((self.host, self.port))
        self.rx.start()
        self.tx.start()

    def cmd_estop(self, target, hiz, soft):
        b_hiz = 0x01 if hiz else 0x00
        b_soft = 0x01 if soft else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_ESTOP, self.__SUBCODE_CMD, target, 0, self.__nextid(), 2) + struct.pack('<BB', b_hiz, b_soft))

    def cmd_setconfig(self, target, queue, config):
        data = json.dumps(config, separators=(',', ':'))
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_SETCONFIG, self.__SUBCODE_CMD, target, queue, self.__nextid(), len(data)+1) + data + struct.pack('x'))

    def cmd_lastwill(self, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_LASTWILL, self.__SUBCODE_CMD, 0, 0, self.__nextid(), 1) + struct.pack('<B', queue))

    def cmd_stop(self, target, queue, hiz, soft):
        b_hiz = 0x01 if hiz else 0x00
        b_soft = 0x01 if soft else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_STOP, self.__SUBCODE_CMD, target, queue, self.__nextid(), 2) + struct.pack('<BB', b_hiz, b_soft))

    def cmd_run(self, target, queue, dir, stepss):
        b_dir = 0x01 if dir else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_RUN, self.__SUBCODE_CMD, target, queue, self.__nextid(), 5) + struct.pack('<Bf', b_dir, stepss))

    def cmd_stepclock(self, target, queue, dir):
        b_dir = 0x01 if dir else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_STEPCLOCK, self.__SUBCODE_CMD, target, queue, self.__nextid(), 1) + struct.pack('<B', b_dir))

    def cmd_move(self, target, queue, dir, microsteps):
        b_dir = 0x01 if dir else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_MOVE, self.__SUBCODE_CMD, target, queue, self.__nextid(), 1) + struct.pack('<BI', b_dir, microsteps))

    def cmd_goto(self, target, queue, hasdir, dir, pos):
        b_hasdir = 0x01 if hasdir else 0x00
        b_dir = 0x01 if dir else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_GOTO, self.__SUBCODE_CMD, target, queue, self.__nextid(), 6) + struct.pack('<BBi', b_hasdir, b_dir, pos))

    def cmd_gountil(self, target, queue, action, dir, stepss):
        b_action = 0x00 is action else 0x01
        b_dir = 0x01 if dir else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_GOUNTIL, self.__SUBCODE_CMD, target, queue, self.__nextid(), 6) + struct.pack('<BBf', b_action, b_dir, stepss))
    
    def cmd_releasesw(self, target, queue, action, dir):
        b_action = 0x00 is action else 0x01
        b_dir = 0x01 if dir else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_RELEASESW, self.__SUBCODE_CMD, target, queue, self.__nextid(), 2) + struct.pack('<BB', b_action, b_dir))

    def cmd_gohome(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_GOHOME, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def cmd_gomark(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_GOMARK, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def cmd_resetpos(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_RESETPOS, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def cmd_setpos(self, target, queue, pos):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_SETPOS, self.__SUBCODE_CMD, target, queue, self.__nextid(), 4) + struct.pack('<i', pos))

    def cmd_setmark(self, target, queue, mark):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_SETMARK, self.__SUBCODE_CMD, target, queue, self.__nextid(), 4) + struct.pack('<i', pos))

    def cmd_waitbusy(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_WAITBUSY, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def cmd_waitrunning(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_WAITRUNNING, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def cmd_waitms(self, target, queue, ms):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_WAITMS, self.__SUBCODE_CMD, target, queue, self.__nextid(), 4) + struct.pack('<I', ms))

    def cmd_waitswitch(self, target, queue, state):
        b_state = 0x01 if state else 0x00
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_WAITSWITCH, self.__SUBCODE_CMD, target, queue, self.__nextid(), 1) + struct.pack('<B', b_state))

    def cmd_emptyqueue(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_EMPTYQUEUE, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def cmd_savequeue(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_SAVEQUEUE, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def cmd_loadqueue(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_LOADQUEUE, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def cmd_copyqueue(self, target, queue, source):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_COPYQUEUE, self.__SUBCODE_CMD, target, queue, self.__nextid(), 1) + struct.pack('<B', source))

    def cmd_runqueue(self, target, queue):
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(self.__OPCODE_RUNQUEUE, self.__SUBCODE_CMD, target, queue, self.__nextid(), 0))

    def _rxworker(self):
        while True:
            pass
            #print(map(ord, self.sock.recv(1024)))

    def _txworker(self):
        while True:
            time.sleep(self.__LTO_PING / (1000.0 * 4.0))
            self.sock.send(self.__preamble(self.__TYPE_PING))
            print(str(time.time()) + " - Sent ping")
        
        

class WifiStepper:
    __comm = None
    __target = 0

    FWD = True
    REV = False

    def __init__(self, host, proto='lowtcp_std', port=0, target=0):
        if proto == 'lowtcp_std':
            if port == 0: port = 1000
            self.__comm = _lowtcp_std(host, port)
        else:
            raise Exception("Unknown protocol: " + proto)

    def connect(self):
        return self.__comm.connect()

    def estop(self, hiz = True, soft = True, target = 0):
        self.__comm.cmd_estop(target, hiz, soft)

    def setconfig(self)

    def stop(self, hiz = True, soft = True, target = 0, queue = 0):
        self.__comm.cmd_stop(target, queue, hiz, soft)

    def goto(self, pos, dir = None, target = 0, queue = 0):
        self.__comm.cmd_goto(target, queue, dir != None, dir, pos)

    def run(self, dir, stepss, target = 0, queue = 0):
        self.__comm.cmd_run(target, queue, dir, stepss)

    def waitbusy(self, target = 0, queue = 0):
        self.__comm.cmd_waitbusy(target, queue)

    def waitrunning(self, target = 0, queue = 0):
        self.__comm.cmd_waitrunning(target, queue)

    def waitms(self, millis, target = 0, queue = 0):
        self.__comm.cmd_waitms(target, queue, millis)

