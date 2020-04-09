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


################
# Compilation and tools

if(UNIX)
  SET(CMAKE_C_COMPILER clang)
  SET(CMAKE_CXX_COMPILER clang++)
endif()

# If we comment out this line, then the build works but the lattice doesn't display. Why?!
set(CMAKE_CXX_FLAGS_RELEASE "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)


option(ENABLE_CCACHE
  "Enable ccache (compiler cache) to speed up compile times"
  ON)
if(ENABLE_CCACHE AND NOT CCACHE)
  find_program(CCACHE ccache)
  if(CCACHE)
    message("Found ccache: ${CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
  else()
    message("Ccache not found!")
  endif()
endif()

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
    message("-- IPO is enabled.")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(SEND_ERROR "-- IPO is not supported: ${output}")
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
      -checks=-*,bugprone-*,clang-analyzer-*,cppcoreguidelines-*,modernize-*,performance-*,portability-*,readability-*;)
  else()
    message(SEND_ERROR "clang-tidy requested but not found")
  endif()
endif()




################
# Libraries setup

if(UNIX)
    include(FindPkgConfig)
    pkg_search_module(SDL2 required sdl2)
    pkg_search_module(GLEW required glew)
    include_directories(${GLEW_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
endif()

if(WIN32)
    include_directories("extern/windows/SDL2-2.0.12/include")
    include_directories("extern/windows/glew-2.1.0/include")
    link_directories("extern/windows/SDL2-2.0.12/lib/x64")
    link_directories("extern/windows/glew-2.1.0/lib/Release/x64")
endif()

################
# External libraries

add_library(imgui STATIC extern/imgui/imgui.cpp extern/imgui/imgui.h extern/imgui/imconfig.h)

add_library(imgui_draw STATIC
  extern/imgui/imgui_draw.cpp extern/imgui/imstb_rectpack.h extern/imgui/imstb_truetype.h)

add_library(imgui_widgets STATIC extern/imgui/imgui_widgets.cpp)

add_library(imgui_impl_sdl STATIC
  extern/imgui/examples/imgui_impl_sdl.cpp
  extern/imgui/examples/imgui_impl_sdl.h)
target_include_directories(imgui_impl_sdl PRIVATE extern/imgui/)

add_library(imgui_impl_opengl3 STATIC
  extern/imgui/examples/imgui_impl_opengl3.cpp
  extern/imgui/examples/imgui_impl_opengl3.h)
target_include_directories(imgui_impl_opengl3 PRIVATE extern/imgui/)

add_library(imgui_demo STATIC extern/imgui/imgui_demo.cpp)
target_link_libraries(imgui_demo PRIVATE imgui_draw)

# Disable static analysis for external libraries.
set_target_properties(imgui imgui_draw imgui_widgets imgui_impl_sdl imgui_impl_opengl3 imgui_demo
  PROPERTIES CXX_CLANG_TIDY "")



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

set(main_exe "percolator")
add_executable(${main_exe} src/main.cpp)
target_include_directories(${main_exe} PUBLIC extern/)

if(UNIX)
  set(PLATFORM_LINK_LIBS ${GLEW_LIBRARIES} ${SDL2_LIBRARIES})
elseif(WIN32)
  set(PLATFORM_LINK_LIBS OpenGL32 SDL2 glew32)
endif()

target_link_libraries(
  ${main_exe} PRIVATE
  lattice imgui_latticeview
  imgui imgui_widgets imgui_impl_sdl imgui_impl_opengl3 imgui_demo
  ${PLATFORM_LINK_LIBS})


################
# Compiler warnings, setup, etc.

# Per-compiler warning flags, and other settings.
# TODO Warnings only for our own code (not libraries in extern/); add more warnings.
if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU" )
  target_compile_options( ${main_exe} PUBLIC -Wall -Wextra -Wunreachable-code -Wpedantic -Wshadow -Wformat)
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")  # Optimize for speed.
endif()
if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  target_compile_options( ${main_exe} PUBLIC
    -Wweak-vtables -Wexit-time-destructors -Wglobal-constructors -Wmissing-noreturn )
endif()
#if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
#  target_compile_options( ${main_exe} PUBLIC /permissive- /W3 /w44265 /w44061 /w44062 /std:c++20 )
#  set_target_properties ( ${main_exe} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
#  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")  # Optimize for speed.
#endif()
if(WIN32)
  if (MSVC)
    set(CMAKE_CXX_FLAGS "/W3 /O2 /permissive-")
  endif()
endif()

if(WIN32)
#  add_definitions(-DWIN32)
#  # Headers should be put in C:/Program\ Files/SDL2/include/SDL2/
#  # Or should we just keep header files within our source tree? (Is that normal?)
#  include_directories("C:/Program\ Files/SDL2/include")
#  link_directories("C:/Program\ Files/SDL2/lib/x64")
endif()

# Copy resources into build.
#add_custom_command(TARGET ${main_exe} POST_BUILD
#  COMMAND ${CMAKE_COMMAND} -E copy_directory
#  ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${main_exe}>/res)

if(WIN32)
  # Copy DLLs (see <https://stackoverflow.com/questions/10671916>).
  add_custom_command(TARGET ${main_exe} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/extern/windows/SDL2-2.0.12/lib/x64/SDL2.dll"
        $<TARGET_FILE_DIR:${main_exe}>)
  add_custom_command(TARGET ${main_exe} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/extern/windows/glew-2.1.0/bin/Release/x64/glew32.dll"
        $<TARGET_FILE_DIR:${main_exe}>)
endif()
