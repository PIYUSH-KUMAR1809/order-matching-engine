import socket
import time
import threading

def read_messages(sock, name):
    try:
        while True:
            data = sock.recv(1024)
            if not data:
                break
            print(f"[{name}] Received: {data.decode().strip()}")
    except:
        pass

def verify():
    # Client 1: Subscriber
    sub_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sub_sock.connect(('localhost', 8080))
    threading.Thread(target=read_messages, args=(sub_sock, "Subscriber"), daemon=True).start()
    
    print("Subscriber connecting...")
    sub_sock.sendall(b"SUBSCRIBE BTC-USD\n")
    time.sleep(0.5)

    # Client 2: Maker
    maker_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    maker_sock.connect(('localhost', 8080))
    print("Maker sending SELL order...")
    maker_sock.sendall(b"SELL BTC-USD 10 50000\n")
    time.sleep(0.5)

    # Client 3: Taker
    taker_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    taker_sock.connect(('localhost', 8080))
    print("Taker sending BUY order...")
    taker_sock.sendall(b"BUY BTC-USD 10 50000\n")
    time.sleep(1)

    sub_sock.close()
    maker_sock.close()
    taker_sock.close()

if __name__ == "__main__":
    verify()
