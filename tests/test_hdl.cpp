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

#include "../../unittest.cpp/unittest.hpp"
#include "../hdl.hpp"

using Test = unittest::Test;
#define assert(cond) unittest_assert(cond)

void test_module() {
  Test("Constant Hashcons").run([&](){
    hdl::Module module("top");
    
    assert(module.constant(hdl::BitString("1010")) == module.constant(hdl::BitString("1010")));
    assert(module.constant(hdl::BitString("1010")) != module.constant(hdl::BitString("1110")));
    assert(module.constant(hdl::BitString("1")) == module.constant(hdl::BitString("1")));
    assert(module.constant(hdl::BitString("0")) != module.constant(hdl::BitString("1")));
    assert(module.constant(hdl::BitString("0")) != module.constant(hdl::BitString("00")));
  });
  
  Test("Operator Hashcons").run([&](){
    hdl::Module module("top");
    
    hdl::Value* a = module.input("a", 32);
    hdl::Value* b = module.input("b", 32);
    
    assert(
      module.op(hdl::Op::Kind::And, {a, b}) ==
      module.op(hdl::Op::Kind::And, {a, b})
    );
  });
  
  Test("Garbage Collection").run([&](){
    hdl::Module module("top");
    
    hdl::Value* clock = module.input("clock", 1);
    hdl::Value* a = module.input("a", 32);
    hdl::Value* b = module.input("b", 32);
    
    hdl::Value* and_op = module.op(hdl::Op::Kind::And, {a, b});
    module.reg(hdl::BitString(32), clock)->next = and_op;
    assert(module.regs().size() == 1);
    
    module.gc();
    assert(module.regs().size() == 0);
    
    and_op = module.op(hdl::Op::Kind::And, {a, b});
    hdl::Reg* reg = module.reg(hdl::BitString(32), clock);
    reg->next = and_op;
    module.output("c", reg);
    assert(module.regs().size() == 1);
    
    module.gc();
    assert(module.regs().size() == 1);
  });
  
  Test("Unknown Value").run([&](){
    hdl::Module module("top");
    hdl::Value* a = module.unknown(32);
    hdl::Value* b = module.unknown(32);
    
    assert(dynamic_cast<hdl::Constant*>(module.op(hdl::Op::Kind::Eq, { a, a })));
    assert(dynamic_cast<hdl::Constant*>(module.op(hdl::Op::Kind::Eq, { b, b })));
    assert(!dynamic_cast<hdl::Constant*>(module.op(hdl::Op::Kind::Eq, { a, b })));
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
  
  Test("Op::Kind::And").run([&](){
    assert(module.op(hdl::Op::Kind::And, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::And, {a, a}) == a);
    assert(module.op(hdl::Op::Kind::And, {a, zero}) == zero);
    assert(module.op(hdl::Op::Kind::And, {a, ones}) == a);
    assert(module.op(hdl::Op::Kind::And, {zero, a}) == zero);
    assert(module.op(hdl::Op::Kind::And, {ones, a}) == a);
  });
  
  Test("Op::Kind::Or").run([&](){
    assert(module.op(hdl::Op::Kind::Or, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Or, {a, a}) == a);
    assert(module.op(hdl::Op::Kind::Or, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::Or, {a, ones}) == ones);
    assert(module.op(hdl::Op::Kind::Or, {zero, a}) == a);
    assert(module.op(hdl::Op::Kind::Or, {ones, a}) == ones);
  });
  
  Test("Op::Kind::Xor").run([&](){
    assert(module.op(hdl::Op::Kind::Xor, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Xor, {a, a}) == zero);
  });
  
  Test("Op::Kind::Not").run([&](){
    assert(module.op(hdl::Op::Kind::Not, {a})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Not, {module.op(hdl::Op::Kind::Not, {a})}) == a);
    assert(module.op(hdl::Op::Kind::Not, {zero}) == ones);
    assert(module.op(hdl::Op::Kind::Not, {ones}) == zero);
  });
  
  Test("Op::Kind::Add").run([&](){
    assert(module.op(hdl::Op::Kind::Add, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Add, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::Add, {zero, a}) == a);
  });
  
  Test("Op::Kind::Sub").run([&](){
    assert(module.op(hdl::Op::Kind::Sub, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Sub, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::Sub, {a, a}) == zero);
  });
  
  Test("Op::Kind::Mul").run([&](){
  });
  
  Test("Op::Kind::Eq").run([&](){
    assert(module.op(hdl::Op::Kind::Eq, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::Eq, {a, a}) == bool_true);
    assert(module.op(hdl::Op::Kind::Eq, {zero, zero}) == bool_true);
    assert(module.op(hdl::Op::Kind::Eq, {zero, ones}) == bool_false);
  });
  
  Test("Op::Kind::LtU").run([&](){
    assert(module.op(hdl::Op::Kind::LtU, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::LtU, {a, a}) == bool_false);
    assert(module.op(hdl::Op::Kind::LtU, {a, zero}) == bool_false);
  });
  
  Test("Op::Kind::LtS").run([&](){
    assert(module.op(hdl::Op::Kind::LtS, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::LtS, {a, a}) == bool_false);
  });
  
  Test("Op::Kind::LeU").run([&](){
    assert(module.op(hdl::Op::Kind::LeU, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::LeU, {a, a}) == bool_true);
    assert(module.op(hdl::Op::Kind::LeU, {zero, a}) == bool_true);
  });
  
  Test("Op::Kind::LeS").run([&](){
    assert(module.op(hdl::Op::Kind::LeS, {a, b})->width == 1);
    
    assert(module.op(hdl::Op::Kind::LeS, {a, a}) == bool_true);
  });
  
  Test("Op::Kind::Concat").run([&](){
    assert(module.op(hdl::Op::Kind::Concat, {a, b})->width == 64);
  });
  
  Test("Op::Kind::Slice").run([&](){
  });
  
  Test("Op::Kind::Shl").run([&](){
    assert(module.op(hdl::Op::Kind::Shl, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Shl, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::Shl, {zero, a}) == zero);
  });
  
  Test("Op::Kind::ShrU").run([&](){
    assert(module.op(hdl::Op::Kind::ShrU, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::ShrU, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::ShrU, {zero, a}) == zero);
  });
  
  Test("Op::Kind::ShrS").run([&](){
    assert(module.op(hdl::Op::Kind::ShrS, {a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::ShrS, {a, zero}) == a);
    assert(module.op(hdl::Op::Kind::ShrS, {zero, a}) == zero);
    assert(module.op(hdl::Op::Kind::ShrS, {ones, a}) == ones);
  });
  
  Test("Op::Kind::Select").run([&](){
    assert(module.op(hdl::Op::Kind::Select, {cond, a, b})->width == 32);
    
    assert(module.op(hdl::Op::Kind::Select, {bool_true, a, b}) == a);
    assert(module.op(hdl::Op::Kind::Select, {bool_false, a, b}) == b);
    assert(module.op(hdl::Op::Kind::Select, {cond, a, a}) == a);
  });
}

void test_sim() {
  Test("Counter Simulation").run([](){
    hdl::Module module("top");
    hdl::Value* clock = module.input("clock", 1);
    hdl::Reg* counter = module.reg(hdl::BitString(32), clock);
    counter->next = module.op(hdl::Op::Kind::Add, {
      counter,
      module.constant(hdl::BitString::from_uint(uint32_t(1)))
    });
    module.output("counter", counter);
    
    {
      hdl::sim::Simulation sim(module);
      bool clock = true;
      for (size_t iter = 0; iter < 100; iter++) {
        sim.update({hdl::BitString::from_bool(clock)});
        assert(sim.outputs()[0].as_uint64() == iter / 2 + 1)
        clock = !clock;
      }
    }
  });
  
  Test("Memory Simulation").run([](){
    hdl::Module module("top");
    hdl::Value* clock = module.input("clock", 1);
    hdl::Value* address = module.input("address", 5);
    hdl::Value* write_value = module.input("write_value", 64);
    hdl::Value* write_enable = module.input("write_enable", 1);
    hdl::Memory* memory = module.memory(64, 32);
    
    module.output("read", memory->read(address));
    memory->write(clock, address, write_enable, write_value);
    
    {
      hdl::sim::Simulation sim(module);
      bool clock = false;
      
      struct MemOp {
        bool is_write = false;
        uint64_t address = 0;
        uint64_t value = 0;
      };
      
      std::vector<MemOp> ops = {
        { true, 0, 123 },
        { false, 0, 123 },
        { false, 1, 0 },
        { true, 1, 456 },
        { false, 0, 123 },
        { false, 1, 456 }
      };
      
      for (MemOp op : ops) {
        for (size_t iter = 0; iter < 2; iter++) {
          sim.update({
            hdl::BitString::from_bool(clock),
            hdl::BitString::from_uint(op.address).truncate(address->width),
            hdl::BitString::from_uint(op.value),
            hdl::BitString::from_bool(op.is_write)
          });
          if (!op.is_write) {
            assert(sim.outputs()[0].as_uint64() == op.value)
          }
          clock = !clock;
        }
      }
    }
  });
}

int main() {
  test_module();
  test_ops();
  test_sim();
  
  return 0;
}
