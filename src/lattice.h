#ifndef LATTICE_H
#define LATTICE_H

#include <atomic>
#include <cstring>
#include <functional>
#include <vector>

enum class SiteStatus : std::int8_t {open, closed, flooded, freshly_flooded};
enum class FlowDirection : int {top, all_sides};
enum class PercolationMode {flow, clusters};

namespace measure {
  using filler = std::function<SiteStatus (int, int)>;
  filler open();
  filler pattern_1();
  filler pattern_2();
  filler pattern_3();
  filler bernoulli(double p);
};

class Site {
public:
  Site(int x_coord, int y_coord)
    : x {x_coord}
    , y {y_coord}
    {}
  Site () =delete;
  // TODO Write copy and move constructors?
  int x;
  int y;
};

using Cluster = std::vector<Site>;

class Lattice {
public:
  // The constructor allocates but does not initialize the lattice. You must call fill() on a
  // Lattice object after creating it.
  Lattice (unsigned int width, unsigned int height)
      : grid_width {width}
      , grid_height {height}
      , grid {nullptr}
      , begun_percolation {false}
      , flow_direction {FlowDirection::all_sides}
  {
    allocate_grid();
  }

  ~Lattice() {
    clear_clusters();
    delete[] grid;
  }

  // TODO Write copy and move constructors
  Lattice () =delete;
  Lattice (const Lattice& rhs)
    : grid_width {rhs.grid_width}
    , grid_height {rhs.grid_height}
    , begun_percolation {rhs.begun_percolation}
    , flow_direction {rhs.flow_direction}
    , freshly_flooded {rhs.freshly_flooded}
    , current_cluster {rhs.current_cluster}
  {
    allocate_grid();
    std::memcpy(grid, rhs.grid, grid_width * grid_height);
    for (auto rhs_cluster : rhs.clusters) {
      Cluster* cluster = new Cluster(*rhs_cluster);
      clusters.emplace_back(cluster);
    }
  }
  Lattice (Lattice&&) =delete;
  Lattice& operator=(const Lattice&) =delete;
  Lattice& operator=(Lattice&&) =delete;

  void resize(unsigned int width, unsigned int height);

  unsigned int get_width () const { return grid_width; }
  unsigned int get_height () const { return grid_height; }

  FlowDirection get_flow_direction();
  void set_flow_direction(FlowDirection direction);

  void fill(measure::filler gen, std::atomic_bool &run);

  bool flood_entryways();
  bool flow_one_step(std::atomic_bool &run);
  void flow_fully(std::atomic_bool &run);
  void find_clusters(std::atomic_bool &run);
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
    // This is faster than searching the vector freshly_flooded.
    return grid_get(x,y) == SiteStatus::freshly_flooded;
  }

  void for_each_site(std::function<void (int, int)> f, std::atomic_bool &run) const;
  void for_each_cluster(std::function<void (Cluster)> f, std::atomic_bool &run) const;

private:
  unsigned int grid_width;
  unsigned int grid_height;
  SiteStatus* grid;  // Flat layout for speed.
  bool begun_percolation;
  FlowDirection flow_direction;
  std::vector<Site> freshly_flooded;

  // For some reason, sorting a vector of raw pointers is *much* faster than sorting a vector of
  // vectors. But why? The difference in memory is not that large:
  // sizeof(&Cluster) == 8; sizeof(Cluster) == 24.
  std::vector<Cluster*> clusters;
  Cluster current_cluster;

  void flow_fully_(bool track_cluster, std::atomic_bool &run);
  void allocate_grid();
  void clear_clusters();

  inline SiteStatus grid_get(int x, int y) const {
    return grid[y * grid_width + x];
  }
  inline void grid_set(int x, int y, SiteStatus new_status) {
    grid[y * grid_width + x] = new_status;
  }
};

#endif  // LATTICE_H
