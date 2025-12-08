#include <iostream>
#include <thread>

#include "MatchingEngine.hpp"
#include "TcpServer.hpp"

int main() {
  MatchingEngine engine;
  TcpServer server(engine, 8080);

  std::cout << "Starting Order Matching Engine Server..." << std::endl;
  server.start();

  // Keep the main thread alive
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}
