#include <chrono>

#include "utility.h"


void pause_ms(unsigned int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

Stopwatch::Stopwatch() {}
Stopwatch::~Stopwatch() {}

void Stopwatch::start() {
  start_time = std::chrono::high_resolution_clock::now();
}

void Stopwatch::stop() {
  running = false;
}

bool Stopwatch::is_running() {
  return running;
}

double Stopwatch::elapsed_ms() {
  auto now {std::chrono::high_resolution_clock::now()};
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
}
