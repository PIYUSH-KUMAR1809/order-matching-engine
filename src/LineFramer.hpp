#pragma once

#include <string>
#include <vector>

// Splits a TCP byte stream into newline-terminated requests.
//
// A single read() can return a partial command, exactly one command, or
// several commands at once. The previous handleClient logic parsed whatever
// arrived as one request: coalesced commands after the first were silently
// dropped, and a command split across reads was submitted truncated (e.g. a
// partially received price). Feed every received chunk here and process each
// returned line; incomplete trailing bytes are held for the next chunk.
class LineFramer {
 public:
  // Appends [data, data + size) and returns every complete line. A trailing
  // '\r' (telnet-style CRLF clients) is stripped from each line.
  std::vector<std::string> feed(const char* data, size_t size) {
    pending_.append(data, size);
    std::vector<std::string> lines;
    size_t pos = 0;
    while ((pos = pending_.find('\n', pos)) != std::string::npos) {
      std::string line = pending_.substr(0, pos);
      pending_.erase(0, pos + 1);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(std::move(line));
      pos = 0;
    }
    return lines;
  }

  const std::string& pending() const { return pending_; }

 private:
  std::string pending_;
};
