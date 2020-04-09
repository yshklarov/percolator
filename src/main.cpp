#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

#ifdef _WIN32
#define NOMINMAX    // Prevent windows.h from clobbering STL's min and max.
#include <windows.h>
#endif

#include <SDL.h>
#undef main  // SDL clobbers "main" on Windows.
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/examples/imgui_impl_sdl.h"
#include "imgui/examples/imgui_impl_opengl3.h"

// Desktop OpenGL function loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple
                                // definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple
                                // definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif


#include "lattice.h"
#include "graphics/imgui_latticeview.h"


// TODO Move the definition of ProbabilityMeasure to a separate header and source file.
enum class ProbabilityMeasure : int
    {open, bernoulli, pattern_1, pattern_2, pattern_3};
// Make sure to keep this in the same order as ProbabilityMeasure, or the dropdown menu will select
// the wrong setting.
const char* ProbabilityMeasureNames[]
    {"Open", "Bernoulli", "Test Pattern 1", "Test Pattern 2", "Test Pattern 3" };

static void HelpMarker(const char* desc) {
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0F);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void begin_disable_items() {
  ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5F);
}
void end_disable_items() {
  ImGui::PopStyleVar();
  ImGui::PopItemFlag();
}

static void show_menu_file() {
  // TODO Set up keyboard shortcut
  if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
    SDL_Event sdlevent;
    sdlevent.type = SDL_QUIT;
    SDL_PushEvent(&sdlevent);
  }
}

void regenerate_lattice(Lattice &lattice, const ProbabilityMeasure measure, const float p) {
  switch (measure) {
  case ProbabilityMeasure::open:
    lattice.fill_pattern(Pattern::open);
    break;
  case ProbabilityMeasure::pattern_1:
    lattice.fill_pattern(Pattern::pattern_1);
    break;
  case ProbabilityMeasure::pattern_2:
    lattice.fill_pattern(Pattern::pattern_2);
    break;
  case ProbabilityMeasure::pattern_3:
    lattice.fill_pattern(Pattern::pattern_3);
    break;
  case ProbabilityMeasure::bernoulli:
    lattice.randomize_bernoulli(p);
    break;
  default:
    break;
  }
}

int main(int, char**) {
#ifdef _WIN32
  // TODO Once we set up logging, we'll have a proper Windows application. For now, we'll just
  // hide the console window. Options: SW_HIDE, SW_MINIMIZE, ...
  ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

  // Set up SDL
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "Error: " << SDL_GetError() << '\n';
    return -1;
  }

  // Decide GL+GLSL versions
#if __APPLE__
  // GL 3.2 Core + GLSL 150
  const char* glsl_version = "#version 150";
  // SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG is always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* window = SDL_CreateWindow(
    "Percolator",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  // Try setting adaptive vsync; if it fails then set regular vsync.
  if (SDL_GL_SetSwapInterval(-1) == -1) {
    SDL_GL_SetSwapInterval(1);
  }

  // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
  bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
  bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
  bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
  bool err = false;
  glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
  bool err = false;
  glbinding::initialize(
    [](const char* name) {
      return (glbinding::ProcAddress)SDL_GL_GetProcAddress(name);
    });
#else
  bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to
                    // requires some form of initialization.
#endif
  if (err) {
    std::cerr << "Failed to initialize OpenGL loader!\n";
#if defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    std::cerr << glewGetErrorString(err) << '\n';
#endif
    return 1;
  }

  // Set up ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Multi-viewport / platform windows
  io.ConfigViewportsNoTaskBarIcon = true;

  // Built-in themes
  //ImGui::StyleColorsClassic();
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look
  // identical to regular ones.
  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0F;
    style.FrameRounding = 3.0F;
    style.FrameBorderSize = 1.0F;  // Show border around widgets for better clarity
    style.Colors[ImGuiCol_WindowBg].w = 0.9F;  // Window background alpha (transparency)
  }

  // Set up platform/renderer bindings
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // State variables
  auto show_demo_window {false};
  auto show_lattice_window {true};
  auto show_about_window {false};
  auto redraw {true};
  auto flowing {false};
  std::chrono::time_point<std::chrono::high_resolution_clock> flow_start_time;
  auto clear_color {ImVec4(0.00F, 0.00F, 0.00F, 1.00F)};  // Background color behind DockSpace
  auto lattice_size {30};
  float flow_speed {10.0F};
  auto lattice {Lattice(lattice_size, lattice_size)};
  auto probability_measure {ProbabilityMeasure::bernoulli};
  const float rect_site_percolation_threshold {0.59274605F};
  float open_p {rect_site_percolation_threshold};
  regenerate_lattice(lattice, probability_measure, open_p);
  auto auto_percolate {false};
  auto auto_flow {false};
  auto regeneration_was_requested {false};
  auto percolation_was_requested {false};
  auto percolation_step_was_requested {false};
  auto flow_was_requested {false};

  // Main loop
  auto done {false};
  while (!done) {
    // Poll and handle events (inputs, window resize, etc.)

    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui
    // wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
    //   application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main
    //   application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application
    // based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) {
        done = true;
      }
      // Window closed
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window)) {
        done = true;
      }
      // Ctrl-Q
      if (event.type == SDL_KEYDOWN &&
          event.key.keysym.sym == SDLK_q &&
          (SDL_GetModState() & KMOD_CTRL)) {
        done = true;
      }
    }

    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    auto viewport {ImGui::GetMainViewport()};
    auto work_pos {viewport->GetWorkPos()};
        
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        show_menu_file();
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("About", NULL, &show_about_window);
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    if (show_about_window) {
      if (ImGui::Begin(
            "About Percolator",
            &show_about_window,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Percolator");

        // Clickable URL.
        // TODO: Move this to a helper function; open link in a web browser window.
        const std::string project_url {"https://github.com/yshklarov/percolator"};
        static auto project_url_clicked {false};
        ImGui::Text("%s", project_url.c_str());
        if (ImGui::IsItemClicked()) {
          ImGui::SetClipboardText(project_url.c_str());
          project_url_clicked = true;
        }
        if (ImGui::IsItemHovered()) {
          const int CURSOR_HAND {7};
          ImGui::SetMouseCursor(CURSOR_HAND);
          if (project_url_clicked) {
            ImGui::SetTooltip("Copied URL to clipboard");
          }
        } else {  // Hide tooltip when mouse cursor leaves.
          project_url_clicked = false;
        }

        ImGui::Separator();
        ImGui::Text("By Yakov Shklarov and 8.5tails");
        if (ImGui::Button("Close", ImVec2(100, 0))) {
          show_about_window = false;
        }
      }
      ImGui::End();
    }

    // Root window (DockSpace)
    {
      const auto opt_fullscreen {true};
      const auto dockspace_flags {ImGuiDockNodeFlags_None};

      // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable
      // into, because it would be confusing to have two docking targets within each others.
      ImGuiWindowFlags imgui_window_flags {ImGuiWindowFlags_NoDocking};
      if (opt_fullscreen) {
        ImGui::SetNextWindowPos(work_pos);
        ImGui::SetNextWindowSize(viewport->GetWorkSize());
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        imgui_window_flags |=
          ImGuiWindowFlags_NoTitleBar |
          ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoResize |
          ImGuiWindowFlags_NoMove;
        imgui_window_flags |=
          ImGuiWindowFlags_NoBringToFrontOnFocus |
          ImGuiWindowFlags_NoNavFocus;
      }

      // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
      // and handle the pass-thru hole, so we ask Begin() to not render a background.
      if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) {
        imgui_window_flags |= ImGuiWindowFlags_NoBackground;
      }

      // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
      // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, all
      // active windows docked into it will lose their parent and become undocked.
      // We cannot preserve the docking relationship between an active window and an inactive
      // docking, otherwise any change of dockspace/settings would lead to windows being stuck in
      // limbo and never being visible.
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
      ImGui::Begin("DockSpace", nullptr, imgui_window_flags);
      ImGui::PopStyleVar();

      if (opt_fullscreen) {
        ImGui::PopStyleVar(2);  // Back to normal WindowRounding  and WindowBorderSize.
      }

      if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("DockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0F, 0.0F), dockspace_flags);

        // Initial window layout
        static bool first_time {true};
        if (first_time) {
          first_time = false;
          auto viewport_size = viewport->GetWorkSize();
          ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
          ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
          ImGui::DockBuilderSetNodeSize(dockspace_id, viewport_size); // Necessary: See imgui_internal.h

          // Try to set up a square Lattice window.
          float split_ratio = std::max(0.20f, (viewport_size.x - viewport_size.y) / viewport_size.x);
          ImGuiID left;
          ImGuiID right;
          ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, split_ratio, &left, &right);
          ImGui::DockBuilderDockWindow("Control", left);
          ImGui::DockBuilderDockWindow("Lattice", right);

          ImGui::DockBuilderFinish(dockspace_id);
        }
      } else {
        ImGui::Text("ERROR: Docking is not enabled!");
      }

      ImGui::End();  // DockSpace
    }

    // Control window
    {
      if (ImGui::Begin("Control")) {
        // Leave room for line(s) below us
        ImGui::BeginChild("main config", ImVec2(0, - 2 * ImGui::GetFrameHeightWithSpacing()));

        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Lattice")) {
          // TODO Known bug: If resizing takes a long time and user clicks somewhere else in the
          // meantime, the slider moves to the horizontal position of the new click. To fix it:
          // Never do long computations in the GUI thread.

          const int min_size = 1, max_size = 8000;
          const float min_size_f = min_size, max_size_f = max_size;
          // ImGui seems to support nonlinear sliders only for Float (e.g., SliderInt doesn't
          // support nonlinear sliding), so we must convert.
          static float lattice_size_f {static_cast<float>(lattice_size)};
          if (ImGui::SliderScalar("Size", ImGuiDataType_Float, &lattice_size_f,
                                  &min_size_f, &max_size_f, "%.0f", 3.0F)) {
            // Workaround for ImGui "feature": SliderInt doesn't enforce min/max bounds during
            // keyboard input, so we have to remember the old input. During keyboard input,
            // SliderInt doesn't re-load the int into the text area if it changes, so after we
            // clamp the value SliderInt returns true every frame until it loses focus. As a
            // workaround, we only resize if the value *actually* changed.
            // (See <https://github.com/ocornut/imgui/issues/1829>)
            auto old_lattice_size {lattice_size};
            lattice_size = static_cast<int>(lattice_size_f);
            lattice_size_f = std::max(min_size_f, lattice_size_f);
            lattice_size_f = std::min(max_size_f, lattice_size_f);
            // Convert these separately to avoid floating-point rounding shenanigans.
            lattice_size = std::max(min_size, lattice_size);
            lattice_size = std::min(max_size, lattice_size);
            if (lattice_size != old_lattice_size) {
              lattice.resize(lattice_size, lattice_size);
              regeneration_was_requested = true;
            }
          }
          ImGui::SameLine();
          HelpMarker("Size (= height = width) of the lattice. Ctrl-click for keyboard input.");
          ImGui::Spacing();
          ImGui::Spacing();
        }

        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Probability measure")) {
          auto previous_probability_measure {probability_measure};
          if(ImGui::Combo("Measure",
                          (int*)&probability_measure,
                          ProbabilityMeasureNames,
                          IM_ARRAYSIZE(ProbabilityMeasureNames))) {
            if (previous_probability_measure != probability_measure) {
              regeneration_was_requested = true;
            }
          }

          if (probability_measure == ProbabilityMeasure::bernoulli) {
            float old_open_p {0};
            old_open_p = open_p;
            if (ImGui::SliderFloat("p", &open_p, 0.0F, 1.0F, "%.6f")) {
              // Work around ImGui's unclamped keyboard input behavior.
              // See comment at lattice_size SliderScalar).
              open_p = std::max(0.0F, open_p);
              open_p = std::min(1.0F, open_p);
              if (open_p != old_open_p) {
                lattice.resize(lattice_size, lattice_size);
                regeneration_was_requested = true;
              }
            }
            ImGui::SameLine();
            HelpMarker("The probability of each site being open. Ctrl-click for keyboard input.");
            if (ImGui::Button("Randomize")) {
              // TODO Let the user choose the RNG -- there's a tradeoff between speed and quality.
              // Choices: Xorshift32, PCG, ...?
              regeneration_was_requested = true;
            }
          }
          ImGui::Spacing();
          ImGui::Spacing();
        }

        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Percolation")) {
          if (lattice.is_fully_flooded() or auto_percolate) {
            begin_disable_items();
            ImGui::Button("Percolate!");
            end_disable_items();
          } else if (ImGui::Button("Percolate!")) {
            percolation_was_requested = true;
          }
          ImGui::SameLine();
          if (ImGui::Button("Clear")) {
            lattice.unflood();
            redraw = true;
            auto_percolate = false;
            if (auto_flow) {
              // TODO Find a more elegant way to deal with auto_flow.
              flow_was_requested = true;
            }
          }
          ImGui::SameLine();
          if (ImGui::Checkbox("Auto-percolate", &auto_percolate) and auto_percolate) {
            percolation_was_requested = true;
            auto_flow = false;  // It makes no sense to have both at once.
          }

          if (auto_flow) {
            begin_disable_items();
            ImGui::Button("Pause flow");
            end_disable_items();
          } else if (!flowing) {
            if (lattice.is_fully_flooded() or auto_percolate) {
              begin_disable_items();
              ImGui::Button("Begin flow");
              end_disable_items();
            } else if (ImGui::Button("Begin flow")) {
              flow_was_requested = true;
            }
          } else if (ImGui::Button("Pause flow")) {
            flowing = false;
          }

          ImGui::SameLine();
          if (lattice.is_fully_flooded() or auto_percolate or auto_flow) {
            begin_disable_items();
            ImGui::Button("Single step");
            end_disable_items();
          } else if (ImGui::Button("Single step")) {
            percolation_step_was_requested = true;
          }

          ImGui::SameLine();
          if (ImGui::Checkbox("Auto-flow", &auto_flow)) {
            if (auto_flow) {
              auto_percolate = false;  // It makes no sense to have both at once.
              flow_was_requested = true;
            } else {
              flowing = false;
            }
          }

          static const float min_speed = 1.0F;
          static const float max_speed = 5000.0F;
          if (ImGui::SliderScalar("Flow speed", ImGuiDataType_Float, &flow_speed,
                                  &min_speed, &max_speed, "%.1f", 2.0F)) {
            // Work around ImGui's unclamped keyboard input behavior.
            // See comment at lattice_size SliderScalar).
            flow_speed = std::max(min_speed, flow_speed);
            flow_speed = std::min(max_speed, flow_speed);
          }

          ImGui::SameLine();
          HelpMarker("The rate of fluid flow through the lattice (in steps per second).");
        }
            
        ImGui::EndChild();

        ImGui::Checkbox("Lattice viewport", &show_lattice_window);
        // TODO Only enable for "Devel" build type (with CMake: how?)
        //ImGui::SameLine();
        //ImGui::Checkbox("Demo Window", &show_demo_window);
        ImGui::Text("GUI framerate: %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      }
      ImGui::End();

      if (regeneration_was_requested) {
        regenerate_lattice(lattice, probability_measure, open_p);
        if (auto_flow) {
          flow_was_requested = true;
        } else {
          flowing = false;
        }
      }
      if (percolation_was_requested or auto_percolate) {
        flowing = false;
        lattice.percolate();
      }
      if (percolation_step_was_requested) {
        lattice.percolate_step();
      }
      if (regeneration_was_requested or
          percolation_was_requested or
          percolation_step_was_requested) {
        // We must redraw even if lattice.percolate_step() returned false, because freshly_flooded
        // may have changed.
        redraw = true;
        regeneration_was_requested = false;
        percolation_was_requested = false;
        percolation_step_was_requested = false;
      }

      if (flow_was_requested) {
        flowing = true;
        flow_start_time = std::chrono::high_resolution_clock::now();
        flow_was_requested = false;
        lattice.percolate_step(); // Begin immediately.
        redraw = true;
      }

      if (flowing and not lattice.is_fully_flooded()) {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds delay{static_cast<int>(1'000'000.F / flow_speed)};
        const int steps = (now - flow_start_time) / delay;
        if (steps > 0) {
          redraw = true;
        }
        auto i {0};
        while (i < steps) {
          lattice.percolate_step();
          ++i;
          if (lattice.is_fully_flooded()) {
            if (!auto_flow) {
              flowing = false;
            }
            break;
          }
        }
        flow_start_time += i * delay;  // Adjust precisely for the next frame.
      }
    }  // Control window

    // Lattice window
    if (show_lattice_window) {
      ImGui::Begin("Lattice", &show_lattice_window);
      Latticeview(&lattice, redraw);
      redraw = false;
      ImGui::End();
    }  // Lattice window

    // Demo window (useful for development: picking out widgets.)
    if (show_demo_window) {
      ImGui::ShowDemoWindow(&show_demo_window);
    }

    // Render
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and render additional platform windows.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
      SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_GL_SwapWindow(window);
  }

  // Clean up
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
