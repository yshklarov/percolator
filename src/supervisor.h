#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <condition_variable>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

#include "utility.h"
#include "lattice.h"


// Oversees a single lattice. All member functions (except possibly the constructor) return
// immediately: any necessary computations proceed asynchronously.
class Supervisor {
public:
  Supervisor(unsigned int width = 1, unsigned int height = 1, measure::filler f = measure::open());
  ~Supervisor();

  Supervisor() =delete;
  Supervisor(const Lattice& rhs) =delete;
  Supervisor(Lattice&&) =delete;
  Supervisor& operator=(const Lattice&) =delete;
  Supervisor& operator=(Lattice&&) =delete;

  void set_size(unsigned int width, unsigned int height);
  void set_measure(measure::filler f);
  void fill();
  void abort_stale_operations();
  void set_flow_direction(FlowDirection direction);
  void set_torus(bool is_torus);
  void flood_entryways();
  void flow_n_steps(unsigned int n);
  void flow_fully();
  void start_flow();
  void stop_flow();
  void set_flow_speed(float steps_per_second);
  bool is_flowing();
  void find_clusters();
  unsigned int num_clusters();
  bool done_percolation();
  void reset_percolation();
  auto get_cluster_sizes()
    -> std::optional<std::map<const unsigned int, unsigned int, ReverseCmp>>;
  float cluster_largest_proportion();
  Lattice* get_lattice_copy(double copy_timeout_ms = 100.0);
  void request_copy();
  std::optional<std::string> busy();
  bool errors_exist();
  void clear_one_error();
  const std::string get_first_error();
  void abort();

private:
  void make_lattice_copy_if_needed();
  void compute_cluster_sizes();
  void worker();

  Lattice* lattice {nullptr};
  std::mutex lattice_mutex;
  Lattice* lattice_copy {nullptr};
  std::mutex lattice_copy_mutex;
  std::atomic_bool lattice_copy_requested {false};
  std::mutex size_mutex;
  unsigned int lattice_width;
  unsigned int lattice_height;
  measure::filler lattice_measure;
  std::mutex lattice_measure_mutex;
  std::map<const unsigned int, unsigned int, ReverseCmp> cluster_sizes;
  std::mutex cluster_sizes_mutex;
  std::atomic_size_t max_cluster_size {0};
  FlowDirection flow_direction;
  std::atomic_bool torus;

  std::atomic_bool flowing {false};
  std::mutex flowing_mutex;
  std::condition_variable(flowing_abort);

  std::atomic<float> flow_speed {1.0F};

  std::atomic_bool running {false};
  std::atomic_bool running_cluster_sizes {false};
  std::atomic_bool running_copy {false};
  std::atomic_bool running_fill {false};
  std::atomic_bool running_percolation {false};
  std::atomic_bool running_reset {false};
  std::future<void> flow_thread;
  volatile std::atomic_bool changed_since_copy {true};

  std::queue<std::string> errors;

  std::chrono::time_point<std::chrono::high_resolution_clock> flow_start_time;

  std::atomic_bool terminate_requested {false};
  std::atomic_bool reset_requested {false};
  std::atomic_bool flood_entryways_requested {false};
  std::atomic_bool fill_requested {false};
  std::atomic_bool flow_fully_requested {false};
  std::atomic_uint64_t flow_steps_requested {0};
  std::atomic_bool find_clusters_requested {false};
  std::mutex request_mutex;

  std::thread worker_thread;
};


#endif  // SUPERVISOR_H
