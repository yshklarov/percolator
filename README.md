Percolator
==========

Percolator is a tool for running simulations of [percolation](https://en.wikipedia.org/wiki/Percolation_theory) on a planar lattice. Currently it only does site percolation on the square lattice.


Screenshots
-----------

![screenshot 1](screenshots/screenshot_1.png)

![screenshot 1](screenshots/screenshot_2.png)


Building
--------

Requirements: Percolator uses [ImGui](https://github.com/ocornut/imgui) for the user
interface. ImGui supports many platforms and graphics backends, so it should be relatively
straightforward to get Percolator to build on your system.

**Linux**: Install SDL2, GLEW, and CMake. Then use the standard CMake build procedure, for example:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ cmake --build . -v -j8
    $ ./percolator

**Windows**: Load the ```CMakeLists.txt``` file in any IDE that supports CMake (e.g., Visual Studio or Qt Creator). But the Windows build doesn't work yet, sorry!!
