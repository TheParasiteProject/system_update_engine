//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_EXTENT_UTILS_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_EXTENT_UTILS_H_

#include <limits>
#include <string>
#include <vector>

#include <base/logging.h>

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/payload_constants.h"
#include "update_engine/update_metadata.pb.h"

// Utility functions for manipulating Extents and lists of blocks.

namespace chromeos_update_engine {
struct ExtentLess {
  constexpr bool operator()(const Extent& x, const Extent& y) const {
    if (x.start_block() == y.start_block()) {
      return x.num_blocks() < y.num_blocks();
    }
    return x.start_block() < y.start_block();
  }
};

// |block| must either be the next block in the last extent or a block
// in the next extent. This function will not handle inserting block
// into an arbitrary place in the extents.
void AppendBlockToExtents(std::vector<Extent>* extents, uint64_t block);

// Takes a collection (vector or RepeatedPtrField) of Extent and
// returns a vector of the blocks referenced, in order.
template <typename T>
std::vector<uint64_t> ExpandExtents(const T& extents) {
  std::vector<uint64_t> ret;
  for (const auto& extent : extents) {
    if (extent.start_block() == kSparseHole) {
      ret.resize(ret.size() + extent.num_blocks(), kSparseHole);
    } else {
      for (uint64_t block = extent.start_block();
           block < (extent.start_block() + extent.num_blocks());
           block++) {
        ret.push_back(block);
      }
    }
  }
  return ret;
}

// Stores all Extents in 'extents' into 'out'.
void StoreExtents(const std::vector<Extent>& extents,
                  google::protobuf::RepeatedPtrField<Extent>* out);

// Stores all extents in |extents| into |out_vector|.
void ExtentsToVector(const google::protobuf::RepeatedPtrField<Extent>& extents,
                     std::vector<Extent>* out_vector);

// Returns a string representing all extents in |extents|.
std::string ExtentsToString(const std::vector<Extent>& extents);
std::string ExtentsToString(
    const google::protobuf::RepeatedPtrField<Extent>& extents);

// Takes a pointer to extents |extents| and extents |extents_to_add|, and
// merges them by adding |extents_to_add| to |extents| and normalizing.
void ExtendExtents(
    google::protobuf::RepeatedPtrField<Extent>* extents,
    const google::protobuf::RepeatedPtrField<Extent>& extents_to_add);

// Takes a vector of extents and normalizes those extents. Expects the extents
// to be sorted by start block. E.g. if |extents| is [(1, 2), (3, 5), (10, 2)]
// then |extents| will be changed to [(1, 7), (10, 2)].
void NormalizeExtents(std::vector<Extent>* extents);

// Return a subsequence of the list of blocks passed. Both the passed list of
// blocks |extents| and the return value are expressed as a list of Extent, not
// blocks. The returned list skips the first |block_offset| blocks from the
// |extents| and cotains |block_count| blocks (or less if |extents| is shorter).
std::vector<Extent> ExtentsSublist(const std::vector<Extent>& extents,
                                   uint64_t block_offset,
                                   uint64_t block_count);

bool operator==(const Extent& a, const Extent& b) noexcept;

bool operator!=(const Extent& a, const Extent& b) noexcept;

// TODO(zhangkelvin) This is ugly. Rewrite using C++20's coroutine once
// that's available. Unfortunately with C++17 this is the best I could do.

// An iterator that takes a sequence of extents, and iterate over blocks
// inside this sequence of extents.
// Example usage:

// BlockIterator it1{src_extents};
// while(!it1.is_end()) {
//    auto block = *it1;
//    Do stuff with |block|
// }
struct BlockIterator {
  explicit BlockIterator(
      const google::protobuf::RepeatedPtrField<Extent>& extents)
      : extents_(extents) {}

  BlockIterator& operator++() {
    CHECK_LT(cur_extent_, extents_.size());
    block_offset_++;
    if (block_offset_ >= extents_[cur_extent_].num_blocks()) {
      cur_extent_++;
      block_offset_ = 0;
    }
    return *this;
  }

  [[nodiscard]] bool is_end() { return cur_extent_ >= extents_.size(); }
  [[nodiscard]] uint64_t operator*() {
    return extents_[cur_extent_].start_block() + block_offset_;
  }

  const google::protobuf::RepeatedPtrField<Extent>& extents_;
  int cur_extent_ = 0;
  size_t block_offset_ = 0;
};

std::ostream& operator<<(std::ostream& out, const Extent& extent);
std::ostream& operator<<(std::ostream& out, const std::vector<Extent>& extent);
std::ostream& operator<<(std::ostream& out, const std::set<Extent>& extents);

std::ostream& operator<<(std::ostream& out,
                         const std::set<Extent, ExtentLess>& extents);
std::ostream& operator<<(
    std::ostream& out,
    Range<std::set<Extent, ExtentLess>::const_iterator> range);

std::ostream& operator<<(
    std::ostream& out,
    const google::protobuf::RepeatedPtrField<Extent>& extent);

template <typename Container>
size_t GetNthBlock(const Container& extents, const size_t n) {
  size_t cur_block_count = 0;
  for (const auto& extent : extents) {
    if (n - cur_block_count < extent.num_blocks()) {
      return extent.start_block() + (n - cur_block_count);
    }
    cur_block_count += extent.num_blocks();
  }
  return std::numeric_limits<size_t>::max();
}

constexpr bool ExtentContains(const Extent& extent, size_t block) {
  return extent.start_block() <= block &&
         block < extent.start_block() + extent.num_blocks();
}

// return true iff |big| extent contains |small| extent
constexpr bool ExtentContains(const Extent& big, const Extent& small) {
  return big.start_block() <= small.start_block() &&
         small.start_block() + small.num_blocks() <=
             big.start_block() + big.num_blocks();
}

template <typename T>
constexpr void Dedup(T* container) {
  std::sort(container->begin(), container->end());
  container->erase(std::unique(container->begin(), container->end()),
                   container->end());
}

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_EXTENT_UTILS_H_
