// Viewport widget for ImGui. Display a percolation lattice.

#include <atomic>
#include <cmath>
#include <cstdint>

#include "utility.h"
#include "imgui_latticeview.h"
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

bool make_gl_texture_from_lattice(
      const Lattice* data,
      GLuint* out_texture,
      PercolationMode percolation_mode) {
  unsigned int width {data->get_width()};
  unsigned int height {data->get_height()};

  // Textures can be very large: No point in reallocating unless the size has changed.
  static auto must_reallocate {true};
  static thread_local int texture_data_width {0}, texture_data_height {0};
  static thread_local uint32_t* texture_data {nullptr};
  if (width != texture_data_width or
      height != texture_data_height) {
    must_reallocate = true;
  }
  if (must_reallocate) {
    delete[] texture_data;
    texture_data = nullptr;
    // TODO how do we have multiple LatticeViews in the same thread? LatticeView should be
    // encapsulated in a class, with a destructor to free texture_data. Get rid of this
    // "thread_local" nonsense.
    texture_data = new uint32_t[width * height] {};
    texture_data_width = width;
    texture_data_height = height;
  }

  // Draw pixels into texture.
  // TODO only redraw what's changed. [See branch: fast-redraw]
  // TODO Don't use OpenGL to scale down giant textures: It's buggy (visual artefacts).
  // TODO Idea: Pass the entire block of data in the lattice to a shader, and do the color
  // translation on the GPU.
  constexpr uint32_t grey = 0x202020FF;
  constexpr uint32_t red = 0x0000FFFF;
  constexpr uint32_t green = 0x00FF00FF;
  constexpr uint32_t blue = 0x004CFFFF;
  constexpr uint32_t cyan = 0x2CCDFFFF;
  constexpr uint32_t black = 0x000000FF;
  constexpr uint32_t white = 0xFFFFFFFF;
  std::atomic_bool until_completion {true};
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
        site_color = green;
        IM_ASSERT(false);
        break;
      }
      texture_data[y*width + x] = site_color;
    }, until_completion);
  if (percolation_mode == PercolationMode::clusters) {
    // Overlay the clusters.
    uint32_t cluster_color {blue};  // Color of largest cluster
    uint32_t cluster_color_increment {0x09315700};
    data->for_each_cluster(
      [&] (const Cluster &cluster) {
        for (const auto site : cluster) {
          texture_data[site.y * width + site.x] = cluster_color;
        }
        cluster_color += cluster_color_increment;
      }, until_completion);
  }

  static GLuint texture {0};  // OpenGL texture identifier
  if (must_reallocate) {
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Set up filtering parameters for display.
    // This may look backwards, but it's correct: We need nearest-neighbour (GL_NEAREST) when blowing
    // up because we want big pixels to show up as squares, and we want interpolation (GL_LINEAR)
    // when shrinking a texture to fit, so that it looks nicer. Speed isn't an issue here.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  }

  // TODO When downsampling, we can get a Moire pattern. So do our own interpolation instead.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, texture_data);
  if (must_reallocate) {
    // Free old texture explicitly to fix animation jitter. "glDeleteTextures silently ignores 0's
    // and names that do not correspond to existing textures." -- OpenGL docs
    glDeleteTextures(1, out_texture);
    must_reallocate = false;
  }
  *out_texture = texture;

  return true;
}


// If data is nullptr then the previous data is shown again, which is much cheaper
// computationally. If data is nullptr and there is no previous data, then an empty frame is shown.
void Latticeview(const Lattice* data, PercolationMode percolation_mode) {
  static GLuint image_texture {0};
  static unsigned int image_width {0};
  static unsigned int image_height {0};

  if (data) {
    image_width = data->get_width();
    image_height = data->get_height();
    auto made_texture {make_gl_texture_from_lattice(data, &image_texture, percolation_mode)};
    IM_ASSERT(made_texture);
    IM_ASSERT(glIsTexture(image_texture));
  }

  auto window {ImGui::GetCurrentWindow()};
  if (window->SkipItems) {
    return;
  }
  const ImVec2 pos {window->DC.CursorPos};
  const ImVec2 frame_size {ImGui::GetContentRegionAvail()};
  const ImRect frame(pos, pos + frame_size);
  if (!ImGui::ItemAdd(frame, 0)) {
    return;
  }

  if (!glIsTexture(image_texture)) {
    // There's nothing to draw, not even from previous calls.
    return;
  }

  // For usage of ImGui::Image, see:
  // https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#About-texture-coordinates
  ImGui::Image((void*)(intptr_t)image_texture, ImVec2(frame.GetWidth(), frame.GetHeight()));

  // Render border, unless zoomed out too far.
  ImVec2 square_size {frame_size.x / image_width, frame_size.y / image_height};
  float resolution {std::min(square_size.x, square_size.y)};
  if (resolution >= 20.0F) {
    int thickness {std::max(1, (int)((resolution - 20.0F) / 16.0F))};
    // Prevent antialiasing when thickness is even.
    float offset {thickness % 2 == 0 ? 0.5F : 0.0F};
    float alpha {clamp((resolution - 20.0F) / 20.0F, 0.0F, 1.0F)};
    auto border_color = ImGui::GetColorU32(ImVec4(0.0F, 0.0F, 0.0F, alpha));
    for (auto y {1}; y < image_height; ++y) {
      // Calling floor() prevents ImGui from doing antialiasing when thickness == 1.
      auto top {ImVec2(frame.Min.x, floor(frame.Min.y + y * square_size.y) + offset)};
      auto bot {ImVec2(frame.Max.x, floor(frame.Min.y + y * square_size.y) + offset)};
      window->DrawList->AddLine(top, bot, border_color, (float)thickness);
    }
    for (auto x {1}; x < image_width; ++x) {
      auto left {ImVec2(floor(frame.Min.x + x * square_size.x) + offset, frame.Min.y)};
      auto right {ImVec2(floor(frame.Min.x + x * square_size.x) + offset,frame.Max.y)};
      window->DrawList->AddLine(left, right, border_color, (float)thickness);
    }
  }
}
