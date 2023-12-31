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

#ifndef HDL_ANALYSIS_HPP
#define HDL_ANALYSIS_HPP

#include <map>

#include "hdl.hpp"

namespace hdl {
  namespace analysis {
    struct AffineValue {
      std::map<Value*, BitString> factors;
      BitString constant;
      
      AffineValue() {}
      AffineValue(const BitString& _constant): constant(_constant) {}
      AffineValue(Value* _value): constant(_value->width) {
        factors[_value] = BitString::one(width());
      }
      AffineValue(Value* _value, const BitString& _factor): constant(_value->width) {
        factors[_value] = _factor;
      }
      
      static AffineValue build(Value* value, std::map<Value*, AffineValue>& affine) {
        if (affine.find(value) != affine.end()) {
          return affine.at(value);
        }
        
        AffineValue affine_value(value, BitString::one(value->width));
        if (Constant* constant = dynamic_cast<Constant*>(value)) {
          affine_value = AffineValue(constant->value);
        } else if (Op* op = dynamic_cast<Op*>(value)) {
          #define arg(index) build(op->args[index], affine)
          
          switch (op->kind) {
            case Op::Kind::Add: affine_value = arg(0) + arg(1); break;
            case Op::Kind::Sub: affine_value = arg(0) - arg(1); break;
            case Op::Kind::Shl:
              if (Constant* constant = dynamic_cast<Constant*>(op->args[1])) {
                BitString factor = BitString::one(op->args[0]->width);
                factor = factor << constant->value.as_uint64();
                affine_value = arg(0) * factor;
              }
            break;
            default: break;
          }
          
          #undef arg
        }
        
        affine.insert({ value, affine_value });
        return affine.at(value);
      }
      
      static AffineValue build(Value* value) {
        std::map<Value*, AffineValue> affine;
        build(value, affine);
        return affine.at(value);
      }
      
      size_t width() const { return constant.width(); }
      bool is_constant() const { return factors.size() == 0; }
      
      #define additive_binop(op) \
        AffineValue operator op(const AffineValue& other) const { \
          AffineValue result = *this; \
          result.constant = constant op other.constant; \
          for (const auto& [value, factor] : other.factors) { \
            BitString result_factor(width()); \
            if (result.factors.find(value) != result.factors.end()) { \
              result_factor = result.factors.at(value); \
            } \
            result_factor = result_factor op factor; \
            if (!result_factor.is_zero()) { \
              result.factors[value] = result_factor; \
            } else { \
              result.factors.erase(value); \
            } \
          } \
          return result; \
        }
      
      additive_binop(+);
      additive_binop(-);
      
      #undef additive_binop
      
      AffineValue operator*(const BitString& other) const {
        if (width() != other.width()) {
          throw Error("Width mismatch");
        }
        
        if (other.is_zero()) {
          return AffineValue(BitString(width()));
        }
        
        AffineValue result = *this;
        result.constant = (result.constant.mul_u(other)).slice_width(0, width());
        for (const auto& [value, factor] : factors) {
          result.factors[value] = (factor.mul_u(other)).slice_width(0, width());
        }
        return result;
      }
      
      bool operator==(const AffineValue& other) const {
        return constant == other.constant && factors == other.factors;
      }
      
      bool operator!=(const AffineValue& other) const {
        return !(*this == other);
      }
      
      std::optional<bool> static_equal(const AffineValue& other) const {
        if (factors == other.factors) {
          return constant == other.constant;
        }
        
        return {};
      }
      
      hdl::Value* build(hdl::Module& module) const {
        hdl::Value* result = module.constant(constant);
        for (const auto& [value, factor] : factors) {
          hdl::Value* term = value;
          if (factor != BitString::one(width())) {
            // TODO: Prove that (x << 0) + (x << 1) + ... + (x << n) = ~x + 1
            term = module.op(Op::Kind::Slice, {
              module.op(Op::Kind::Mul, {term, module.constant(factor)}),
              module.constant(BitString::from_uint(0)),
              module.constant(BitString::from_uint(width()))
            });
          }
          result = module.op(Op::Kind::Add, {result, term});
        }
        return result;
      }
    };
  }
}

#endif
