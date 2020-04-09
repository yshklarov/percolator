#pragma once

#include <vector>

enum class SiteStatus : std::int8_t {open, closed, flooded, freshly_flooded};
enum class Pattern : int {open, pattern_1, pattern_2, pattern_3};

struct point {
  point(int x_coord, int y_coord)
    : x {x_coord}
    , y {y_coord}
    {}
  int x;
  int y;
};

class Lattice {
public:
  Lattice(const int width, const int height)
      : grid_width {width}
      , grid_height {height}
      , begun_flooding {false}
  {
    allocate_grid();
  }

  ~Lattice() {
    delete grid;
  }

  // TODO Write copy and move constructors
  Lattice (const Lattice&) = delete;
  Lattice (Lattice&&) =delete;
  Lattice& operator=(const Lattice&) =delete;
  Lattice& operator=(Lattice&&) =delete;

  bool is_open(int x, int y) const {
    return grid_get(x,y) == SiteStatus::open;
  }

  bool is_flooded(int x, int y) const {
    return
      grid_get(x,y) == SiteStatus::flooded or
      grid_get(x,y) == SiteStatus::freshly_flooded;
  }

  bool is_freshly_flooded(int x, int y) const {
    // This is *much* faster than searching the vector freshly_flooded.
    return grid_get(x,y) == SiteStatus::freshly_flooded;
  }

  SiteStatus site_status(int x, int y) const {
    return grid_get(x,y);
  }
  
  bool is_fully_flooded() {
    return begun_flooding and freshly_flooded.empty();
  }

  void resize(const int width, const int height);

  int get_width () const { return grid_width; }
  int get_height () const { return grid_height; }

  void fill_pattern(Pattern pattern);
  void unflood();
  void randomize_bernoulli(double p = 0.5);
  void percolate();
  bool percolate_step();

private:
  int grid_width;
  int grid_height;
  bool begun_flooding;
  std::vector<point> freshly_flooded;
  SiteStatus* grid;  // Flat layout for speed.

  void allocate_grid();
  bool flood_first_row();

  inline SiteStatus grid_get(int x, int y) const {
    return grid[y * grid_width + x];
  }
  inline void grid_set(int x, int y, SiteStatus new_status) {
    grid[y * grid_width + x] = new_status;
  }
};
