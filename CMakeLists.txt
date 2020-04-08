cmake_minimum_required(VERSION 3.17)

set(project_name "percolator")
set(project_desc "Lattice percolation simulation playground")
project(${project_name}
  DESCRIPTION ${project_desc}
  LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  # Default build type
  set(CMAKE_BUILD_TYPE Release)
endif()


################
# Compilation and tools

if(UNIX)
  SET(CMAKE_C_COMPILER clang)
  SET(CMAKE_CXX_COMPILER clang++)
endif()

# If we comment out this O3 line, then the build works but the lattice doesn't display. Why?!
set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)


# Uncomment to enable static code analysis.
#set(CMAKE_CXX_CLANG_TIDY 
#  clang-tidy;
#  -header-filter=.;
#  -checks=-*,bugprone-*,clang-analyzer-*,cppcoreguidelines-*,modernize-*,performance-*,portability-*,readability-*;)


################
# Libraries setup

if (WIN32)
    # Windows-specific instructions
endif (WIN32)
if(UNIX AND NOT APPLE)
    # Linux-specific instructions
endif()
if(APPLE)
    # OSX-specific instructions
endif()

if(UNIX AND NOT APPLE)
    include(FindPkgConfig)
    pkg_search_module(SDL2 required sdl2)
    pkg_search_module(GLEW required glew)
endif()

include_directories(${GLEW_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})


################
# External libs

add_library(imgui STATIC extern/imgui/imgui.cpp extern/imgui/imgui.h extern/imgui/imconfig.h)
set_target_properties(imgui PROPERTIES CXX_CLANG_TIDY "")

add_library(imgui_draw STATIC
  extern/imgui/imgui_draw.cpp extern/imgui/imstb_rectpack.h extern/imgui/imstb_truetype.h)
set_target_properties(imgui_draw PROPERTIES CXX_CLANG_TIDY "")

add_library(imgui_widgets STATIC extern/imgui/imgui_widgets.cpp)
set_target_properties(imgui_widgets PROPERTIES CXX_CLANG_TIDY "")

add_library(imgui_impl_sdl STATIC
  extern/imgui/examples/imgui_impl_sdl.cpp
  extern/imgui/examples/imgui_impl_sdl.h)
target_include_directories(imgui_impl_sdl PRIVATE extern/imgui/)
set_target_properties(imgui_impl_sdl PROPERTIES CXX_CLANG_TIDY "")

add_library(imgui_impl_opengl3 STATIC
  extern/imgui/examples/imgui_impl_opengl3.cpp
  extern/imgui/examples/imgui_impl_opengl3.h)
target_include_directories(imgui_impl_opengl3 PRIVATE extern/imgui/)
set_target_properties(imgui_impl_opengl3 PROPERTIES CXX_CLANG_TIDY "")

add_library(imgui_demo STATIC extern/imgui/imgui_demo.cpp)
target_link_libraries(imgui_demo PRIVATE imgui_draw)
set_target_properties(imgui_demo PROPERTIES CXX_CLANG_TIDY "")


################
# This project

add_library(
  lattice STATIC
  src/lattice.cpp
  src/lattice.h)
target_link_libraries(
  lattice PUBLIC stdc++ m)

add_library(
  imgui_latticeview STATIC
  src/graphics/imgui_latticeview.cpp
  src/graphics/imgui_latticeview.h
  extern/imgui/imgui_internal.h
  extern/imgui/imstb_textedit.h)
target_include_directories(imgui_latticeview PRIVATE extern/ src/)

add_executable(
  ${project_name}
  src/main.cpp)
target_include_directories(${project_name} PUBLIC extern/)
target_link_libraries(
  ${project_name} PUBLIC
  lattice imgui_latticeview
  imgui imgui_widgets imgui_impl_sdl imgui_impl_opengl3 imgui_demo
  ${GLEW_LIBRARIES} ${SDL2_LIBRARIES})


################
# Compiler warnings, setup, etc.

# What's the purpose of this?
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# Per-compiler warning flags, and other settings.
# TODO Warnings only for our own code (not libraries in extern/); add more warnings.
if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU" )
    target_compile_options( ${project_name} PUBLIC -Wall -Wextra -Wunreachable-code -Wpedantic -Wshadow -Wformat)
    set(CMAKE_CXX_FLAGS_DEBUG "-g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")  # Optimize for speed.
endif()
if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    target_compile_options( ${project_name} PUBLIC -Wweak-vtables -Wexit-time-destructors -Wglobal-constructors -Wmissing-noreturn )
endif()
if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    target_compile_options( ${project_name} PUBLIC /W3 /w44265 /w44061 /w44062 /std:c++17 )
    set_target_properties ( ${project_name} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
#    set_target_properties ( ${project_name} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")  # Optimize for speed.
endif()

if(WIN32)
    add_definitions(-DWIN32)
#    # Headers should be put in C:/Program\ Files/SDL2/include/SDL2/
#    # Or should we just keep header files within our source tree? (Is that normal?)
#    include_directories("C:/Program\ Files/SDL2/include")
#    link_directories("C:/Program\ Files/SDL2/lib/x64")
endif()

# Copy resources into build.
#add_custom_command(TARGET ${project_name} POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#    ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${project_name}>/res)

# TODO Get Windows build to work.
# if(WIN32)
# #Copying DLLs (see <https://stackoverflow.com/questions/10671916>).
# add_custom_command(TARGET ${project_name} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${PROJECT_SOURCE_DIR}/lib/windows/SDL2-2.0.8/lib/x64/SDL2.dll"
#         "${PROJECT_SOURCE_DIR}/lib/windows/SDL2_ttf-2.0.14/lib/x64/libfreetype-6.dll"
#         "${PROJECT_SOURCE_DIR}/lib/windows/SDL2_ttf-2.0.14/lib/x64/SDL2_ttf.dll"
#         "${PROJECT_SOURCE_DIR}/lib/windows/SDL2_ttf-2.0.14/lib/x64/zlib1.dll"
#         "${PROJECT_SOURCE_DIR}/lib/windows/SDL2_mixer-2.0.2/lib/x64/SDL2_mixer.dll"
#         "${PROJECT_SOURCE_DIR}/lib/windows/SDL2_mixer-2.0.2/lib/x64/libmpg123-0.dll"
#         $<TARGET_FILE_DIR:${project_name}>)
# endif()
