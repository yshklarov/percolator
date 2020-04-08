// Viewport widget for ImGui. Display a percolation lattice.

#include <cstdint>

#include "imgui_latticeview.h"
#include "lattice.h"
#include "imgui/imgui.h"

// Needed for operator overloading for ImVec2
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui_internal.h"

#include <GL/gl.h>

bool make_gl_texture_from_lattice(const Lattice* data, GLuint* out_texture) {
  int width {data->get_width()};
  int height {data->get_height()};

  // Create a OpenGL texture identifier.
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  // Set up filtering parameters for display.
  // This may look backwards, but it's correct: We need nearest-neighbour (GL_NEAREST) when blowing
  // up because we want big pixels to show up as squares, and we want interpolation (GL_LINEAR)
  // when shrinking a texture to fit, so that it looks nicer. Speed isn't an issue here.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  // Draw pixels into texture.
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  uint32_t* texture_data = new uint32_t[width * height] {};
  constexpr auto grey = 0x202020FF;
  constexpr auto blue = 0x004CFFFF;
  constexpr auto cyan = 0x2CCDFFFF;
  //constexpr auto black = 0x000000FF;
  constexpr auto white = 0xFFFFFFFF;
  for (auto y {0}; y < height; ++y) {
    for (auto x {0}; x < width; ++x) {
      auto site_color {grey};
      if (data->is_freshly_flooded(x, y)) {
        site_color = cyan;
      } else if (data->is_flooded(x, y)) {
        site_color = blue;
      } else if (data->is_open(x, y)) {
        site_color = white;
      }
      texture_data[y*width + x] = site_color;
    }
  }
    
  // TODO When the lattice is very large (10,000 x 10,000), we can get artefacts. Maybe
  // manually downsample texture_data before sending it to glTexImage2D.
  // TODO When downsampling, we can get a Moire pattern. So do our own interpolation instead.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, texture_data);
  delete[] texture_data;

  glDeleteTextures(1, out_texture);   // Free old texture explicitly to fix animation jitter.
  *out_texture = texture;

  return true;
}


void Latticeview(const Lattice* data, bool redraw) {
  static GLuint image_texture {0};
  const int image_width {data->get_width()};
  const int image_height {data->get_height()};

  if (redraw) {
    IM_ASSERT(make_gl_texture_from_lattice(data, &image_texture));
  }
  IM_ASSERT(glIsTexture(image_texture));

  auto window {ImGui::GetCurrentWindow()};
  if (window->SkipItems)
    return;
  const ImVec2 pos {window->DC.CursorPos};
  const ImVec2 frame_size {ImGui::GetContentRegionAvail()};
  const ImRect frame(pos, pos + frame_size);
  if (!ImGui::ItemAdd(frame, 0))
    return;

  ImGui::Image((void*)(intptr_t)image_texture, ImVec2(frame.GetWidth(), frame.GetHeight()));

  // Render border, unless zoomed out too far.
  ImVec2 square_size {frame_size.x / image_width, frame_size.y / image_height};
  auto resolution {fmin(square_size.x, square_size.y)};
  if (resolution >= 20) {
    float thickness {fmax(0.0F, (resolution - 20.0F) / 16.F)};
    float alpha {fmin(1.0f, fmax(0.0F, (resolution - 20.0F) / 20.0F))};
    auto border_color = ImGui::GetColorU32(ImVec4(0.0F, 0.0F, 0.0F, alpha));
    for (auto y {1}; y < image_height; ++y) {
      auto top {ImVec2(frame.Min.x, frame.Min.y + y * square_size.y)};
      auto bot {ImVec2(frame.Max.x, frame.Min.y + y * square_size.y)};
      window->DrawList->AddLine(top, bot, border_color, thickness);
    }
    for (auto x {1}; x < image_width; ++x) {
      auto left {ImVec2(frame.Min.x + x * square_size.x, frame.Min.y)};
      auto right {ImVec2(frame.Min.x + x * square_size.x, frame.Max.y)};
      window->DrawList->AddLine(left, right, border_color, thickness);
    }
  }
}
