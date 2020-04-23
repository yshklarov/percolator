#include <atomic>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>

#include "imgui/imgui.h"

#include "latticewindow.h"
#include "utility.h"
#include "lattice.h"

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
  , worker_thread { [this]() { worker(); } }
{
  reset_view();
}

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
  ImGui::BeginChild(title.c_str(), ImVec2(0, - footer_height_to_reserve));

  const ImVec2 pos {window->DC.CursorPos};
  const ImVec2 frame_size {ImGui::GetContentRegionAvail()};
  const ImRect frame(pos, pos + frame_size);

  if (ImGui::ItemAdd(frame, 0)) {
    // Frame is not clipped, so show the lattice.
    if (texture_data_ready) {
      texture_data_ready = false;
      send_texture_data();
    }

    // Mouse controls
    if (ImGui::IsItemHovered()) {
      auto io {ImGui::GetIO()};

      // Mouse scroll: zooming
      int mouse_wheel_input {static_cast<int>(io.MouseWheel * 1.01F)}; // Nearest integer
      if (mouse_wheel_input != 0) {
        float zoom_scale_old {pow(zoom_increment, (float)zoom_level)};
        zoom_level += mouse_wheel_input;
        if (gl_texture_wraparound) {
          // Prevent video glitches and floating point errors.
          zoom_level = clamp(zoom_level, min_zoom_level, max_zoom_level);
        } else {
          // Can't zoom out to more than 100%.
          zoom_level = clamp(zoom_level, 0, max_zoom_level);
        }
        zoom_scale = pow(zoom_increment, (float)zoom_level);
        if (mouse_wheel_input && ImGui::IsMousePosValid()) {
          // Normalized mouse coordinates: Between 0.0F and 1.0F.
          ImVec2 mouse_pos {
            (io.MousePos.x - pos.x) / frame_size.x,
            (io.MousePos.y - pos.y) / frame_size.y
          };
          uv0.x += mouse_pos.x * (zoom_scale_old - zoom_scale);
          uv0.y += mouse_pos.y * (zoom_scale_old - zoom_scale);
        }
      }

      // Mouse drag: panning
      if (ImGui::IsMouseDragging(0, 0.0F)) {
        ImVec2 drag {io.MouseDelta};
        uv0.x -= drag.x * zoom_scale / frame_size.x;
        uv0.y -= drag.y * zoom_scale / frame_size.y;
      }
    }

    // Re-adjust zoom / pan.
    if (!gl_texture_wraparound) {
      // Clamp zoom level.
      zoom_level = std::max(0, zoom_level);
      zoom_scale = pow(zoom_increment, (float)zoom_level);
      // Re-adjust to not see boundary.
      uv0.x = clamp(uv0.x, 0.0F, 1.0F - zoom_scale);
      uv0.y = clamp(uv0.y, 0.0F, 1.0F - zoom_scale);
    }

    if (glIsTexture(gl_texture)) {
      // There's something to draw: we've rendered at least once before.

      // UV texture coordinates: see
      //     <https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples>.
      ImVec2 uv1 {uv0.x + zoom_scale, uv0.y + zoom_scale};
      ImGui::Image((void*)(intptr_t)gl_texture, frame_size, uv0, uv1);

      // Render grid lines, unless zoomed out too far.
      // TODO Make this less glitchy on high zoom levels. Is there any way to "clip" the DrawList
      // to stay inside the Image area?
      ImVec2 square_size {
        frame_size.x / (zoom_scale * gl_texture_width),
        frame_size.y / (zoom_scale * gl_texture_height)};
      float resolution {std::min(square_size.x, square_size.y)};
      if (resolution >= 20.0F) {
        auto draw_list {ImGui::GetForegroundDrawList()};
        int thickness {std::max(1, (int)((resolution - 20.0F) / 16.0F))};
        float offset_line {thickness % 2 == 0 ? 0.5F : 0.0F};  // prevent antialiasing
        float offset_x {fmod((1.0F - uv0.x) * frame_size.x / zoom_scale, square_size.x)};
        float offset_y {fmod((1.0F - uv0.y) * frame_size.y / zoom_scale, square_size.y)};
        // Work around mathematically incorrect fmod
        while (offset_x < 0.0F) {
          offset_x += square_size.x;
        }
        while (offset_y < 0.0F) {
          offset_y += square_size.y;
        }
        float alpha {clamp((resolution - 20.0F) / 20.0F, 0.0F, 1.0F)};
        auto border_color = ImGui::GetColorU32(ImVec4(0.0F, 0.0F, 0.0F, alpha));
        for (auto y {pos.y}; y + offset_y < frame.Max.y; y += square_size.y) {
          // Calling floor() prevents ImGui from doing antialiasing when thickness == 1.
          auto left {ImVec2(frame.Min.x, floor(y + offset_y) + offset_line)};
          auto right {ImVec2(frame.Max.x, floor(y + offset_y) + offset_line)};
          draw_list->AddLine(left, right, border_color, (float)thickness);
        }
        for (auto x {pos.x}; x + offset_x < frame.Max.x; x += square_size.x) {
          auto top {ImVec2(floor(x + offset_x) + offset_line, frame.Min.y)};
          auto bot {ImVec2(floor(x + offset_x) + offset_line, frame.Max.y)};
          draw_list->AddLine(top, bot, border_color, (float)thickness);
        }
      }
    }
  }  // Frame holding the lattice
  ImGui::EndChild();

  if (painting) {
    if (ImGui::SmallButton("Abort")) {
      painting = false;
    }
    ImGui::SameLine();
    ImGui::Text("Rendering...");
  } else {
    ImGui::Text("Scale: %.0f%% (scroll to zoom; drag to pan)", 100.0F / zoom_scale);
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(0.0F, 0.0F));  // Horizontal spacing
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset view")) {
      reset_view();
    }
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
  texture_data_mutex.lock();
  if (width != texture_data_width or
      height != texture_data_height or
      texture_data == nullptr) {
    delete[] texture_data;
    texture_data = new uint32_t[width * height];
    texture_data_width = width;
    texture_data_height = height;
    texture_data_mutex.unlock();

    delete[] texture_data_painting;
    texture_data_painting = new uint32_t[width * height];
  } else {
    texture_data_mutex.unlock();
  }

  // Draw pixels into texture.
  // TODO Don't use OpenGL to scale down giant textures: It's buggy (visual artefacts).
  //      Idea: Pass the entire block of data in the lattice to a shader, and do the color
  //      translation on the GPU.
  constexpr uint32_t grey = 0x202020FF;
  constexpr uint32_t red = 0xFF0000FF;
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
        site_color = red;
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
    uint32_t* tmp {texture_data};
    texture_data = texture_data_painting;
    texture_data_painting = tmp;
    texture_data_wraparound = data->is_torus();
    texture_data_mutex.unlock();
    texture_data_ready = true;
    painting = false;
  }
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
  // TODO Try glTexSubImage2D to copy without allocating?
  texture_data_mutex.lock();
  glTexImage2D(
    GL_TEXTURE_2D, 0, GL_RGBA,
    texture_data_width,
    texture_data_height,
    0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
    texture_data);
  IM_ASSERT(glIsTexture(gl_texture));
  gl_texture_width = texture_data_width;
  gl_texture_height = texture_data_height;
  gl_texture_wraparound = texture_data_wraparound;
  texture_data_mutex.unlock();
}

void LatticeWindow::reset_view() {
  zoom_level = 0;
  zoom_scale = 1.0F;
  uv0.x = 0.0F;
  uv0.y = 0.0F;
}
