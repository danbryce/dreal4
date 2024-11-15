/*
   Copyright 2023 Smart Information Flow Technologies, LLC.

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
#pragma once

#include <random>
#include <vector>

#include "dreal/contractor/contractor.h"
#include "dreal/contractor/contractor_status.h"
#include "dreal/solver/config.h"
#include "dreal/solver/formula_evaluator.h"
#include "dreal/solver/icp.h"
#include "dreal/solver/icp_seq.h"
#include "dreal/solver/icp_stat.h"

namespace dreal {

class MctsNode {
 public:
  MctsNode() = delete;
  ~MctsNode();
  explicit MctsNode(Box box);
  explicit MctsNode(Box box, MctsNode* parent);

  /// Returns a const reference of the embedded box.
  const Box& box() const;

  const Box& delta_sat_box() const;

  void evaluate(TimerGuard& eval_timer_guard,
                // const Contractor& contractor,
                const std::vector<FormulaEvaluator>& formula_evaluators,
                ContractorStatus* const cs, const Config& config
                // ,
                // IcpStat& stat
  );

  const std::vector<MctsNode*>& children() const;
  const MctsNode* parent() const;
  double visited() const;
  void increment_visited();
  double wins() const;
  void increment_wins();
  double value();
  int index() const;

  bool expand(const std::vector<FormulaEvaluator>& formula_evaluators,
              ContractorStatus* const cs, const Contractor& contractor,
              TimerGuard& branch_timer_guard, TimerGuard& eval_timer_guard,
              TimerGuard& prune_timer_guard, const Config& config,
              IcpStat& stat, double preferred_threshold);
  double simulate_box(Box& sim_box,
                      const std::vector<FormulaEvaluator>& formula_evaluators,
                      ContractorStatus* const cs, const Contractor& contractor,
                      TimerGuard& eval_timer_guard,
                      TimerGuard& prune_timer_guard, const Config& config,
                      IcpStat& stat, std::default_random_engine& rnd,
                      double preferred_precision);
  double simulate(const std::vector<FormulaEvaluator>& formula_evaluators,
                  ContractorStatus* const cs, const Contractor& contractor,
                  TimerGuard& eval_timer_guard, TimerGuard& prune_timer_guard,
                  const Config& config, IcpStat& stat,
                  std::default_random_engine& rnd, double preferred_precision);
  MctsNode* select();
  void backpropagate(double wins);
  double preferred_width_ratio(const Config& config);

  bool unsat();
  bool delta_sat();
  bool sat();
  bool terminal();

 private:
  Box box_;
  Box delta_sat_box_;
  MctsNode* parent_;
  std::vector<MctsNode*> children_;
  double visited_{0};
  double wins_{0};
  bool unsat_{false};
  bool delta_sat_{false};
  bool sat_{false};
  bool terminal_{false};
  double value_{0.0};
  optional<DynamicBitset> evaluation_result_;
  int index_;
};

/// Class for ICP (Interval Constraint Propagation) algorithm.
class IcpMcts : public IcpSeq {
 public:
  /// Constructs an IcpMcts based on @p config.
  explicit IcpMcts(const Config& config);

  bool CheckSat(const Contractor& contractor,
                const std::vector<FormulaEvaluator>& formula_evaluators,
                ContractorStatus* cs) override;

 private:
  double MctsBP(MctsNode* node,
                const std::vector<FormulaEvaluator>& formula_evaluators,
                ContractorStatus* const cs, const Contractor& contractor,
                const Contractor& heuristic_contractor,
                TimerGuard& branch_timer_guard, TimerGuard& eval_timer_guard,
                TimerGuard& prune_timer_guard, IcpStat& stat,
                std::default_random_engine& rnd, double preferred_precision);

  // Generate a lower-cost contractor to use in simulation
  optional<Contractor> make_heuristic_contractor(const Contractor& contractor);

  // If `stack_left_box_first_` is true, we add the left box from the
  // branching operation to the `stack`. Otherwise, we add the right
  // box first.
  bool stack_left_box_first_{false};
};

}  // namespace dreal
