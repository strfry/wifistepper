import threading
import socket


class _lowtcp:
    def __init__(self, host, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.host = host
        self.port = port
        self.rxthread = threading.Thread(name='lowtcp rxworker', target=self._rxworker)
        self.rxthread.setDaemon(True)

    def connect(self):
        self.sock.connect((self.host, self.port))
        self.rxthread.start()

    def _rxworker(self):
        while True:
            print(map(ord, self.sock.recv(1024)))
        

class WifiStepper:
    __comm = None
    __target = 0

    def __init__(self, host, proto='lowtcp', port=0, target=0):
        if proto == 'lowtcp':
            if port == 0: port = 1000
            self.__comm = _lowtcp(host, port)
        else:
            raise Exception("Unknown protocol: " + proto)

    def connect(self):
        return self.__comm.connect()

