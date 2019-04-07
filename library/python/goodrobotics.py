import threading
import time
import struct
import socket


class _lowtcp:
    __L_MAGIC_1 = (0xAE)
    __L_MAGIC_2 = (0x7B11)
    __L_VERSION = (0x01)

    __OPCODE_PING = (0x00)
    __OPCODE_HELO = (0x01)
    __OPCODE_ESTOP = (0x11)
    __OPCODE_STOP = (0x12)
    __OPCODE_GOTO = (0x13)
    __OPCODE_RUN = (0x14)
    __OPCODE_WAITBUSY = (0x21)
    __OPCODE_WAITRUNNING = (0x22)
    __OPCODE_WAITMS = (0x23)

    __SUBCODE_NACK = (0x00)
    __SUBCODE_ACK = (0x01)
    __SUBCODE_CMD = (0x02)

    __LTO_PING = (1000)

    def __header(self, opcode, subcode, target, length):
        return struct.pack('<BHBIBBBIQH', self.__L_MAGIC_1, self.__L_MAGIC_2, self.__L_VERSION, 0, opcode, subcode, target, 0, 0, length)

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
        self.tx.start();

    def cmd_estop(self, target, hiz, soft):
        b_hiz = 0x01 if hiz else 0x00
        b_soft = 0x01 if soft else 0x00
        self.sock.send(self.__header(self.__OPCODE_ESTOP, self.__SUBCODE_CMD, target, 2) + struct.pack('<BB', b_hiz, b_soft))

    def cmd_stop(self, target, hiz, soft):
        b_hiz = 0x01 if hiz else 0x00
        b_soft = 0x01 if soft else 0x00
        self.sock.send(self.__header(self.__OPCODE_STOP, self.__SUBCODE_CMD, target, 2) + struct.pack('<BB', b_hiz, b_soft))

    def cmd_goto(self, target, pos):
        self.sock.send(self.__header(self.__OPCODE_GOTO, self.__SUBCODE_CMD, target, 4) + struct.pack('<i', pos))

    def cmd_run(self, target, dir, stepss):
        b_dir = 0x01 if dir else 0x00
        self.sock.send(self.__header(self.__OPCODE_RUN, self.__SUBCODE_CMD, target, 5) + struct.pack('<Bf', b_dir, stepss))

    def cmd_waitbusy(self, target):
        self.sock.send(self.__header(self.__OPCODE_WAITBUSY, self.__SUBCODE_CMD, target, 0))

    def cmd_waitrunning(self, target):
        self.sock.send(self.__header(self.__OPCODE_WAITRUNNING, self.__SUBCODE_CMD, target, 0))

    def cmd_waitms(self, target, ms):
        self.sock.send(self.__header(self.__OPCODE_WAITMS, self.__SUBCODE_CMD, target, 4) + struct.pack('<I', ms))

    def _rxworker(self):
        while True:
            pass
            #print(map(ord, self.sock.recv(1024)))

    def _txworker(self):
        while True:
            time.sleep(self.__LTO_PING / (1000.0 * 4.0))
            self.sock.send(self.__header(self.__OPCODE_PING, self.__SUBCODE_NACK, 0, 0))
        
        

class WifiStepper:
    __comm = None
    __target = 0

    FWD = True
    REV = False

    def __init__(self, host, proto='lowtcp', port=0, target=0):
        if proto == 'lowtcp':
            if port == 0: port = 1000
            self.__comm = _lowtcp(host, port)
        else:
            raise Exception("Unknown protocol: " + proto)

    def connect(self):
        return self.__comm.connect()

    def estop(self, hiz = True, soft = True, target = 0):
        self.__comm.cmd_estop(target, hiz, soft)

    def stop(self, hiz = True, soft = True, target = 0):
        self.__comm.cmd_stop(target, hiz, soft)

    def goto(self, position, target = 0):
        self.__comm.cmd_goto(target, position)

    def run(self, dir, stepss, target = 0):
        self.__comm.cmd_run(target, dir, stepss)

    def waitbusy(self, target = 0):
        self.__comm.cmd_waitbusy(target)

    def waitrunning(self, target = 0):
        self.__comm.cmd_waitrunning(target)

    def waitms(self, millis, target = 0):
        self.__comm.cmd_waitms(target, millis)

