#ifndef LATTICE_H
#define LATTICE_H

#include <atomic>
#include <cstring>
#include <functional>
#include <vector>


#pragma pack(push, 1)  // Only use 1 byte per Site
struct Site {
  bool open : 1;
  bool flooded : 1;
  bool fresh : 1;
  bool :1;  // padding
  bool connected_up : 1;
  bool connected_down : 1;
  bool connected_left : 1;
  bool connected_right : 1;
};
#pragma pack(pop)

struct Coords {
public:
  Coords(int x_coord, int y_coord)
    : x {x_coord}
    , y {y_coord}
    {}
  Coords() =delete;
  int x;
  int y;
};

enum class FlowDirection : int {top, all_sides};
enum class PercolationMode {flow, clusters};

namespace measure {
  using filler = std::function<bool (int, int)>;
  filler open();
  filler pattern_1();
  filler pattern_2();
  filler pattern_3();
  filler bernoulli(double p);
};

using Cluster = std::vector<Coords>;

class Lattice {
public:
  Lattice (unsigned int width, unsigned int height);
  ~Lattice();

  // TODO Write all copy/move constructors
  Lattice() =delete;
  Lattice(const Lattice& rhs);
  Lattice(Lattice&&) =delete;
  Lattice& operator=(const Lattice&) =delete;
  Lattice& operator=(Lattice&&) =delete;

  void resize(unsigned int width, unsigned int height);

  unsigned int get_width() const;
  unsigned int get_height() const;

  FlowDirection get_flow_direction();
  void set_flow_direction(FlowDirection direction);
  void set_torus(bool is_torus);
  bool is_torus() const;

  void fill(measure::filler gen, std::atomic_bool &run);

  bool flood_entryways();
  bool flow_one_step(std::atomic_bool &run);
  void flow_fully(std::atomic_bool &run);
  void find_clusters(std::atomic_bool &run);
  void sort_clusters();
  unsigned int num_clusters() const;
  bool done_percolation();
  void reset_percolation();

  Site get_site(int x, int y) const;
  void set_site(int x, int y, Site site);
  bool is_open(int x, int y) const;
  bool is_flooded(int x, int y) const;
  bool is_freshly_flooded(int x, int y) const;

  void for_each_site(std::function<void (int, int)> f, std::atomic_bool &run) const;
  void for_each_cluster(std::function<void (Cluster)> f, std::atomic_bool &run) const;

private:
  Site* grid;
  unsigned int grid_width;
  unsigned int grid_height;
  bool begun_percolation;
  FlowDirection flow_direction;
  bool torus {false};
  std::vector<Coords> freshly_flooded;

  // For some reason, sorting a vector of raw pointers is *much* faster than sorting a vector of
  // vectors. But why? The difference in memory is not that large: sizeof(&Cluster) == 8;
  // sizeof(Cluster) == 24.
  // TODO Keep clusters in our own contiguous data structure: Otherwise, allocating/deallocating
  // all clusters is very slow, which includes e.g. destructing a Lattice.
  std::vector<Cluster*> clusters;
  Cluster current_cluster;

  bool flow_one_step_torus(std::atomic_bool &run);
  void flow_fully_(bool track_cluster, std::atomic_bool &run);
  void allocate_grid();
  void clear_clusters();

  Site* get_site_ptr(int x, int y);
};

#endif  // LATTICE_H
