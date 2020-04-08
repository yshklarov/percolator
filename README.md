Percolator
==========

Percolator is a tool for running simulations of [percolation](https://en.wikipedia.org/wiki/Percolation_theory) on a planar lattice. Currently it only does site percolation on the square lattice.

Features
--------

  * Not much so far. Check back soon!

Screenshots
-----------

![screenshot 1](screenshots/screenshot_1.png)

![screenshot 1](screenshots/screenshot_2.png)

Usage
-----
  * (TODO)

Installation
------------

Binaries for Windows are [available here](https://yakov.shklarov.com/percolator/), but they could be out of date.


Building from source
--------------------

Requirements: Percolator uses [ImGui](https://github.com/ocornut/imgui) for the user
interface. ImGui supports many platforms and graphics backends, so it should be relatively
straightforward to get Percolator to build on your system.

**Linux**:
    $ mkdir build
    $ cd build
    $ cmake ..
    $ cmake --build . -v
    $ ./percolator

**Windows**: We use CMake. Load the CMakeLists.txt file in any IDE that supports CMake (e.g., Visual Studio or Qt Creator). The Windows build doesn't work yet, sorry!!

**OS X**: You are entirely on your own. You might try to port Percolator to ImGui's sdl_metal backend.
