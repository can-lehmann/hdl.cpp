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

#ifndef HDL_S_EXPR_HPP
#define HDL_S_EXPR_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <variant>
#include <optional>

#include "hdl.hpp"

#define throw_error(Error, msg) { \
  std::ostringstream error_message; \
  error_message << msg; \
  throw Error(error_message.str()); \
}

namespace hdl {
  namespace s_expr {
    class Reader {
    public:
      using Value = std::variant<hdl::Value*, hdl::Memory*>;
    private:
      struct Token {
        enum class Kind {
          Eof,
          Name, Constant,
          ParOpen, ParClose
        };
        
        using Value = std::variant<std::string, PartialBitString>;
        
        Kind kind = Kind::Eof;
        Value value;
        
        Token() {}
        Token(const Kind& _kind): kind(_kind) {}
        Token(const Kind& _kind, const Value& _value): kind(_kind), value(_value) {}
      };
      
      class TokenStream {
      private:
        std::istream& _stream;
        Token _prev;
        Token _token;
        
        bool is_whitespace(char chr) const {
          return chr == ' ' || 
                 chr == '\n' ||
                 chr == '\t' ||
                 chr == '\r';
        }
        
        bool is_stop_char(char chr) const {
          return is_whitespace(chr) || chr == '(' || chr == ')';
        }
        
        bool is_digit(char chr) const {
          return chr >= '0' && chr <= '9';
        }
        
        std::optional<PartialBitString> parse_constant(const std::string& source) const {
          const char* cur = source.c_str();
          size_t width = 0;
          while (is_digit(*cur)) { 
            width *= 10;
            width += *cur - '0';
            cur++;
          }
          if (*cur != '\'') {
            return {};
          }
          cur++;
          size_t base_log2 = 0;
          switch (*cur) {
            case 'b': base_log2 = 1; break;
            case 'o': base_log2 = 3; break;
            case 'h': base_log2 = 4; break;
            default: return {};
          }
          cur++;
          BitString known = ~BitString(width);
          BitString value(width);
          size_t digits = source.size() - (cur - source.c_str());
          size_t bits = digits * base_log2;
          if (bits > width) {
            return {};
          }
          while (*cur >= '0' && *cur <= '9' ||
                 *cur >= 'a' && *cur <= 'f' ||
                 *cur >= 'A' && *cur <= 'F' ||
                 *cur == 'X' ||
                 *cur == 'x') {
            if (*cur == 'x' || *cur == 'X') {
              for (size_t it = base_log2; it-- > 0; ) {
                known.set(--bits, false);
              }
            } else {
              size_t digit = 0;
              
              if (*cur >= '0' && *cur <= '9') {
                digit = *cur - '0';
              } else if (*cur >= 'a' && *cur <= 'f') {
                digit = (*cur - 'a') + 10;
              } else if (*cur >= 'A' && *cur <= 'F') {
                digit = (*cur - 'A') + 10;
              } else {
                throw_error(Error, "Unreachable");
              }
              
              if (digit >= (1 << base_log2)) {
                return {};
              }
              
              for (size_t it = base_log2; it-- > 0; ) {
                --bits;
                known.set(bits, true);
                value.set(bits, (digit >> it) & 1);
              }
            }
            cur++;
          }
          if (*cur != '\0') {
            return {};
          }
          if (bits != 0) {
            throw_error(Error, "Unreachable");
          }
          return PartialBitString(known, value);
        }
        
        Token next_token() {
          while (is_whitespace(_stream.peek())) {
            _stream.get();
          }
          
          if (_stream.eof() || _stream.peek() == EOF) {
            return Token(Token::Kind::Eof);
          }
          
          char chr = _stream.get();
          switch (chr) {
            case '(': return Token(Token::Kind::ParOpen);
            case ')': return Token(Token::Kind::ParClose);
            default: {
              std::string value;
              value.push_back(chr);
              while (!_stream.eof() && _stream.peek() != EOF && !is_stop_char(_stream.peek())) {
                value.push_back(_stream.get());
              }
              std::optional<PartialBitString> constant = parse_constant(value);
              if (constant.has_value()) {
                return Token(Token::Kind::Constant, constant.value());
              }
              return Token(Token::Kind::Name, value);
            }
          }
        }
      public:
        TokenStream(std::istream& stream): _stream(stream) {
          _token = next_token();
        }
        
        const Token& prev() const { return _prev; }
        const Token::Value& value() const { return _prev.value; }
        
        bool next(Token::Kind kind) const {
          return _token.kind == kind;
        }
        
        bool take(Token::Kind kind) {
          if (next(kind)) {
            _prev = _token;
            _token = next_token();
            return true;
          }
          return false;
        }
        
        void expect(Token::Kind kind) {
          if (!take(kind)) {
            throw_error(Error, "Expected token");
          }
        }
        
      };
    
      Module& _module;
      std::unordered_map<std::string, Value> _bindings;
      
      Value read_match(TokenStream& stream) {
        stream.expect(Token::Kind::Constant);
        PartialBitString pattern = std::get<PartialBitString>(stream.value());
        Value value = read(stream);
        stream.expect(Token::Kind::ParClose);
        
        return _module.op(Op::Kind::Eq, {
          _module.op(Op::Kind::And, {
            _module.constant(pattern.known()),
            std::get<hdl::Value*>(value)
          }),
          _module.constant(pattern.value() & pattern.known())
        });
      }
      
      Value read(TokenStream& stream) {
        if (stream.take(Token::Kind::Name)) {
          const std::string& name = std::get<std::string>(stream.value());
          if (_bindings.find(name) == _bindings.end()) {
            throw_error(Error, "Undefined variable " << name);
          }
          return _bindings.at(name);
        } else if (stream.take(Token::Kind::Constant)) {
          const PartialBitString& constant = std::get<PartialBitString>(stream.value());
          if (!constant.is_fully_known()) {
            throw_error(Error, "Constants may not include unknown (x) bits");
          }
          return _module.constant(constant.value());
        } else if (stream.take(Token::Kind::ParOpen)) {
          stream.expect(Token::Kind::Name);
          std::string op_name = std::get<std::string>(stream.value());
          
          if (op_name == "Match") {
            return read_match(stream);
          }
          
          std::vector<Value> args;
          while (!stream.take(Token::Kind::ParClose)) {
            if (stream.next(Token::Kind::Eof)) {
              throw_error(Error, "Unbalanced parentheses");
            }
            
            args.push_back(read(stream));
          }
          
          std::optional<Op::Kind> kind;
          for (size_t it = 0; it < Op::KIND_COUNT; it++) {
            if (op_name == Op::KIND_NAMES[it]) {
              kind = Op::Kind(it);
              break;
            }
          }
          
          if (!kind.has_value()) {
            throw_error(Error, "Unknown operator " << op_name);
          }
          
          std::vector<hdl::Value*> value_args;
          value_args.reserve(args.size());
          for (const Value& value : args) {
            value_args.push_back(std::get<hdl::Value*>(value));
          }
          
          return _module.op(kind.value(), value_args);
        } else {
          throw_error(Error, "Unexpected token");
        }
      }
    public:
      Reader(Module& module): _module(module) {}
      
      void define(const std::string& name, Value value) {
        _bindings[name] = value;
      }
      
      void define_module() {
        for (Reg* reg : _module.regs()) {
          if (reg->name.size() > 0) {
            define(reg->name, reg);
          }
        }
        
        for (Input* input : _module.inputs()) {
          if (input->name.size() > 0) {
            define(input->name, input);
          }
        }
        
        for (Memory* memory : _module.memories()) {
          if (memory->name.size() > 0) {
            define(memory->name, memory);
          }
        }
      }
      
      Value read(std::istream& stream) {
        TokenStream token_stream(stream);
        Value value = read(token_stream);
        if (!token_stream.next(Token::Kind::Eof)) {
          throw_error(Error, "Did not reach eof");
        }
        return value;
      }
      
      Value load(const char* path) {
        std::ifstream file;
        file.open(path);
        if (!file) {
          throw_error(Error, "Failed to open \"" << path << "\"");
        }
        return read(file);
      }
      
      Value read(const std::string& source) {
        std::istringstream stream(source);
        return read(stream);
      }
    };
  }
}

#undef throw_error

#endif
