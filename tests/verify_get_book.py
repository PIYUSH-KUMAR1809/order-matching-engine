import socket
import time

def verify():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 8080))
    
    print("Sending orders...")
    sock.sendall(b"BUY BTC-USD 10 50000\n")
    print(f"Buy Response: {sock.recv(1024).decode().strip()}")
    time.sleep(0.1)
    sock.sendall(b"SELL BTC-USD 5 51000\n") # No match, should stay in book
    print(f"Sell Response: {sock.recv(1024).decode().strip()}")
    time.sleep(0.1)
    
    print("Requesting Order Book...")
    sock.sendall(b"GET_BOOK BTC-USD\n")
    data = sock.recv(4096).decode().strip()
    print(f"Received: {data}")
    
    if "BIDS 50000 10" in data and "ASKS 51000 5" in data:
        print("SUCCESS: Order Book verified.")
    else:
        print("FAILURE: Order Book data incorrect.")
        
    sock.close()

if __name__ == "__main__":
    verify()
