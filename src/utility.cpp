#include "utility.h"

void pause_ms(unsigned int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
