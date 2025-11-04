from socket import *
from abc import ABC, abstractmethod

K_MAX_MSG = 4096

class Node:
    sock : socket

    @abstractmethod
    def to_string(self) -> str:
        pass

    def send(self) -> str:
        totalsent = 0
        s = self.to_string()
        while totalsent < len(s):
            sent = self.sock.send(s[totalsent:])
            if sent == 0:
                raise RuntimeError("socket connection broken")
            totalsent = totalsent + sent
        return self.receive().decode()

    def receive(self) -> bytes:
        chunks = []
        bytes_recd = 0
        while bytes_recd < K_MAX_MSG:
            chunk = self.sock.recv(min(K_MAX_MSG - bytes_recd, 2048))
            if chunk == b'':
                break
            chunks.append(chunk)
            bytes_recd = bytes_recd + len(chunk)
        return b''.join(chunks)

class String(Node):
    def __init__(self, sock : socket, s : str):
        self._s   = s
        self.sock = sock

    def to_string(self) -> str:
        return f"+{self._s}\r\n"
    
class Integer(Node):
    def __init__(self, sock : socket, i : int):
        self._i   = i
        self.sock = sock

    def to_string(self) -> str:
        return f":{str(self._i)}\r\n"
    
class BulkString(Node):
    def __init__(self, sock : socket, s : str):
        self._s   = s
        self.sock = sock

    def to_string(self) -> str:
        return f"${len(self._s)}\r\n{self._s}\r\n"
    
class Array(Node):
    def __init__(self, sock : socket, a : list[Node]):
        self._a   = a
        self.sock = sock

    def to_string(self) -> str:
        s = f"*{len(self._a)}\r\n"
        for n in self._a:
            s += n.to_string()
        return s

class Redis:
    def __init__(self, HOSTNAME, PORT):
        self.HOST = HOSTNAME
        self.PORT = PORT
        self.s = socket(AF_INET, SOCK_STREAM)
        self.s.connect((self.HOST, self.PORT))

    def send(self, s : str) -> str:
        totalsent = 0
        while totalsent < len(s):
            sent = self.s.send(s[totalsent:].encode())
            if sent == 0:
                raise RuntimeError("socket connection broken")
            totalsent = totalsent + sent
        return self.receive().decode()

    def receive(self) -> bytes:
        chunks = []
        bytes_recd = 0
        while bytes_recd < K_MAX_MSG:
            chunk = self.s.recv(min(K_MAX_MSG - bytes_recd, 2048))
            if chunk == b'':
                break
            chunks.append(chunk)
            bytes_recd = bytes_recd + len(chunk)
        return b''.join(chunks)

    def handshake(self) -> str:
        response = self.send("HELLO 3\r\n").decode()
        print(f"Handshake response : \"{response}\"")
        return response

    def send_int(self, i : int) -> str:
        node = Integer(self.s, i)
        print(f"Sending {node.to_string()}...")
        return node.send()

    def send_string(self, s : str) -> str:
        node = String(self.s, s)
        print(f"Sending {node.to_string()}...")
        return node.send()


    def send_bulk_string(self, s : str) -> str:
        node = BulkString(self.s, s)
        print(f"Sending {node.to_string()}...")
        return node.send()

    def send_array(self, l : list[Node]) -> str:
        node = Array(self.s, l)
        print(f"Sending {node.to_string()}...")
        return node.send()

    def interactive(self):
        while True:
            s = input('>> ')
            eval(s)