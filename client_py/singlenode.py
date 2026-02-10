import socket
import json
import struct
import time
import threading
import statistics
import random
import string

SERVER_ADDR = ("127.0.0.1", 8080)

TOTAL_REQUESTS = 1000      # 总请求数
CONCURRENCY = 10           # 并发线程数
MODE = "STORE"             # STORE / GET / MIXED


# ------------------------
# 协议层
# ------------------------
def send_msg(sock, payload: bytes):
    msg = struct.pack("!I", len(payload)) + payload
    sock.sendall(msg)


def recv_msg(sock):
    hdr = sock.recv(4)
    if not hdr:
        return None
    length = struct.unpack("!I", hdr)[0]

    data = b''
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            return None
        data += chunk
    return data


# ------------------------
# 构造请求
# ------------------------
def random_image_id():
    return "img_" + ''.join(random.choices(string.ascii_letters + string.digits, k=8))


def make_store_payload(image_id):
    data = {
        "image_id": image_id,
        "width": 1280,
        "height": 720,
        "image_hash": "hash_" + image_id,
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
    return ("STORE " + json.dumps(data)).encode()


def make_get_payload(image_id):
    return f"GET {image_id}".encode()


# ------------------------
# Worker
# ------------------------
def worker(thread_id, requests, latencies, stored_ids):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(SERVER_ADDR)

        for _ in range(requests):
            if MODE == "STORE":
                image_id = random_image_id()
                payload = make_store_payload(image_id)
                stored_ids.append(image_id)

            elif MODE == "GET":
                if not stored_ids:
                    continue
                image_id = random.choice(stored_ids)
                payload = make_get_payload(image_id)

            else:  # MIXED
                if random.random() < 0.5:
                    image_id = random_image_id()
                    payload = make_store_payload(image_id)
                    stored_ids.append(image_id)
                else:
                    if not stored_ids:
                        continue
                    image_id = random.choice(stored_ids)
                    payload = make_get_payload(image_id)

            start = time.time()
            send_msg(sock, payload)
            resp = recv_msg(sock)
            end = time.time()

            if resp is None:
                continue

            latencies.append((end - start) * 1000)  # ms

        sock.close()
    except Exception as e:
        print(f"[Thread {thread_id}] error:", e)


# ------------------------
# 主流程
# ------------------------
def main():
    print("==== YOLO Storage Benchmark ====")
    print(f"Server       : {SERVER_ADDR[0]}:{SERVER_ADDR[1]}")
    print(f"Mode         : {MODE}")
    print(f"Concurrency  : {CONCURRENCY}")
    print(f"Total req    : {TOTAL_REQUESTS}")
    print("--------------------------------")

    per_thread = TOTAL_REQUESTS // CONCURRENCY
    threads = []
    latencies = []
    stored_ids = []

    start_time = time.time()

    for i in range(CONCURRENCY):
        t = threading.Thread(
            target=worker,
            args=(i, per_thread, latencies, stored_ids)
        )
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    duration = time.time() - start_time
    qps = len(latencies) / duration if duration > 0 else 0

    print("\n==== RESULT ====")
    print(f"Completed requests : {len(latencies)}")
    print(f"Total time (s)     : {duration:.2f}")
    print(f"QPS               : {qps:.2f}")

    if latencies:
        latencies.sort()
        print(f"Avg latency (ms)  : {statistics.mean(latencies):.2f}")
        print(f"P50 latency (ms)  : {latencies[int(0.50 * len(latencies))]:.2f}")
        print(f"P95 latency (ms)  : {latencies[int(0.95 * len(latencies))]:.2f}")
        print(f"P99 latency (ms)  : {latencies[int(0.99 * len(latencies))]:.2f}")


if __name__ == "__main__":
    main()
