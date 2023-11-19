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

#define throw_error(Error, msg) { \
  std::ostringstream error_message; \
  error_message << msg; \
  throw Error(error_message.str()); \
}

namespace hdl {
  namespace textir {
    class Reader {
    private:
      Module& _module;
      
      bool is_digit(char chr) const {
        return chr >= '0' && chr <= '9';
      }
      
      bool is_whitespace(char chr) const {
        return chr == ' ' || chr == '\t' || chr == '\r';
      }
      
      void skip_whitespace(std::istream& stream) const {
        while (!stream.eof() && is_whitespace(stream.peek())) {
          stream.get();
        }
      }
      
      size_t read_size(std::istream& stream) const {
        skip_whitespace(stream);
        size_t value = 0;
        if (stream.eof() || !is_digit(stream.peek())) {
          throw_error(Error, "Expected number");
        }
        stream >> value;
        return value;
      }
      
      size_t read_id(std::istream& stream) const {
        return read_size(stream);
      }
      
      char hex_digit(char chr) const {
        if (chr >= '0' && chr <= '9') {
          return chr - '0';
        } else if (chr >= 'a' && chr <= 'f') {
          return chr - 'a' + 10;
        } else if (chr >= 'A' && chr <= 'F') {
          return chr - 'A' + 10;
        } else {
          throw_error(Error, "Invalid hex digit");
        }
      }
      
      std::string read_string(std::istream& stream) const {
        skip_whitespace(stream);
        
        if (stream.get() != '\"') {
          throw_error(Error, "Expected \"");
        }
        
        std::string string;
        while (stream.peek() != '\"') {
          if (stream.eof()) {
            throw_error(Error, "Unterminated string literal");
          }
          
          if (stream.peek() == '\\') {
            stream.get();
            if (stream.get() != 'x') {
              throw_error(Error, "Expected x");
            }
            
            char chr = hex_digit(stream.get()) << 4;
            chr |= hex_digit(stream.get());
            string.push_back(chr);
          } else {
            string.push_back(char(stream.get()));
          }
        }
        
        stream.get();
        return string;
      }
      
      std::string read_word(std::istream& stream) const {
        skip_whitespace(stream);
        std::string word;
        while (!stream.eof() && stream.peek() != '\n' && !is_whitespace(stream.peek())) {
          word.push_back(char(stream.get()));
        }
        return word;
      }
      
      BitString read_bit_string(std::istream& stream) const {
        size_t width = read_size(stream);
        if (stream.get() != '\'') {
          throw_error(Error, "Expected \'");
        }
        if (stream.get() != 'b') {
          throw_error(Error, "Expected b");
        }
        
        BitString bit_string(width);
        
        std::string bits = read_word(stream);
        for (size_t it = 0; it < bits.size(); it++) {
          char bit = bits[bits.size() - it - 1];
          if (bit != '0' && bit != '1') {
            throw_error(Error, "Invalid binary digit");
          }
          bit_string.set(it, bit == '1');
        }
        
        return bit_string;
      }
    public:
      Reader(Module& module): _module(module) {}
      
      static Module read_module(std::istream& stream) {
        Module module("top");
        Reader reader(module);
        reader.read(stream);
        return std::move(module);
      }
      
      static Module load_module(const char* path) {
        Module module("top");
        Reader reader(module);
        reader.load(path);
        return std::move(module);
      }
      
      void read(std::istream& stream) const {
        std::unordered_map<size_t, Value*> values;
        std::unordered_map<size_t, Memory*> memories;
        
        skip_whitespace(stream);
        while (!stream.eof()) {
          while (!stream.eof() && stream.peek() == '#') {
            while (!stream.eof() && stream.get() != '\n') {}
            skip_whitespace(stream);
          }
        
          size_t id = 0;
          bool has_id = false;
          if (is_digit(stream.peek())) {
            id = read_id(stream);
            has_id = true;
            skip_whitespace(stream);
            if (stream.get() != '=') {
              throw_error(Error, "Expected =");
            }
            skip_whitespace(stream);
          }
          
          std::string cmd = read_word(stream);
          
          if (cmd == "input") {
            std::string name = read_string(stream);
            size_t width = read_size(stream);
            if (!has_id) { throw_error(Error, "Does not have id"); }
            values[id] = _module.input(name, width);
          } else if (cmd == "reg") {
            BitString initial = read_bit_string(stream);
            Reg* reg = _module.reg(initial, nullptr);
            reg->name = read_string(stream);
            if (!has_id) { throw_error(Error, "Does not have id"); }
            values[id] = reg;
          } else if (cmd == "memory") {
            size_t width = read_size(stream);
            size_t size = read_size(stream);
            hdl::Memory* memory = _module.memory(width, size);
            memory->name = read_string(stream);
            if (!has_id) { throw_error(Error, "Does not have id"); }
            memories[id] = memory;
          } else if (cmd == "next") {
            Reg* reg = dynamic_cast<Reg*>(values.at(read_size(stream)));
            reg->clock = values.at(read_id(stream));
            reg->next = values.at(read_id(stream));
          } else if (cmd == "read") {
            Memory* memory = memories.at(read_id(stream));
            Value* address = values.at(read_id(stream));
            if (!has_id) { throw_error(Error, "Does not have id"); }
            values[id] = memory->read(address);
          } else if (cmd == "write") {
            Memory* memory = memories.at(read_id(stream));
            Value* clock = values.at(read_id(stream));
            Value* address = values.at(read_id(stream));
            Value* enable = values.at(read_id(stream));
            Value* value = values.at(read_id(stream));
            memory->write(clock, address, enable, value);
          } else if (cmd == "output") {
            std::string name = read_string(stream);
            Value* value = values.at(read_id(stream));
            _module.output(name, value);
          } else if (cmd == "constant") {
            BitString bit_string = read_bit_string(stream);
            if (!has_id) { throw_error(Error, "Does not have id"); }
            values[id] = _module.constant(bit_string);
          } else {
            Op::Kind kind;
            bool has_kind = false;
            for (size_t it = 0; it < Op::KIND_COUNT; it++) {
              if (cmd == Op::KIND_NAMES[it]) {
                kind = Op::Kind(it);
                has_kind = true;
                break;
              }
            }
            
            if (!has_kind) {
              throw_error(Error, "Unknown command " << cmd);
            }
            
            std::vector<Value*> args;
            
            skip_whitespace(stream);
            while (!stream.eof() && stream.peek() != '\n') {
              args.push_back(values.at(read_id(stream)));
              skip_whitespace(stream);
            }
            
            if (!has_id) { throw_error(Error, "Does not have id"); }
            values[id] = _module.op(kind, args);
          }
          
          skip_whitespace(stream);
          
          if (!stream.eof() && stream.get() != '\n') {
            throw_error(Error, "Expected newline or EOF");
          }
          
          skip_whitespace(stream);
        }
      }
      
      void load(const char* path) const {
        std::ifstream file;
        file.open(path);
        if (!file) {
          throw_error(Error, "Failed to open \"" << path << "\"");
        }
        read(file);
      }
    };
    
    class Printer {
    private:
      Module& _module;
      
      bool is_printable(char chr) const {
        return (chr >= '!' && chr <= '~' && chr != '\\' && chr != '\"') || chr == ' ';
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
          throw_error(Error, "Unreachable");
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

#undef throw_error

#endif
