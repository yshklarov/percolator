#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

#ifdef _WIN32
#define NOMINMAX    // Prevent windows.h from clobbering STL's min and max.
#include <windows.h>
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/examples/imgui_impl_glfw.h"
#include "imgui/examples/imgui_impl_opengl3.h"

#include <glad/glad.h>   // Desktop OpenGL function loader
#include <GLFW/glfw3.h>  // OpenGL utility library


#include "lattice.h"
#include "graphics/imgui_latticeview.h"


static void glfw_error_callback(int error, const char* description) {
  std::cerr << "GLFW error " << error << ": " << description << std::endl;
}


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

static void show_menu_file(GLFWwindow* window) {
  if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
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
    assert(false);
    break;
  }
}


void handle_keyboard_input(GLFWwindow* window) {
  // Ctrl-Q should work everywhere, even if ImGui wants to capture keyboard.
  auto io {ImGui::GetIO()};
  if (ImGui::IsKeyPressed('Q') && io.KeyCtrl) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
  // If ImGui wants keyboard focus for itself (e.g. text input widget), then don't handle it here.
  if (!io.WantCaptureKeyboard) {
    // Handle other keys here.
  }
}


int main(int, char**) {
#ifdef _WIN32
  // TODO Set up a WinMain. For now, we'll just hide the console window.
  // Options: SW_HIDE, SW_MINIMIZE, ...
  ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

  // Set up GLFW window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW!\n";
    return 1;
  }

  // Set versions GL 3.3 + GLSL 330
  const char* glsl_version = "#version 330";
  // This *must* match the GLAD loader's version, or we get weird OpenGL exceptions.
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

  // Create window with graphics context
  GLFWwindow* window = glfwCreateWindow(1280, 720, "Percolator", NULL, NULL);
  if (window == NULL) {
    std::cerr << "GLFW failed to create window!\n";
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Initialize OpenGL loader
  if (gladLoadGL() == 0) {
    std::cerr << "Failed to initialize OpenGL loader!\n";
    return 1;
  }

  // Set up ImGui context
  IMGUI_CHECKVERSION();
  auto context {ImGui::CreateContext()};
  ImGuiIO& io {ImGui::GetIO()}; (void)io;

  // Keyboard navigation. Note this means that io.WantCaptureKeyboard will always be true.
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Docking
  // ImGui multi-viewport / platform windows are still too buggy, so we'll leave this disabled.
  //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  //io.ConfigViewportsNoTaskBarIcon = true;

  // Override imgui's defaults "imgui.ini" and "imgui_log.ini".
  io.IniFilename = "percolator.ini";
  io.LogFilename = "percolator_log.ini";

  // Built-in themes
  //ImGui::StyleColorsClassic();
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look
  // identical to regular ones.
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGuiStyle& style {ImGui::GetStyle()};
    style.WindowRounding = 0.0F;
    style.FrameRounding = 3.0F;
    style.FrameBorderSize = 1.0F;  // Show border around widgets for better clarity
    style.Colors[ImGuiCol_WindowBg].w = 1.0F;  // Window background alpha (transparency)
  }

  // Set up platform/renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // State variables
#ifdef DEVEL_FEATURES
  auto show_demo_window {false};
#endif
  auto show_lattice_window {true};
  auto show_about_window {false};
  auto redraw {true};
  auto flowing {false};
  std::chrono::time_point<std::chrono::high_resolution_clock> flow_start_time;
  auto clear_color {ImVec4(0.00F, 0.00F, 0.00F, 1.00F)};  // Background color behind DockSpace
  auto lattice_size {30};
  float flow_speed {10.0F};
  FlowDirection flow_direction {FlowDirection::top};
  auto lattice {Lattice(lattice_size, lattice_size)};
  lattice.setFlowDirection(flow_direction);
  auto probability_measure {ProbabilityMeasure::bernoulli};
  const float rect_site_percolation_threshold {0.59274605F};
  float open_p {rect_site_percolation_threshold};
  regenerate_lattice(lattice, probability_measure, open_p);
  auto percolation_mode {PercolationMode::flow};
  auto auto_percolate {false};
  auto auto_find_clusters {false};
  auto auto_flow {false};
  auto regeneration_was_requested {false};
  auto percolation_was_requested {false};
  auto percolation_step_was_requested {false};
  auto flow_was_requested {false};

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Poll and handle events (inputs, window resize, etc.)
    glfwPollEvents();
    handle_keyboard_input(window);

    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto viewport {ImGui::GetMainViewport()};
    auto work_pos {viewport->GetWorkPos()};
        
    // Main menu
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        show_menu_file(window);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("About", NULL, &show_about_window);
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    // About window
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
      const auto dockspace_fullscreen {true};
      const auto dockspace_flags {ImGuiDockNodeFlags_None};

      // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable
      // into, because it would be confusing to have two docking targets within each others.
      ImGuiWindowFlags imgui_window_flags {ImGuiWindowFlags_NoDocking};
      if (dockspace_fullscreen) {
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

      if (dockspace_fullscreen) {
        ImGui::PopStyleVar(2);  // Back to normal WindowRounding and WindowBorderSize.
      }

      if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("DockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0F, 0.0F), dockspace_flags);

        // Initial window layout
        static bool first_time {true};
        if (first_time) {
          auto viewport_size {viewport->GetWorkSize()};
          // Workaround for tiling window managers: If the viewport size is 0, then the window
          // manager must be doing something weird, so we'll wait till the next frame.
          if (viewport_size.x != 0) {
            first_time = false;
            // Only set up the layout if there aren't already saved layout settings.
            if (context->SettingsWindows.empty()) {
              ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
              ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
              ImGui::DockBuilderSetNodeSize(dockspace_id, viewport_size); // Necessary: See imgui_internal.h

              // Make the Lattice window square, if possible.
              float split_ratio = std::max(0.20F, (viewport_size.x - viewport_size.y) / viewport_size.x);
              ImGuiID left;
              ImGuiID right;
              ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 1.0F - split_ratio, &right, &left);
              ImGui::DockBuilderDockWindow("Control", left);
              ImGui::DockBuilderDockWindow("Lattice", right);

              ImGui::DockBuilderFinish(dockspace_id);
            }
          }
        }
      } else {
        ImGui::Text("ERROR: Docking is not enabled!");
      }

      ImGui::End();  // DockSpace
    }

    // Control window
    {
      if (ImGui::Begin("Control")) {
#ifdef DEVEL_FEATURES
        const int nlines = 2;
#else
        const int nlines = 1;
#endif
        const float footer_height_to_reserve {
          ImGui::GetStyle().ItemSpacing.y +  // Separator
          nlines * ImGui::GetFrameHeightWithSpacing()}; // Buttons
        // Leave room for line(s) below us
        ImGui::BeginChild("main config", ImVec2(0, - footer_height_to_reserve));

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
            auto previous_lattice_size {lattice_size};
            lattice_size = static_cast<int>(lattice_size_f);
            lattice_size_f = std::max(min_size_f, lattice_size_f);
            lattice_size_f = std::min(max_size_f, lattice_size_f);
            // Convert these separately to avoid floating-point rounding shenanigans.
            lattice_size = std::max(min_size, lattice_size);
            lattice_size = std::min(max_size, lattice_size);
            if (lattice_size != previous_lattice_size) {
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
            float previous_open_p {0};
            previous_open_p = open_p;
            if (ImGui::SliderFloat("p", &open_p, 0.0F, 1.0F, "%.6f")) {
              // Work around ImGui's unclamped keyboard input behavior.
              // See comment at lattice_size SliderScalar).
              open_p = std::max(0.0F, open_p);
              open_p = std::min(1.0F, open_p);
              if (open_p != previous_open_p) {
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
          ImGui::Text("Mode:"); ImGui::SameLine();
          auto previous_percolation_mode {percolation_mode};
          if (ImGui::RadioButton("Simulate flow",
                                 (int *)&percolation_mode, (int)PercolationMode::flow)
              and percolation_mode != previous_percolation_mode) {
            lattice.reset_percolation();
            if (auto_flow) {
              flow_was_requested = true;
            }
            redraw = true;
          }
          ImGui::SameLine();
          if (ImGui::RadioButton("Show clusters",
                                 (int *)&percolation_mode, (int)PercolationMode::clusters)
              and percolation_mode != previous_percolation_mode) {
            lattice.reset_percolation();
            flowing = false;
            redraw = true;
          }

          if (percolation_mode == PercolationMode::flow) {
            if (lattice.done_percolation() or auto_percolate) {
              begin_disable_items();
              ImGui::Button("Percolate!");
              end_disable_items();
            } else if (ImGui::Button("Percolate!")) {
              percolation_was_requested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
              lattice.reset_percolation();
              redraw = true;
              auto_percolate = false;
              if (auto_flow) {
                // TODO Find a more elegant way to deal with auto_flow.
                flow_was_requested = true;
              } else {
                flowing = false;
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
              if (auto_percolate or lattice.done_percolation()) {
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
            if (auto_percolate or auto_flow or lattice.done_percolation()) {
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

            ImGui::Text("Direction:"); ImGui::SameLine();
            // TODO don't do anything if same radiobutton is re-clicked.
            if (ImGui::RadioButton("From the top",
                                   (int *)&flow_direction, (int)FlowDirection::top)) {
              lattice.setFlowDirection(flow_direction);
              lattice.flood_entryways();
              redraw = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("From all sides",
                                   (int *)&flow_direction, (int)FlowDirection::all_sides)) {
              lattice.setFlowDirection(flow_direction);
              lattice.flood_entryways();
              redraw = true;
            }
          } else if (percolation_mode == PercolationMode::clusters) {
            if (auto_find_clusters or lattice.done_percolation()) {
              begin_disable_items();
              ImGui::Button("Find clusters");
              end_disable_items();
            } else if (ImGui::Button("Find clusters")) {
              percolation_was_requested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
              lattice.reset_percolation();
              redraw = true;
              auto_find_clusters = false;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Auto-find", &auto_find_clusters) and auto_find_clusters) {
              percolation_was_requested = true;
            }
            if (auto_find_clusters or lattice.done_percolation()) {
              auto n {lattice.num_clusters()};
              ImGui::Text(n == 1 ? "Found %d cluster" : "Found %d clusters", n);
            }
          }
        }
            
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Checkbox("Show lattice", &show_lattice_window);
#ifdef DEVEL_FEATURES
        ImGui::SameLine(); ImGui::Checkbox("Demo Window", &show_demo_window);
        ImGui::Text("GUI framerate: %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
#endif
      }
      ImGui::End();

      if (regeneration_was_requested) {
        regenerate_lattice(lattice, probability_measure, open_p);
        if (percolation_mode == PercolationMode::flow and auto_flow) {
          flow_was_requested = true;
        } else {
          flowing = false;
        }
      }
      if ((percolation_mode == PercolationMode::flow and auto_percolate)
          or (percolation_mode == PercolationMode::clusters and auto_find_clusters)) {
        percolation_was_requested = true;
      }
      if (percolation_was_requested) {
        flowing = false;
        if (percolation_mode == PercolationMode::flow
            and not lattice.done_percolation()) {
          lattice.flow_fully();
          redraw = true;
        }
        if (percolation_mode == PercolationMode::clusters
            and not lattice.done_percolation()) {
          lattice.find_clusters();
          redraw = true;
        }
        percolation_was_requested = false;
      }
      if (percolation_step_was_requested) {
        flowing = false;
        lattice.flow_one_step();
      }
      if (regeneration_was_requested or
          percolation_step_was_requested) {
        // We must redraw even if lattice.flow_one_step() returned false, because freshly_flooded
        // may have changed.
        redraw = true;
        regeneration_was_requested = false;
        percolation_step_was_requested = false;
      }

      if (flow_was_requested) {
        flowing = true;
        flow_start_time = std::chrono::high_resolution_clock::now();
        flow_was_requested = false;
        lattice.flow_one_step(); // Begin immediately.
        redraw = true;
      }

      if (flowing and not lattice.done_percolation()) {
        // TODO Replace this mess with proper timer+scheduler abstractions.
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds delay{static_cast<int>(1'000'000.F / flow_speed)};
        const int steps = (now - flow_start_time) / delay;
        if (steps > 0) {
          redraw = true;
        }
        auto i {0};
        while (i < steps) {
          lattice.flow_one_step();
          ++i;
          if (lattice.done_percolation()) {
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
      if (redraw and percolation_mode == PercolationMode::clusters) {
        // Keep the cluster color scheme the same every time.
        lattice.sort_clusters();
      }
      Latticeview(&lattice, redraw, percolation_mode);
      redraw = false;
      ImGui::End();
    }  // Lattice window

#ifdef DEVEL_FEATURES
    // Demo window (useful for development: picking out widgets.)
    if (show_demo_window) {
      ImGui::ShowDemoWindow(&show_demo_window);
    }
#endif

    // Render
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows (Platform functions may change the current
    // OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.)
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      GLFWwindow* backup_current_context = glfwGetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window);
  }

  // Clean up
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
