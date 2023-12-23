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

#include <map>
#include <sstream>

#include "../../unittest.cpp/unittest.hpp"
#include "../hdl.hpp"
#include "../hdl_textir.hpp"

using Test = unittest::Test;
#define assert(cond) unittest_assert(cond)

class StructuralEquivalence {
private:
  hdl::Module& _a_module;
  hdl::Module& _b_module;
  
  bool _is_equivalent = true;
  std::map<hdl::Value*, hdl::Value*> _values;
  std::map<hdl::Memory*, hdl::Memory*> _memories;
  
  bool match(const hdl::Memory::Write& a, const hdl::Memory::Write& b) {
    return match(a.clock, b.clock) &&
           match(a.address, b.address) &&
           match(a.enable, b.enable) &&
           match(a.value, b.value);
  }
  
  bool match(hdl::Memory* a, hdl::Memory* b) {
    if (_memories.find(a) != _memories.end()) {
      return _memories.at(a) == b;
    }
    
    _memories.insert({a, b});
    
    if (a->width != b->width) { return false; }
    if (a->size != b->size) { return false; }
    if (a->name != b->name) { return false; }
    if (a->writes.size() != b->writes.size()) { return false; }
    
    for (size_t it = 0; it < a->writes.size(); it++) {
      if (!match(a->writes[it], b->writes[it])) {
        return false;
      }
    }
    
    return true;
  }
  
  bool match(hdl::Value* a, hdl::Value* b) {
    if (_values.find(a) != _values.end()) {
      return _values.at(a) == b;
    }
    
    _values.insert({a, b});
    
    #define matches(Type, name, value) Type* name = dynamic_cast<Type*>(value)
    #define require(Type, name, value) \
      matches(Type, name, value); \
      if (name == nullptr) { return false; }
    
    if (matches(hdl::Constant, a_constant, a)) {
      require(hdl::Constant, b_constant, b);
      return a_constant->value == b_constant->value;
    } else if (matches(hdl::Unknown, a_unknown, a)) {
      require(hdl::Unknown, b_unknown, b);
      return a->width == b->width;
    } else if (matches(hdl::Op, a_op, a)) {
      require(hdl::Op, b_op, b);
      
      if (a_op->kind != b_op->kind) { return false; }
      if (a_op->args.size() != b_op->args.size()) { return false; }
      
      for (size_t it = 0; it < a_op->args.size(); it++) {
        if (!match(a_op->args[it], b_op->args[it])) {
          return false;
        }
      }
      
      return true;
    } else if (matches(hdl::Memory::Read, a_read, a)) {
      require(hdl::Memory::Read, b_read, b);
      return match(a_read->memory, b_read->memory) &&
             match(a_read->address, b_read->address);
    } else if (matches(hdl::Reg, a_reg, a)) {
      require(hdl::Reg, b_reg, b);
      return a_reg->initial == b_reg->initial &&
             a_reg->name == b_reg->name &&
             a_reg->width == b_reg->width &&
             match(a_reg->clock, b_reg->clock) &&
             match(a_reg->next, b_reg->next);
    } else if (matches(hdl::Input, a_input, a)) {
      require(hdl::Input, b_input, b);
      return a_input->width == b_input->width &&
             a_input->name == b_input->name;
    } else {
      throw hdl::Error("Unknown value");
    }
    
    #undef matches
    #undef require
  }
  
  bool match(const hdl::Output& a, const hdl::Output& b) {
    return a.name == b.name && match(a.value, b.value);
  }
  
public:
  StructuralEquivalence(hdl::Module& a_module, hdl::Module& b_module):
      _a_module(a_module), _b_module(b_module) {
    
    if (_a_module.name() == _b_module.name() &&
        _a_module.outputs().size() == _b_module.outputs().size()) {
      _is_equivalent = true;
      for (size_t it = 0; it < _a_module.outputs().size(); it++) {
        if (!match(_a_module.outputs()[it], _b_module.outputs()[it])) {
          _is_equivalent = false;
          break;
        }
      }
    } else {
      _is_equivalent = false;
    }
  }
  
  bool is_equivalent() const { return _is_equivalent; }
  std::map<hdl::Value*, hdl::Value*> values() const { return _values; }
  std::map<hdl::Memory*, hdl::Memory*> memories() const { return _memories; }
  
};

void check(hdl::Module& module) {
  std::ostringstream out_stream;
  hdl::textir::Printer printer(module);
  printer.print(out_stream);
  
  std::istringstream in_stream(out_stream.str());
  hdl::Module read_module = hdl::textir::Reader::read_module(in_stream);
  
  StructuralEquivalence equivalence(module, read_module);
  assert(equivalence.is_equivalent());
}

int main() {
  Test("empty").run([](){
    hdl::Module module("top");
    check(module);
  });
  
  Test("constant").run([](){
    hdl::Module module("top");
    module.output("zeros", module.constant(hdl::BitString("000000")));
    module.output("ones", module.constant(hdl::BitString("1111111111")));
    module.output("constant", module.constant(hdl::BitString("0001010")));
    
    check(module);
  });
  
  Test("input").run([](){
    hdl::Module module("top");
    module.output("out", module.input("in", 8));
    
    check(module);
  });
  
  Test("unknown").run([](){
    hdl::Module module("top");
    module.output("out", module.unknown(8));
    
    check(module);
  });
  
  Test("operator/add").run([](){
    hdl::Module module("top");
    hdl::Value* a = module.input("a", 8);
    hdl::Value* b = module.input("b", 8);
    module.output("out", module.op(hdl::Op::Kind::Add, {a, b}));
    
    check(module);
  });
  
  Test("operator/select").run([](){
    hdl::Module module("top");
    hdl::Value* cond = module.input("cond", 1);
    hdl::Value* a = module.input("a", 8);
    hdl::Value* b = module.input("b", 8);
    module.output("out", module.op(hdl::Op::Kind::Select, {cond, a, b}));
    
    check(module);
  });
  
  Test("operator/select/swapped").run([](){
    hdl::Module module("top");
    hdl::Value* cond = module.input("cond", 1);
    hdl::Value* a = module.input("a", 8);
    hdl::Value* b = module.input("b", 8);
    module.output("out", module.op(hdl::Op::Kind::Select, {cond, b, a}));
    
    check(module);
  });
  
  Test("reg").run([](){
    hdl::Module module("top");
    hdl::Value* clock = module.input("clock", 1);
    hdl::Reg* reg = module.reg(hdl::BitString("0000"), clock);
    module.output("reg", reg);
    
    check(module);
  });
  
  Test("reg/initialized").run([](){
    hdl::Module module("top");
    hdl::Value* clock = module.input("clock", 1);
    hdl::Reg* reg = module.reg(hdl::BitString("1010"), clock);
    module.output("reg", reg);
    
    check(module);
  });
  
  Test("reg/named").run([](){
    hdl::Module module("top");
    hdl::Value* clock = module.input("clock", 1);
    hdl::Reg* reg = module.reg(hdl::BitString("1010"), clock);
    reg->name = "register name\nnewline";
    module.output("reg", reg);
    
    check(module);
  });
  
  Test("reg/counter").run([](){
    hdl::Module module("top");
    
    hdl::Value* clock = module.input("clock", 1);
    hdl::Reg* counter = module.reg(hdl::BitString("0000"), clock);
    
    counter->next = module.op(hdl::Op::Kind::Add, {
      counter,
      module.constant(hdl::BitString("0001"))
    });
    
    module.output("counter", counter);
    
    check(module);
  });

  Test("memory").run([](){
    hdl::Module module("top");
    hdl::Value* clock = module.input("clock", 1);
    hdl::Value* address = module.input("address", 5);
    hdl::Value* write_value = module.input("write_value", 64);
    hdl::Value* write_enable = module.input("write_enable", 1);
    hdl::Memory* memory = module.memory(64, 32);
    memory->name = "memory name";
    
    module.output("read", memory->read(address));
    memory->write(clock, address, write_enable, write_value);
    
    check(module);
  });

  return 0;
}
