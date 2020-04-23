#include <atomic>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>

#include "utility.h"
#include "latticewindow.h"
#include "lattice.h"
#include "imgui/imgui.h"

// Needed for operator overloading for ImVec2
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui_internal.h"

#ifdef _WIN32
#define NOMINMAX    // Prevent windows.h from clobbering STL's min and max.
#include <windows.h>
#endif
//#include <GL/gl.h>   // Already handled by glad/glad.h
#include <glad/glad.h>


LatticeWindow::LatticeWindow(const std::string &window_title)
  : title {window_title}
  , worker_thread { [this]() { worker(); } } {}

LatticeWindow::~LatticeWindow() {
  painting = false;
  running = false;
  worker_cond.notify_all();  // Get worker to stop
  worker_thread.join();
  delete lattice;
}

// Send a lattice to be rendered. The currently-rendering lattice will be rendered first, then the
// latest-pushed lattice will be rendered. Any intermediate lattices will be deleted.
// LatticeWindow deletes data when it's done with it.
void LatticeWindow::push_data(Lattice* data) {
  IM_ASSERT(data != nullptr);
  std::unique_lock<std::mutex> lock {lattice_mutex};
  if (current_render_disposable) {
    painting = false;  // Abort current render immediately.
    current_render_disposable = false;
  }
  lattice = data;
  worker_cond.notify_all();
}

void LatticeWindow::show(bool &visible) {
  ImGui::Begin(title.c_str(), &visible);
  ScopeGuard imgui_guard_1 {[]() { ImGui::End(); }};
  auto window {ImGui::GetCurrentWindow()};
  if (window->SkipItems) {
    return;  // Window is not visible.
  }

  // Leave spacing for the message line at the bottom of the window.
  const float footer_height_to_reserve {ImGui::GetTextLineHeightWithSpacing()};
  ImGui::BeginChild("Lattice", ImVec2(0, - footer_height_to_reserve));

  const ImVec2 pos {window->DC.CursorPos};
  const ImVec2 frame_size {ImGui::GetContentRegionAvail()};
  const ImRect frame(pos, pos + frame_size);

  if (ImGui::ItemAdd(frame, 0)) {
    // Frame is not clipped, so show the lattice.
    texture_data_mutex.lock();
    if (texture_data_ready) {
      send_texture_data();
      texture_data_ready = false;
    }
    texture_data_mutex.unlock();

    // There's something to draw: we've rendered at least once before.
    if (glIsTexture(gl_texture)) {

      // For usage of ImGui::Image, see:
      // <https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples \
      //                                                    #About-texture-coordinates>
      ImGui::Image((void*)(intptr_t)gl_texture, ImVec2(frame.GetWidth(), frame.GetHeight()));

      // Render border, unless zoomed out too far.
      auto draw_list {ImGui::GetForegroundDrawList()};
      ImVec2 square_size {frame_size.x / gl_texture_width, frame_size.y / gl_texture_height};
      float resolution {std::min(square_size.x, square_size.y)};
      if (resolution >= 20.0F) {
        int thickness {std::max(1, (int)((resolution - 20.0F) / 16.0F))};
        // Prevent antialiasing when thickness is even.
        float offset {thickness % 2 == 0 ? 0.5F : 0.0F};
        float alpha {clamp((resolution - 20.0F) / 20.0F, 0.0F, 1.0F)};
        auto border_color = ImGui::GetColorU32(ImVec4(0.0F, 0.0F, 0.0F, alpha));
        for (auto y {1}; y < gl_texture_height; ++y) {
          // Calling floor() prevents ImGui from doing antialiasing when thickness == 1.
          auto top {ImVec2(frame.Min.x, floor(frame.Min.y + y * square_size.y) + offset)};
          auto bot {ImVec2(frame.Max.x, floor(frame.Min.y + y * square_size.y) + offset)};
          draw_list->AddLine(top, bot, border_color, (float)thickness);
        }
        for (auto x {1}; x < gl_texture_width; ++x) {
          auto left {ImVec2(floor(frame.Min.x + x * square_size.x) + offset, frame.Min.y)};
          auto right {ImVec2(floor(frame.Min.x + x * square_size.x) + offset,frame.Max.y)};
          draw_list->AddLine(left, right, border_color, (float)thickness);
        }
      }
    }
  }

  ImGui::EndChild();
  if (painting) {
    if (ImGui::SmallButton("Abort")) {
      painting = false;
    }
    ImGui::SameLine();
    ImGui::Text("Rendering...");
  }
}

// Once this is called, the next time a lattice is pushed via push_data(), any render operations
// will be canceled and the new lattice will begin rendering without delay.
void LatticeWindow::mark_render_disposable() {
  current_render_disposable = true;
}

void LatticeWindow::worker() {
  while (running) {
    Lattice* tmp_lattice {nullptr};
    {
      std::unique_lock<std::mutex> lock {lattice_mutex};
      if (lattice == nullptr) {
        worker_cond.wait(
          lock,
          [this]() {
            return lattice != nullptr || !running;
          });
      }
      tmp_lattice = lattice;
      lattice = nullptr;
    }
    if (running) {
      paint_texture_data(tmp_lattice);
    }
    delete tmp_lattice;
  }
}

void LatticeWindow::paint_texture_data(const Lattice* data) {
  IM_ASSERT(data != nullptr);

  painting = true;
  unsigned int width {data->get_width()};
  unsigned int height {data->get_height()};

  // Textures can be very large: No point in reallocating unless the size has changed.  We use two
  // buffers so that rendering can be done in parallel with sending to the GPU. This is important
  // during "flowing" mode.
  if (width != texture_data_width or
      height != texture_data_height or
      texture_data == nullptr) {
    texture_data_mutex.lock();
    delete[] texture_data;
    texture_data = new uint32_t[width * height];
    texture_data_width = width;
    texture_data_height = height;
    texture_data_mutex.unlock();

    delete[] texture_data_painting;
    texture_data_painting = new uint32_t[width * height];
  }

  // Draw pixels into texture.
  // TODO Don't use OpenGL to scale down giant textures: It's buggy (visual artefacts).
  //      Idea: Pass the entire block of data in the lattice to a shader, and do the color
  //      translation on the GPU.
  constexpr uint32_t grey = 0x202020FF;
  constexpr uint32_t red = 0x0000FFFF;
  constexpr uint32_t green = 0x00FF00FF;
  constexpr uint32_t blue = 0x004CFFFF;
  constexpr uint32_t cyan = 0x2CCDFFFF;
  constexpr uint32_t black = 0x000000FF;
  constexpr uint32_t white = 0xFFFFFFFF;
  data->for_each_site(
    [&] (const int x, const int y) {
      uint32_t site_color {white};
      switch (data->site_status(x, y)) {
      case SiteStatus::open:
        site_color = white;
        break;
      case SiteStatus::closed:
        site_color = grey;
        break;
      case SiteStatus::flooded:
        site_color = blue;
        break;
      case SiteStatus::freshly_flooded:
        site_color = cyan;
        break;
      default:
        // This can sometimes happen if lattice generation is aborted (so there's garbage in the
        // memory).
        site_color = black;
        break;
      }
      texture_data_painting[y*width + x] = site_color;
    }, painting);

  // Overlay clusters, if any.
  uint32_t cluster_color {blue};  // Color of largest cluster
  auto next_color {
    [](uint32_t c) {
      // TODO What sequence of colors has good contrast? Try HSV instead of RGB.
      const static uint32_t cluster_color_increment {0x1A316A00};
      return c + cluster_color_increment;
    }};
  data->for_each_cluster(
    [&] (const Cluster &cluster) {
      for (const auto site : cluster) {
        texture_data_painting[site.y * width + site.x] = cluster_color;
      }
      cluster_color = next_color(cluster_color);
    }, painting);

  if (painting) {  // Unless aborted
    texture_data_mutex.lock();
    auto tmp {texture_data};
    texture_data = texture_data_painting;
    texture_data_painting = tmp;
    texture_data_ready = true;
    texture_data_mutex.unlock();
  }
  painting = false;
}

void LatticeWindow::send_texture_data() {
  // Free old texture explicitly to fix animation jitter. "glDeleteTextures silently ignores 0's
  // and names that do not correspond to existing textures." -- OpenGL docs
  glDeleteTextures(1, &gl_texture);
  glGenTextures(1, &gl_texture);
  glBindTexture(GL_TEXTURE_2D, gl_texture);

  // Set up filtering parameters for display.
  // This may look backwards, but: We need nearest-neighbour (GL_NEAREST) when blowing up because
  // we want big pixels to show up as squares, and we want interpolation (GL_LINEAR) when
  // shrinking a texture to fit, so that it looks nicer. Speed isn't an issue here.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  // TODO When downsampling, we can get a Moire pattern. So do our own interpolation, instead.
  // TODO This is quite slow for large textures, and sometimes causes a noticeable delay. But to
  // call this in a separate thread, we have to figure out how context management works in OpenGL.
  glTexImage2D(
    GL_TEXTURE_2D, 0, GL_RGBA,
    texture_data_width,
    texture_data_height,
    0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
    texture_data);
  IM_ASSERT(glIsTexture(gl_texture));
  gl_texture_width = texture_data_width;
  gl_texture_height = texture_data_height;
}
