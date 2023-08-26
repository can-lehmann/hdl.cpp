// Copyright (c) 2023 Can Joshua Lehmann

#include <iostream>

#include "../hdl.hpp"

int main() {
  hdl::Module module("top");
  
  hdl::Value* clock = module.input("clock", 1);
  hdl::Reg* counter = module.reg(module.constant(hdl::BitString("0000")), clock);
  
  counter->next = module.op(hdl::Op::Kind::Select, {
    module.op(hdl::Op::Kind::Eq, {
      counter, module.constant(hdl::BitString("1000"))
    }),
    counter,
    module.op(hdl::Op::Kind::Add, {
      counter
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
