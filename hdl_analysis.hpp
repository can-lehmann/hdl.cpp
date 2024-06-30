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
#include <unordered_set>

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
      
      template <class Fn>
      void write(std::ostream& stream, const Fn& write_value) const {
        bool is_first = true;
        if (!constant.is_zero()) {
          constant.write_short(stream);
          is_first = false;
        }
        
        for (const auto& [value, factor] : factors) {
          if (!is_first) {
            stream << " + ";
          }
          if (factor != BitString::one(factor.width())) {
            factor.write_short(stream);
            stream << " * ";
          }
          write_value(stream, value);
          is_first = false;
        }
      }
    };
    
    class Dependencies {
    private:
      bool _indirect = false;
      std::unordered_set<Value*> _values;
      std::unordered_set<Reg*> _regs;
      std::unordered_set<Memory*> _memories;
    public:
      Dependencies(bool indirect): _indirect(indirect) {}
      
      static Dependencies direct() { return Dependencies(false); }
      static Dependencies indirect() { return Dependencies(true); }
      
      const std::unordered_set<Value*>& values() const { return _values; }
      const std::unordered_set<Reg*>& regs() const { return _regs; }
      const std::unordered_set<Memory*>& memories() const { return _memories; }
      
      bool has(Value* value) const {
        return _values.find(value) != _values.end();
      }
      
      bool has(Memory* memory) const {
        return _memories.find(memory) != _memories.end();
      }
      
      void trace(Value* value) {
        if (_values.find(value) != _values.end()) {
          return;
        }
        
        std::vector<Value*> stack;
        stack.push_back(value);
        
        #define push(value) \
          if (_values.find(value) == _values.end()) { \
            stack.push_back(value); \
          }
        
        while (!stack.empty()) {
          Value* value = stack.back();
          stack.pop_back();
          
          _values.insert(value);
          
          if (Reg* reg = dynamic_cast<Reg*>(value)) {
            _regs.insert(reg);
            if (_indirect) {
              push(reg->clock);
              push(reg->next);
            }
          } else if (Op* op = dynamic_cast<Op*>(value)) {
            for (Value* arg : op->args) {
              push(arg);
            }
          } else if (Memory::Read* read = dynamic_cast<Memory::Read*>(value)) {
            push(read->address);
            if (_memories.find(read->memory) == _memories.end()) {
              _memories.insert(read->memory);
              if (_indirect) {
                for (const Memory::Write& write : read->memory->writes) {
                  push(write.clock);
                  push(write.address);
                  push(write.enable);
                  push(write.value);
                }
              }
            }
          }
        }
        
        #undef push
      }
      
    };
    
    // An interval of the form [a, b]
    // If a <=u b
    //   [a, b] = { x | a <=u x <=u b } ⊂ Z / 2^n Z
    // If b <u a
    //   [a, b] = { x | x <=u b or a <=u x } ⊂ Z / 2^n Z
    // 
    // Consider the case n = 3
    //   The interval [2, 6] = {2, 3, 4, 5, 6} could be represented as
    //     --[---]-|
    //   While the interval [7, 1] = {7, 0, 1} could be represented as
    //     -]-----[|
    // 
    // This encoding allows encoding intervals independent of their signedness.
    struct Interval {
      BitString min;
      BitString max;
      
      Interval(const BitString& _value):
        min(_value), max(_value) {}
      
      explicit Interval(const PartialBitString& _value):
        min(_value.value() & _value.known()),
        max((_value.value() & _value.known()) | ~_value.known()) {}
      
      Interval(PartialBitString::Bool value):
        Interval(PartialBitString::from_bool(value)) {}
      
      Interval(const BitString& _min, const BitString& _max):
          min(_min), max(_max) {
        normalize_inplace();
      }
      
      static Interval from_bool(PartialBitString::Bool value) {
        return Interval(value);
      }
      
      static Interval from_size_minus_one(const BitString& min, const BitString& size_minus_one) {
        return Interval(min, min + size_minus_one);
      }
      
      inline size_t width() const { return min.width(); }
      inline bool has_unsigned_wrap() const { return max.lt_u(min); }
    
    private:
      static BitString dist(const BitString& low, const BitString& high) {
        if (high.lt_u(low)) {
          return high + ~BitString(low.width()) - low + BitString::one(low.width());
        } else {
          return high - low;
        }
      }
      
      // The interval containing all values of Z / 2^n Z is always stored as [0, 2 ^ n - 1]
      void normalize_inplace() {
        if (max + BitString::one(width()) == min) {
          min = BitString(width());
          max = ~BitString(width());
        }
      }
      
    public:
      BitString size_minus_one() const {
        return dist(min, max);
      }
      
      bool contains(const BitString& value) const {
        if (has_unsigned_wrap()) {
          return min.le_u(value) || value.le_u(max);
        } else {
          return min.le_u(value) && value.le_u(max);
        }
      }
      
      // Flattens the interval into a ring of higher modulus.
      // Consider [3'h6, 3'h2]
      //   --]---[-|
      // Flattening into Z/4Z results in [4'h6, 4'h8]
      //   ------[---]-----|
      // By choosing the new modulus adequately, it can be guaranteed that
      // no interval will result in an unsigned wrap.
      Interval flatten(const BitString& zero, size_t width) const {
        return Interval::from_size_minus_one(
          dist(zero, min).zero_extend(width),
          dist(min, max).zero_extend(width)
        );
      }
      
      // Inverse of the flatten operation.
      Interval truncate(const BitString& zero, size_t to_width) {
        if (size_minus_one().slice_width(to_width, width() - to_width).is_zero()) {
          return Interval(min.truncate(to_width) + zero, max.truncate(to_width) + zero);
        } else {
          return Interval(BitString(to_width), ~BitString(to_width));
        }
      }
      
    private:
      Interval merge_assume_min(const Interval& other) const {
        Interval a = flatten(min, width() + 4);
        Interval b = other.flatten(min, width() + 4);
        return Interval(a.min.min_u(b.min), a.max.max_u(b.max)).truncate(min, width());
      }
      
    public:
      // There are two options for merging two intervals.
      // Consider
      //   -----[]-|
      // and
      //   -[-]----|
      // You could either merge them like this:
      //   ---]-[--|
      // Or like this
      //   -[----]-|
      // We choose the one with the smallest resulting interval.
      // Note that both options may have a resulting interval of the same size.
      Interval merge(const Interval& other) const {
        Interval a = merge_assume_min(other);
        Interval b = other.merge_assume_min(*this);
        if (a.size_minus_one().lt_u(b.size_minus_one())) {
          return a;
        } else {
          return b;
        }
      }
      
      Interval operator~() const {
        // Proof
        // if a <=u b
        //   ~[a, b]
        //   = ~{ x | a <=u x <=u b }
        //   = { ~x | a <=u x <=u b }
        //   = { 2^n - 1 - x | a <=u x <=u b }
        //       y = 2^n - 1 - x <=> x = 2^n - 1 - y
        //   = { y | a <=u 2^n - 1 - y and 2^n - 1 - y <=u b  }
        //   = { y | y <=u 2^n - 1 - a and 2^n - 1 - b <=u y  }
        //   = { y | 2^n - 1 - b <=u y <=u 2^n - 1 - a }
        //   = { y | ~b <=u y <=u ~a }
        //   = [~b, ~a]
        // if a >u b
        //   ~[a, b]
        //   = ~{ x | x <=u b or a <=u x }
        //   = { 2^n - 1 - x | x <=u b or a <=u x }
        //   = { y | 2^n - 1 - y <=u b or a <=u 2^n - 1 - y }
        //   = { y | 2^n - 1 - b <=u y or y <=u 2^n - 1 - a }
        //   = { y | y <=u ~a or ~b <=u y }
        //   = [~b, ~a]
        
        return Interval(~max, ~min);
      }
      
      Interval operator+(const Interval& other) const {
        Interval a = flatten(min, width() + 4);
        Interval b = other.flatten(min, width() + 4);
        return Interval(a.min + b.min, a.max + b.max).truncate(min + min, width());
      }
      
      Interval operator-(const Interval& other) const {
        return *this + ~other + BitString::one(width());
      }
      
      Interval select(const Interval& then, const Interval& otherwise) const {
        if (!contains(BitString::from_bool(true))) {
          return otherwise;
        } else if (!contains(BitString::from_bool(false))) {
          return then;
        } else {
          return then.merge(otherwise);
        }
      }
      
      PartialBitString as_partial_bit_string() const {
        if (has_unsigned_wrap()) {
          // => contains 0 and ~0
          return PartialBitString(width());
        } else {
          // All digits less significant than the most significant bit
          // that is different in min and max are unknown.
          BitString unknown = min ^ max;
          for (size_t it = 1; it < width(); it <<= 1) {
            unknown = unknown | unknown.shr_u(it);
          }
          return PartialBitString(~unknown, min);
        }
      }
      
      // All remaining operators are implemented by convering to PartialBitString.
      // This may lose some precision.
      
      #define binop(name, RetType, impl) \
        RetType name(const Interval& other) const { \
          PartialBitString a = as_partial_bit_string(); \
          PartialBitString b = other.as_partial_bit_string(); \
          return RetType(impl); \
        }
      
      binop(operator&, Interval,  a & b)
      binop(operator|, Interval, a | b)
      binop(operator^, Interval, a ^ b)
      binop(mul_u, Interval, a.mul_u(b))
      binop(concat, Interval, a.concat(b))
      binop(operator<<, Interval, a << b)
      binop(shr_u, Interval, a.shr_u(b))
      binop(shr_s, Interval, a.shr_s(b))
      
      binop(eq, PartialBitString::Bool, a.eq(b))
      binop(lt_u, PartialBitString::Bool, a.lt_u(b))
      binop(lt_s, PartialBitString::Bool, a.lt_s(b))
      binop(le_u, PartialBitString::Bool, a.le_u(b))
      binop(le_s, PartialBitString::Bool, a.le_s(b))
      
      #undef binop
      
      void write(std::ostream& stream) const {
        if (min == max) {
          stream << '{';
          min.write_short(stream);
          stream << '}';
        } else if (size_minus_one().is_uint(1)) {
          stream << '{';
          min.write_short(stream);
          stream << ", ";
          max.write_short(stream);
          stream << '}';
        } else if (has_unsigned_wrap()) {
          Interval(BitString(width()), max).write(stream);
          stream << " ∪ ";
          Interval(min, ~BitString(width())).write(stream);
        } else {
          stream << '[';
          min.write_short(stream);
          stream << ", ";
          max.write_short(stream);
          stream << ']';
        }
      }
      
      bool operator==(const Interval& other) const {
        return min == other.min && max == other.max;
      }
      
      bool operator!=(const Interval& other) const {
        return !(*this == other);
      }
    };
  }
}

#endif
