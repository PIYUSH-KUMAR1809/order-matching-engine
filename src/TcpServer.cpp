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

TcpServer::TcpServer(Exchange &engine, int port)
    : engine_(engine), port_(port), serverSocket_(-1), running_(false) {
  engine_.setTradeCallback([this](const std::vector<Trade> &trades) {
    for (const auto &trade : trades) {
      std::string symbol = engine_.getSymbolName(trade.symbolId);
      broadcastTrade(symbol, trade.price, trade.quantity);
    }
  });
}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::start() {
  serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket_ < 0) {
    std::cerr << "Error creating socket\n";
    return false;
  }

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port_);

  if (bind(serverSocket_, reinterpret_cast<struct sockaddr *>(&serverAddr),
           sizeof(serverAddr)) < 0) {
    std::cerr << "Error binding socket\n";
    return false;
  }

  if (listen(serverSocket_, 10) < 0) {
    std::cerr << "Error listening\n";
    return false;
  }

  running_ = true;
  std::cout << "Server started on port " << port_ << "\n";
  acceptThread_ = std::jthread(&TcpServer::acceptLoop, this);
  return true;
}

void TcpServer::stop() {
  running_ = false;
  if (serverSocket_ >= 0) {
    close(serverSocket_);
    serverSocket_ = -1;
  }
}

void TcpServer::acceptLoop() {
  while (running_) {
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    int clientSocket =
        accept(serverSocket_, reinterpret_cast<struct sockaddr *>(&clientAddr),
               &clientLen);

    if (clientSocket < 0) {
      if (running_) {
        std::cerr << "Error accepting connection\n";
      }
      continue;
    }

    std::cout << "New connection from " << inet_ntoa(clientAddr.sin_addr)
              << "\n";
    clientThreads_.emplace_back(&TcpServer::handleClient, this, clientSocket);
  }
}

void TcpServer::handleClient(int clientSocket) {
  std::array<char, 1024> buffer{};
  while (running_) {
    buffer.fill(0);
    ssize_t bytesRead = read(clientSocket, buffer.data(), buffer.size() - 1);

    if (bytesRead <= 0) {
      break;
    }

    std::string request(buffer.data());
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
    Quantity quantity = 0;
    Price price = 0;
    ss >> symbol >> quantity >> price;

    OrderSide side = (command == "BUY") ? OrderSide::Buy : OrderSide::Sell;

    static std::atomic<OrderId> nextId{1};
    OrderId id = nextId++;
    uint64_t clientOrderId = 0;
    if (ss.rdbuf()->in_avail() > 0) {
      ss >> clientOrderId;
    }

    int32_t symbolId = engine_.registerSymbol(symbol, -1);

    Order order(id, clientOrderId, symbolId, side, OrderType::Limit, price,
                quantity);

    engine_.submitOrder(order);

    std::stringstream response;
    response << "ORDER_ACCEPTED_ASYNC " << id << "\n";
    return response.str();

  } else if (command == "CANCEL") {
    std::string symbol;
    OrderId id = 0;
    ss >> symbol >> id;
    int32_t symbolId = engine_.registerSymbol(symbol, -1);
    engine_.cancelOrder(symbolId, id);
    return "CANCEL_REQUEST_SENT\n";

  } else if (command == "PRINT") {
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
    int32_t symbolId = engine_.registerSymbol(symbol, -1);
    const OrderBook *book = engine_.getOrderBook(symbolId);
    if (!book) {
      return "ERROR_NO_BOOK\n";
    }

    std::stringstream response;
    response << "BOOK " << symbol << " BIDS";

    Price p = book->getBestBid();
    int levels = 0;

    const auto &bidMask = book->getBidMask();

    while (p > 0 && levels < 20) {
      if (bidMask.test(p)) {
        const auto &level = book->getLevel(p, OrderSide::Buy);
        if (level.activeCount > 0) {
          bool hasActive = false;
          for (const auto &order : level.orders) {
            if (order.active && order.quantity > 0) {
              response << " " << order.price << " " << order.quantity;
              hasActive = true;
            }
          }
          if (hasActive) levels++;
        }
      }
      p--;
    }

    response << " ASKS";
    p = book->getBestAsk();
    levels = 0;

    const auto &askMask = book->getAskMask();
    if (p != -1) {
      while (p < OrderBook::MAX_PRICE && levels < 20) {
        if (askMask.test(p)) {
          const auto &level = book->getLevel(p, OrderSide::Sell);
          if (level.activeCount > 0) {
            bool hasActive = false;
            for (const auto &order : level.orders) {
              if (order.active && order.quantity > 0) {
                response << " " << order.price << " " << order.quantity;
                hasActive = true;
              }
            }
            if (hasActive) levels++;
          }
        }
        p++;
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

void TcpServer::broadcastTrade(const std::string &symbol, Price price,
                               Quantity quantity) {
  std::lock_guard<std::mutex> lock(subscribersMutex_);
  if (subscribers_.find(symbol) == subscribers_.end()) {
    return;
  }

  std::stringstream ss;
  ss << "TRADE " << symbol << " " << price << " " << quantity << "\n";
  std::string msg = ss.str();

  for (int clientSocket : subscribers_[symbol]) {
    write(clientSocket, msg.c_str(), msg.length());
  }
}
