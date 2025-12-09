#include <iostream>
#include <thread>

#include "Exchange.hpp"
#include "TcpServer.hpp"

int main() {
  Exchange engine;
  TcpServer server(engine, 8080);

  std::cout << "Starting Order Matching Engine Server..." << std::endl;
  if (!server.start()) {
    std::cerr << "Failed to start server" << std::endl;
    return 1;
  }

  // Keep the main thread alive
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}
