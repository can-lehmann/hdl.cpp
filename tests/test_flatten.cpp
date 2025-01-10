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

#include <inttypes.h>
#include <sstream>

#include "../../unittest.cpp/unittest.hpp"
#include "../hdl.hpp"
#include "../hdl_flatten.hpp"

using Test = unittest::Test;
#define assert(cond) unittest_assert(cond)

void test_op(hdl::Op::Kind kind,
             const std::vector<std::vector<size_t>>& arg_width_cases) {
  std::ostringstream name;
  name << "Op::Kind::" << kind;
  Test(name.str()).run([&](){
    for (const std::vector<size_t>& arg_widths : arg_width_cases) {
      hdl::Module module("top");
      hdl::flatten::Flattening flattening(module);
      
      std::vector<hdl::Value*> args;
      std::vector<std::vector<hdl::Value*>> args_bits;
      size_t states = 1;
      for (size_t width : arg_widths) {
        hdl::Value* arg = module.input("", width);
        args.push_back(arg);
        flattening.define(arg, flattening.split(arg));
        states *= 1 << width;
      }
      
      hdl::Value* op = module.op(kind, args);
      flattening.flatten(op);
      
      module.output("expected", op);
      module.output("result", flattening.join(flattening[op]));
      
      std::ostringstream path;
      path << "tests/graphs/flatten_" << kind << ".gv";
      hdl::graphviz::Printer printer(module);
      printer.save(path.str().c_str());
      
      hdl::sim::Simulation sim(module);
      
      for (size_t state = 0; state < states; state++) {
        std::vector<hdl::BitString> inputs;
        size_t cur = state;
        for (size_t width : arg_widths) {
          inputs.push_back(hdl::BitString::from_uint(cur & ((1 << width) - 1)).truncate(width));
          cur >>= width;
        }
        
        sim.update(inputs);
        
        if (sim.outputs()[0] != sim.outputs()[1]) {
          std::cout << "inputs = ";
          for (const hdl::BitString& input : inputs) {
            std::cout << ' ' << input;
          }
          std::cout << std::endl;
          std::cout << "expected = " << sim.outputs()[0] << std::endl;
          std::cout << "result = " << sim.outputs()[1] << std::endl;
        }
        
        assert(sim.outputs()[0] == sim.outputs()[1]);
      }
    }
  });
}

int main() {
  test_op(hdl::Op::Kind::And, {{2, 2}});
  test_op(hdl::Op::Kind::Or, {{2, 2}});
  test_op(hdl::Op::Kind::Xor, {{2, 2}});
  test_op(hdl::Op::Kind::Not, {{2}});
  test_op(hdl::Op::Kind::Add, {{4, 4}});
  test_op(hdl::Op::Kind::Sub, {{4, 4}});
  test_op(hdl::Op::Kind::Mul, {{4, 4}, {2, 3}});
  test_op(hdl::Op::Kind::Eq, {{4, 4}});
  test_op(hdl::Op::Kind::LtU, {{3, 3}, {4, 4}});
  test_op(hdl::Op::Kind::LtS, {{3, 3}, {4, 4}});
  test_op(hdl::Op::Kind::Concat, {{3, 2}});
  // TODO: Slice
  test_op(hdl::Op::Kind::Shl, {{4, 2}});
  test_op(hdl::Op::Kind::ShrU, {{4, 2}, {5, 2}, {3, 2}});
  test_op(hdl::Op::Kind::ShrS, {{4, 2}, {5, 2}, {3, 2}});
  test_op(hdl::Op::Kind::Select, {{1, 3, 3}});
  
  return 0;
}
