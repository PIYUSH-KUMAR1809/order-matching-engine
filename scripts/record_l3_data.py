import websocket
import json
import time
import csv
import sys
import threading
import ssl
import os

SYMBOL = "btcusdt"
OUTPUT_FILE = "data/market_data.csv"
DURATION_SECONDS = 60
BATCH_SIZE = 100

file_lock = threading.Lock()
start_time = time.time()
message_count = 0
running = True

def on_message(ws, message):
    global message_count, running
    
    if not running:
        ws.close()
        return

    if time.time() - start_time > DURATION_SECONDS:
        print(f"\nRecording complete. {message_count} messages saved.")
        running = False
        ws.close()
        return

    data = json.loads(message)
    
    payload = data.get('data', {})
    event_type = payload.get('e')
    
    row = None
    
    if event_type == 'trade':
        side = 'S' if payload['m'] else 'B'
        price = payload['p']
        qty = payload['q']
        timestamp = payload['T']
        row = [timestamp, 'T', side, price, qty]
        
    elif event_type == 'depthUpdate':
        timestamp = payload['E'] 
        updates = []
        
        for price, qty in payload.get('b', []):
            op_type = 'C' if float(qty) == 0 else 'A'
            updates.append([timestamp, op_type, 'B', price, qty])
            
        for price, qty in payload.get('a', []):
            op_type = 'C' if float(qty) == 0 else 'A'
            updates.append([timestamp, op_type, 'S', price, qty])
            
        if updates:
            with file_lock:
                writer.writerows(updates)
                message_count += len(updates)
            return

    if row:
        with file_lock:
            writer.writerow(row)
            message_count += 1
            
    if message_count % 1000 == 0:
        sys.stdout.write(f"\rCaptured {message_count} events...")
        sys.stdout.flush()

def on_error(ws, error):
    print(f"Error: {error}")

def on_close(ws, close_status_code, close_msg):
    print("\nConnection closed")

def on_open(ws):
    print(f"Connected to Binance. Recording {SYMBOL} for {DURATION_SECONDS} seconds...")
    params = [f"{SYMBOL}@trade", f"{SYMBOL}@depth@100ms"]
    subscribe_message = {
        "method": "SUBSCRIBE",
        "params": params,
        "id": 1
    }
    ws.send(json.dumps(subscribe_message))

if __name__ == "__main__":
    if len(sys.argv) > 1:
        DURATION_SECONDS = int(sys.argv[1])
        
    os.makedirs("data", exist_ok=True)
    
    csv_file = open(OUTPUT_FILE, 'w', newline='')
    writer = csv.writer(csv_file)
    writer.writerow(["timestamp", "type", "side", "price", "quantity"])
    
    socket = "wss://stream.binance.com:9443/stream?streams=" + f"{SYMBOL}@trade/{SYMBOL}@depth@100ms"
    
    ws = websocket.WebSocketApp(socket,
                              on_open=on_open,
                              on_message=on_message,
                              on_error=on_error,
                              on_close=on_close)
    
    try:
        ws.run_forever(sslopt={"cert_reqs": ssl.CERT_NONE})
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        csv_file.close()
