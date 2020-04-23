//#include <random>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#ifdef HAVE_TBB
#include <execution>
#endif
#include <functional>
#include <limits>

#include "lattice.h"


// The constructor allocates but does not initialize the lattice. You must call fill() on a
// Lattice object after creating it.
Lattice::Lattice (unsigned int width, unsigned int height)
  : grid {nullptr}
  , grid_width {width}
  , grid_height {height}
  , begun_percolation {false}
  , flow_direction {FlowDirection::all_sides}
{
  allocate_grid();
}

Lattice::~Lattice() {
  clear_clusters();
  delete[] grid;
}

// Copy constructor
Lattice::Lattice (const Lattice& rhs)
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

void Lattice::resize(const unsigned int width, const unsigned int height) {
  delete[] grid;
  grid = nullptr;
  grid_width = width;
  grid_height = height;
  // TODO This is slow for large grids. Can we avoid doing it unless strictly necessary?
  allocate_grid();
  clear_clusters();
  freshly_flooded.clear();
  begun_percolation = false;
}

unsigned int Lattice::get_width() const { return grid_width; }
unsigned int Lattice::get_height() const { return grid_height; }

FlowDirection Lattice::get_flow_direction() {
  return this->flow_direction;
}

void Lattice::set_flow_direction(FlowDirection direction) {
  this->flow_direction = direction;
}

void Lattice::set_torus(bool is_torus) {
  torus = is_torus;
}

// Xorshift: Fast RNG. Copied from <https://en.wikipedia.org/wiki/Xorshift>.
// TODO get rid of this: 32 bits isn't enough for what we're doing.
uint32_t xorshift32() {
	// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
  // TODO Seed with system clock.
	static uint32_t state {1};  // Must be initialized to nonzero.
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}
// uint32_t xorshift128() {
// 	// Algorithm "xor128" from p. 5 of Marsaglia, "Xorshift RNGs"
//   // The state must be initialized to not be all zero
//   static uint32_t a {10}, b {20}, c {30}, d {40};
// 	uint32_t t = d;

// 	uint32_t const s = a;
// 	d = c;
// 	c = b;
// 	b = s;

// 	t ^= t << 11;
// 	t ^= t >> 8;
// 	return a = t ^ s ^ (s >> 19);
// }

namespace measure {
  filler open() {
    // This has to be static so that we can compare fillers, e.g., open() == open() is true.
    const static auto f {
      [](int x, int y) {
        return SiteStatus::open;
      }};
    return f;
  }

  filler pattern_1() {
    const static auto f {
      [](int x, int y) {
        return (x + y) % 2 ? SiteStatus::open : SiteStatus::closed;
      }};
    return f;
  }
  filler pattern_2() {
    const static auto f {
      [](int x, int y) {
        return x % 5 || y % 5 ? SiteStatus::open : SiteStatus::closed;
      }};
    return f;
  }
  filler pattern_3() {
    const static auto f {
      [](int x, int y) {
        return (x + y) % 10 ? SiteStatus::open : SiteStatus::closed;
      }};
    return f;
  }

  filler bernoulli(double p) {
    // For me, xorshift32 yields about 30% faster lattice generation than GCC's default std rng.
    // TODO Try doing it on the GPU instead: see e.g. cuRAND, or
    // <http://www0.cs.ucl.ac.uk/staff/ucacbbl/ftp/papers/langdon_2009_CIGPU.pdf>.

    //static std::random_device rd;  // Seed
    //static std::default_random_engine gen(rd());
    //std::bernoulli_distribution dis(p);  // Call dis(gen) for a new random bool.

    // TODO Fix bug: bernoulli(1.0) has a nonzero probability of returning false.
    const uint32_t barrier = p * std::numeric_limits<uint32_t>::max();
    const auto gen {[barrier]() { return xorshift32() < barrier; }};

    // TODO Return the *same* lambda if called with the same p twice (do this more elegantly: look
    // into memoization of functions).
    return [gen](int x, int y) { return
        gen() ? SiteStatus::open : SiteStatus::closed; };
  }
};

void Lattice::fill(measure::filler f, std::atomic_bool &run) {
  clear_clusters();
  freshly_flooded.clear();
  begun_percolation = false;
  for_each_site(
    [&](int x, int y) {
      grid_set(x, y, f(x, y));
    }, run);
}

// Return true if anything new got flooded.
bool Lattice::flood_entryways() {
  auto flooded_something_new {false};
  auto flood_entryway {
    [this, &flooded_something_new](int x, int y) {
      if (grid_get(x, y) == SiteStatus::open) {
        grid_set(x, y, SiteStatus::freshly_flooded);
        freshly_flooded.emplace_back(x, y);
        flooded_something_new = true;
      }
    }};
  begun_percolation = true;

  switch (flow_direction) {
  case FlowDirection::top:
    for (auto x{0}; x < grid_width; ++x) {
      flood_entryway(x, 0);
    }
    break;
  case FlowDirection::all_sides: {
    const std::array<unsigned int, 2> ys {0, grid_height - 1};
    for (auto y : ys) {
      for (auto x{0}; x < grid_width; ++x) {
        flood_entryway(x, y);
      }
    }
    const std::array<unsigned int, 2> xs {0, grid_width - 1};
    for (auto x : xs) {
      for (auto y{1}; y < grid_height - 1; ++y) {
        flood_entryway(x, y);
      }
    }
    break; }
  default:
    assert(false);
    break;
  }

  return flooded_something_new;
}

// Returns true if anything new gets flooded.
bool Lattice::flow_one_step(std::atomic_bool &run) {
  if (torus) {
    return flow_one_step_torus(run);
  }
  if (!begun_percolation) {
    return flood_entryways();
  }
  static std::vector<Site> currently_flooding;
  currently_flooding.clear();
  for (auto p : freshly_flooded) {
    if (!run) { break; }
    grid_set(p.x, p.y, SiteStatus::flooded);  // No longer fresh
    if (p.y > 0 && grid_get(p.x, p.y - 1) == SiteStatus::open) {
      grid_set(p.x, p.y - 1, SiteStatus::freshly_flooded);
      currently_flooding.emplace_back(p.x, p.y-1);
    }
    if (p.y < grid_height - 1 && grid_get(p.x, p.y + 1) == SiteStatus::open) {
      grid_set(p.x, p.y + 1, SiteStatus::freshly_flooded);
      currently_flooding.emplace_back(p.x, p.y+1);
    }
    if (p.x > 0 && grid_get(p.x - 1, p.y) == SiteStatus::open) {
      grid_set(p.x - 1, p.y, SiteStatus::freshly_flooded);
      currently_flooding.emplace_back(p.x-1, p.y);
    }
    if (p.x < grid_width - 1 && grid_get(p.x + 1, p.y) == SiteStatus::open) {
      grid_set(p.x + 1, p.y, SiteStatus::freshly_flooded);
      currently_flooding.emplace_back(p.x+1, p.y);
    }
  }
  freshly_flooded.swap(currently_flooding);
  return not freshly_flooded.empty();
}

inline bool Lattice::flow_one_step_torus(std::atomic_bool &run) {
  if (!begun_percolation) {
    return flood_entryways();
  }
  static std::vector<Site> currently_flooding;
  currently_flooding.clear();
  for (auto p : freshly_flooded) {
    if (!run) { break; }
    grid_set(p.x, p.y, SiteStatus::flooded);  // No longer fresh
    auto nx {p.x};
    auto ny {p.y - 1};
    if (ny < 0) { ny += grid_height; };
    if (grid_get(nx, ny) == SiteStatus::open) {
      grid_set(nx, ny, SiteStatus::freshly_flooded);
      currently_flooding.emplace_back(nx, ny);
    }
    ny += 2;
    if (ny > grid_height - 1 ) { ny -= grid_height; }
    if (grid_get(nx, ny) == SiteStatus::open) {
      grid_set(nx, ny, SiteStatus::freshly_flooded);
      currently_flooding.emplace_back(nx, ny);
    }
    nx -= 1;
    if (nx < 0) { nx += grid_width; }
    ny = p.y;
    if (grid_get(nx, ny) == SiteStatus::open) {
      grid_set(nx, ny, SiteStatus::freshly_flooded);
      currently_flooding.emplace_back(nx, ny);
    }
    nx += 2;
    if (nx > grid_width - 1) { nx -= grid_width; }
    if (grid_get(nx, ny) == SiteStatus::open) {
      grid_set(nx, ny, SiteStatus::freshly_flooded);
      currently_flooding.emplace_back(nx, ny);
    }
  }
  freshly_flooded.swap(currently_flooding);
  return not freshly_flooded.empty();
}

void Lattice::flow_fully(std::atomic_bool &run) {
  flow_fully_(false, run);
}

void Lattice::flow_fully_(bool track_cluster, std::atomic_bool &run) {
  if (!begun_percolation) {
    flood_entryways();
  }
  if (track_cluster) {
    do {
      // Append the newly flooded sites to the current cluster.
      current_cluster.insert(
        std::end(current_cluster),
        std::begin(freshly_flooded),
        std::end(freshly_flooded));
    } while (run && flow_one_step(run));
  } else {
    while (run && flow_one_step(run)) {};
  }
}

void Lattice::find_clusters(std::atomic_bool &run) {
  reset_percolation();
  clear_clusters();
  begun_percolation = true;
  auto begin_flooding_at {
    [&](int x, int y) -> bool {
      freshly_flooded.clear();
      if (grid_get(x, y) == SiteStatus::open) {
        grid_set(x, y, SiteStatus::freshly_flooded);
        freshly_flooded.emplace_back(x, y);
        return true;
      }
      return false;
    }};
  for_each_site(
    [&](int x, int y) {
      if (begin_flooding_at(x, y)) {
        flow_fully_(true, run);
        clusters.emplace_back(new Cluster {current_cluster});
        current_cluster.clear();
      }
    }, run);
  freshly_flooded.clear();
}

// Sort all clusters by size in descending order.
void Lattice::sort_clusters() {
  std::sort(
    // Enable parallelized sorting algorithm if we can.
#ifdef HAVE_TBB
    std::execution::par_unseq,
#endif
    clusters.begin(),
    clusters.end(),
    [&](const auto cluster1, const auto cluster2) {
      return cluster1->size() > cluster2->size();
    });
}

unsigned int Lattice::num_clusters() const {
  return clusters.size();
}

bool Lattice::done_percolation() {
  return begun_percolation and freshly_flooded.empty();
}

void Lattice::reset_percolation() {
  for (auto i {0}; i < grid_width * grid_height; ++i) {
    if (grid[i] == SiteStatus::flooded or
        grid[i] == SiteStatus::freshly_flooded) {
      grid[i] = SiteStatus::open;
    }
  }
  clear_clusters();
  freshly_flooded.clear();
  begun_percolation = false;
}

SiteStatus Lattice::site_status(int x, int y) const {
  return grid_get(x,y);
}
bool Lattice::is_open(int x, int y) const {
  return grid_get(x,y) == SiteStatus::open;
}
bool Lattice::is_flooded(int x, int y) const {
  return
    grid_get(x,y) == SiteStatus::flooded or
    grid_get(x,y) == SiteStatus::freshly_flooded;
}
bool Lattice::is_freshly_flooded(int x, int y) const {
  // This is faster than searching the vector freshly_flooded.
  return grid_get(x,y) == SiteStatus::freshly_flooded;
}

void Lattice::for_each_site(std::function<void (int, int)> f, std::atomic_bool &run) const {
  for (auto y {0}; y < grid_height && run; ++y) {
    for (auto x {0}; x < grid_width && run; ++x) {
      f(x, y);
    }
  }
}

void Lattice::for_each_cluster(std::function<void (Cluster)> f, std::atomic_bool &run) const {
  for (auto cluster : clusters) {
    if (!run) { break; }
    f(*cluster);
  }
}

// Allocates memory for the lattice, but does not initialize it. Leaves the lattice in an invalid
// state, possibly containing garbage data: The caller must subsequently fill the lattice by
// calling fill().
void Lattice::allocate_grid() {
  grid = new SiteStatus[grid_width * grid_height];
}

void Lattice::clear_clusters() {
  for (auto cluster : clusters) {
    delete cluster;
  }
  clusters.clear();
}

inline SiteStatus Lattice::grid_get(int x, int y) const {
  return grid[y * grid_width + x];
}
inline void Lattice::grid_set(int x, int y, SiteStatus new_status) {
  grid[y * grid_width + x] = new_status;
}
