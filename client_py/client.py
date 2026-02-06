import socket
import struct

def send_msg(sock, data: bytes):
    sock.sendall(struct.pack("!I", len(data)) + data)

def recv_msg(sock):
    hdr = sock.recv(4)
    if not hdr:
        return None
    length = struct.unpack("!I", hdr)[0]
    return sock.recv(length)

sock = socket.socket()
sock.connect(("127.0.0.1", 8080))

send_msg(sock, b"hello server")
reply = recv_msg(sock)
print(reply.decode())

send_msg(sock, b'{"cmd":"ping"}')
print(recv_msg(sock).decode())
