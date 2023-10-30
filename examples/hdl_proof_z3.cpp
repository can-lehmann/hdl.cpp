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
  
  z3::context context;
  z3::solver solver(context);
  hdl::proof::z3::Builder builder(context);
  builder.free(a);
  builder.free(b);
  builder.require(solver, eq, hdl::BitString::from_bool(false));
  
  std::cout << solver.to_smt2() << std::endl;
  
  if (solver.check() == z3::sat) {
    std::cout << "Counterexample" << std::endl;
    std::cout << "a = " << builder.interp(solver, a) << std::endl;
    std::cout << "b = " << builder.interp(solver, b) << std::endl;
    std::cout << "eq = " << builder.interp(solver, eq) << std::endl;
  } else {
    std::cout << "Proven" << std::endl;
  }
  
  return 0;
}
