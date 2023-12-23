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

#ifndef HDL_PROOF_Z3
#define HDL_PROOF_Z3

#include <vector>
#include <map>
#include <optional>
#include <string>
#include <sstream>

#include <z3++.h>

#include "hdl.hpp"

namespace hdl {
  namespace proof {
    namespace z3 {
      
      class Builder {
      private:
        ::z3::context& _context;
        std::map<const Value*, ::z3::expr> _values;
        std::map<const Memory*, ::z3::expr> _memories;
        
        ::z3::expr bool2bv(::z3::expr expr) {
          return ::z3::ite(expr,
            build(BitString::from_bool(true)),
            build(BitString::from_bool(false))
          );
        }
      public:
        Builder(::z3::context& context): _context(context) {}
        
        void free(const Value* value) {
          std::ostringstream name;
          name << "value" << _values.size();
          _values.emplace(value, _context.bv_const(name.str().c_str(), value->width));
        }
        
        void free(const Memory* memory) {
          std::ostringstream name;
          name << "memory" << _memories.size();
          ::z3::sort sort = _context.array_sort(
            _context.int_sort(),
            _context.bv_sort(memory->width)
          );
          _memories.emplace(memory, _context.constant(name.str().c_str(), sort));
        }
        
        void define(const Value* value, ::z3::expr expr) {
          // TODO: Check sort
          _values.emplace(value, expr);
        }
        
        void define(const Memory* memory, ::z3::expr expr) {
          // TODO: Check sort
          _memories.emplace(memory, expr);
        }
        
        ::z3::expr build(const BitString& bit_string) {
          bool bits[bit_string.width()];
          for (size_t it = 0; it < bit_string.width(); it++) {
            bits[it] = bit_string[it];
          }
          return _context.bv_val(bit_string.width(), &bits[0]);
        }
        
        ::z3::expr build(const Value* value) {
          if (_values.find(value) != _values.end()) {
            return _values.at(value);
          }
          
          std::optional<::z3::expr> expr;
          if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
            expr = build(constant->value);
          } else if (const Op* op = dynamic_cast<const Op*>(value)) {
            for (const Value* arg : op->args) {
              build(arg);
            }
            
            #define arg(index) _values.at(op->args[index])
            
            switch (op->kind) {
              case Op::Kind::And: expr = arg(0) & arg(1); break;
              case Op::Kind::Or: expr = arg(0) | arg(1); break;
              case Op::Kind::Xor: expr = arg(0) ^ arg(1); break;
              case Op::Kind::Not: expr = ~arg(0); break;
              case Op::Kind::Add: expr = arg(0) + arg(1); break;
              case Op::Kind::Sub: expr = arg(0) - arg(1); break;
              case Op::Kind::Mul: expr = arg(0) * arg(1); break; // TODO
              case Op::Kind::Eq: expr = bool2bv(arg(0) == arg(1)); break;
              case Op::Kind::LtU: expr = bool2bv(::z3::ult(arg(0), arg(1))); break;
              case Op::Kind::LtS: expr = bool2bv(::z3::slt(arg(0), arg(1))); break;
              case Op::Kind::LeU: expr = bool2bv(::z3::ule(arg(0), arg(1))); break;
              case Op::Kind::LeS: expr = bool2bv(::z3::sle(arg(0), arg(1))); break;
              case Op::Kind::Concat: expr = ::z3::concat(arg(0), arg(1)); break;
              case Op::Kind::Slice: {
                size_t width = dynamic_cast<const Constant*>(op->args[2])->value.as_uint64();
                if (const Constant* const_offset = dynamic_cast<const Constant*>(op->args[1])) {
                  size_t offset = const_offset->value.as_uint64();
                  expr = arg(0).extract(offset + width - 1, offset);
                } else {
                  expr = ::z3::lshr(arg(0), arg(1)).extract(width - 1, 0);
                }
              }
              break;
              case Op::Kind::Shl: expr = ::z3::shl(arg(0), arg(1)); break;
              case Op::Kind::ShrU: expr = ::z3::lshr(arg(0), arg(1)); break;
              case Op::Kind::ShrS: expr = ::z3::ashr(arg(0), arg(1)); break;
              case Op::Kind::Select: expr = ::z3::ite(arg(0).bit2bool(0), arg(1), arg(2)); break;
              default:
                throw Error("Operator not implemented");
            }
            
            #undef arg
          } else if (const Memory::Read* read = dynamic_cast<const Memory::Read*>(value)) {
            // TODO: Check bounds
            ::z3::expr address = ::z3::bv2int(build(read->address), false);
            expr = _memories.at(read->memory)[address];
          } else {
            throw Error("Unable to build z3::expr for value");
          }
          
          _values.emplace(value, expr.value());
          return expr.value();
        }
        
        void require(::z3::solver& solver, const Value* value, const BitString& string) {
          solver.add(build(value) == build(string));
        }
        
        BitString interp(::z3::solver& solver, const Value* value) {
          auto model = solver.get_model();
          auto numeral = model.eval(_values.at(value), true);
          BitString bit_string(numeral.get_sort().bv_size());
          for (size_t it = 0; it < bit_string.width(); it++) {
            switch (model.eval(numeral.bit2bool(0), true).bool_value()) {
              case Z3_L_TRUE: bit_string.set(it, true); break;
              case Z3_L_FALSE: bit_string.set(it, false); break;
              default: throw Error("");
            }
          }
          return bit_string;
        }
      };
    }
  }
}

#endif
