#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "Exchange.hpp"

class TcpServer {
 public:
  TcpServer(Exchange &engine, int port);
  ~TcpServer();

  bool start();
  void stop();

 private:
  void acceptLoop();
  void handleClient(int clientSocket);
  std::string processRequest(int clientSocket, const std::string &request);
  void removeClient(int clientSocket);
  void broadcastTrade(const std::string &symbol, double price, double quantity);

  Exchange &engine_;
  int port_;
  int serverSocket_;
  std::atomic<bool> running_;
  std::jthread acceptThread_;
  std::vector<std::jthread> clientThreads_;

  std::mutex subscribersMutex_;
  std::unordered_map<std::string, std::vector<int>> subscribers_;
};
