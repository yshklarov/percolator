cmake_minimum_required(VERSION 3.16)

set(project_name "percolator")
set(project_desc "Lattice percolation simulation playground")
project(${project_name}
  DESCRIPTION ${project_desc}
  LANGUAGES CXX)

################
# General build options

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(
    STATUS "No build type specified. Defaulting to 'Release'.")
  set(CMAKE_BUILD_TYPE
    Release
    CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui, ccmake
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug"
    "Release"
    "MinSizeRel"
    "RelWithDebInfo")
endif()

# For special features disabled in the release build.
# In code, put "#ifdef DEVEL_BUILD <...> #endif".
option(ENABLE_DEVEL_FEATURES
  "Special features useful only during development"
  OFF)
if(ENABLE_DEVEL_FEATURES)
  add_compile_definitions(DEVEL_FEATURES)
endif()


################
# Compilation and tools

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(UNIX)
  SET(CMAKE_C_COMPILER clang)
  SET(CMAKE_CXX_COMPILER clang++)
  #SET(CMAKE_C_COMPILER gcc)
  #SET(CMAKE_CXX_COMPILER g++)
endif()

if(UNIX)
  option(ENABLE_CCACHE
    "Enable ccache (compiler cache) to speed up compile times"
    ON)
  if(ENABLE_CCACHE)
    find_program(CCACHE ccache)
    if(CCACHE)
      message("Found ccache: ${CCACHE}")
      set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
    else()
      message("Ccache not found!")
    endif()
  endif()
endif()
mark_as_advanced(FORCE CCACHE)

# Export a compile_commands.json. Useful for various clang tools, Sourcetrail, etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# IPO makes builds very slow, so we'll leave it off by default.
option(ENABLE_IPO
  "Enable Iterprocedural Optimization, aka Link-Time Optimization (LTO)"
  OFF)
if(ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT result OUTPUT output)
  if(result)
    message("IPO is enabled.")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(SEND_ERROR "IPO is not supported: ${output}")
  endif()
endif()


option(ENABLE_CLANG_TIDY
  "Enable static code analysis with clang-tidy"
  OFF)
if(ENABLE_CLANG_TIDY)
  find_program(CLANGTIDY clang-tidy)
  if(CLANGTIDY)
    message("Found clang-tidy: ${CLANGTIDY}")
    set(CMAKE_CXX_CLANG_TIDY
      ${CLANGTIDY};
      -header-filter=/src;  # Static analysis only for our headers
      -checks=-*,bugprone-*,clang-analyzer-*,cppcoreguidelines-*,modernize-*,performance-*,portability-*,readability-*,-cppcoreguidelines-avoid-magic-numbers,-readability-magic-numbers,-cppcoreguidelines-pro-type-vararg,-readability-isolate-declaration,-modernize-use-trailing-return-type,-modernize-use-nodiscard,-cppcoreguidelines-pro-bounds-pointer-arithmetic;)
  else()
    message(SEND_ERROR "clang-tidy requested but not found")
  endif()
endif()
mark_as_advanced(FORCE CLANGTIDY)


################
# Compiler warnings, setup, etc.

# Per-compiler warning flags, and other settings.
# TODO Warnings only for our own code (not libraries in extern/); add more warnings.
if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU" )
  set(compiler_warning_flags -Wall -Wextra -Wunreachable-code -Wpedantic -Wshadow -Wformat)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")  # Optimize for speed.
endif()
if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  list(APPEND compiler_warning_flags "-Wweak-vtables -Wexit-time-destructors -Wglobal-constructors -Wmissing-noreturn")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(CMAKE_CXX_FLAGS "/W3 /permissive-")
  # Statically link the MSVC runtime library. This must be set *before* defining compile targets.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()


################
# Libraries setup

include_directories("extern/glad/include")

if(UNIX)
  include(FindPkgConfig)
  pkg_search_module(GLFW REQUIRED glfw3)
  include_directories(${GLFW_INCLUDE_DIRS})
endif()

if(WIN32)
  include_directories("extern/windows/glfw-3.3.2/include")
  link_directories("extern/windows/glfw-3.3.2/lib")
endif()


################
# External libraries

add_library(glad STATIC extern/glad/src/glad.c extern/glad/include/glad/glad.h)
set_source_files_properties(extern/glad/src/glad.c PROPERTIES LANGUAGE CXX)
target_include_directories(glad PRIVATE extern/glad/include/)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE CXX)
if(UNIX)
  target_link_libraries(glad PRIVATE dl)
endif()

add_library(imgui STATIC extern/imgui/imgui.cpp extern/imgui/imgui.h extern/imgui/imconfig.h)

add_library(imgui_draw STATIC
  extern/imgui/imgui_draw.cpp extern/imgui/imstb_rectpack.h extern/imgui/imstb_truetype.h)

add_library(imgui_widgets STATIC extern/imgui/imgui_widgets.cpp)

add_library(imgui_impl_glfw STATIC
  extern/imgui/examples/imgui_impl_glfw.cpp
  extern/imgui/examples/imgui_impl_glfw.h)
target_include_directories(imgui_impl_glfw PRIVATE extern/imgui/)

add_library(imgui_impl_opengl3 STATIC
  extern/imgui/examples/imgui_impl_opengl3.cpp
  extern/imgui/examples/imgui_impl_opengl3.h)
target_include_directories(imgui_impl_opengl3 PRIVATE extern/imgui/)
target_compile_options(imgui_impl_opengl3 PRIVATE -DIMGUI_IMPL_OPENGL_LOADER_GLAD)

add_library(imgui_demo STATIC extern/imgui/imgui_demo.cpp)
target_link_libraries(imgui_demo PRIVATE imgui_draw)

# Disable static analysis for external libraries.
if(ENABLE_CLANG_TIDY)
  set_target_properties(glad imgui imgui_draw imgui_widgets imgui_impl_glfw imgui_impl_opengl3 imgui_demo
    PROPERTIES CXX_CLANG_TIDY "")
endif()

################
# This project

add_library(
  lattice STATIC
  src/lattice.cpp
  src/lattice.h)
if(UNIX)
  target_link_libraries(lattice PUBLIC stdc++ m)
endif()

add_library(
  imgui_latticeview STATIC
  src/graphics/imgui_latticeview.cpp
  src/graphics/imgui_latticeview.h)
target_include_directories(imgui_latticeview PRIVATE extern/ src/)

if(UNIX)
  set(PLATFORM_RESOURCES "")
  set(PLATFORM_LINK_LIBS ${GLFW_LIBRARIES} GL)
elseif(WIN32)
  set(PLATFORM_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/res/windows.rc")
  set(PLATFORM_LINK_LIBS OpenGL32 glfw3dll)
endif()

set(main_exe "percolator")
add_executable(${main_exe} src/main.cpp ${PLATFORM_RESOURCES})
target_include_directories(${main_exe} PUBLIC extern/)
target_link_libraries(
  ${main_exe} PRIVATE
  lattice imgui_latticeview
  glad
  imgui imgui_widgets imgui_impl_glfw imgui_impl_opengl3 imgui_demo
  ${PLATFORM_LINK_LIBS})
target_compile_options( ${main_exe} PUBLIC ${compiler_warning_flags} )


################
# Installation

# Copy resources into build. (We have no resources so far.)
#add_custom_command(TARGET ${main_exe} POST_BUILD
#  COMMAND ${CMAKE_COMMAND} -E copy_directory
#  ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${main_exe}>/res)

if(WIN32)
  # Copy DLLs to build directory (see <https://stackoverflow.com/questions/10671916>).
  set(WIN32_DLLS
      "glfw-3.3.2/lib/glfw3.dll")
  foreach(F IN LISTS WIN32_DLLS)
    add_custom_command(TARGET ${main_exe} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E
            copy_if_different "${PROJECT_SOURCE_DIR}/extern/windows/${F}"
                              $<TARGET_FILE_DIR:${main_exe}>
        COMMENT "Copying ${F} to build directory.")
  endforeach()

  option(BUILD_PACKAGE_FOR_DISTRIBUTION
    "Create a .zip file for distribution after building"
    OFF)
  if(BUILD_PACKAGE_FOR_DISTRIBUTION)
    # Copy the necessary files into the build tree.
    add_custom_command(TARGET ${main_exe} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E
            copy "${PROJECT_SOURCE_DIR}/LICENSE"
                 "$<TARGET_FILE_DIR:${main_exe}>"
        COMMENT "Copying distribution files to build directory.")
    add_custom_command(TARGET ${main_exe} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E
            copy_directory "${PROJECT_SOURCE_DIR}/extern/licenses"
                           "$<TARGET_FILE_DIR:${main_exe}>/licenses"
        COMMENT "Copying licenses to build directory.")
    add_custom_command(TARGET ${main_exe} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E
            rename $<TARGET_FILE_DIR:${main_exe}>/LICENSE
                   $<TARGET_FILE_DIR:${main_exe}>/licenses/${project_name}.txt)

    # Create the zip file.
    set(PACKAGE_FILES ${WIN32_DLLS})
    list(TRANSFORM PACKAGE_FILES REPLACE ".*/" "")  # Strip paths from DLL filenames
    list(PREPEND PACKAGE_FILES "${main_exe}.exe")
    list(APPEND PACKAGE_FILES "licenses")
    list(TRANSFORM PACKAGE_FILES PREPEND "$<TARGET_FILE_DIR:${main_exe}>/")
    add_custom_command(TARGET ${main_exe} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E
          tar "cfv" "${project_name}_windows-x64.zip" --format=zip ${PACKAGE_FILES}
      COMMENT "Creating .zip file.")
  endif()
endif()

if(UNIX)
  install(TARGETS ${main_exe} DESTINATION bin)
endif()
