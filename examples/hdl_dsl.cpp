// Copyright (c) 2023 Can Joshua Lehmann

#include "../hdl_dsl.hpp"

int main() {
  hdl::Module module("top");
  
  hdl::dsl::synth(module, [&](){
    using namespace hdl::dsl;
    
    Input<U<1>> clock("clock");
    Reg<U<32>> reg;
    
    on(clock, [&](){
      reg = reg + 1;
    });
  });
  
  return 0;
}
