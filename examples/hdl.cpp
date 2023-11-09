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

#include "../hdl.hpp"

int main() {
  hdl::Module module("top");
  
  hdl::Value* clock = module.input("clock", 1);
  hdl::Reg* counter = module.reg(hdl::BitString("0000"), clock);
  
  counter->next = module.op(hdl::Op::Kind::Select, {
    module.op(hdl::Op::Kind::Eq, {
      counter, module.constant(hdl::BitString("1000"))
    }),
    counter,
    module.op(hdl::Op::Kind::Add, {
      counter,
      module.constant(hdl::BitString("0001"))
    })
  });
  
  
  module.output("counter", counter);
  
  hdl::verilog::Printer printer(module);
  printer.print(std::cout);
  
  hdl::graphviz::Printer gv_printer(module);
  gv_printer.save("graph.gv");
  
  {
    hdl::sim::Simulation simulation(module);
    bool clock = false;
    for (size_t step = 0; step < 32; step++) {
      simulation.update({hdl::BitString::from_bool(clock)});
      std::cout << simulation.outputs()[0] << std::endl;
      clock = !clock;
    }
  }
  
  return 0;
}
