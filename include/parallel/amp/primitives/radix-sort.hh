#pragma once

#include <cstdint>
#include <type_traits>
#include <amp.h>
#include <amp_graphics.h>

#define WG_COUNT 64
#define WG_SIZE 256
#define BLOCK_SIZE 1024  // (4 * WG_SIZE)
#define BITS_PER_PASS 4
#define RADICES 16       // (1 << BITS_PER_PASS)
#define RADICES_MASK 0xf // (RADICES - 1)

#define EACH(i, size) for (uint32_t i = 0; i < size; i++)

namespace parallel {
namespace amp {

  namespace details {
    using namespace concurrency;
    using namespace concurrency::graphics;

    template<typename T> T to_mask(T n) restrict(cpu, amp) { return (1 << n) - 1; }
    template<typename T> T bfe(T src, uint shift, uint n) restrict(cpu, amp)
    { return (src >> shift) & to_mask(n); }
    template<typename T> T bfe_sign(T src, uint shift, uint n) restrict(cpu, amp)
    { return ((((src >> shift) & to_mask(n - 1))
                               ^ to_mask(n - 1))
                               & to_mask(n - 1))
      | (src >> shift) & (1 << (n - 1)); }

    template<typename T>
    T clamp(T x, T minVal, T maxVal) restrict(cpu, amp) { return min(max(x, minVal), maxVal); }
    template<typename T, typename S>
    auto mix(T x, S y, uint_4 a) restrict(cpu, amp) { return x * a + y * (1 - a); }
    template<typename T>
    uint_4 lessThan(T x, T y) restrict(cpu, amp) {
      return uint_4(x.x < y.x, x.y < y.y, x.z < y.z, x.w < y.w);
    }
    template<typename T>
    uint_4 equal(T x, T y) restrict(cpu, amp) {
      return uint_4(x.x == y.x, x.y == y.y, x.z == y.z, x.w == y.w);
    }
    template<typename T>
    typename short_vector<typename T::value_type, 4>::type
    get_by(T src, uint_4 idx) restrict(cpu, amp) {
      return typename short_vector<typename T::value_type, 4>::type(src[idx.x], src[idx.y], src[idx.z], src[idx.w]);
    }
    template<typename T>
    typename short_vector<T, 4>::type
    get_by(T * src, uint_4 idx) restrict(cpu, amp) {
      return typename short_vector<T, 4>::type(src[idx.x], src[idx.y], src[idx.z], src[idx.w]);
    }
    template<typename T>
    void inc_by(T src, uint_4 idx, uint_4 flag) restrict(cpu, amp) {
      src[idx.x] += flag.x;
      src[idx.y] += flag.y;
      src[idx.z] += flag.z;
      src[idx.w] += flag.w;
    }
    template<typename T, typename S>
    void set_by(T dest, uint_4 idx, S val) restrict(cpu, amp) {
      dest[idx.x] = val.x;
      dest[idx.y] = val.y;
      dest[idx.z] = val.z;
      dest[idx.w] = val.w;
    }
    template<typename T, typename S>
    void set_by(T dest, uint_4 idx, S val, uint_4 flag) restrict(cpu, amp) {
      if (flag.x) dest[idx.x] = val.x;
      if (flag.y) dest[idx.y] = val.y;
      if (flag.z) dest[idx.z] = val.z;
      if (flag.w) dest[idx.w] = val.w;
    }
    struct blocks_info { uint count; uint offset; };
    blocks_info get_blocks_info(const uint n, const uint wg_idx) restrict(cpu, amp) {
      const uint aligned = n + BLOCK_SIZE - (n % BLOCK_SIZE);
      const uint blocks = (n + BLOCK_SIZE - 1) / BLOCK_SIZE;
      const uint blocks_per_wg = (blocks + WG_COUNT - 1) / WG_COUNT;
      const int n_blocks = int(aligned / BLOCK_SIZE) - int(blocks_per_wg * wg_idx);
      return blocks_info {
        uint(clamp(n_blocks, 0, int(blocks_per_wg))),
        blocks_per_wg * BLOCK_SIZE * wg_idx
      };
    }
    uint prefix_sum(uint data, uint & total_sum, tiled_index<WG_SIZE> t_idx) restrict(amp) {
      tile_static uint local_sort[WG_SIZE * 2];
      const auto LC_IDX = t_idx.local[0];
      local_sort[LC_IDX] = 0;
      const auto lc_idx = LC_IDX + WG_SIZE;
      local_sort[lc_idx] = data;
      t_idx.barrier.wait();
      uint tmp;
      for (uint i = 1; i < WG_SIZE; i <<= 1) {
        tmp = local_sort[lc_idx - i];
        t_idx.barrier.wait();
        local_sort[lc_idx] += tmp;
        t_idx.barrier.wait();
      }
      total_sum = local_sort[WG_SIZE * 2 - 1];
      return local_sort[lc_idx - 1];
    }
    uint prefix_scan(uint_4 & v) restrict(cpu, amp) {
      uint sum = 0;
      uint tmp;
      tmp = v.x; v.x = sum; sum += tmp;
      tmp = v.y; v.y = sum; sum += tmp;
      tmp = v.z; v.z = sum; sum += tmp;
      tmp = v.w; v.w = sum; sum += tmp;
      return sum;
    }
    void sort_bits(uint_4 & sort, uint_4 & sort_val,
      tiled_index<WG_SIZE> t_idx, uint * local_sort, uint * local_sort_val,
      uint shift, bool descending, bool is_signed) restrict(amp) {
      auto signs = bfe_sign(sort, shift, BITS_PER_PASS);
      const auto LC_IDX = t_idx.local[0];
      const auto addr = 4 * LC_IDX + uint_4(0, 1, 2, 3);
      EACH(i_bit, BITS_PER_PASS) {
        const auto mask = (1 << i_bit);
        const auto cmp = equal((is_signed ? signs : (sort >> shift)) & mask, uint_4(descending != is_signed) * mask);
        auto key = cmp;
        uint total;
        key += prefix_sum(prefix_scan(key), total, t_idx);
        t_idx.barrier.wait();;

        const uint_4 dest_addr = mix(key, addr - key + total, cmp);
        set_by(local_sort, dest_addr, sort);
        set_by(local_sort_val, dest_addr, sort_val);
        t_idx.barrier.wait();;

        sort = get_by(local_sort, addr);
        sort_val = get_by(local_sort_val, addr);
        t_idx.barrier.wait();;

        if (is_signed) {
          set_by(local_sort, dest_addr, signs);
          t_idx.barrier.wait();;

          signs = get_by(local_sort, addr);
          t_idx.barrier.wait();;
        }
      }
    }
  }

  template<typename T>
  void radix_sort(concurrency::accelerator_view const & av,
    concurrency::array_view<uint32_t> key, concurrency::array_view<uint32_t> index,
    bool descending = false) {
    using namespace concurrency;
    using namespace concurrency::graphics;
    array<uint> histogram(WG_COUNT * RADICES, av);
    array<uint> key_out(key.extent, av);
    array<uint> index_out(index.extent, av);
    auto tile = extent<1>(WG_COUNT * WG_SIZE).tile<WG_SIZE>();
    auto prefix_tile = extent<1>(WG_SIZE).tile<WG_SIZE>();
    array_view<uint> data_key_in = key;
    array_view<uint> data_key_out = key_out;
    array_view<uint> data_index_in = index;
    array_view<uint> data_index_out = index_out;
    for (uint ib = 0; ib < 32; ib += 4) {
      auto shift = ib;
      auto is_signed = std::is_signed<T>::value && ib == 28;
      auto key_index = true;
      concurrency::parallel_for_each(av, tile,
        [=, &histogram](tiled_index<WG_SIZE> t_idx) restrict(amp) {
          tile_static uint local_histogram[WG_SIZE * RADICES];
          const auto LC_IDX = t_idx.local[0];
          const auto WG_IDX = t_idx.tile[0];
          EACH(i, RADICES) local_histogram[i * WG_SIZE + LC_IDX] = 0;
          t_idx.barrier.wait();
          const auto n = data_key_in.extent.size();
          const auto blocks = details::get_blocks_info(n, WG_IDX);
          auto addr = blocks.offset + 4 * LC_IDX + uint_4(0, 1, 2, 3);
          EACH(i_block, blocks.count) {
            const auto less_than = details::lessThan(addr, uint_4(n));
            const auto data_vec = details::get_by(data_key_in, addr);
            const auto k = is_signed
              ? details::bfe_sign(data_vec, shift, BITS_PER_PASS)
              : details::bfe(data_vec, shift, BITS_PER_PASS);
            const auto key = descending != is_signed ? (RADICES_MASK - k) : k;
            const auto local_key = key * WG_SIZE + LC_IDX;
            details::inc_by(local_histogram, local_key, less_than);
            addr += BLOCK_SIZE;
            if (LC_IDX < RADICES) {
              uint sum = 0; EACH(i, WG_SIZE) sum += local_histogram[LC_IDX * WG_SIZE + i];
              histogram[LC_IDX * WG_COUNT + WG_IDX] = sum;
            }
          }
        });
      concurrency::parallel_for_each(av, prefix_tile,
        [=, &histogram](tiled_index<WG_SIZE> t_idx) restrict(amp) {
          tile_static uint seed;
          const auto LC_IDX = t_idx.local[0];
          const auto WG_IDX = t_idx.tile[0];
          seed = 0;
          t_idx.barrier.wait();

          EACH(d, RADICES) {
            auto val = 0u;
            auto idx = d * WG_COUNT + LC_IDX;
            if (LC_IDX < WG_COUNT) val = histogram[idx];
            uint total;
            auto res = details::prefix_sum(val, total, t_idx);
            if (LC_IDX < WG_COUNT) histogram[idx] = res + seed;
            if (LC_IDX == WG_COUNT - 1) seed += res + val;
            t_idx.barrier.wait();
          }
        });
      concurrency::parallel_for_each(av, tile,
        [=, &histogram](tiled_index<WG_SIZE> t_idx) restrict(amp) {
          tile_static uint local_histogram_to_carry[RADICES];
          tile_static uint local_histogram[RADICES * 2];
          tile_static uint local_sort[BLOCK_SIZE];
          tile_static uint local_sort_val[BLOCK_SIZE];
          const auto LC_IDX = t_idx.local[0];
          const auto WG_IDX = t_idx.tile[0];
          const uint carry_idx = (descending && !is_signed ? (RADICES_MASK - LC_IDX) : LC_IDX);
          if (LC_IDX < RADICES) local_histogram_to_carry[LC_IDX] = histogram[carry_idx * WG_COUNT + WG_IDX];
          t_idx.barrier.wait();

          const uint def = (uint(!descending) * 0xffffffff) ^ (uint(is_signed) * 0x80000000);
          const uint n = data_key_in.extent.size();
          const auto blocks = details::get_blocks_info(n, WG_IDX);
          uint_4 addr = blocks.offset + 4 * LC_IDX + uint_4(0, 1, 2, 3);
          EACH(i_block, blocks.count) {
            const auto less_than = details::lessThan(addr, uint_4(n));
            const auto less_than_val = details::lessThan(addr, uint_4(key_index ? n : 0));
            const auto data_vec = details::get_by(data_key_in, addr);
            const auto data_val_vec = details::get_by(data_index_in, addr);
            auto sort = details::mix(data_vec, def, less_than);
            auto sort_val = details::mix(data_val_vec, 0, less_than_val);
            details::sort_bits(sort, sort_val, t_idx, local_sort, local_sort_val, shift, descending, is_signed);
            auto k = is_signed
              ? details::bfe_sign(sort, shift, BITS_PER_PASS)
              : details::bfe(sort, shift, BITS_PER_PASS);
            const auto key = (descending != is_signed) ? (RADICES_MASK - k) : k;
            const auto hist_key = key + RADICES;
            const auto local_key = key + (LC_IDX / RADICES) * RADICES;
            k = is_signed ? key : k;
            const auto offset = details::get_by(local_histogram_to_carry, k) + 4 * LC_IDX + uint_4(0, 1, 2, 3);
            local_sort[LC_IDX] = 0;
            t_idx.barrier.wait();

            details::inc_by(local_sort, local_key, less_than);
            t_idx.barrier.wait();

            const auto lc_idx = LC_IDX + RADICES;
            if (LC_IDX < RADICES) {
              local_histogram[LC_IDX] = 0;
              uint sum = 0; EACH(i, WG_SIZE / RADICES) sum += local_sort[i * RADICES + LC_IDX];
              local_histogram_to_carry[carry_idx] += local_histogram[lc_idx] = sum;
            }
            t_idx.barrier.wait();

            if (LC_IDX < RADICES)
              local_histogram[lc_idx] = local_histogram[lc_idx - 1];
            t_idx.barrier.wait();
            if (LC_IDX < RADICES)
              local_histogram[lc_idx] +=
                local_histogram[lc_idx - 3] +
                local_histogram[lc_idx - 2] +
                local_histogram[lc_idx - 1];
            t_idx.barrier.wait();
            if (LC_IDX < RADICES)
              local_histogram[lc_idx] +=
                local_histogram[lc_idx - 12] +
                local_histogram[lc_idx - 8] +
                local_histogram[lc_idx - 4];
            t_idx.barrier.wait();

            const auto out_key = offset - details::get_by(local_histogram, hist_key);
            details::set_by(data_key_out, out_key, sort, less_than);
            details::set_by(data_index_out, out_key, sort_val, less_than_val);
            t_idx.barrier.wait();
            addr += BLOCK_SIZE;
          }
        });
      std::swap(data_key_in, data_key_out);
      std::swap(data_index_in, data_index_out);
    }
  }

}
}
