// Copyright 2023 Can Joshua Lehmann
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include "../hdl_proof.hpp"

int main() {
  hdl::Module module("top");
  hdl::Value* a = module.input("a", 32);
  hdl::Value* b = module.input("b", 32);
  hdl::Value* eq = module.op(hdl::Op::Kind::Eq, {
    module.op(hdl::Op::Kind::Add, {a, b}),
    module.op(hdl::Op::Kind::Sub, {
      a,
      module.op(hdl::Op::Kind::Add, {
        module.op(hdl::Op::Kind::Not, {b}),
        module.constant(hdl::BitString("00000000000000000000000000000001"))
      })
    })
  });
  
  hdl::proof::CnfBuilder builder;
  builder.free(a);
  builder.free(b);
  builder.require(eq, hdl::BitString::from_bool(false));
  hdl::proof::Cnf cnf = builder.cnf();
  std::cout << "CNF: " << cnf.size() << std::endl;
  hdl::proof::Cnf simplified = cnf.simplify();
  std::cout << "Simplified: " << simplified.size() << std::endl;
  simplified.save("proof.cnf");
  
  return 0;
}
