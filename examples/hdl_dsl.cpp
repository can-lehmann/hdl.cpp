// Copyright (c) 2023 Can Joshua Lehmann

#include <iostream>
#include "../hdl_dsl.hpp"

int main() {
  hdl::Module module("top");
  
  hdl::dsl::synth(module, [&](){
    using namespace hdl::dsl;
    
    Input<Bool> clock("clock");
    Reg<U<32>> iter;
    Reg<U<32>> out;
    Mem<U<32>, 10> mem;
    
    on(clock, ${
      when(iter < 9, ${
        iter = iter + 1;
      }, ${
        iter = 0;
      });
      
      mem.write(iter, iter << 1);
      out = mem[iter];
    });
    
    
    module.output("out", out.value());
  });
  
  hdl::sim::Simulation sim(module);
  bool clock = false;
  for (size_t timestep = 0; timestep < 100; timestep++) {
    sim.update({hdl::BitString::from_bool(clock)});
    for (size_t it = 0; it < module.outputs().size(); it++) {
      std::cout << module.outputs()[it].name << " = " << sim.outputs()[it] << std::endl;
    }
    clock = !clock;
  }
  
  return 0;
}
