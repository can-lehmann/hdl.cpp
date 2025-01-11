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
#include "../hdl_known_bits.hpp"

int main() {
  hdl::Module module("top");
  
  
  hdl::Value* cond = module.input("cond", 1);
  hdl::Value* a = module.input("a", 32);
  hdl::Value* b = module.input("b", 32);
  
  module.output("c", module.op(hdl::Op::Kind::Select, {
    cond,
    module.op(hdl::Op::Kind::And, {a, b}),
    module.op(hdl::Op::Kind::Add, {a, b})
  }));
  
  using KnownBits = hdl::known_bits::KnownBits;
  
  hdl::Module partial_module("top");
  
  KnownBits known_bits(partial_module);
  known_bits.lower(module);
  
  hdl::graphviz::Printer gv_printer(partial_module);
  gv_printer.save("known_bits.gv");
}

