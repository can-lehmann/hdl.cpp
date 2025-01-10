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

#ifndef HDL_FLATTEN_HPP
#define HDL_FLATTEN_HPP

#include <map>
#include <vector>
#include <optional>

#include "hdl.hpp"

namespace hdl {
  namespace flatten {
    class Flattening {
    private:
      using Bits = std::vector<Value*>;
      
      Module& _module;
      std::unordered_map<Value*, Bits> _values;
      
      Value* select(Value* cond, Value* a, Value* b) {
        return _module.op(Op::Kind::Or, {
          _module.op(Op::Kind::And, {
            cond, a
          }),
          _module.op(Op::Kind::And, {
            _module.op(Op::Kind::Not, {cond}),
            b
          })
        });
      }
      
      Bits select(Value* cond, const Bits& a, const Bits& b) {
        Bits bits(a.size());
        for (size_t it = 0; it < bits.size(); it++) {
          bits[it] = select(cond, a[it], b[it]);
        }
        return bits;
      }
      
      Bits add_sub(const Bits& a, const Bits& b, bool is_sub) {
        Bits c(a.size());
        
        Value* carry = _module.constant(BitString::from_bool(is_sub));
        for (size_t it = 0; it < a.size(); it++) {
          Value* b_bit = b[it];
          if (is_sub) {
            b_bit = _module.op(Op::Kind::Not, {b_bit});
          }
          
          c[it] = _module.op(Op::Kind::Xor, {
            _module.op(Op::Kind::Xor, {a[it], b_bit}),
            carry
          });
          
          carry = _module.op(Op::Kind::Or, {
            _module.op(Op::Kind::Or, {
              _module.op(Op::Kind::And, {carry, a[it]}),
              _module.op(Op::Kind::And, {carry, b_bit})
            }),
            _module.op(Op::Kind::And, {a[it], b_bit})
          });
        }
        
        return c;
      }
      
      Bits shr(const Bits& a, const Bits& b, bool is_signed) {
        Bits result = a;
        
        for (size_t it = 0; it < b.size(); it++) {
          for (size_t it2 = 0; it2 < result.size(); it2++) {
            size_t shift_index = it2 + (1 << it);
            Value* shifted = nullptr;
            if (it < sizeof(int) * 8 && shift_index < result.size()) {
              shifted = result[shift_index];
            } else {
              if (is_signed) {
                shifted = a.back();
              } else {
                shifted = _module.constant(BitString::from_bool(false));
              }
            }
            result[it2] = select(b[it], shifted, result[it2]);
          }
        }
        
        return result;
      }
      
      Bits shl(const Bits& a, const Bits& b) {
        Bits result = a;
        
        for (size_t it = 0; it < b.size(); it++) {
          for (size_t it2 = result.size(); it2-- > 0; ) {
            Value* shifted = nullptr;
            if (it < sizeof(int) * 8 && it2 >= (1 << it)) {
              shifted = result[it2 - (1 << it)];
            } else {
              shifted = _module.constant(BitString::from_bool(false));
            }
            result[it2] = select(b[it], shifted, result[it2]);
          }
        }
        
        return result;
      }
      
      Bits mul(const Bits& a, const Bits& b) {
        Bits result(a.size() + b.size());
        for (size_t it = 0; it < result.size(); it++) {
          result[it] = _module.constant(BitString::from_bool(false));
        }
        
        for (size_t shift = 0; shift < b.size(); shift++) {
          Bits shifted_a;
          shifted_a.reserve(result.size());
          for (size_t it = 0; it < shift; it++) {
            shifted_a.push_back(_module.constant(BitString::from_bool(false)));
          }
          
          shifted_a.insert(shifted_a.end(), a.begin(), a.end());
          
          while (shifted_a.size() < result.size()) {
            shifted_a.push_back(_module.constant(BitString::from_bool(false)));
          }
          
          result = select(b[shift], add_sub(result, shifted_a, false), result);
        }
        
        return result;
      }
      
      Value* lt_u(const Bits& a, const Bits& b) {
        Value* result = _module.constant(BitString::from_bool(false));
        Value* inactive = _module.constant(BitString::from_bool(false));
        for (size_t it = a.size(); it-- > 0; ) {
          result = _module.op(Op::Kind::Or, {
            result,
            _module.op(Op::Kind::And, {
              _module.op(Op::Kind::Not, {inactive}),
              _module.op(Op::Kind::And, {
                _module.op(Op::Kind::Not, {a[it]}),
                b[it]
              })
            })
          });
          
          inactive = _module.op(Op::Kind::Or, {
            inactive,
            _module.op(Op::Kind::Xor, {a[it], b[it]})
          });
        }
        
        return result;
      }
      
      Value* lt_s(const Bits& a, const Bits& b) {
        return select(
          _module.op(Op::Kind::Xor, {a.back(), b.back()}),
          _module.op(Op::Kind::And, {
            a.back(),
            _module.op(Op::Kind::Not, {b.back()})
          }),
          lt_u(a, b)
        );
      }
    public:
      Flattening(Module& module): _module(module) {}
      
      void define(Value* value, const Bits& bits) {
        _values[value] = bits;
      }
      
      std::vector<Value*> operator[](Value* value) const {
        return _values.at(value);
      }
      
      Bits split(Value* value) {
        Bits bits;
        for (size_t it = 0; it < value->width; it++) {
          bits.push_back(_module.op(hdl::Op::Kind::Slice, {
            value,
            _module.constant(BitString::from_uint(it)),
            _module.constant(BitString::from_uint(1))
          }));
        }
        return bits;
      }
      
      Value* join(const Bits& bits) {
        Value* value = bits[0];
        for (size_t it = 1; it < bits.size(); it++) {
          value = _module.op(hdl::Op::Kind::Concat, {
            bits[it], value
          });
        }
        return value;
      }
      
      // Flattens value to only use single bit wide And, Or, Xor and Not operators
      void flatten(Value* value) {
        if (_values.find(value) != _values.end()) {
          return;
        }
        
        Bits bits;
        bits.reserve(value->width);
        if (Constant* constant = dynamic_cast<Constant*>(value)) {
          for (size_t it = 0; it < value->width; it++) {
            bool bit = constant->value.at(it);
            bits.push_back(_module.constant(BitString::from_bool(bit)));
          }
        } else if (Unknown* unknown = dynamic_cast<Unknown*>(value)) {
          for (size_t it = 0; it < value->width; it++) {
            bits.push_back(_module.unknown(1));
          }
        } else if (Op* op = dynamic_cast<Op*>(value)) {
          for (Value* arg : op->args) {
            flatten(arg);
          }
          
          #define arg(index) _values.at(op->args[index])
          
          switch (op->kind) {
            case Op::Kind::And:
            case Op::Kind::Or:
            case Op::Kind::Xor:
              for (size_t it = 0; it < op->width; it++) {
                bits.push_back(_module.op(op->kind, {
                  arg(0)[it], arg(1)[it]
                }));
              }
            break;
            case Op::Kind::Not:
              for (size_t it = 0; it < op->width; it++) {
                bits.push_back(_module.op(op->kind, {arg(0)[it]}));
              }
            break;
            case Op::Kind::Add: bits = add_sub(arg(0), arg(1), false); break;
            case Op::Kind::Sub: bits = add_sub(arg(0), arg(1), true); break;
            case Op::Kind::Mul: bits = mul(arg(0), arg(1)); break;
            case Op::Kind::Eq: {
              Value* is_not_eq = _module.constant(BitString::from_bool(false));
              for (size_t it = 0; it < op->args[0]->width; it++) {
                is_not_eq = _module.op(Op::Kind::Or, {
                  is_not_eq,
                  _module.op(Op::Kind::Xor, {
                    arg(0)[it], arg(1)[it]
                  })
                });
              }
              bits.push_back(_module.op(Op::Kind::Not, {is_not_eq}));
            }
            break;
            case Op::Kind::LtU: bits = {lt_u(arg(0), arg(1))}; break;
            case Op::Kind::LtS: bits = {lt_s(arg(0), arg(1))}; break;
            case Op::Kind::Concat:
              bits.insert(bits.end(), arg(1).begin(), arg(1).end());
              bits.insert(bits.end(), arg(0).begin(), arg(0).end());
            break;
            case Op::Kind::Slice: {
              Bits shifted = shr(arg(0), arg(1), false);
              bits.insert(bits.end(), shifted.begin(), shifted.begin() + op->width);
            }
            break;
            case Op::Kind::Shl: bits = shl(arg(0), arg(1)); break;
            case Op::Kind::ShrU: bits = shr(arg(0), arg(1), false); break;
            case Op::Kind::ShrS: bits = shr(arg(0), arg(1), true); break;
            case Op::Kind::Select: bits = select(arg(0)[0], arg(1), arg(2)); break;
          }
          
          #undef arg
        } else {
          throw Error("");
        }
        
        _values.insert({value, bits});
      }
      
    };
  };
};

#endif
