#include "TcpServer.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>

TcpServer::TcpServer(MatchingEngine &engine, int port)
    : engine_(engine), port_(port), serverSocket_(-1), running_(false) {}

TcpServer::~TcpServer() { stop(); }

void TcpServer::start() {
  serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket_ < 0) {
    std::cerr << "Error creating socket" << std::endl;
    return;
  }

  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port_);

  if (bind(serverSocket_, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) <
      0) {
    std::cerr << "Error binding socket" << std::endl;
    return;
  }

  if (listen(serverSocket_, 10) < 0) {
    std::cerr << "Error listening" << std::endl;
    return;
  }

  running_ = true;
  std::cout << "Server started on port " << port_ << std::endl;
  acceptThread_ = std::thread(&TcpServer::acceptLoop, this);
}

void TcpServer::stop() {
  running_ = false;
  if (serverSocket_ >= 0) {
    close(serverSocket_);
    serverSocket_ = -1;
  }
  if (acceptThread_.joinable()) {
    acceptThread_.join();
  }
  for (auto &t : clientThreads_) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void TcpServer::acceptLoop() {
  while (running_) {
    sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    int clientSocket =
        accept(serverSocket_, (struct sockaddr *)&clientAddr, &clientLen);

    if (clientSocket < 0) {
      if (running_) {
        std::cerr << "Error accepting connection" << std::endl;
      }
      continue;
    }

    std::cout << "New connection from " << inet_ntoa(clientAddr.sin_addr)
              << std::endl;
    clientThreads_.emplace_back(&TcpServer::handleClient, this, clientSocket);
  }
}

void TcpServer::handleClient(int clientSocket) {
  char buffer[1024];
  while (running_) {
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);

    if (bytesRead <= 0) {
      break;  // Client disconnected or error
    }

    std::string request(buffer);
    std::string response = processRequest(clientSocket, request);
    write(clientSocket, response.c_str(), response.length());
  }
  close(clientSocket);
  removeClient(clientSocket);
}

std::string TcpServer::processRequest(int clientSocket,
                                      const std::string &request) {
  std::stringstream ss(request);
  std::string command;
  ss >> command;

  if (command == "BUY" || command == "SELL") {
    std::string symbol;
    double quantity;
    double price;
    ss >> symbol >> quantity >> price;

    OrderType type = (command == "BUY") ? OrderType::Buy : OrderType::Sell;
    // Simple ID generation for demo
    static std::atomic<OrderId> nextId{1};
    OrderId id = nextId++;

    Order order(id, symbol, type, OrderKind::Limit, price, quantity);
    auto trades = engine_.submitOrder(order);

    std::stringstream response;
    response << "ORDER_ADDED " << id;
    if (!trades.empty()) {
      response << " EXECUTED " << trades.size() << " TRADES";
      for (const auto &trade : trades) {
        broadcastTrade(symbol, trade.price, trade.quantity);
      }
    }
    response << "\n";
    return response.str();

  } else if (command == "CANCEL") {
    OrderId id;
    ss >> id;
    engine_.cancelOrder(id);
    return "ORDER_CANCELLED\n";

  } else if (command == "PRINT") {
    // This is a bit tricky because printOrderBook writes to stdout.
    // For a real server, we'd want it to return a string.
    // For now, let's just acknowledge.
    return "PRINT_REQUESTED_CHECK_SERVER_LOGS\n";

  } else if (command == "SUBSCRIBE") {
    std::string symbol;
    ss >> symbol;
    {
      std::lock_guard<std::mutex> lock(subscribersMutex_);
      subscribers_[symbol].push_back(clientSocket);
    }
    return "SUBSCRIBED " + symbol + "\n";

  } else if (command == "GET_BOOK") {
    std::string symbol;
    ss >> symbol;
    const OrderBook *book = engine_.getOrderBook(symbol);
    if (!book) {
      return "ERROR_NO_BOOK\n";
    }

    std::stringstream response;
    response << "BOOK " << symbol << " BIDS";
    for (const auto &pair : book->getBids()) {
      for (const auto &order : pair.second) {
        response << " " << order.price << " " << order.quantity;
      }
    }
    response << " ASKS";
    for (const auto &pair : book->getAsks()) {
      for (const auto &order : pair.second) {
        response << " " << order.price << " " << order.quantity;
      }
    }
    response << "\n";
    return response.str();
  }

  return "UNKNOWN_COMMAND\n";
}

void TcpServer::removeClient(int clientSocket) {
  std::lock_guard<std::mutex> lock(subscribersMutex_);
  for (auto &pair : subscribers_) {
    auto &clients = pair.second;
    clients.erase(std::remove(clients.begin(), clients.end(), clientSocket),
                  clients.end());
  }
}

void TcpServer::broadcastTrade(const std::string &symbol, double price,
                               double quantity) {
  std::lock_guard<std::mutex> lock(subscribersMutex_);
  if (subscribers_.find(symbol) == subscribers_.end()) {
    return;
  }

  std::stringstream ss;
  ss << "TRADE " << symbol << " " << price << " " << quantity << "\n";
  std::string msg = ss.str();

  for (int clientSocket : subscribers_[symbol]) {
    // In a real system, we should handle partial writes and errors here
    write(clientSocket, msg.c_str(), msg.length());
  }
}
