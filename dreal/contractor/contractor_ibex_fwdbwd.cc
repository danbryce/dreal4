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
#include "dreal/contractor/contractor_ibex_fwdbwd.h"

#include <sstream>
#include <utility>

#include "dreal/util/assert.h"
#include "dreal/util/ibex_converter.h"
#include "dreal/util/logging.h"
#include "dreal/util/math.h"
#include "dreal/util/stat.h"
#include "dreal/util/timer.h"

using std::cout;
using std::make_unique;
using std::ostream;
using std::ostringstream;

namespace dreal {

namespace {
class ContractorIbexFwdbwdStat : public Stat {
 public:
  explicit ContractorIbexFwdbwdStat(const bool enabled) : Stat{enabled} {};
  ContractorIbexFwdbwdStat(const ContractorIbexFwdbwdStat&) = delete;
  ContractorIbexFwdbwdStat(ContractorIbexFwdbwdStat&&) = delete;
  ContractorIbexFwdbwdStat& operator=(const ContractorIbexFwdbwdStat&) = delete;
  ContractorIbexFwdbwdStat& operator=(ContractorIbexFwdbwdStat&&) = delete;
  ~ContractorIbexFwdbwdStat() override {
    if (enabled()) {
      using fmt::print;
      print(cout, "{:<45} @ {:<20} = {:>15}\n",
            "Total # of ibex-fwdbwd Pruning", "Pruning level", num_pruning_);
      print(cout, "{:<45} @ {:<20} = {:>15}\n",
            "Total # of ibex-fwdbwd Pruning (zero-effect)", "Pruning level",
            num_zero_effect_pruning_);
      if (num_pruning_) {
        print(cout, "{:<45} @ {:<20} = {:>15f} sec\n",
              "Total time spent in Pruning", "Pruning level",
              timer_pruning_.seconds());
      }
      print(cout, "{:<45} @ {:<20} = {:>15f} sec\n",
            "Total time spent in making constraints", "Pruning level",
            timer_make_constraint_.seconds());
    }
  }

  int num_zero_effect_pruning_{0};
  int num_pruning_{0};

  Timer timer_pruning_;
  Timer timer_make_constraint_;
};
}  // namespace

//---------------------------------------
// Implementation of ContractorIbexFwdbwd
//---------------------------------------
ContractorIbexFwdbwd::ContractorIbexFwdbwd(Formula f, const Box& box,
                                           const Config& config)
    : ContractorCell{Contractor::Kind::IBEX_FWDBWD, DynamicBitset(box.size()),
                     config},
      f_{std::move(f)},
      ibex_converter_{box} {
  static ContractorIbexFwdbwdStat stat{DREAL_LOG_INFO_ENABLED};
  TimerGuard timer_guard(&stat.timer_make_constraint_, stat.enabled(), true);
  timer_guard.resume();
  // Build num_ctr and ctc_.
  expr_ctr_.reset(ibex_converter_.Convert(f_));
  if (expr_ctr_) {
    num_ctr_ = make_unique<ibex::NumConstraint>(ibex_converter_.variables(),
                                                *expr_ctr_);
    // Build input.
    DynamicBitset& input{mutable_input()};
    for (const Variable& var : f_.GetFreeVariables()) {
      input.set(box.index(var));
    }
  } else {
    is_dummy_ = true;
  }
  timer_guard.pause();
}

void ContractorIbexFwdbwd::Prune(ContractorStatus* cs) const {
  thread_local ContractorIbexFwdbwdStat stat{DREAL_LOG_INFO_ENABLED};
  DREAL_ASSERT(!is_dummy_ && num_ctr_);

  Box::IntervalVector& iv{cs->mutable_box().mutable_interval_vector()};
  DREAL_LOG_TRACE("ContractorIbexFwdbwd::Prune");
  DREAL_LOG_TRACE("CTC = {}", *num_ctr_);
  DREAL_LOG_TRACE("F = {}", f_);
  const Box::IntervalVector old_iv{iv};
  stat.timer_pruning_.resume();
  const bool is_inner{num_ctr_->f.backward(num_ctr_->right_hand_side(),
                                           iv)};  // true if unchanged.
  stat.timer_pruning_.pause();
  if (stat.enabled()) {
    stat.num_pruning_++;
  }
  bool changed{false};
  // Update output.
  if (!is_inner) {
    if (iv.is_empty()) {
      changed = true;
      cs->mutable_output().set();
    } else {
      DynamicBitset::size_type i_bit = input().find_first();
      while (i_bit != DynamicBitset::npos) {
        if (old_iv[i_bit] != iv[i_bit]) {
          cs->mutable_output().set(i_bit);
          changed = true;
        }
        i_bit = input().find_next(i_bit);
      }
    }
  }
  // Update used constraints.
  if (changed) {
    cs->AddUsedConstraint(f_);
    if (stat.enabled()) {
      ostringstream oss;
      DisplayDiff(oss, cs->box().variables(), old_iv,
                  cs->box().interval_vector());
      DREAL_LOG_TRACE("Changed\n{}", oss.str());
    }
  } else {
    if (stat.enabled()) {
      stat.num_zero_effect_pruning_++;
    }
    DREAL_LOG_TRACE("NO CHANGE");
  }
}

ostream& ContractorIbexFwdbwd::display(ostream& os) const {
  DREAL_ASSERT(num_ctr_);
  return os << "IbexFwdbwd(" << *num_ctr_ << ")";
}

bool ContractorIbexFwdbwd::is_dummy() const { return is_dummy_; }

}  // namespace dreal
