import socket
import time
import random

HOST = '127.0.0.1'
PORT = 8080

def send_command(sock, cmd):
    sock.sendall((cmd + '\n').encode())
    time.sleep(0.01)

def seed_data():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, PORT))
            print(f"Connected to {HOST}:{PORT}")
            
            mid_price = 50000
            
            print("Seeding Bids...")
            for i in range(50):
                price = mid_price - random.randint(1, 1000)
                qty = random.randint(1, 100)
                send_command(s, f"BUY BTC-USD {qty} {price}")

            print("Seeding Asks...")
            for i in range(50):
                price = mid_price + random.randint(10, 1000)
                qty = random.randint(1, 100)
                send_command(s, f"SELL BTC-USD {qty} {price}")

            print("Executing Trades...")
            for i in range(10):
                match_price = mid_price + 50
                send_command(s, f"BUY BTC-USD 5 {match_price}")
                
            print("Done! Order Book should look populated.")
            
    except ConnectionRefusedError:
        print("Error: Could not connect to server. Is it running?")

if __name__ == "__main__":
    seed_data()
