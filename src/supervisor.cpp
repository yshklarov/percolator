#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

#include "utility.h"
#include "supervisor.h"

Supervisor::Supervisor(unsigned int width, unsigned int height, measure::filler f)
  : lattice_width {width}
  , lattice_height {height}
  , lattice_measure {f}
  , flow_direction {FlowDirection::top}
  , worker_thread { [this]() { worker(); } }
{
  fill();
}

Supervisor::~Supervisor() {
  stop_flow();

  // Terminate worker thread
  request_mutex.lock();
  terminate_requested = true;
  request_mutex.unlock();
  running = false;
  running_fill = false;
  running_percolation = false;
  running_reset = false;
  worker_thread.join();

  lattice_mutex.lock();
  delete lattice;
  lattice_mutex.unlock();
}

// Sets the size of the lattice. Note that fill() must be called subsequently to actually create
// the new lattice.
void Supervisor::set_size(unsigned int width, unsigned int height) {
  lattice_width = width;
  lattice_height = height;
}

// Sets a new measure (but does not fill the lattice).
void Supervisor::set_measure(measure::filler f) {
  lattice_measure_mutex.lock();
  lattice_measure = f;
  lattice_measure_mutex.unlock();
}

// Replaces the lattice by a new one, randomly filled.
void Supervisor::fill() {
  request_mutex.lock();
  fill_requested = true;
  request_mutex.unlock();
}

// Stop doing tasks that have duplicates already queued up.
void Supervisor::purge_stale_operations() {
  request_mutex.lock();
  if (running_fill && fill_requested) {
    // Start over filling immediately.
    running_fill = false;
  }
  if (running_percolation && (flow_fully_requested || find_clusters_requested)) {
    // Start over percolation immediately.
    running_percolation = false;
  }
  request_mutex.unlock();
}

void Supervisor::set_flow_direction(FlowDirection direction) {
  flow_direction = direction;
}

void Supervisor::set_torus(bool is_torus) {
  torus = is_torus;
}

void Supervisor::flood_entryways() {
  request_mutex.lock();
  flood_entryways_requested = true;
  request_mutex.unlock();
}

void Supervisor::flow_n_steps(unsigned int n) {
  request_mutex.lock();
  flow_steps_requested += n;
  request_mutex.unlock();
}

void Supervisor::flow_fully() {
  request_mutex.lock();
  flow_fully_requested = true;
  find_clusters_requested = false;
  request_mutex.unlock();
}

void Supervisor::start_flow() {
  stop_flow();  // Don't run multiple flow threads concurrently.
  flow_thread = std::async(
    std::launch::async,
    [&]() {
      flowing = true;
      flow_start_time = std::chrono::high_resolution_clock::now();
      while(flowing) {
        // This mutex only exists for the condition variable.
        std::unique_lock<std::mutex> lock {flowing_mutex};
        auto steps_per_second {flow_speed.load()};
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds delay{
          std::max(1, static_cast<int>(1'000'000.0F / steps_per_second))};
        const int num_steps = (now - flow_start_time) / delay;
        if (num_steps > 0) {
          flow_n_steps(num_steps);
        }
        // Adjust precisely for the next frame.
        flow_start_time += num_steps * delay;
        // No reason to go faster than 120 fps.
        flowing_abort.wait_for(lock, std::chrono::milliseconds(8));
      }
    });
}

void Supervisor::stop_flow() {
  if (flowing) {
    flowing = false;
    flowing_mutex.lock();
    flowing_abort.notify_all();
    flowing_mutex.unlock();
    flow_thread.wait(); // Should terminate very soon.
  }
}

void Supervisor::set_flow_speed(float steps_per_second) {
  assert(flow_speed > 0);
  flow_speed = steps_per_second;
}

bool Supervisor::is_flowing() {
  return flowing;
}

void Supervisor::find_clusters() {
  request_mutex.lock();
  find_clusters_requested = true;
  flow_fully_requested = false;
  request_mutex.unlock();
}

unsigned int Supervisor::num_clusters() {
  if (!lattice || !lattice_mutex.try_lock()) {
    return 0;
  }
  auto num {lattice->num_clusters()};
  lattice_mutex.unlock();
  return num;
}

bool Supervisor::done_percolation() {
  if (!lattice) {
    return false;
  }
  if (!lattice_mutex.try_lock()) {
    return false;
  }
  auto done {lattice->done_percolation()};
  lattice_mutex.unlock();
  return done;
}

void Supervisor::reset_percolation() {
  stop_flow();
  request_mutex.lock();
  find_clusters_requested = false;
  flow_fully_requested = false;
  flow_steps_requested = 0;
  reset_requested = true;
  request_mutex.unlock();
}

// Returns the cluster sizes, unless they're busy being computed.
// TODO Return a saner data structure, e.g., a sorted vector of pairs.
auto Supervisor::get_cluster_sizes()
      -> std::optional<std::map<const unsigned int, unsigned int, ReverseCmp>> {
  std::unique_lock<std::mutex> lock(cluster_sizes_mutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    return std::nullopt;
  }
  return cluster_sizes;
}

float Supervisor::cluster_largest_proportion() {
  auto base {100.0F};
  // TODO How to properly detect the proportion of open sites, regardless of measure?
  // We don't actually have access to the measure here: it's just an opaque function.
  //if (lattice_measure == measure::bernoulli(p)) {
  //  // This is inaccurate, of course, because the true proportion of open sites is
  //  // almost never exactly bernoulli_p.
  //  base /= bernoulli_p;
  //}
  return base * max_cluster_size / (lattice_width * lattice_height);
}

// If the lattice has changed since the last time this function was called, calls
// f(lattice). Otherwise, or if a copy of the lattice isn't available yet, calls
// f(nullptr). Synchronous.
void Supervisor::give_lattice_to(std::function<void (Lattice*)> f) {
  if (lattice_copy_mutex.try_lock()) {
    if (lattice_copy) {
      f(lattice_copy);
      delete lattice_copy;
      lattice_copy = nullptr;
    } else {
      if (changed_since_copy) {
        request_mutex.lock();
        lattice_copy_requested = true;
        request_mutex.unlock();
      }
      f(nullptr);
    }
    lattice_copy_mutex.unlock();
  } else {
    f(nullptr);
  }
}

// Returns a string if a computation is currently in progress.
// TODO add timer: only busy if has been busy for n milliseconds. E.g., busy_for(100);
// busy_for(0): returns true if has been busy on the same job for at least n ms.
std::optional<std::string> Supervisor::busy() {
  if (running_cluster_sizes) {
    return "Computing cluster sizes";
  }
  if (running_fill) {
    return "Filling lattice";
  }
  if (running_percolation) {
    return "Computing percolation";
  }
  if (running_reset) {
    return "Resetting lattice";
  }
  if (running) {
    return "Computing";  // Generic busy message
  }
  return std::nullopt;
}

bool Supervisor::errors_exist() {
  return not errors.empty();
}

void Supervisor::clear_one_error() {
  errors.pop();
}

const std::string Supervisor::get_first_error() {
  return errors.front();
}

// Aborts most operations (but doesn't stop flow).
void Supervisor::abort() {
  request_mutex.lock();

  running = false;
  running_fill = false;
  running_percolation = false;
  running_reset = false;
  reset_requested = false;
  flood_entryways_requested = false;
  fill_requested = false;
  flow_fully_requested = false;
  find_clusters_requested = false;

  request_mutex.unlock();
}

void Supervisor::make_lattice_copy_if_needed() {
  request_mutex.lock();
  if (lattice_copy_requested && lattice) {
    lattice_copy_requested = false;
    request_mutex.unlock();
    lattice_copy_mutex.lock();
    delete lattice_copy;
    lattice_mutex.lock();
    lattice_copy = new Lattice(*lattice);
    changed_since_copy = false;
    lattice_mutex.unlock();
    lattice_copy_mutex.unlock();
  } else {
    request_mutex.unlock();
  }
}

void Supervisor::compute_cluster_sizes() {
  std::unique_lock<std::mutex> lock(cluster_sizes_mutex);
  cluster_sizes.clear();
  max_cluster_size = 0;
  auto f {
    [&] (Cluster cluster) {
      size_t size {cluster.size()};
      cluster_sizes[size] += 1;
      max_cluster_size = std::max(size, max_cluster_size.load());
    }};
  lattice_mutex.lock();
  running_cluster_sizes = true;
  lattice->for_each_cluster(f, std::ref(running_cluster_sizes));
  running_cluster_sizes = false;
  lattice_mutex.unlock();
}

void Supervisor::worker() {
  while (!terminate_requested) {
    request_mutex.lock();
    // TODO De-duplicate this
    if (reset_requested) {
      reset_requested = false;
      request_mutex.unlock();
      lattice_mutex.lock();
      running_reset = true;
      changed_since_copy = true;
      lattice->reset_percolation();
      lattice_mutex.unlock();

      // Don't make a lattice copy after resetting, unless there's nothing more to do.
      request_mutex.lock();
      if (!flow_fully_requested && !find_clusters_requested && flow_steps_requested == 0) {
        request_mutex.unlock();
        if (running_reset) {
          make_lattice_copy_if_needed();
        }
      } else {
        request_mutex.unlock();
      }
      running_reset = false;
    } else if (lattice && flood_entryways_requested) {
      flood_entryways_requested = false;
      request_mutex.unlock();
      lattice_mutex.lock();
      running = true;
      changed_since_copy = true;
      lattice->set_flow_direction(flow_direction);
      lattice->flood_entryways();
      lattice_mutex.unlock();
      if (running) {
        make_lattice_copy_if_needed();
      }
      running = false;
    } else if (fill_requested) {
      fill_requested = false;
      request_mutex.unlock();
      bool bad_alloc {false};
      lattice_mutex.lock();
      running_fill = true;
      changed_since_copy = true;
      if (!lattice ||
          lattice->get_width() != lattice_width ||
          lattice->get_height() != lattice_height) {
        delete lattice;
        try {
          lattice = new Lattice(lattice_width, lattice_height);
        } catch (std::bad_alloc& ba) {
          // This is not very useful: modern operating systems over-allocate memory so the error
          // will typically occur later when the memory is accessed.
          errors.push("Not enough memory.");
          bad_alloc = true;
          // There's probably enough memory for this.
          lattice = new Lattice(1, 1);
        }
      }
      lattice->set_flow_direction(flow_direction);
      lattice_measure_mutex.lock();
      auto lm {lattice_measure};
      lattice_measure_mutex.unlock();
      lattice->fill(lm, std::ref(running_fill));
      lattice_mutex.unlock();

      // Don't make a lattice copy after filling, unless there's nothing more to do. If we made a
      // lattice copy here, the GUI would sometimes briefly show an un-percolated lattice when it
      // isn't desired.
      request_mutex.lock();
      if (!flow_fully_requested && !find_clusters_requested && flow_steps_requested == 0) {
        // TODO Fix bug: If the user slides the size slider with auto-percolate on, sometimes we
        // just fill and re-fill and never get to percolation, so nothing is ever shown until the
        // user stops.
        request_mutex.unlock();
        if (running_fill) {
          make_lattice_copy_if_needed();
        }
      } else {
        request_mutex.unlock();
      }
      running_fill = false;
    } else if (flow_fully_requested) {
      flow_fully_requested = false;
      flow_steps_requested = 0;
      request_mutex.unlock();
      lattice_mutex.lock();
      running_percolation = true;
      changed_since_copy = true;
      lattice->set_flow_direction(flow_direction);
      lattice->set_torus(torus);
      lattice->flow_fully(std::ref(running_percolation));
      lattice_mutex.unlock();
      if (running_percolation) {
        make_lattice_copy_if_needed();
      }
      running_percolation = false;
    } else if (find_clusters_requested) {
      find_clusters_requested = false;
      request_mutex.unlock();
      lattice_mutex.lock();
      running_percolation = true;
      changed_since_copy = true;
      lattice->set_torus(torus);
      lattice->find_clusters(std::ref(running_percolation));
      if (running_percolation) {
        lattice->sort_clusters();
      }
      lattice_mutex.unlock();
      if (running_percolation) {
        make_lattice_copy_if_needed();
      }
      running_percolation = false;
      compute_cluster_sizes();
    } else if (flow_steps_requested > 0) {
      flow_steps_requested -= 1;
      request_mutex.unlock();
      lattice_mutex.lock();
      running = true;
      changed_since_copy = true;
      lattice->set_flow_direction(flow_direction);
      lattice->set_torus(torus);
      bool did_flow {lattice->flow_one_step(std::ref(running))};
      lattice_mutex.unlock();
      if (running) {
        make_lattice_copy_if_needed();
      }
      running = false;
      if (flowing && !did_flow) {
        stop_flow();
      }
    } else {
      // Nothing to do
      request_mutex.unlock();
      make_lattice_copy_if_needed();
      pause_ms(8); // TODO Wait for a condition variable instead
    }
  }  // while (!terminated)
}  // worker()
