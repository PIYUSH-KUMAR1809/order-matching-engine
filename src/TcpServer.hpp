#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "MatchingEngine.hpp"

class TcpServer {
 public:
  TcpServer(MatchingEngine &engine, int port);
  ~TcpServer();

  void start();
  void stop();

 private:
  void acceptLoop();
  void handleClient(int clientSocket);
  std::string processRequest(int clientSocket, const std::string &request);
  void removeClient(int clientSocket);
  void broadcastTrade(const std::string &symbol, double price, double quantity);

  MatchingEngine &engine_;
  int port_;
  int serverSocket_;
  std::atomic<bool> running_;
  std::thread acceptThread_;
  std::vector<std::thread> clientThreads_;

  std::mutex subscribersMutex_;
  std::unordered_map<std::string, std::vector<int>> subscribers_;
};
