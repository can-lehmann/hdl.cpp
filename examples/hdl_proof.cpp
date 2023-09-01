// Copyright (c) 2023 Can Joshua Lehmann

#include <iostream>
#include "../hdl_proof.hpp"

int main() {
  hdl::Module module("top");
  hdl::Value* a = module.input("a", 32);
  hdl::Value* b = module.input("b", 32);
  hdl::Value* sum = module.op(hdl::Op::Kind::Add, {a, b});
  hdl::Value* eq = module.op(hdl::Op::Kind::Eq, {
    sum,
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
  builder.cnf().save("proof.cnf");
  
  return 0;
}
