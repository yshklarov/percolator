#ifndef LATTICEWINDOW_H
#define LATTICEWINDOW_H

#include <atomic>
#include <condition_variable>
#include <string>
#include <thread>
#include <glad/glad.h>

#include "imgui/imgui.h"

#include "lattice.h"


class LatticeWindow {
public:
  LatticeWindow(const std::string &title);
  ~LatticeWindow();

  LatticeWindow() =delete;
  LatticeWindow(const Lattice& rhs) =delete;
  LatticeWindow(Lattice&&) =delete;
  LatticeWindow& operator=(const Lattice&) =delete;
  LatticeWindow& operator=(Lattice&&) =delete;

  void push_data(Lattice* data);
  void show(bool &visible, bool torus);
  void mark_render_disposable();

private:
  const std::string title;

  constexpr static float zoom_increment {0.8F};
  int zoom_level {0};
  ImVec2 uv0 {0.0F, 0.0F};
  float zoom_scale {1.0F};

  std::mutex lattice_mutex;
  Lattice* lattice {nullptr};

  std::atomic_bool running {true};
  std::atomic_bool painting {false};
  std::thread worker_thread;
  std::condition_variable worker_cond;

  GLuint gl_texture {0};
  unsigned int gl_texture_width {0};
  unsigned int gl_texture_height {0};

  std::mutex texture_data_mutex;
  uint32_t* texture_data {nullptr};
  uint32_t* texture_data_painting {nullptr};
  int texture_data_width {0};
  int texture_data_height {0};
  std::atomic_bool texture_data_ready {false};

  std::atomic_bool current_render_disposable {false};

  void worker();
  void paint_texture_data(const Lattice* data);
  void send_texture_data();
};

#endif  // LATTICEWINDOW_H
