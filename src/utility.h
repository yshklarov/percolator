#ifndef UTILITY_H
#define UTILITY_H

#include <algorithm>
#include <cassert>
#include <chrono>
#include <thread>


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

void pause_ms(unsigned int ms);

#endif  // UTILITY_H
