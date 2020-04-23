#ifndef UTILITY_H
#define UTILITY_H

#include <algorithm>
#include <cassert>
#include <chrono>
#include <thread>


class ScopeGuard {
public:
  ScopeGuard(std::function<void ()> callback)
    : cb {callback} {}
  ~ScopeGuard() { cb(); }
private:
  std::function<void ()> cb;
};

// Return the closest value that belongs to the interval [min, max].
// E.g., clamp(-3, 0, 10) == 0; clamp(3, 0, 10) == 3.
template<typename T>
inline T clamp(T value, T min, T max) {
  assert(min <= max);
  return std::max(std::min(value, max), min);
}

// Comparison for sorting in reverse order
struct ReverseCmp {
  bool operator()(const unsigned int& lhs, const unsigned int& rhs) const {
    return lhs > rhs;
  }
};

class Stopwatch {
public:
  Stopwatch();
  ~Stopwatch();

  void start();
  void stop();
  bool is_running();
  double elapsed_ms();
private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
  bool running {false};
};

void pause_ms(unsigned int ms);

#endif  // UTILITY_H
