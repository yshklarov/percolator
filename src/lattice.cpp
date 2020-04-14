//#include <random>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>

#include "lattice.h"

void Lattice::resize(const int width, const int height) {
  delete[] grid;
  grid_width = width;
  grid_height = height;
  // TODO This is slow for large grids. Can we avoid doing it unless strictly necessary?
  allocate_grid();
  freshly_flooded.clear();
  begun_percolation = false;
}

FlowDirection Lattice::getFlowDirection() {
  return this->flow_direction;
}

void Lattice::setFlowDirection(FlowDirection direction) {
  this->flow_direction = direction;
}

// Xorshift: Fast RNG. Copied from <https://en.wikipedia.org/wiki/Xorshift>.
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

// Return a function (of no arguments) that returns true with probability p.
auto get_bernoulli_gen(const double p) {
  // For me, xorshift32 yields about 30% faster lattice generation than GCC's default std rng.
  // TODO Try doing it on the GPU instead: see e.g. cuRAND, or
  // <http://www0.cs.ucl.ac.uk/staff/ucacbbl/ftp/papers/langdon_2009_CIGPU.pdf>.

  //static std::random_device rd;  // Seed
  //static std::default_random_engine gen(rd());
  //std::bernoulli_distribution dis(p);  // Call dis(gen) for a new random bool.

  // TODO Fix bug: bernoulli(1.0) has a nonzero probability of returning false.
  const uint32_t barrier = p * std::numeric_limits<uint32_t>::max();
  return [barrier]() { return xorshift32() < barrier; };
}

void Lattice::fill_pattern(Pattern pattern) {
  std::function<SiteStatus (int, int)> status_of;
  switch (pattern) {
  case Pattern::pattern_1:  // Dense checkerboard
    status_of = [](int x, int y) { return
        (x + y) % 2 ? SiteStatus::open : SiteStatus::closed; };
    break;
  case Pattern::pattern_2:  // Sparse checkerboard
    status_of = [](int x, int y) { return
        x % 5 || y % 5 ? SiteStatus::open : SiteStatus::closed; };
    break;
  case Pattern::pattern_3:  // Diagonal checkerboard
    status_of = [](int x, int y) { return
        (x + y) % 10 ? SiteStatus::open : SiteStatus::closed; };
    break;
  case Pattern::open:  // All sites open
    status_of = [](int x, int y) { return SiteStatus::open; };
    break;
  default:
    assert(false);
    break;
  }
  for_each_site(
    [&](int x, int y) {
      grid[y * grid_width + x] = status_of(x, y);
    });
  clusters.clear();
  freshly_flooded.clear();
  begun_percolation = false;
}

void Lattice::randomize_bernoulli(const double p) {
  const auto is_open = get_bernoulli_gen(p);
  for (auto i {0}; i < grid_width * grid_height; ++i) {
    grid[i] = is_open()
      ? SiteStatus::open
      : SiteStatus::closed;
  }
  clusters.clear();
  freshly_flooded.clear();
  begun_percolation = false;
}

// Return true if anything new got flooded.
bool Lattice::flood_entryways() {
  auto flooded_something_new {false};
  auto flood_entryway {
    [this, &flooded_something_new](int x, int y) {
      if (grid_get(x, y) == SiteStatus::open) {
        grid_set(x, y, SiteStatus::freshly_flooded);
        freshly_flooded.push_back({x, y});
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
    const std::array<int, 2> ys {0, grid_height - 1};
    for (auto y : ys) {
      for (auto x{0}; x < grid_width; ++x) {
        flood_entryway(x, y);
      }
    }
    const std::array<int, 2> xs {0, grid_width - 1};
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
inline bool Lattice::flow_one_step() {
  if (!begun_percolation) {
    return flood_entryways();
  }
  static std::vector<Site> currently_flooding;
  currently_flooding.clear();
  for (auto p : freshly_flooded) {
    grid_set(p.x, p.y, SiteStatus::flooded);  // No longer fresh
    if (p.y > 0 && grid_get(p.x, p.y - 1) == SiteStatus::open) {
      grid_set(p.x, p.y - 1, SiteStatus::freshly_flooded);
      currently_flooding.push_back({p.x, p.y-1});
    }
    if (p.y < grid_height - 1 && grid_get(p.x, p.y + 1) == SiteStatus::open) {
      grid_set(p.x, p.y + 1, SiteStatus::freshly_flooded);
      currently_flooding.push_back({p.x, p.y+1});
    }
    if (p.x > 0 && grid_get(p.x - 1, p.y) == SiteStatus::open) {
      grid_set(p.x - 1, p.y, SiteStatus::freshly_flooded);
      currently_flooding.push_back({p.x-1, p.y});
    }
    if (p.x < grid_width - 1 && grid_get(p.x + 1, p.y) == SiteStatus::open) {
      grid_set(p.x + 1, p.y, SiteStatus::freshly_flooded);
      currently_flooding.push_back({p.x+1, p.y});
    }
  }
  freshly_flooded.swap(currently_flooding);
  return not freshly_flooded.empty();
}

void Lattice::flow_fully() {
  flow_fully_(false);
}

void Lattice::flow_fully_(bool track_cluster) {
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
    } while (flow_one_step());
  } else {
    while (flow_one_step());
  }
}

void Lattice::find_clusters() {
  reset_percolation();
  clusters.clear();
  begun_percolation = true;
  auto begin_flooding_at {
    [&](int x, int y) -> bool {
      freshly_flooded.clear();
      if (grid_get(x, y) == SiteStatus::open) {
        grid_set(x, y, SiteStatus::freshly_flooded);
        freshly_flooded.push_back({x, y});
        return true;
      }
      return false;
    }};
  for_each_site(
    [&](int x, int y) {
      if (begin_flooding_at(x, y)) {
        flow_fully_(true);
        clusters.push_back(current_cluster);
        current_cluster.clear();
      }
    });
  freshly_flooded.clear();
}

// Sort all clusters by size in descending order.
void Lattice::sort_clusters() {
  std::sort(
    clusters.begin(),
    clusters.end(),
    [&](auto cluster1, auto cluster2) {
      return cluster1.size() > cluster2.size();
    });
}

unsigned int Lattice::num_clusters() {
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
  clusters.clear();
  freshly_flooded.clear();
  begun_percolation = false;
}

void Lattice::for_each_site(std::function<void (int, int)> f) const {
  for (auto y {0}; y < grid_height; ++y) {
    for (auto x {0}; x < grid_width; ++x) {
      f(x, y);
    }
  }
}

void Lattice::for_each_cluster(std::function<void (Cluster)> f) const {
  for (auto cluster : clusters) {
    f(cluster);
  }
}

void Lattice::allocate_grid() {
  grid = new SiteStatus[grid_width * grid_height];
  for (auto i {0}; i < grid_width * grid_height; ++i) {
    grid[i] = SiteStatus::open;
  }
}
