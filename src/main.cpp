#include <algorithm>
#include <iostream>
#include <map>
#include <string>

#ifdef _WIN32
#define NOMINMAX    // Prevent windows.h from clobbering STL's min and max.
#include <windows.h>
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/examples/imgui_impl_glfw.h"
#include "imgui/examples/imgui_impl_opengl3.h"
#include "utility.h"

#include <glad/glad.h>   // Desktop OpenGL function loader
#include <GLFW/glfw3.h>  // OpenGL utility library


#include "lattice.h"
#include "supervisor.h"
#include "graphics/latticewindow.h"

// TODO Find a better way of dealing with this and regenerate_lattice().
// Required by ImGui Combo menu. Make sure to keep ID and IDNames in the same order, or the
// dropdown menu will select the wrong measure.
enum class MeasureID : int
    {bernoulli, open, pattern_1, pattern_2, pattern_3, MAX};
const char* MeasureIDNames[]
    {"Bernoulli", "Open", "Test Pattern 1", "Test Pattern 2", "Test Pattern 3"};

static void help_marker(const char* help_text) {
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0F);
    ImGui::TextUnformatted(help_text);
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

void push_style_hue(float hue) {
  ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(hue, 0.6f, 0.6f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(hue, 0.7f, 0.7f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(hue, 0.8f, 0.8f));
}
void pop_style_hue() {
  ImGui::PopStyleColor(3);
}

static void glfw_error_callback(int error, const char* description) {
  std::cerr << "GLFW error " << error << ": " << description << std::endl;
}

static void show_main_menu(GLFWwindow* window, bool &about_window_visible) {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      ImGui::MenuItem("About", nullptr, &about_window_visible);
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

// TODO Expunge this evil function.
void regenerate_lattice(Supervisor &supervisor, const MeasureID gui_measure, const float p) {
  switch (gui_measure) {
  case MeasureID::open:
    supervisor.set_measure(measure::open());
    break;
  case MeasureID::pattern_1:
    supervisor.set_measure(measure::pattern_1());
    break;
  case MeasureID::pattern_2:
    supervisor.set_measure(measure::pattern_2());
    break;
  case MeasureID::pattern_3:
    supervisor.set_measure(measure::pattern_3());
    break;
  case MeasureID::bernoulli:
    supervisor.set_measure(measure::bernoulli(p));
    break;
  default:
    IM_ASSERT(false);
    break;
  }
  supervisor.fill();
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

int initialize_gui(GLFWwindow* &window) {
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
  window = glfwCreateWindow(1280, 720, "Percolator", nullptr, nullptr);
  if (!window) {
    std::cerr << "GLFW failed to create window!\n";
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Initialize OpenGL loader
  if (gladLoadGL() == 0) {
    std::cerr << "Failed to initialize OpenGL loader!\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  // Set up ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io {ImGui::GetIO()};

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

  return 0;
}

void cleanup_gui(GLFWwindow* window) {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
}

void show_about_window(bool &about_window_visible) {
  if (!about_window_visible) {
    return;
  }
  if (ImGui::Begin(
        "About Percolator",
        &about_window_visible,
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
      about_window_visible = false;
    }
  }
  ImGui::End();
}

// The root dockspace is the main window that the other windows can dock to.
void show_root_dockspace() {
  const auto dockspace_fullscreen {true};
  const auto dockspace_flags {ImGuiDockNodeFlags_None};
  const auto context {ImGui::GetCurrentContext()};
  const auto io {ImGui::GetIO()};
  const auto viewport {ImGui::GetMainViewport()};
  const auto work_pos {viewport->GetWorkPos()};

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
          ImGui::DockBuilderSetNodeSize(dockspace_id, viewport_size); // See imgui_internal.h

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
  ImGui::End();
}

int main(int, char**) {
  // Initialize GUI
  GLFWwindow* window;
  if (initialize_gui(window) != 0) {
    std::cerr << "Fatal error: Could not initialize GUI.\n";
    return 1;
  }
  auto io {ImGui::GetIO()};

  // GUI state variables
#ifdef DEVEL_FEATURES
  auto demo_window_visible {false};
#endif
  auto lattice_window_visible {true};
  auto about_window_visible {false};

  auto lattice_size {250};  // Always square, for now.

  auto gui_measure {MeasureID::bernoulli};
  const float rect_site_percolation_threshold {0.59274605F};
  float bernoulli_p {rect_site_percolation_threshold};

  auto percolation_mode {PercolationMode::flow};
  float flow_speed {20.0F};
  FlowDirection flow_direction {FlowDirection::top};
  bool torus {false};
  auto auto_percolate {false};
  auto auto_flow {false};
  auto auto_find_clusters {true};

  // A supervisor oversees a single lattice. TODO Sync up measure here with gui_measure.
  auto supervisor {Supervisor(lattice_size, lattice_size, measure::pattern_3())};
  supervisor.set_flow_speed(flow_speed);
  supervisor.set_flow_direction(flow_direction);
  supervisor.set_torus(torus);

  LatticeWindow lattice_window {"Lattice"};

  auto do_autos_if_needed {
    [&]() {
      if (percolation_mode == PercolationMode::flow) {
        if (auto_percolate) {
          supervisor.flow_fully();
        } else if (auto_flow) {
          supervisor.start_flow();
        }
      } else if (percolation_mode == PercolationMode::clusters and auto_find_clusters) {
        supervisor.find_clusters();
      }
    }};

  regenerate_lattice(supervisor, gui_measure, bernoulli_p);
  do_autos_if_needed();

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Poll and handle events (mouse/keyboard inputs, window resize, etc.)
    glfwPollEvents();
    handle_keyboard_input(window);

    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Show various minor windows/widgets
    show_root_dockspace();  // Holds all the other "big" windows
    show_main_menu(window, about_window_visible);
    show_about_window(about_window_visible);
#ifdef DEVEL_FEATURES
    // Demo window (useful for development: picking out widgets.)
    if (demo_window_visible) {
      ImGui::ShowDemoWindow(&demo_window_visible);
    }
#endif

    // Control window
    {
      if (ImGui::Begin("Control")) {
        // Leave spacing for the line(s) at the bottom of the window.
#ifdef DEVEL_FEATURES
#define text_lines 2  // Message & framerate display
#else
#define text_lines 1  // Message
#endif
        const float footer_height_to_reserve {
          ImGui::GetStyle().ItemSpacing.y +  // Separator
          ImGui::GetFrameHeightWithSpacing() +  // Checkboxes
          text_lines * ImGui::GetTextLineHeightWithSpacing()};
        ImGui::BeginChild("Main controls", ImVec2(0, - footer_height_to_reserve));

        ImGui::SetNextTreeNodeOpen(false, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Lattice")) {
          const int min_size {1}, max_size {10'000};
          const float min_size_f = min_size, max_size_f = max_size;
          // ImGui seems to support nonlinear sliders only for Float (e.g., SliderInt doesn't
          // support nonlinear sliding), so we must convert.
          static float lattice_size_f {static_cast<float>(lattice_size)};
          if (ImGui::SliderScalar("Size", ImGuiDataType_Float, &lattice_size_f,
                                  &min_size_f, &max_size_f, "%.0f", 2.5F)) {
            // Workaround for ImGui "feature": SliderInt doesn't enforce min/max bounds during
            // keyboard input, so we have to remember the old input. During keyboard input,
            // SliderInt doesn't re-load the int into the text area if it changes, so after we
            // clamp the value SliderInt returns true every frame until it loses focus. As a
            // workaround, we only resize if the value *actually* changed.
            // (See <https://github.com/ocornut/imgui/issues/1829>)
            // It's actually OK to call supervisor.set_size() too many times--it does nothing if the
            // size is unchanged--so we could get away with not keeping track of
            // previous_lattice_size here. But that would be too ugly.
            auto previous_lattice_size {lattice_size};
            lattice_size = static_cast<int>(lattice_size_f);
            // Clamp them separately to avoid floating-point rounding shenanigans.
            lattice_size_f = clamp(lattice_size_f, min_size_f, max_size_f);
            lattice_size = clamp(lattice_size, min_size, max_size);
            if (lattice_size != previous_lattice_size) {
              supervisor.stop_flow();
              supervisor.set_size(lattice_size, lattice_size);
              regenerate_lattice(supervisor, gui_measure, bernoulli_p);
              do_autos_if_needed();
            }
          }
          // If the user releases the mouse button, don't waste time filling an old size. This
          // keeps the user interface responsive. Note that we don't want to call this if the user
          // hasn't released the mouse button, or else dragging the slider wouldn't look clean.
          if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0)) {
            supervisor.abort_stale_operations();
            lattice_window.mark_render_disposable();
          }

          ImGui::SameLine();
          help_marker("Size (= height = width) of the lattice. Ctrl-click for keyboard input.");

          if (ImGui::Checkbox("Torus", &torus)) {
            supervisor.set_torus(torus);
            supervisor.reset_percolation();
            do_autos_if_needed();
          }
          ImGui::SameLine();
          help_marker("Whether to wrap around the sides");

          ImGui::Spacing(); ImGui::Spacing();
        }  // Lattice controls

        ImGui::SetNextTreeNodeOpen(false, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Measure")) {
          auto previous_gui_measure {gui_measure};
          if (ImGui::Combo("Measure##combo",
                           (int*)&gui_measure,
                           MeasureIDNames,
                           (int)MeasureID::MAX)) {
            if (gui_measure != previous_gui_measure) {
              supervisor.abort();
              regenerate_lattice(supervisor, gui_measure, bernoulli_p);
              do_autos_if_needed();
            }
          }

          if (gui_measure == MeasureID::bernoulli) {
            float previous_bernoulli_p {bernoulli_p};
            if (ImGui::SliderFloat("p", &bernoulli_p, 0.0F, 1.0F, "%.6f")) {
              // Work around ImGui's unclamped keyboard input behavior.
              // See comment at lattice_size SliderScalar).
              bernoulli_p = clamp(bernoulli_p, 0.0F, 1.0F);
              if (bernoulli_p != previous_bernoulli_p) {
                supervisor.stop_flow();
                regenerate_lattice(supervisor, gui_measure, bernoulli_p);
                do_autos_if_needed();
              }
            }
            // For responsiveness (see comment below lattice size slider)
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0)) {
              supervisor.abort_stale_operations();
              lattice_window.mark_render_disposable();
            }

            ImGui::SameLine();
            help_marker("The probability of each site being open. Ctrl-click for keyboard input.");
            if (ImGui::Button("Randomize")) {
              // TODO Let the user choose the RNG -- there's a tradeoff between speed and quality.
              // Choices: Xorshift32, PCG, ...?
              supervisor.abort();
              regenerate_lattice(supervisor, gui_measure, bernoulli_p);
              do_autos_if_needed();
            }
          }
          ImGui::Spacing();
          ImGui::Spacing();
        } // Measure controls

        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Percolation")) {
          ImGui::AlignTextToFramePadding();
          ImGui::Text("Mode:"); ImGui::SameLine();
          auto previous_percolation_mode {percolation_mode};
          if (ImGui::RadioButton("Simulate flow",
                                 (int *)&percolation_mode, (int)PercolationMode::flow)
              and percolation_mode != previous_percolation_mode) {
            supervisor.reset_percolation();
            if (auto_percolate) {
              supervisor.flow_fully();
              supervisor.abort_stale_operations();
              lattice_window.mark_render_disposable();
            } else if (auto_flow) {
              supervisor.start_flow();
            }
          }
          ImGui::SameLine();
          if (ImGui::RadioButton("Show clusters",
                                 (int *)&percolation_mode, (int)PercolationMode::clusters)
              and percolation_mode != previous_percolation_mode) {
            supervisor.reset_percolation();
            if (auto_find_clusters) {
              supervisor.find_clusters();
              supervisor.abort_stale_operations();
              lattice_window.mark_render_disposable();
            }
          }

          if (percolation_mode == PercolationMode::flow) {
            constexpr float purple {0.9F};
            push_style_hue(purple);
            if (supervisor.done_percolation() or auto_percolate) {
              begin_disable_items();
              ImGui::Button("Percolate!");
              end_disable_items();
            } else if (ImGui::Button("Percolate!")) {
              supervisor.stop_flow();
              supervisor.flow_fully();
            }
            pop_style_hue();
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
              supervisor.reset_percolation();
              auto_percolate = false;
              supervisor.stop_flow();
              do_autos_if_needed();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Auto-percolate", &auto_percolate) and auto_percolate) {
              auto_flow = false;  // It makes no sense to have both at once.
              supervisor.stop_flow();
              supervisor.flow_fully();
            }

            if (auto_flow) {
              begin_disable_items();
              ImGui::Button("Pause flow###begin_pause_flow");
              end_disable_items();
            } else if (!supervisor.is_flowing()) {
              if (auto_percolate || supervisor.done_percolation()) {
                begin_disable_items();
                ImGui::Button("Begin flow###begin_pause_flow");
                end_disable_items();
              } else if (ImGui::Button("Begin flow###begin_pause_flow")) {
                supervisor.start_flow();
              }
            } else if (ImGui::Button("Pause flow###begin_pause_flow")) {
              supervisor.stop_flow();
            }

            ImGui::SameLine();
            if (auto_percolate or auto_flow or supervisor.done_percolation()) {
              begin_disable_items();
              ImGui::Button("Single step");
              end_disable_items();
            } else if (ImGui::Button("Single step")) {
              supervisor.stop_flow();
              supervisor.flow_n_steps(1);
            }

            ImGui::SameLine();
            if (ImGui::Checkbox("Auto-flow", &auto_flow)) {
              if (auto_flow) {
                auto_percolate = false;  // It makes no sense to have both at once.
                supervisor.start_flow();
              } else {
                supervisor.stop_flow();
              }
            }

            static const float min_speed {1.0F};
            static const float max_speed {5000.0F};
            if (ImGui::SliderScalar("Flow speed", ImGuiDataType_Float, &flow_speed,
                                    &min_speed, &max_speed, "%.1f", 2.0F)) {
              // Work around ImGui's unclamped keyboard input behavior.
              // See comment at lattice_size SliderScalar).
              flow_speed = clamp(flow_speed, min_speed, max_speed);
              supervisor.set_flow_speed(flow_speed);
            }

            ImGui::SameLine();
            help_marker("The rate of fluid flow through the lattice (in steps per second).");

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Direction:"); ImGui::SameLine();
            // TODO don't do anything if same radiobutton is re-clicked.
            if (ImGui::RadioButton(torus ? "From top / bottom" : "From top",
                                   (int *)&flow_direction, (int)FlowDirection::top)) {
              supervisor.set_flow_direction(flow_direction);
              supervisor.flood_entryways();
              if (auto_percolate) {
                supervisor.reset_percolation();  // Clear whatever came "from all sides".
                supervisor.flow_fully();
              }
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("From all sides",
                                   (int *)&flow_direction, (int)FlowDirection::all_sides)) {
              supervisor.set_flow_direction(flow_direction);
              supervisor.flood_entryways();
              if (auto_percolate) {
                supervisor.flow_fully();
              }
            }
          } else if (percolation_mode == PercolationMode::clusters) {
            if (auto_find_clusters or supervisor.done_percolation()) {
              begin_disable_items();
              ImGui::Button("Find clusters");
              end_disable_items();
            } else if (ImGui::Button("Find clusters")) {
              supervisor.find_clusters();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
              supervisor.reset_percolation();
              auto_find_clusters = false;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Auto-find", &auto_find_clusters) and auto_find_clusters) {
              supervisor.find_clusters();
            }
            if (auto_find_clusters or supervisor.done_percolation()) {
              // Show cluster count
              auto n {supervisor.num_clusters()};
              ImGui::Text(n == 1 ? "Found %d cluster" : "Found %d clusters", n);
#ifdef DEVEL_FEATURES
              if (ImGui::TreeNode("Clusters")) {
                ImGui::Text("Largest cluster: %02.0f%%", supervisor.cluster_largest_proportion());
                {
                  // Show table: How many clusters there are of each size
                  ImGui::BeginChild(
                    "Cluster sizes",
                    ImVec2(200, ImGui::GetTextLineHeightWithSpacing() * 20.0F),
                    true);
                  ImGui::Columns(2, "clustersizescolumns");
                  ImGui::Text("Size"); ImGui::NextColumn();
                  ImGui::Text("Count"); ImGui::NextColumn();
                  ImGui::Separator();
                  // TODO Why is this so slow?
                  auto cluster_sizes {supervisor.get_cluster_sizes()};
                  if (cluster_sizes != std::nullopt) {
                    for (auto [sz, ct] : cluster_sizes.value()) {
                      ImGui::Text("%-8d", sz);
                      ImGui::NextColumn();
                      ImGui::Text("%-8d", ct);
                      ImGui::NextColumn();
                    }
                  }
                  ImGui::EndChild();
                }
                ImGui::TreePop();
              }
#endif
            }
          }
        }  // Percolation controls
            
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::NewLine();
        std::optional<std::string> busy_string {supervisor.busy()};
        static Stopwatch stopwatch;
        constexpr double busy_message_minimum_ms {0.0};
        if (busy_string != std::nullopt) {
          if (!stopwatch.is_running()) {
            stopwatch.start();
          }
          if (stopwatch.elapsed_ms() >= busy_message_minimum_ms) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Abort")) {
              stopwatch.stop();
              supervisor.abort();
            }
            ImGui::SameLine();
            ImGui::Text("%s...", busy_string.value().c_str());
          }
        } else {
          stopwatch.stop();
        }

        if (supervisor.errors_exist()) {
          ImGui::SameLine();
          ImGui::Text("Error: %s", supervisor.get_first_error().c_str());
          ImGui::SameLine();
          if (ImGui::SmallButton("Dismiss")) {
            supervisor.clear_one_error();
          }
        }
        ImGui::Checkbox("Show lattice", &lattice_window_visible);
#ifdef DEVEL_FEATURES
        ImGui::SameLine(); ImGui::Checkbox("Demo Window", &demo_window_visible);
        ImGui::Text("GUI framerate: %.3f ms/frame (%.1f FPS)",
                    1000.0F / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
#endif
      }
      ImGui::End();
    }  // Control window

    // Lattice window
    if (lattice_window_visible) {
      Lattice* lattice = supervisor.get_lattice_copy();
      if (lattice) {
        lattice_window.push_data(lattice);
      }
      lattice_window.show(lattice_window_visible);
    }  // Lattice window


    // End-of-frame boilerplate

    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    static const auto bg_color {ImVec4(0.00F, 0.00F, 0.00F, 1.00F)};  // black
    glClearColor(bg_color.x, bg_color.y, bg_color.z, bg_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform windows.
    //if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    //  GLFWwindow* backup_current_context = glfwGetCurrentContext();
    //  ImGui::UpdatePlatformWindows();
    //  ImGui::RenderPlatformWindowsDefault();
    //  glfwMakeContextCurrent(backup_current_context);
    //}

    glfwSwapBuffers(window);
  }  // Main loop

  cleanup_gui(window);

  return 0;
}
