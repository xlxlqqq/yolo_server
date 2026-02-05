import socket
import struct

def send_msg(sock, data: bytes):
    sock.sendall(struct.pack("!I", len(data)) + data)

def recv_msg(sock):
    raw_len = sock.recv(4)
    if not raw_len:
        return None
    length = struct.unpack("!I", raw_len)[0]
    return sock.recv(length)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 8080))   # 端口换成你的

send_msg(sock, b"hello server")
resp = recv_msg(sock)

print("server replied:", resp)

sock.close()
