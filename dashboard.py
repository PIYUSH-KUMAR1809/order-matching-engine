import streamlit as st
import socket
import pandas as pd
import time
import threading

# Set page config
st.set_page_config(page_title="Order Matching Engine Dashboard", layout="wide")

st.title("🚀 High-Performance Order Matching Engine")

# Sidebar for connection
st.sidebar.header("Connection")
host = st.sidebar.text_input("Host", "localhost")
port = st.sidebar.number_input("Port", 8080)
symbol = st.sidebar.text_input("Symbol", "BTC-USD")

# State for data
if 'bids' not in st.session_state:
    st.session_state.bids = []
if 'asks' not in st.session_state:
    st.session_state.asks = []
if 'trades' not in st.session_state:
    st.session_state.trades = []

def get_order_book():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((host, port))
        s.sendall(f"GET_BOOK {symbol}\n".encode())
        data = s.recv(4096).decode().strip()
        s.close()
        
        if data.startswith("BOOK"):
            parts = data.split()
            # Format: BOOK SYMBOL BIDS P Q ... ASKS P Q ...
            try:
                bids_idx = parts.index("BIDS")
                asks_idx = parts.index("ASKS")
                
                bids_data = parts[bids_idx+1:asks_idx]
                asks_data = parts[asks_idx+1:]
                
                new_bids = []
                for i in range(0, len(bids_data), 2):
                    new_bids.append({"Price": float(bids_data[i]), "Quantity": float(bids_data[i+1])})
                    
                new_asks = []
                for i in range(0, len(asks_data), 2):
                    new_asks.append({"Price": float(asks_data[i]), "Quantity": float(asks_data[i+1])})
                
                return new_bids, new_asks
            except ValueError:
                pass
    except Exception as e:
        st.error(f"Connection Error: {e}")
    return [], []

# Auto-refresh loop
placeholder = st.empty()

while True:
    bids, asks = get_order_book()
    
    with placeholder.container():
        col1, col2 = st.columns(2)
        
        with col1:
            st.subheader(f"Bids (Buy {symbol})")
            if bids:
                df_bids = pd.DataFrame(bids)
                st.dataframe(df_bids, use_container_width=True)
            else:
                st.info("No Bids")
                
        with col2:
            st.subheader(f"Asks (Sell {symbol})")
            if asks:
                df_asks = pd.DataFrame(asks)
                st.dataframe(df_asks, use_container_width=True)
            else:
                st.info("No Asks")
    
    time.sleep(1)
