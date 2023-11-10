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

#ifndef HDL_TEXTIR_HPP
#define HDL_TEXTIR_HPP

#include <unordered_map>
#include <string>
#include <sstream>

#include "hdl.hpp"

namespace hdl {
  namespace textir {
    class Reader {
    private:
      Module& _module;
    public:
      Reader(Module& module): _module(module) {}
      
      static Module load_module(const char* path) {
        Module module("top");
        Reader reader(module);
        reader.load(path);
        return std::move(module);
      }
      
      void read(std::istream& stream) const {
        
      }
      
      void load(const char* path) const {
        std::ifstream file;
        file.open(path);
        read(file);
      }
    };
    
    class Printer {
    private:
      Module& _module;
      
      bool is_printable(char chr) const {
        return chr >= '!' && chr <= '~' && chr != '\\' && chr != '\"';
      }
      
      void print(std::ostream& stream, const char* str) const {
        static const char* HEX_DIGITS = "0123456789abcdef";
        
        stream << '\"';
        for (const char* cur = str; *cur != '\0'; cur++) {
          if (is_printable(*cur)) {
            stream << *cur;
          } else {
            stream << "\\x" << HEX_DIGITS[(*cur >> 4) & 0xf] << HEX_DIGITS[*cur & 0xf];
          }
        }
        stream << '\"';
      }
      
      void print(std::ostream& stream, const std::string& str) const {
        print(stream, str.c_str());
      }
      
      void print(std::ostream& stream, const BitString& bit_string) const {
        bit_string.write_short(stream);
      }
      
      struct Context {
        std::ostream& stream;
        size_t id_count = 0;
        std::unordered_map<const Value*, size_t> values;
        std::unordered_map<const Memory*, size_t> memories;
        
        Context(std::ostream& _stream): stream(_stream) {}
        
        size_t alloc(const Value* value) {
          size_t id = id_count++;
          values[value] = id;
          return id;
        }
        
        size_t alloc(const Memory* memory) {
          size_t id = id_count++;
          memories[memory] = id;
          return id;
        }
        
        size_t operator[](const Value* value) const { return values.at(value); }
        size_t operator[](const Memory* memory) const { return memories.at(memory); }
        
        bool has(const Value* value) const { return values.find(value) != values.end(); }
      };
      
      void print(Value* value, Context& context) const {
        if (context.has(value)) {
          return;
        }
        
        if (Constant* constant = dynamic_cast<Constant*>(value)) {
          context.stream << context.alloc(value) << " = constant ";
          print(context.stream, constant->value);
        } else if (Op* op = dynamic_cast<Op*>(value)) {
          for (Value* arg : op->args) {
            print(arg, context);
          }
          
          context.stream << context.alloc(value) << " = " << op->kind;
          for (Value* arg : op->args) {
            context.stream << ' ' << context[arg];
          }
        } else if (Memory::Read* read = dynamic_cast<Memory::Read*>(value)) {
          print(read->address, context);
          
          context.stream << context.alloc(value) << " = read ";
          context.stream << context[read->memory] << ' ';
          context.stream << context[read->address];
        } else {
          throw Error("Unreachable");
        }
        
        context.stream << '\n';
        
      }
      
    public:
      Printer(Module& module): _module(module) {}
      
      void print(std::ostream& stream) const {
        Context context(stream);
        
        for (Input* input : _module.inputs()) {
          stream << context.alloc(input) << " = input ";
          print(stream, input->name);
          stream << " " << input->width << '\n';
        }
        
        for (Reg* reg : _module.regs()) {
          stream << context.alloc(reg) << " = reg ";
          print(stream, reg->initial);
          stream << ' ';
          print(stream, reg->name);
          stream << '\n';
        }
        
        for (Memory* memory : _module.memories()) {
          stream << context.alloc(memory) << " = memory ";
          stream << memory->width << ' ' << memory->size << ' ';
          print(stream, memory->name);
          stream << '\n';
        }
        
        for (Reg* reg : _module.regs()) {
          print(reg->clock, context);
          print(reg->next, context);
          stream << "next " << context[reg] << ' ';
          stream << context[reg->clock] << ' ';
          stream << context[reg->next] << '\n';
        }
        
        for (Memory* memory : _module.memories()) {
          for (const Memory::Write& write : memory->writes) {
            print(write.clock, context);
            print(write.address, context);
            print(write.enable, context);
            print(write.value, context);
            
            stream << "write " << context[memory] << ' ';
            stream << context[write.clock] << ' ';
            stream << context[write.address] << ' ';
            stream << context[write.enable] << ' ';
            stream << context[write.value] << '\n';
          }
        }
        
        for (const Output& output : _module.outputs()) {
          print(output.value, context);
          stream << "output ";
          print(stream, output.name);
          stream << ' ' << context[output.value] << '\n';
        }
      }
      
      void save(const char* path) const {
        std::ofstream file;
        file.open(path);
        print(file);
      }
    };
  }
}

#endif
