/*
   Copyright 2017 Toyota Research Institute

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "dreal/solver/brancher.h"

#include "dreal/util/assert.h"
#include "dreal/util/logging.h"

namespace dreal {

using std::make_pair;
using std::pair;

pair<double, int> FindMaxDiam(const Box& box, const DynamicBitset& active_set) {
  DREAL_ASSERT(!active_set.none());
  double max_diam{0.0};
  int max_diam_idx{-1};
  DynamicBitset::size_type idx = active_set.find_first();
  while (idx != DynamicBitset::npos) {
    const Box::Interval& iv_i{box[idx]};
    const double diam_i{iv_i.diam()};
    if (diam_i > max_diam && iv_i.is_bisectable()) {
      max_diam = diam_i;
      max_diam_idx = idx;
    }
    idx = active_set.find_next(idx);
  }
  return make_pair(max_diam, max_diam_idx);
}

pair<double, int> FindPreferredDiam(const Box& box,
                                    const DynamicBitset& active_set,
                                    const unordered_set<std::string> preferred,
                                    double preferred_threshold) {
  DREAL_ASSERT(!active_set.none());
  double max_diam{std::numeric_limits<double>::min()};
  int max_diam_idx{-1};
  bool is_preferred = false;
  DynamicBitset::size_type idx = active_set.find_first();
  while (idx != DynamicBitset::npos) {
    const Box::Interval& iv_i{box[idx]};
    const double diam_i{iv_i.diam()};
    bool is_i_preferred =
        preferred.find(box.variable(idx).get_name()) != preferred.end();

    // Prefer to split pref. vars of max_diam.  Need to check pref.
    // against the preferred_threshold because it may have converged
    bool can_split = (((is_i_preferred && diam_i > preferred_threshold) ||
                       (!is_i_preferred && diam_i > max_diam)) &&
                      iv_i.is_bisectable());

    // If haven't found a suitable variable yet, anything works
    if (max_diam_idx == -1 && can_split) {
      is_preferred = is_i_preferred;
      max_diam = diam_i;
      max_diam_idx = idx;
    }
    // variable is preferred and either first preferred or has greater width
    else if (is_i_preferred && can_split &&
             (!is_preferred ||
              (isinf(diam_i) && isinf(max_diam) &&
               static_cast<int>(idx) < max_diam_idx) ||
              (!isinf(diam_i) && diam_i > max_diam))) {
      is_preferred = is_i_preferred;
      max_diam = diam_i;
      max_diam_idx = idx;

    }
    // variable is not preferred and has greater width
    else if (!is_preferred && can_split && diam_i > max_diam) {
      is_preferred = is_i_preferred;
      max_diam = diam_i;
      max_diam_idx = idx;
    }

    idx = active_set.find_next(idx);
  }
  return make_pair(max_diam, max_diam_idx);
}

pair<double, int> FindMinDiam(const Box& box, const DynamicBitset& active_set) {
  DREAL_ASSERT(!active_set.none());
  double min_diam{INFINITY};
  int min_diam_idx{-1};
  DynamicBitset::size_type idx = active_set.find_first();
  while (idx != DynamicBitset::npos) {
    const Box::Interval& iv_i{box[idx]};
    const double diam_i{iv_i.diam()};
    if (diam_i < min_diam && iv_i.is_bisectable() && diam_i >= 1e-10) {
      min_diam = diam_i;
      min_diam_idx = idx;
    }
    idx = active_set.find_next(idx);
  }
  return make_pair(min_diam, min_diam_idx);
}

int BranchLargestFirst(const Box& box, const DynamicBitset& active_set,
                       Box* const left, Box* const right) {
  DREAL_ASSERT(!active_set.none());

  const pair<double, int> max_diam_and_idx{FindMaxDiam(box, active_set)};
  // const pair<double, int> max_diam_and_idx{FindMinDiam(box, active_set)};
  const int branching_dim{max_diam_and_idx.second};
  if (branching_dim >= 0) {
    pair<Box, Box> bisected_boxes{box.bisect(branching_dim)};
    *left = std::move(bisected_boxes.first);
    *right = std::move(bisected_boxes.second);
    DREAL_LOG_DEBUG(
        "Branch {} [{}]\n"
        "on {}\n"
        "Box1=\n{}\n"
        "Box2=\n{}",
        box.variable(branching_dim), max_diam_and_idx.first, box, *left,
        *right);
    return branching_dim;
  }
  return -1;
}

int BranchPreferredFirst(const Box& box, const DynamicBitset& active_set,
                         const unordered_set<std::string> preferred,
                         double preferred_threshold, Box* const left,
                         Box* const right) {
  DREAL_ASSERT(!active_set.none());

  const pair<double, int> max_diam_and_idx{
      FindPreferredDiam(box, active_set, preferred, preferred_threshold)};
  // const pair<double, int> max_diam_and_idx{FindMinDiam(box, active_set)};
  const int branching_dim{max_diam_and_idx.second};
  if (branching_dim >= 0) {
    pair<Box, Box> bisected_boxes{box.bisect(branching_dim)};
    *left = std::move(bisected_boxes.first);
    *right = std::move(bisected_boxes.second);
    DREAL_LOG_DEBUG(
        "Branch {} [{}]\n"
        "on {}\n"
        "Box1=\n{}\n"
        "Box2=\n{}",
        box.variable(branching_dim), max_diam_and_idx.first, box, *left,
        *right);
    return branching_dim;
  }
  return -1;
}
}  // namespace dreal
