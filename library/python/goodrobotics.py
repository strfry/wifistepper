import threading
import time
import struct
import socket
import json

import Queue as queue


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
    __LTO_ACK = (2000)

    __PACK_HELLO = '<36s36s36sH24sIBB'
    __PACK_STD = '<BBBBHH'

    def __preamble(self, type):
        return struct.pack('<BHB', self.__L_MAGIC_1, self.__L_MAGIC_2, type)

    def __header(self, opcode, subcode, target, queue, packetid, length):
        return struct.pack('<BBBBHH', opcode, subcode, target, queue, packetid, length)

    def __nextid(self):
        self.last_id += 1
        return self.last_id

    def __init__(self, host, port):
        self.meta = dict()
        self.wait_dict = dict()
        self.enabled_std = False
        self.enabled_crypto = False
        self.last_id = 0
        self.last_ping = 0

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.host = host
        self.port = port
        self.rx = threading.Thread(name='lowtcp rxworker', target=self._rxworker)
        self.rx.setDaemon(True)
        self.tx = threading.Thread(name='lowtcp txworker', target=self._txworker)
        self.tx.setDaemon(True)

    def _send(self, opcode, subcode, target, queue, data = ''):
        packetid = self.__nextid()
        self.sock.send(self.__preamble(self.__TYPE_STD) + self.__header(opcode, subcode, target, queue, packetid, len(data)) + data)
        return packetid

    def _waitack(self, packetid):
        q = queue.Queue(1)
        self.wait_dict[packetid] = q
        try: return q.get(True, self.__LTO_ACK / 1000.0)
        except queue.Empty: return None
        finally: del self.wait_dict[packetid]

    def connect(self):
        self.sock.connect((self.host, self.port))
        self.rx.start()
        self.tx.start()

        # Send hello
        self.sock.send(self.__preamble(self.__TYPE_HELLO))

    

    def cmd_estop(self, target, hiz, soft):
        b_hiz = 0x01 if hiz else 0x00
        b_soft = 0x01 if soft else 0x00
        return self._waitack(self._send(self.__OPCODE_ESTOP, self.__SUBCODE_CMD, target, 0, struct.pack('<BB', b_hiz, b_soft)))

    def cmd_setconfig(self, target, queue, config):
        data = json.dumps(config, separators=(',', ':'))
        return self._waitack(self._send(self.__OPCODE_SETCONFIG, self.__SUBCODE_CMD, target, queue, data + struct.pack('x')))

    def cmd_getconfig(self, target):
        pass

    def cmd_lastwill(self, queue):
        return self._waitack(self._send(self.__OPCODE_LASTWILL, self.__SUBCODE_CMD, 0, 0, struct.pack('<B', queue)))

    def cmd_stop(self, target, queue, hiz, soft):
        b_hiz = 0x01 if hiz else 0x00
        b_soft = 0x01 if soft else 0x00
        return self._waitack(self._send(self.__OPCODE_STOP, self.__SUBCODE_CMD, target, queue, struct.pack('<BB', b_hiz, b_soft)))

    def cmd_run(self, target, queue, dir, stepss):
        pid = self.__nextid()
        b_dir = 0x01 if dir else 0x00
        return self._waitack(self._send(self.__OPCODE_RUN, self.__SUBCODE_CMD, target, queue, struct.pack('<Bf', b_dir, stepss)))

    def cmd_stepclock(self, target, queue, dir):
        b_dir = 0x01 if dir else 0x00
        return self._waitack(self._send(self.__OPCODE_STEPCLOCK, self.__SUBCODE_CMD, target, queue, struct.pack('<B', b_dir)))

    def cmd_move(self, target, queue, dir, microsteps):
        b_dir = 0x01 if dir else 0x00
        return self._waitack(self._send(self.__OPCODE_MOVE, self.__SUBCODE_CMD, target, queue, struct.pack('<BI', b_dir, microsteps)))

    def cmd_goto(self, target, queue, hasdir, dir, pos):
        b_hasdir = 0x01 if hasdir else 0x00
        b_dir = 0x01 if dir else 0x00
        return self._waitack(self._send(self.__OPCODE_GOTO, self.__SUBCODE_CMD, target, queue, struct.pack('<BBi', b_hasdir, b_dir, pos)))

    def cmd_gountil(self, target, queue, action, dir, stepss):
        b_action = 0x01 if action else 0x00
        b_dir = 0x01 if dir else 0x00
        return self._waitack(self._send(self.__OPCODE_GOUNTIL, self.__SUBCODE_CMD, target, queue, struct.pack('<BBf', b_action, b_dir, stepss)))
    
    def cmd_releasesw(self, target, queue, action, dir):
        b_action = 0x01 if action else 0x00
        b_dir = 0x01 if dir else 0x00
        return self._waitack(self._send(self.__OPCODE_RELEASESW, self.__SUBCODE_CMD, target, queue, struct.pack('<BB', b_action, b_dir)))

    def cmd_gohome(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_GOHOME, self.__SUBCODE_CMD, target, queue))

    def cmd_gomark(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_GOMARK, self.__SUBCODE_CMD, target, queue))

    def cmd_resetpos(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_RESETPOS, self.__SUBCODE_CMD, target, queue))

    def cmd_setpos(self, target, queue, pos):
        return self._waitack(self._send(self.__OPCODE_SETPOS, self.__SUBCODE_CMD, target, queue, struct.pack('<i', pos)))

    def cmd_setmark(self, target, queue, mark):
        return self._waitack(self._send(self.__OPCODE_SETMARK, self.__SUBCODE_CMD, target, queue, struct.pack('<i', pos)))

    def cmd_waitbusy(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_WAITBUSY, self.__SUBCODE_CMD, target, queue))

    def cmd_waitrunning(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_WAITRUNNING, self.__SUBCODE_CMD, target, queue))

    def cmd_waitms(self, target, queue, ms):
        return self._waitack(self._send(self.__OPCODE_WAITMS, self.__SUBCODE_CMD, target, queue, struct.pack('<I', ms)))

    def cmd_waitswitch(self, target, queue, state):
        b_state = 0x01 if state else 0x00
        return self._waitack(self._send(self.__OPCODE_WAITSWITCH, self.__SUBCODE_CMD, target, queue, struct.pack('<B', b_state)))

    def cmd_emptyqueue(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_EMPTYQUEUE, self.__SUBCODE_CMD, target, queue))

    def cmd_savequeue(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_SAVEQUEUE, self.__SUBCODE_CMD, target, queue))

    def cmd_loadqueue(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_LOADQUEUE, self.__SUBCODE_CMD, target, queue))

    def cmd_copyqueue(self, target, queue, source):
        return self._waitack(self._send(self.__OPCODE_COPYQUEUE, self.__SUBCODE_CMD, target, queue, struct.pack('<B', source)))

    def cmd_runqueue(self, target, queue):
        return self._waitack(self._send(self.__OPCODE_RUNQUEUE, self.__SUBCODE_CMD, target, queue))

    def _handle_rx(self, opcode, subcode, address, queue, packetid, data):
        #print "Handle RX", opcode, subcode, address, queue, packetid
        if subcode == self.__SUBCODE_ACK:
            (d_id,) = struct.unpack('<I', data)
            if packetid in self.wait_dict:
                try: self.wait_dict[packetid].put(d_id, False)
                except queue.Full: pass

    def _rxworker(self):
        while True:
            (p_magic1,) = struct.unpack('<B', self.sock.recv(1))
            if p_magic1 != self.__L_MAGIC_1: continue

            (p_magic2, p_type) = struct.unpack('<HB', self.sock.recv(3))
            if p_magic2 != self.__L_MAGIC_2: continue

            if p_type == self.__TYPE_ERROR:
                pass
            elif p_type == self.__TYPE_HELLO:
                (d_product, d_model, d_swbranch, d_version, d_hostname, d_chipid, self.enabled_std, self.enabled_crypto) = struct.unpack(self.__PACK_HELLO, self.sock.recv(struct.calcsize(self.__PACK_HELLO)))
                self.meta.update({'product':d_product, 'model':d_model, 'swbranch':d_swbranch, 'version':d_version, 'hostname':d_hostname, 'chipid':d_chipid})
                self.last_id = 0

            elif p_type == self.__TYPE_GOODBYE:
                #self.sock.close()
                pass

            elif p_type == self.__TYPE_PING:
                self.last_ping = time.time()

            elif p_type == self.__TYPE_STD:
                (h_opcode, h_subcode, h_address, h_queue, h_packetid, h_length) = struct.unpack(self.__PACK_STD, self.sock.recv(struct.calcsize(self.__PACK_STD)))
                data = [] if h_length == 0 else self.sock.recv(h_length)
                self._handle_rx(h_opcode, h_subcode, h_address, h_queue, h_packetid, data)

            elif p_type == self.__TYPE_CRYPTO:
                pass

    def _txworker(self):
        while True:
            time.sleep(self.__LTO_PING / (1000.0 * 4.0))
            self.sock.send(self.__preamble(self.__TYPE_PING))
            #print(str(time.time()) + " - Sent ping")
        
        

class WifiStepper:
    __comm = None
    __target = 0

    FWD = True
    REV = False

    RESET = False
    COPYMARK = True

    CLOSED = True
    OPEN = False

    class Config:
        def __init__(self):
            pass

        def tojson(self):
            return ''

    def __init__(self, host, proto='lowtcp_std', port=0, target=0):
        if proto == 'lowtcp_std':
            if port == 0: port = 1000
            self.__comm = _lowtcp_std(host, port)
        else:
            raise Exception("Unknown protocol: " + proto)

    def connect(self):
        return self.__comm.connect()

    def estop(self, hiz = True, soft = True, target = 0):
        return self.__comm.cmd_estop(target, hiz, soft)

    def setconfig(self, config, target = 0, queue = 0):
        return self.__comm.cmd_setconfig(target, queue, config.tojson())

    def getconfig(self):
        pass

    def lastwill(self, queue):
        return self.__comm.cmd_lastwill(queue)

    def stop(self, hiz = True, soft = True, target = 0, queue = 0):
        return self.__comm.cmd_stop(target, queue, hiz, soft)

    def run(self, dir, stepss, target = 0, queue = 0):
        return self.__comm.cmd_run(target, queue, dir, stepss)

    def stepclock(self, dir, target = 0, queue = 0):
        return self.__comm.cmd_stepclock(target, queue, dir)

    def move(self, dir, microsteps, target = 0, queue = 0):
        return self.__comm.cmd_move(target, queue, dir, microsteps)

    def goto(self, pos, dir = None, target = 0, queue = 0):
        return self.__comm.cmd_goto(target, queue, dir != None, dir, pos)

    def gountil(self, action, dir, stepss, target = 0, queue = 0):
        return self.__comm.cmd_gountil(target, queue, action, dir, stepss)

    def releasesw(self, action, dir, target = 0, queue = 0):
        return self.__comm.cmd_releasesw(target, queue, action, dir)

    def gohome(self, target = 0, queue = 0):
        return self.__comm.cmd_gohome(target, queue)

    def gomark(self, target = 0, queue = 0):
        return self.__comm.cmd_gomark(target, queue)

    def resetpos(self, target = 0, queue = 0):
        return self.__comm.cmd_resetpos(target, queue)

    def setpos(self, pos, target = 0, queue = 0):
        return self.__comm.cmd_setpos(target, queue, pos)

    def setmark(self, pos, target = 0, queue = 0):
        return self.__comm.cmd_setmark(target, queue, pos)

    def waitbusy(self, target = 0, queue = 0):
        return self.__comm.cmd_waitbusy(target, queue)

    def waitrunning(self, target = 0, queue = 0):
        return self.__comm.cmd_waitrunning(target, queue)

    def waitms(self, ms, target = 0, queue = 0):
        return self.__comm.cmd_waitms(target, queue, ms)

    def waitswitch(self, state, target = 0, queue = 0):
        return self.__comm.cmd_waitswitch(target, queue, state)

    def emptyqueue(self, queue, target = 0):
        return self.__comm.cmd_emptyqueue(target, queue)

    def savequeue(self, queue, target = 0):
        return self.__comm.cmd_savequeue(target, queue)

    def loadqueue(self, queue, target = 0):
        return self.__comm.cmd_loadqueue(target, queue)

    def copyqueue(self, queue, source, target = 0):
        return self.__comm.cmd_copyqueue(target, queue, source)

    def runqueue(self, queue, target = 0):
        return self.__comm.cmd_runqueue(target, queue)
