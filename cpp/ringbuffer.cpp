// © 2021 Erik Rigtorp <erik@rigtorp.se>
// SPDX-License-Identifier: CC0-1.0

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

struct ringbuffer {
  std::vector<int> data_{};
  alignas(64) std::atomic<size_t> readIdx_{0};
  alignas(64) std::atomic<size_t> writeIdx_{0};

  ringbuffer(size_t capacity) : data_(capacity, 0) {}

  bool push(int val) {
    auto const writeIdx = writeIdx_.load(std::memory_order_relaxed);
    auto nextWriteIdx = writeIdx + 1;
    if (nextWriteIdx == data_.size()) {
      nextWriteIdx = 0;
    }
    if (nextWriteIdx == readIdx_.load(std::memory_order_acquire)) {
      return false;
    }
    data_[writeIdx] = val;
    writeIdx_.store(nextWriteIdx, std::memory_order_release);
    return true;
  }

  bool pop(int &val) {
    auto const readIdx = readIdx_.load(std::memory_order_relaxed);
    if (readIdx == writeIdx_.load(std::memory_order_acquire)) {
      return false;
    }
    val = data_[readIdx];
    auto nextReadIdx = readIdx + 1;
    if (nextReadIdx == data_.size()) {
      nextReadIdx = 0;
    }
    readIdx_.store(nextReadIdx, std::memory_order_release);
    return true;
  }
};

struct ringbuffer2 {
  std::vector<int> data_{};
  alignas(64) std::atomic<size_t> readIdx_{0};
  alignas(64) size_t writeIdxCached_{0};
  alignas(64) std::atomic<size_t> writeIdx_{0};
  alignas(64) size_t readIdxCached_{0};

  ringbuffer2(size_t capacity) : data_(capacity, 0) {}

  bool push(int val) {
    auto const writeIdx = writeIdx_.load(std::memory_order_relaxed);
    auto nextWriteIdx = writeIdx + 1;
    if (nextWriteIdx == data_.size()) {
      nextWriteIdx = 0;
    }
    if (nextWriteIdx == readIdxCached_) {
      readIdxCached_ = readIdx_.load(std::memory_order_acquire);
      if (nextWriteIdx == readIdxCached_) {
        return false;
      }
    }
    data_[writeIdx] = val;
    writeIdx_.store(nextWriteIdx, std::memory_order_release);
    return true;
  }

  bool pop(int &val) {
    auto const readIdx = readIdx_.load(std::memory_order_relaxed);
    if (readIdx == writeIdxCached_) {
      writeIdxCached_ = writeIdx_.load(std::memory_order_acquire);
      if (readIdx == writeIdxCached_) {
        return false;
      }
    }
    val = data_[readIdx];
    auto nextReadIdx = readIdx + 1;
    if (nextReadIdx == data_.size()) {
      nextReadIdx = 0;
    }
    readIdx_.store(nextReadIdx, std::memory_order_release);
    return true;
  }
};

void pinThread(int cpu) {
  if (cpu < 0) {
    return;
  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) ==
      -1) {
    perror("pthread_setaffinity_no");
    exit(1);
  }
}

template <typename T> void bench(int cpu1, int cpu2) {
  const size_t queueSize = 100000;
  const int64_t iters = 100000000;

  T q(queueSize);
  auto t = std::thread([&] {
    pinThread(cpu1);
    for (int i = 0; i < iters; ++i) {
      int val;
      while (!q.pop(val))
        ;
      if (val != i) {
        throw std::runtime_error("");
      }
    }
  });

  pinThread(cpu2);

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iters; ++i) {
    while (!q.push(i))
      ;
  }
  while (q.readIdx_.load(std::memory_order_relaxed) !=
         q.writeIdx_.load(std::memory_order_relaxed))
    ;
  auto stop = std::chrono::steady_clock::now();
  t.join();
  std::cout << iters * 1000000000 /
                   std::chrono::duration_cast<std::chrono::nanoseconds>(stop -
                                                                        start)
                       .count()
            << " ops/s" << std::endl;
}

int main(int argc, char *argv[]) {

  int cpu1 = -1;
  int cpu2 = -1;

  if (argc == 3) {
    cpu1 = std::stoi(argv[1]);
    cpu2 = std::stoi(argv[2]);
  }

  // bench<ringbuffer>(cpu1, cpu2);
  bench<ringbuffer2>(cpu1, cpu2);

  return 0;
}
