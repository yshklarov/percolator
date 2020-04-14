#pragma once

#include <functional>
#include <vector>

enum class SiteStatus : std::int8_t {open, closed, flooded, freshly_flooded};
enum class Pattern : int {open, pattern_1, pattern_2, pattern_3};
enum class FlowDirection : int {top, all_sides};
enum class PercolationMode {flow, clusters};

class Site {
public:
  Site(int x_coord, int y_coord)
    : x {x_coord}
    , y {y_coord}
    {}
  Site () =delete;
  // TODO Write copy and move constructors
  int x;
  int y;
};

typedef std::vector<Site> Cluster;

class Lattice {
public:
  Lattice (const int width, const int height)
      : grid_width {width}
      , grid_height {height}
      , begun_percolation {false}
      , flow_direction {FlowDirection::all_sides}
  {
    allocate_grid();
  }

  ~Lattice() {
    delete grid;
  }

  // TODO Write copy and move constructors
  Lattice () =delete;
  Lattice (const Lattice&) = delete;
  Lattice (Lattice&&) =delete;
  Lattice& operator=(const Lattice&) =delete;
  Lattice& operator=(Lattice&&) =delete;

  void resize(const int width, const int height);

  int get_width () const { return grid_width; }
  int get_height () const { return grid_height; }

  FlowDirection getFlowDirection();
  void setFlowDirection(FlowDirection direction);

  void fill_pattern(Pattern pattern);
  void randomize_bernoulli(double p = 0.5);

  bool flood_entryways();
  bool flow_one_step();
  void flow_fully();
  void find_clusters();
  void sort_clusters();
  unsigned int num_clusters();
  bool done_percolation();
  void reset_percolation();

  SiteStatus site_status(int x, int y) const {
    return grid_get(x,y);
  }
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

  void for_each_site(std::function<void (int, int)> f) const;
  void for_each_cluster(std::function<void (Cluster)> f) const;

private:
  int grid_width;
  int grid_height;
  SiteStatus* grid;  // Flat layout for speed.
  bool begun_percolation;
  FlowDirection flow_direction;
  std::vector<Site> freshly_flooded;
  // For some reason, sorting a vector of pointers is *much* faster than sorting a vector of vectors.
  std::vector<Cluster*> clusters;
  Cluster current_cluster;

  void flow_fully_(bool track_cluster);
  void allocate_grid();
  void clear_clusters();

  inline SiteStatus grid_get(int x, int y) const {
    return grid[y * grid_width + x];
  }
  inline void grid_set(int x, int y, SiteStatus new_status) {
    grid[y * grid_width + x] = new_status;
  }
};
