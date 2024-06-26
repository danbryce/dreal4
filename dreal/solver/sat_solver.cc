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
#include "dreal/solver/sat_solver.h"

#include <fstream>
#include <ostream>
#include <sstream>
#include <utility>

#include "dreal/util/assert.h"
#include "dreal/util/exception.h"
#include "dreal/util/fopencookie.h"
#include "dreal/util/logging.h"
#include "dreal/util/stat.h"
#include "dreal/util/timer.h"

namespace dreal {

using std::cout;
using std::set;
using std::vector;

SatSolver::SatSolver(const Config& config)
    : sat_{picosat_init()}, compute_unsat_core_{config.unsat_core()} {
  if (compute_unsat_core_) {
    picosat_enable_trace_generation(sat_);
  }
  // Enable partial checks via picosat_deref_partial. See the call-site in
  // SatSolver::CheckSat().
  picosat_save_original_clauses(sat_);
  if (config.random_seed() != 0) {
    picosat_set_seed(sat_, config.random_seed());
    DREAL_LOG_DEBUG("SatSolver::Set Random Seed {}", config.random_seed());
  }
  picosat_set_global_default_phase(
      sat_, static_cast<int>(config.sat_default_phase()));
  DREAL_LOG_DEBUG("SatSolver::Set Default Phase {}",
                  config.sat_default_phase());
}

SatSolver::SatSolver(const Config& config, const vector<Formula>& clauses)
    : SatSolver{config} {
  AddClauses(clauses);
}

SatSolver::~SatSolver() { picosat_reset(sat_); }

void SatSolver::AddFormula(const Formula& f) {
  DREAL_LOG_DEBUG("SatSolver::AddFormula({})", f);
  vector<Formula> clauses{cnfizer_.Convert(f)};
  // Collect Tseitin variables.
  for (const auto& p : cnfizer_.map()) {
    tseitin_variables_.insert(p.first.get_id());
  }
  for (Formula& clause : clauses) {
    clause = predicate_abstractor_.Convert(clause);
  }
  AddClauses(clauses);
}

void SatSolver::AddFormulas(const vector<Formula>& formulas) {
  for (const Formula& f : formulas) {
    AddFormula(f);
  }
}

void SatSolver::AddLearnedClause(const set<Formula>& formulas) {
  for (const Formula& f : formulas) {
    AddLiteral(!predicate_abstractor_.Convert(f));
  }
  picosat_add(sat_, 0);
}

void SatSolver::AddClauses(const vector<Formula>& formulas) {
  for (const Formula& f : formulas) {
    AddClause(f);
  }
}

void SatSolver::AddClause(const Formula& f) {
  DREAL_LOG_DEBUG("SatSolver::AddClause({})", f);
  // Set up Variable ⇔ Literal (in SAT) map.
  for (const Variable& var : f.GetFreeVariables()) {
    MakeSatVar(var);
  }
  // Add clauses to SAT solver.
  DoAddClause(f);
}

namespace {
class SatSolverStat : public Stat {
 public:
  explicit SatSolverStat(const bool enabled) : Stat{enabled} {};
  SatSolverStat(const SatSolverStat&) = default;
  SatSolverStat(SatSolverStat&&) = default;
  SatSolverStat& operator=(const SatSolverStat&) = delete;
  SatSolverStat& operator=(SatSolverStat&&) = delete;
  ~SatSolverStat() override {
    if (enabled()) {
      using fmt::print;
      print(cout, "{:<45} @ {:<20} = {:>15}\n", "Total # of CheckSat",
            "SAT level", num_check_sat_);
      print(cout, "{:<45} @ {:<20} = {:>15f} sec\n",
            "Total time spent in SAT checks", "SAT level",
            timer_check_sat_.seconds());
    }
  }

  int num_check_sat_{0};
  Timer timer_check_sat_;
};
}  // namespace

optional<SatSolver::Model> SatSolver::CheckSat() {
  static SatSolverStat stat{DREAL_LOG_INFO_ENABLED};
  DREAL_LOG_DEBUG("SatSolver::CheckSat(#vars = {}, #clauses = {})",
                  picosat_variables(sat_),
                  picosat_added_original_clauses(sat_));
  stat.num_check_sat_++;
  // Call SAT solver.
  TimerGuard check_sat_timer_guard(&stat.timer_check_sat_,
                                   DREAL_LOG_INFO_ENABLED);
  const int ret{picosat_sat(sat_, -1 /* decision_limit == no limit */)};
  check_sat_timer_guard.pause();

  Model model;
  if (ret == PICOSAT_SATISFIABLE) {
    // SAT Case.
    const auto& var_to_formula_map = predicate_abstractor_.var_to_formula_map();
    for (int i = 1; i <= picosat_variables(sat_); ++i) {
      const int model_i{has_picosat_pop_used_ ? picosat_deref(sat_, i)
                                              : picosat_deref_partial(sat_, i)};
      if (model_i == 0) {
        continue;
      }
      const auto it_var = to_sym_var_.find(i);
      if (it_var == to_sym_var_.end()) {
        // There is no symbolic::Variable corresponding to this
        // picosat variable (int). This could be because of
        // picosat_push/pop.
        continue;
      }
      const Variable& var{it_var->second};
      const auto it = var_to_formula_map.find(var);
      if (it != var_to_formula_map.end()) {
        DREAL_LOG_TRACE("SatSolver::CheckSat: Add theory literal {}{} to Model",
                        model_i == 1 ? "" : "¬", var);
        auto& theory_model = model.second;
        theory_model.emplace_back(var, model_i == 1);
      } else if (tseitin_variables_.count(var.get_id()) == 0) {
        DREAL_LOG_TRACE(
            "SatSolver::CheckSat: Add Boolean literal {}{} to Model ",
            model_i == 1 ? "" : "¬", var);
        auto& boolean_model = model.first;
        boolean_model.emplace_back(var, model_i == 1);
      } else {
        DREAL_LOG_TRACE(
            "SatSolver::CheckSat: Skip {}{} which is a temporary variable.",
            model_i == 1 ? "" : "¬", var);
      }
    }
    DREAL_LOG_DEBUG("SatSolver::CheckSat() Found a model.");
    return model;
  } else if (ret == PICOSAT_UNSATISFIABLE) {
    DREAL_LOG_DEBUG("SatSolver::CheckSat() No solution.");
    if (compute_unsat_core_) {
      unsat_core_ = ExtractUnsatCore();
    }
    // UNSAT Case.
    return {};
  } else {
    DREAL_ASSERT(ret == PICOSAT_UNKNOWN);
    DREAL_LOG_CRITICAL("PICOSAT returns PICOSAT_UNKNOWN.");
    throw DREAL_RUNTIME_ERROR("PICOSAT returns PICOSAT_UNKNOWN.");
  }
}

void SatSolver::Pop() {
  DREAL_LOG_DEBUG("SatSolver::Pop()");
  tseitin_variables_.pop();
  to_sym_var_.pop();
  to_sat_var_.pop();
  picosat_pop(sat_);
  has_picosat_pop_used_ = true;
  // unsat_core_ = Formula::True();
}

void SatSolver::Push() {
  DREAL_LOG_DEBUG("SatSolver::Push()");
  picosat_push(sat_);

  // // Create a variable for the context variable sat_var
  // const int sat_var{picosat_push(sat_)};
  // std::ostringstream var_name;
  // var_name << "context_" << sat_var;
  // Variable var{var_name.str(), Variable::Type::BOOLEAN};

  to_sat_var_.push();
  // to_sat_var_.insert(var.get_id(), sat_var);
  to_sym_var_.push();
  // to_sym_var_.insert(sat_var, var);
  tseitin_variables_.push();
}

void SatSolver::AddLiteral(const Formula& f) {
  DREAL_ASSERT(is_variable(f) ||
               (is_negation(f) && is_variable(get_operand(f))));
  if (is_variable(f)) {
    // f = b
    const Variable& var{get_variable(f)};
    DREAL_ASSERT(var.get_type() == Variable::Type::BOOLEAN);
    // Add l = b
    picosat_add(sat_, to_sat_var_[var.get_id()]);
  } else {
    // f = ¬b
    DREAL_ASSERT(is_negation(f) && is_variable(get_operand(f)));
    const Variable& var{get_variable(get_operand(f))};
    DREAL_ASSERT(var.get_type() == Variable::Type::BOOLEAN);
    // Add l = ¬b
    picosat_add(sat_, -to_sat_var_[var.get_id()]);
  }
}

void SatSolver::DoAddClause(const Formula& f) {
  if (is_disjunction(f)) {
    // f = l₁ ∨ ... ∨ lₙ
    for (const Formula& l : get_operands(f)) {
      AddLiteral(l);
    }
  } else {
    // f = b or f = ¬b.
    AddLiteral(f);
  }
  picosat_add(sat_, 0);
}

void SatSolver::MakeSatVar(const Variable& var) {
  auto it = to_sat_var_.find(var.get_id());
  if (it != to_sat_var_.end()) {
    // Found.
    return;
  }
  // It's not in the maps, let's make one and add it.
  const int sat_var{picosat_inc_max_var(sat_)};
  to_sat_var_.insert(var.get_id(), sat_var);
  to_sym_var_.insert(sat_var, var);
  DREAL_LOG_DEBUG("SatSolver::MakeSatVar({} ↦ {})", var, sat_var);
}

const Formula SatSolver::ExtractUnsatCore() {
  DREAL_LOG_DEBUG("SatSolver::ExtractUnsatCore() Writing unsat core ...");

  // struct memfile_cookie mycookie;
  // std::stringstream buf;
  // mycookie.buf = &buf;
  // cookie_io_functions_t memfile_func;
  // create_stream_as_file_memfile_func(memfile_func);
  // FILE* core_file = create_cookie_stream(mycookie, memfile_func);

  // FILE* core_file;
  FILE* core_file = fopen("core.dimacs", "w");

  picosat_write_clausal_core(sat_, core_file);

  fclose(core_file);

  std::ifstream file("core.dimacs");

  Formula core = Formula::True();

  if (file) {
    // std::stringstream& buffer = mycookie.buf;
    // std::stringstream buffer{mycookie.buf->str()};
    // file.close();

    // // Read from the buffer
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    DREAL_LOG_DEBUG("SatSolver::ExtractUnsatCore() Read unsat core: {}",
                    buffer.str());

    string p;
    string cnf;
    int num_clauses;
    int num_vars;

    buffer >> p >> cnf >> num_vars >> num_clauses;

    DREAL_LOG_DEBUG(
        "SatSolver::ExtractUnsatCore() Reading {} clauses over {} variables",
        num_clauses, num_vars);
    std::set<Formula> clauses;
    const auto& var_to_formula_map = predicate_abstractor_.var_to_formula_map();
    for (int c = 0; c < num_clauses; c++) {
      std::set<Formula> clause_vars;
      int literal;
      do {
        buffer >> literal;
        if (literal != 0) {
          int picosat_var = literal;
          if (literal < 0) {
            picosat_var = -1 * picosat_var;
          }
          const auto it_var = to_sym_var_.find(picosat_var);
          bool is_context_var = false;
          if (it_var == to_sym_var_.end()) {
            // There is no symbolic::Variable corresponding to this
            // picosat variable (int). This could be because of
            // picosat_push/pop.
            is_context_var = true;
            // continue;
          }

          bool is_tseitin_var = false;
          if (!is_context_var) {
            const Variable& var{it_var->second};
            const auto it = var_to_formula_map.find(var);
            if (it != var_to_formula_map.end()) {
              is_tseitin_var = false;
            } else if (tseitin_variables_.count(var.get_id()) == 0) {
              is_tseitin_var = false;
            } else {
              is_tseitin_var = true;
            }
            DREAL_LOG_DEBUG(
                "SatSolver::ExtractUnsatCore() Extracted literal: {}, var: {}, "
                "tseitin: {}, context: {}",
                literal, var, is_tseitin_var, is_context_var);
          } else {
            DREAL_LOG_DEBUG(
                "SatSolver::ExtractUnsatCore() Extracted literal: {}, tseitin: "
                "{}, context: {}",
                literal, is_tseitin_var, is_context_var);
          }

          // bool is_context_var = var.get_name().find("context_") == 0;
          if (literal != picosat_var) {  // literal may be negative,
                                         // picosat_var is an atom
            // if is_context_var and negative, then literal is false and can be
            // ignored
            if (!is_context_var) {  // || !unit_propagate_context_vars) {
              const Variable& var{it_var->second};
              clause_vars.insert(!Formula(var));
            }

          } else {
            // if is_context_var and positive, then clause is true and can be
            // ignored
            if (is_context_var) {
              clause_vars.erase(clause_vars.begin(), clause_vars.end());
              break;
            } else {
              const Variable& var{it_var->second};
              clause_vars.insert(Formula(var));
            }
          }
        }
      } while (literal != 0);

      //  only keep non empty clauses
      if (clause_vars.size() > 0) {
        Formula clause = make_disjunction(clause_vars);

        clauses.insert(clause);

        DREAL_LOG_DEBUG("SatSolver::ExtractUnsatCore() Extracted clause {}: {}",
                        c, clause);
      } else {
        DREAL_LOG_DEBUG("SatSolver::ExtractUnsatCore() Skipping clause {} ...",
                        c);
      }
    }
    core = make_conjunction(clauses);
  }

  return core;
}

}  // namespace dreal
