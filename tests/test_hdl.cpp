// Copyright (c) 2023 Can Joshua Lehmann

#include <inttypes.h>

#include "../../unittest.cpp/unittest.hpp"
#include "../hdl.hpp"

using Test = unittest::Test;
#define assert(cond) unittest_assert(cond)

void test_module() {
  Test("Constant Hashcons", [&](){
    hdl::Module module("top");
    
    assert(module.constant(hdl::BitString("1010")) == module.constant(hdl::BitString("1010")));
    assert(module.constant(hdl::BitString("1010")) != module.constant(hdl::BitString("1110")));
    assert(module.constant(hdl::BitString("1")) == module.constant(hdl::BitString("1")));
    assert(module.constant(hdl::BitString("0")) != module.constant(hdl::BitString("1")));
    assert(module.constant(hdl::BitString("0")) != module.constant(hdl::BitString("00")));
  });
  
  Test("Operator Hashcons", [&](){
    hdl::Module module("top");
    
    hdl::Value* a = module.input("a", 32);
    hdl::Value* b = module.input("b", 32);
    
    assert(
      module.op(hdl::Op::Kind::And, {a, b}) ==
      module.op(hdl::Op::Kind::And, {a, b})
    );
  });
  
  Test("Garbage Collection", [&](){
    hdl::Module module("top");
    
    hdl::Value* clock = module.input("clock", 1);
    hdl::Value* a = module.input("a", 32);
    hdl::Value* b = module.input("b", 32);
    
    hdl::Value* and_op = module.op(hdl::Op::Kind::And, {a, b});
    module.reg(module.constant(hdl::BitString(32)), clock)->next = and_op;
    assert(module.regs().size() == 1);
    
    module.gc();
    assert(module.regs().size() == 0);
    
    and_op = module.op(hdl::Op::Kind::And, {a, b});
    hdl::Reg* reg = module.reg(module.constant(hdl::BitString(32)), clock);
    reg->next = and_op;
    module.output("c", reg);
    assert(module.regs().size() == 1);
    
    module.gc();
    assert(module.regs().size() == 1);
  });
}

void test_ops() {
  hdl::Module module("top");
  hdl::Value* zero = module.constant(hdl::BitString(32));
  hdl::Value* ones = module.constant(~hdl::BitString(32));
  hdl::Value* bool_true = module.constant(~hdl::BitString(1));
  hdl::Value* bool_false = module.constant(hdl::BitString(1));
  hdl::Value* a = module.input("a", 32);
  hdl::Value* b = module.input("b", 32);
  hdl::Value* cond = module.input("cond", 1);
  
  Test("Op::Kind::And", [&](){
    assert(module.op(hdl::Op::Kind::And, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::And, {a, a}) == a);
    assert(module.op(hdl::Op::Kind::And, {a, zero}) == zero);
    assert(module.op(hdl::Op::Kind::And, {a, ones}) == a);
    assert(module.op(hdl::Op::Kind::And, {zero, a}) == zero);
    assert(module.op(hdl::Op::Kind::And, {ones, a}) == a);
  });
  
  Test("Op::Kind::Or", [&](){
    assert(module.op(hdl::Op::Kind::Or, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Or, {a, a}) == a);
    assert(module.op(hdl::Op::Kind::Or, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::Or, {a, ones}) == ones);
    assert(module.op(hdl::Op::Kind::Or, {zero, a}) == a);
    assert(module.op(hdl::Op::Kind::Or, {ones, a}) == ones);
  });
  
  Test("Op::Kind::Xor", [&](){
    assert(module.op(hdl::Op::Kind::Xor, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Xor, {a, a}) == zero);
  });
  
  Test("Op::Kind::Not", [&](){
    assert(module.op(hdl::Op::Kind::Not, {a})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Not, {module.op(hdl::Op::Kind::Not, {a})}) == a);
    assert(module.op(hdl::Op::Kind::Not, {zero}) == ones);
    assert(module.op(hdl::Op::Kind::Not, {ones}) == zero);
  });
  
  Test("Op::Kind::Add", [&](){
    assert(module.op(hdl::Op::Kind::Add, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Add, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::Add, {zero, a}) == a);
  });
  
  Test("Op::Kind::Sub", [&](){
    assert(module.op(hdl::Op::Kind::Sub, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Sub, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::Sub, {a, a}) == zero);
  });
  
  Test("Op::Kind::Mul", [&](){
  });
  
  Test("Op::Kind::Eq", [&](){
    assert(module.op(hdl::Op::Kind::Eq, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::Eq, {a, a}) == bool_true);
    assert(module.op(hdl::Op::Kind::Eq, {zero, zero}) == bool_true);
    assert(module.op(hdl::Op::Kind::Eq, {zero, ones}) == bool_false);
  });
  
  Test("Op::Kind::LtU", [&](){
    assert(module.op(hdl::Op::Kind::LtU, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::LtU, {a, a}) == bool_false);
    assert(module.op(hdl::Op::Kind::LtU, {a, zero}) == bool_false);
  });
  
  Test("Op::Kind::LtS", [&](){
    assert(module.op(hdl::Op::Kind::LtS, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::LtS, {a, a}) == bool_false);
  });
  
  Test("Op::Kind::LeU", [&](){
    assert(module.op(hdl::Op::Kind::LeU, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::LeU, {a, a}) == bool_true);
    assert(module.op(hdl::Op::Kind::LeU, {zero, a}) == bool_true);
  });
  
  Test("Op::Kind::LeS", [&](){
    assert(module.op(hdl::Op::Kind::LeS, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::LeS, {a, a}) == bool_true);
  });
  
  Test("Op::Kind::Concat", [&](){
    assert(module.op(hdl::Op::Kind::Concat, {a, b})->width == 64);
  });
  
  Test("Op::Kind::Slice", [&](){
  });
  
  Test("Op::Kind::Shl", [&](){
    assert(module.op(hdl::Op::Kind::Shl, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Shl, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::Shl, {zero, a}) == zero);
  });
  
  Test("Op::Kind::ShrU", [&](){
    assert(module.op(hdl::Op::Kind::ShrU, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::ShrU, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::ShrU, {zero, a}) == zero);
  });
  
  Test("Op::Kind::ShrS", [&](){
    assert(module.op(hdl::Op::Kind::ShrS, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::ShrS, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::ShrS, {zero, a}) == zero);
    assert(module.op(hdl::Op::Kind::ShrS, {ones, a}) == ones);
  });
  
  Test("Op::Kind::Select", [&](){
    assert(module.op(hdl::Op::Kind::Select, {cond, a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Select, {bool_true, a, b}) == a);
    assert(module.op(hdl::Op::Kind::Select, {bool_false, a, b}) == b);
    assert(module.op(hdl::Op::Kind::Select, {cond, a, a}) == a);
  });
}

int main() {
  test_module();
  test_ops();
  
  return 0;
}
