#include <cstdlib>
#include <cstdint>
#include <cinttypes>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <parallel/amp/primitives/radix-sort.hh>

#define EACH(i, size) for (auto i = decltype(size)(0); i < size; i++)

uint64_t ticks(void) {
  const uint64_t ticks_per_second = UINT64_C(10000000);
  static LARGE_INTEGER freq;
  static uint64_t start_time;
  LARGE_INTEGER value;
  QueryPerformanceCounter(&value);
  if (!freq.QuadPart) {
    QueryPerformanceFrequency(&freq);
    start_time = value.QuadPart;
  }
  return ((value.QuadPart - start_time) * ticks_per_second) / freq.QuadPart;
}

template<typename F>
uint64_t timed(F && f) {
  auto start = ticks();
  f();
  return ticks() - start;
};

void test_amp(size_t min_count, size_t max_count, bool debug) {
  using namespace parallel::amp;
  using namespace concurrency;
  auto acc = accelerator();
  auto cpu_acc = accelerator(accelerator::cpu_accelerator);
  std::wcout << L"C++ AMP"      << std::endl
    << L"\t" << acc.description << std::endl;
  array<uint32_t> keys(max_count, cpu_acc.default_view, acc.default_view);
  array<uint32_t> indexes(max_count, cpu_acc.default_view, acc.default_view);
  if (!debug) {
    std::cout << "Warming...";
    radix_sort<int>(acc.default_view, keys.view_as(extent<1>(min_count)), indexes.view_as(extent<1>(min_count)), true);
    acc.default_view.wait();
    std::cout << "done." << std::endl;
  }
  for (size_t count = min_count; count <= max_count; count <<= 1) {
    EACH(i, count) {
      uint32_t j = i == 0 ? 0 : rand() % i;
      keys[i] = keys[j];
      keys[j] = int32_t(i - count / 2);
      indexes[i] = indexes[j];
      indexes[j] = uint32_t(i);
    }
    auto elapsed = timed([&acc, &keys, &indexes, &count] {
      radix_sort<int>(acc.default_view, keys.view_as(extent<1>(count)), indexes.view_as(extent<1>(count)), true);
      acc.default_view.wait();
    });
    auto passed = true;
    for (size_t i = 0; i < count && passed; i++)
      passed &= indexes[i] == count - i - 1;
    std::cout       << std::setprecision(8) << std::setfill(' ')
      << "count "   << std::setw(10)        << count << " "
      << "elapsed " << std::setw(10)        << elapsed << " ticks " << std::setw(10) << elapsed / 10000000. << " sec "
      << "speed "   << std::setw(12)        << (count * 10000000ll) / elapsed                               << " per sec "
      << "- "       << (passed ? "PASSED" : "FAILED")                                                       << std::endl;
  }
  std::cout << "COMPLETE C++ AMP" << std::endl;
}

int main(int argc, char const * argv[]) {
#ifndef NDEBUG
  bool debug = true;
#else
  bool debug = argc > 2;
#endif
  size_t min_count = 1024;
  size_t max_count = 64 * 1024 * 1024;
  if (argc > 1) {
    uint64_t count;
    std::istringstream(argv[1]) >> count;
    if (count < min_count) max_count = min_count;
    if (count > UINT_MAX) max_count = UINT_MAX;
    min_count = max_count = static_cast<size_t>(count);
  }
  srand(max_count);
  test_amp(min_count, max_count, debug);
  std::cout << "Press [ENTER] for exit...";
  std::cin.ignore();
  return 0;
}
