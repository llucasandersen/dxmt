// SPDX-License-Identifier: MIT
#pragma once

#include "util_win32_compat.h"
#include "winemetal.h"
#include "log/log.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace dxmt {

// The ordinary lifetime policy is preserved when headroom is healthy or the
// unix backend cannot report it. Under pressure keep a small working reserve;
// this is a cache limit, never permission to release unfinished GPU work.
inline size_t ringRetainedBlocks(int64_t headroom_mb) {
  if (headroom_mb < 0 || headroom_mb >= 1536)
    return std::numeric_limits<size_t>::max();
  return headroom_mb < 512 ? 1 : 2;
}

inline size_t queryRingRetainedBlocks() {
  static std::atomic<uint64_t> next_sample_ms{0};
  static std::atomic<size_t> retained{std::numeric_limits<size_t>::max()};
  const auto now = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
  auto next = next_sample_ms.load(std::memory_order_relaxed);
  if (now >= next && next_sample_ms.compare_exchange_strong(next, now + 250, std::memory_order_relaxed)) {
    struct madeira_ctl_args a = {};
    a.op = 7;
    MadeiraCtl(&a);
    const int64_t headroom = a.ret == 1 ? int64_t(a.len >> 20) : -1;
    const size_t keep = ringRetainedBlocks(headroom);
    const size_t previous = retained.exchange(keep, std::memory_order_relaxed);
    if (previous != keep)
      Logger::info("[ring-pressure] headroomMB=" + std::to_string(headroom) +
                   " retainBlocks=" + std::to_string(keep == std::numeric_limits<size_t>::max() ? 0 : keep) +
                   " (0=ordinary lifetime)");
  }
  return retained.load(std::memory_order_relaxed);
}

} // namespace dxmt
