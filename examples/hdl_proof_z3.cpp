// Copyright (c) 2023 Can Joshua Lehmann

#include <iostream>
#include "../hdl_proof_z3.hpp"

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
  
  hdl::proof::z3::Builder builder;
  builder.free(a);
  builder.free(b);
  builder.require(eq, hdl::BitString::from_bool(false));
  
  std::cout << builder.to_smt2() << std::endl;
  
  if (builder.satisfiable()) {
    std::cout << "Counter-Example" << std::endl;
    std::cout << "a = " << builder.interp(a) << std::endl;
    std::cout << "b = " << builder.interp(b) << std::endl;
    std::cout << "eq = " << builder.interp(eq) << std::endl;
  } else {
    std::cout << "Proven" << std::endl;
  }
  
  return 0;
}
