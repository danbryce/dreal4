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
#include "dreal/contractor/contractor.h"

#include <type_traits>

#include <gtest/gtest.h>

namespace dreal {
namespace {

GTEST_TEST(ContractorTest, Test) {
  // TODO(soonho): Add more tests.
}

GTEST_TEST(ContractorTest, IsNothrowMoveConstructible) {
  static_assert(std::is_nothrow_move_constructible<Contractor>::value,
                "Contractor should be nothrow_move_constructible.");
}

}  // namespace
}  // namespace dreal
