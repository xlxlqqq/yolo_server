import socket
import json
import struct

data = {
    "image_id": "img_001",
    "width": 1280,
    "height": 720,
    "image_hash": "abc123",
    "boxes": [
        {
            "class_id": 0,
            "x": 0.5,
            "y": 0.5,
            "w": 0.2,
            "h": 0.3,
            "confidence": 0.9
        }
    ]
}

payload = ("STORE " + json.dumps(data)).encode()

msg = struct.pack("!I", len(payload)) + payload

s = socket.socket()
s.connect(("127.0.0.1", 8080))
s.sendall(msg)

resp_len = struct.unpack("!I", s.recv(4))[0]
resp = s.recv(resp_len)
print(resp.decode())

payload = ("GET " + "img_001").encode()
msg = struct.pack("!I", len(payload)) + payload
s.sendall(msg)

resp_len = struct.unpack("!I", s.recv(4))[0]
resp = s.recv(resp_len)
print(resp.decode())
