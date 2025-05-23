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

#ifndef HDL_BITSTRING_HPP
#define HDL_BITSTRING_HPP

#include <inttypes.h>
#include <string.h>
#include <vector>
#include <string>
#include <sstream>

#define throw_error(Error, msg) { \
  std::ostringstream error_message; \
  error_message << msg; \
  throw Error(error_message.str()); \
}

namespace hdl {
  class Error: public std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  class BitString {
  public:
    using Word = uint32_t;
    using DoubleWord = uint64_t;
    static constexpr const size_t WORD_WIDTH = sizeof(Word) * 8;
    
    class WordArray {
    private:
      static constexpr const size_t SMALL_SIZE = 2;
      
      size_t _size = 0;
      union {
        Word _small[2];
        Word* _data;
      };
      
      inline bool is_small() const {
        return _size <= SMALL_SIZE;
      }
    public:
      WordArray(): WordArray(0) {}
      WordArray(size_t size): _size(size) {
        if (is_small()) {
          memset(_small, 0, sizeof(_small));
        } else {
          _data = new Word[_size]();
        }
      }
      
      WordArray(const WordArray& other): _size(other.size()) {
        if (is_small()) {
          memcpy(_small, other._small, sizeof(_small));
        } else {
          _data = new Word[_size]();
          memcpy(_data, other._data, sizeof(Word) * _size);
        }
      }
      
      WordArray& operator=(const WordArray& other) {
        if (&other != this) {
          this->~WordArray();
          new (this) WordArray(other);
        }
        return *this;
      } 
      
      ~WordArray() {
        if (!is_small() && _data != nullptr) {
          delete[] _data;
          _data = nullptr;
        }
      }
      
      size_t size() const { return _size; }
      const Word* begin() const { return is_small() ? _small : _data; }
      const Word* end() const { return begin() + _size; }
      Word* begin() { return is_small() ? _small : _data; }
      Word* end() { return begin() + _size; }
      
      inline const Word& operator[](size_t index) const {
        if (is_small()) {
          return _small[index];
        } else {
          return _data[index];
        }
      }
      
      inline Word& operator[](size_t index) {
        if (is_small()) {
          return _small[index];
        } else {
          return _data[index];
        }
      }
      
      inline const Word& back() const { return (*this)[_size - 1]; } 
    };
    
  private:
    size_t _width = 0;
    WordArray _data;
    
    static size_t word_count(size_t width) {
      return width / WORD_WIDTH + (width % WORD_WIDTH == 0 ? 0 : 1);
    }
    
    static Word mask_lower(size_t bits) {
      return bits == WORD_WIDTH ? ~Word(0) : (Word(1) << bits) - 1;
    }
    
    inline Word high_word_mask() const {
      return _width % WORD_WIDTH == 0 ? ~Word(0) : mask_lower(_width % WORD_WIDTH);
    }
  public:
    BitString() {}
    explicit BitString(size_t width): _width(width), _data(word_count(width)) {}
    
    BitString(const std::string& string):
        _width(string.size()), _data(word_count(string.size())) {
      for (size_t it = 0; it < string.size(); it++) {
        char chr = string[it];
        bool value;
        switch(chr) {
          case '0': value = false; break;
          case '1': value = true; break;
          default: throw_error(Error, "Invalid digit " << chr);
        }
        set(_width - it - 1, value);
      }
    }
    
    static BitString from_bool(bool value) {
      BitString bit_string(1);
      bit_string.set(0, value);
      return bit_string;
    }
    
    template <class T>
    static BitString from_uint(T value) {
      BitString bit_string(sizeof(T) * 8);
      if (sizeof(T) > sizeof(Word)) {
        for (size_t it = 0; it * WORD_WIDTH < sizeof(T) * 8; it++) {
          bit_string._data[it] = Word(value & T(~Word(0)));
          value >>= WORD_WIDTH;
        }
      } else {
        bit_string._data[0] = Word(value);
      }
      return bit_string;
    }
    
    static BitString from_base_log2(size_t base_log2, const std::string& string) {
      BitString bit_string(base_log2 * string.size());
      size_t offset = base_log2 * string.size();
      size_t base = 1 << base_log2;
      for (char chr : string) {
        size_t digit = 0;
        if (chr >= '0' && chr <= '9') {
          digit = chr - '0';
        } else if (chr >= 'a' && chr <= 'z') {
          digit = chr - 'a' + 10;
        } else if (chr >= 'A' && chr <= 'Z') {
          digit = chr - 'A' + 10;
        } else {
          throw_error(Error, "Invalid digit " << chr << " for base " << base);
        }
        
        if (digit >= base) {
          throw_error(Error, "Invalid digit " << chr << " for base " << base);
        }
        
        offset -= base_log2;
        for (size_t it = 0; it < base_log2; it++) {
          bit_string.set(offset + it, digit & (1 << it));
        }
      }
      return bit_string;
    }
    
    static BitString from_oct(const std::string& string) {
      return from_base_log2(3, string);
    }
    
    static BitString from_hex(const std::string& string) {
      return from_base_log2(4, string);
    }
    
    static BitString one(size_t width) {
      BitString bit_string(width);
      bit_string.set(0, true);
      return bit_string;
    }
    
    static BitString upper(size_t width, size_t from_bit) {
      BitString bit_string(width);
      if (from_bit < width) {
        bit_string.fill_upper(from_bit);
      }
      return bit_string;
    }
    
    static BitString random(size_t width) {
      BitString bit_string(width);
      for (size_t it = 0; it < width; it++) {
        bit_string.set(it, rand() & 1);
      }
      return bit_string;
    }
    
    inline size_t width() const { return _width; }
    inline const WordArray& data() const { return _data; }
    inline WordArray& data() { return _data; } 
    
    bool at(size_t index) const {
      if (index >= _width) {
        throw_error(Error, "Index " << index << " out of bounds for BitString of width " << _width);
      }
      return (_data[index / WORD_WIDTH] & (1 << index % WORD_WIDTH)) != 0;
    }
    
    inline bool operator[](size_t index) const { return at(index); }
    
    void set(size_t index, bool value) {
      if (index >= _width) {
        throw_error(Error, "Index " << index << " out of bounds for BitString of width " << _width);
      }
      if (value) {
        _data[index / WORD_WIDTH] |= 1 << index % WORD_WIDTH;
      } else {
        _data[index / WORD_WIDTH] &= ~(1 << index % WORD_WIDTH);
      }
    }
  
  private:
    void ensure_same_width(const BitString& other) const {
      if (_width != other._width) {
        throw_error(Error, "BitStrings must have the same width, but got " << _width << " and " << other._width);
      }
    }
  public:
  
    #define BINOP(op) \
      BitString operator op(const BitString& other) const { \
        ensure_same_width(other); \
        BitString result(_width); \
        for (size_t it = 0; it < _data.size(); it++) { \
          result._data[it] = _data[it] op other._data[it]; \
        } \
        return result; \
      }
    
    BINOP(&);
    BINOP(|);
    BINOP(^);
    
    #undef BINOP
    
    BitString operator~() const {
      BitString result(_width);
      for (size_t it = 0; it < _data.size(); it++) {
        result._data[it] = ~_data[it];
      }
      return result;
    }
  
  private:
    template <bool initial_carry, bool invert> 
    BitString add_carry(const BitString& other) const {
      ensure_same_width(other);
      
      BitString sum(_width);
      DoubleWord carry = initial_carry ? 1 : 0;
      for (size_t it = 0; it < _data.size(); it++) {
        DoubleWord word_sum = DoubleWord(_data[it]) + DoubleWord(invert ? ~other._data[it] : other._data[it]) + carry;
        sum._data[it] = Word(word_sum) & ~Word(0);
        carry = word_sum >> WORD_WIDTH;
      }
      return sum;
    }
  public:
    BitString operator+(const BitString& other) const {
      return add_carry<false, false>(other);
    }
    
    BitString operator-(const BitString& other) const {
      return add_carry<true, true>(other);
    }
    
  private:
    inline void shl_into(BitString& into, size_t shift) const {
      size_t inner_shift = shift % WORD_WIDTH;
      size_t outer_shift = shift / WORD_WIDTH;
      
      for (size_t it = 0; it + outer_shift < into._data.size() && it < _data.size(); it++) {
        into._data[it + outer_shift] |= _data[it] << inner_shift;
        if (it + outer_shift + 1 < into._data.size() && inner_shift > 0) {
          into._data[it + outer_shift + 1] |= _data[it] >> (WORD_WIDTH - inner_shift);
        }
      }
    }
    
    inline void shr_u_into(BitString& into, size_t shift) const {
      size_t inner_shift = shift % WORD_WIDTH;
      size_t outer_shift = shift / WORD_WIDTH;
      
      for (size_t it = outer_shift; it < _data.size(); it++) {
        Word word = it + 1 == _data.size() ? _data[it] & high_word_mask() : _data[it];
        if (it > outer_shift && inner_shift > 0) {
          into._data[it - outer_shift - 1] |= word << (WORD_WIDTH - inner_shift);
        }
        if (it - outer_shift >= into._data.size()) {
          break;
        }
        into._data[it - outer_shift] |= word >> inner_shift;
      }
    }
    
  public:
    BitString operator<<(size_t shift) const {
      BitString result(_width);
      shl_into(result, shift);
      return result;
    }
    
    BitString shr_u(size_t shift) const {
      BitString result(_width);
      shr_u_into(result, shift);
      return result;
    }
  
  private:
    void fill_upper(size_t from_bit) {
      size_t from_word = from_bit / WORD_WIDTH;
      size_t from_inner = from_bit % WORD_WIDTH;
      
      _data[from_word] |= ~mask_lower(from_inner);
      for (size_t it = from_word + 1; it < _data.size(); it++) {
        _data[it] = ~Word(0);
      }
    }
  public:
    BitString shr_s(size_t shift) const {
      BitString result = shr_u(shift);
      
      if (at(_width - 1)) {
        result.fill_upper(shift >= _width ? 0 : _width - shift);
      }
      
      return result;
    }
    
    BitString operator>>(size_t shift) const {
      return shr_u(shift);
    }
    
    BitString operator<<(const BitString& other) const {
      return *this << other.as_uint64();
    }
    
    BitString shr_u(const BitString& other) const {
      return shr_u(other.as_uint64());
    }
    
    BitString shr_s(const BitString& other) const {
      return shr_s(other.as_uint64());
    }
    
    BitString zero_extend(size_t to_width) const {
      if (to_width < _width) {
        throw_error(Error, "Cannot zero extend from width " << _width << ", to width " << to_width);
      }
      
      BitString result(to_width);
      for (size_t it = 0; it + 1 < _data.size(); it++) {
        result._data[it] = _data[it];
      }
      result._data[_data.size() - 1] = _data.back() & high_word_mask();
      
      return result;
    }
    
    BitString truncate(size_t to_width) const {
      if (to_width > _width) {
        throw_error(Error, "Cannot truncate from width " << _width << ", to width " << to_width);
      }
      
      BitString result(to_width);
      std::copy(_data.begin(), _data.begin() + result._data.size(), result._data.begin());
      return result;
    }
    
    BitString resize_u(size_t to_width) const {
      if (_width == to_width) {
        return *this;
      } else if (_width < to_width) {
        return zero_extend(to_width);
      } else {
        return truncate(to_width);
      }
    }
    
    BitString mul_u(const BitString& other) const {
      BitString result(_width + other._width);
      for (size_t it = 0; it < _width; it++) {
        if (at(it)) {
          result = result + (other.zero_extend(_width + other._width) << it);
        }
      }
      return result;
    }
    
    BitString operator*(const BitString& other) const {
      ensure_same_width(other);
      return mul_u(other).truncate(_width);
    }
    
    void write(std::ostream& stream) const {
      stream << _width << "'b";
      for (size_t it = _width; it-- > 0;) {
        stream << ((*this)[it] ? '1' : '0');
      }
    }
    
    void write_short(std::ostream& stream) const {
      if (_width == 0) {
        stream << "0'b0";
      } else {
        stream << _width << "'b";
        size_t highest_significant_digit = _width - 1;
        while (highest_significant_digit > 0 && !(*this)[highest_significant_digit]) {
          highest_significant_digit--;
        }
        for (size_t it = highest_significant_digit + 1; it-- > 0; ) {
          stream << ((*this)[it] ? '1' : '0');
        }
      }
    }
    
    std::string to_string() const {
      std::ostringstream stream;
      write_short(stream);
      return stream.str();
    }
    
    bool operator==(const BitString& other) const {
      if (_width != other._width) {
        return false;
      }
      
      for (size_t it = 0; it + 1 < _data.size(); it++) {
        if (_data[it] != other._data[it]) {
          return false;
        }
      }
      
      Word mask = high_word_mask();
      return (_data.back() & mask) == (other._data.back() & mask);
    }
    
    inline bool operator!=(const BitString& other) const {
      return !(*this == other);
    }
    
    bool is_zero() const {
      for (size_t it = 0; it + 1 < _data.size(); it++) {
        if (_data[it] != 0) {
          return false;
        }
      }
      
      Word mask = high_word_mask();
      return (_data.back() & mask) == 0;
    }
    
    bool is_all_ones() const {
      for (size_t it = 0; it + 1 < _data.size(); it++) {
        if (_data[it] != ~Word(0)) {
          return false;
        }
      }
      
      Word mask = high_word_mask();
      return (_data.back() & mask) == (~Word(0) & mask);
    }
    
    bool is_uint(uint64_t value) const {
      for (size_t it = 0; it + 1 < _data.size(); it++) {
        if (_data[it] != Word(value)) {
          return false;
        }
        value = value >> WORD_WIDTH;
      }
      
      Word mask = high_word_mask();
      return (_data.back() & mask) == Word(value);
    }
    
    size_t hash() const {
      size_t value = std::hash<size_t>()(_width);
      for (size_t it = 0; it + 1 < _data.size(); it++) {
        value ^= std::hash<Word>()(_data[it]);
      }
      value ^= std::hash<Word>()(_data.back() & high_word_mask());
      return value;
    }
    
    bool eq(const BitString& other) const {
      return (*this) == other;
    }
    
    bool lt_u(const BitString& other) const {
      ensure_same_width(other);
      
      Word mask = high_word_mask();
      if ((_data.back() & mask) < (other._data.back() & mask)) {
        return true;
      } else if ((_data.back() & mask) > (other._data.back() & mask)) {
        return false;
      }
      
      if (_data.size() > 1) {
        for (size_t it = _data.size() - 1; it-- > 0; ) {
          if (_data[it] < other._data[it]) {
            return true;
          } else if (_data[it] > other._data[it]) {
            return false;
          }
        }
      }
      
      return false;
    }
    
    bool le_u(const BitString& other) const {
      return lt_u(other) || (*this) == other;
    }
    
    bool lt_s(const BitString& other) const {
      ensure_same_width(other);
      
      if (at(_width - 1) == other.at(_width - 1)) {
        return lt_u(other);
      } else {
        return at(_width - 1) && !other.at(_width - 1);
      }
    }
    
    bool le_s(const BitString& other) const {
      return lt_s(other) || (*this) == other;
    }
    
    BitString min_u(const BitString& other) const {
      if (lt_u(other)) {
        return *this;
      } else {
        return other;
      }
    }
    
    BitString max_u(const BitString& other) const {
      if (lt_u(other)) {
        return other;
      } else {
        return *this;
      }
    }
    
    BitString concat(const BitString& other) const {
      BitString result(_width + other._width);
      std::copy(other._data.begin(), other._data.end(), result._data.begin());
      result._data[other._data.size() - 1] &= other.high_word_mask();
      shl_into(result, other._width);
      return result;
    }
    
    BitString slice_width(size_t offset, size_t width) const {
      if (offset + width > _width) {
        throw_error(Error,
          "Slice [" << (offset + width - 1) << ":" << offset << "] " <<
          "is out of bounds for BitString of width " << _width
        );
      }
      
      BitString result(width);
      shr_u_into(result, offset);
      return result;
    }
    
    uint64_t as_uint64() const {
      uint64_t value = 0;
      size_t it = 0;
      for (; it * WORD_WIDTH < sizeof(uint64_t) * 8 && it + 1 < _data.size(); it++) {
        value |= uint64_t(_data[it]) << (it * WORD_WIDTH);
      }
      if (it + 1 >= _data.size() && it * WORD_WIDTH < sizeof(uint64_t) * 8) {
        value |= uint64_t(_data.back() & high_word_mask()) << (it * WORD_WIDTH);
      }
      return value;
    }
    
    bool as_bool() const {
      if (_width != 1) {
        throw_error(Error, "Expected BitString to be of width 1, but got width " << _width);
      }
      return at(0);
    }
    
    BitString reverse_words(size_t word_size) const {
      if (_width % word_size != 0) {
        throw_error(Error, "Width must be a multiple of word_size");
      }
      
      BitString result(_width);
      for (size_t word_it = 0; word_it < _width; word_it += word_size) {
        for (size_t bit_it = 0; bit_it < word_size; bit_it++) {
          result.set(_width - word_size - word_it + bit_it, at(word_it + bit_it));
        }
      }
      return result;
    }
    
    size_t popcount() const {
      size_t count = 0;
      for (size_t it = 0; it < _width; it++) {
        if (at(it)) {
          count++;
        }
      }
      return count;
    }
    
    bool is_one_hot() const {
      bool found = false;
      for (size_t it = 0; it < _width; it++) {
        if (at(it)) {
          if (found) {
            return false;
          } else {
            found = true;
          }
        }
      }
      return found;
    }
    
    size_t floor_log2() const {
      for (size_t it = _width; it-- > 0; ) {
        if (at(it)) {
          return it;
        }
      }
      return 0;
    }
    
    size_t ceil_log2() const {
      bool is_power_of_two = true;
      size_t log2 = 0;
      for (size_t it = _width; it-- > 1; ) {
        if (at(it)) {
          if (log2 == 0) {
            log2 = it;
          } else {
            is_power_of_two = false;
            break;
          }
        }
      }
      if (!is_power_of_two && log2 != 0) {
        log2++;
      }
      return log2;
    }
    
    inline size_t flog2() const { return floor_log2(); }
    inline size_t clog2() const { return ceil_log2(); }
    
    size_t find_bit(bool bit) const {
      for (size_t it = 0; it < _width; it++) {
        if (at(it) == bit) {
          return it;
        }
      }
      return _width;
    }
    
    size_t rfind_bit(bool bit) const {
      for (size_t it = _width; it-- > 0; ) {
        if (at(it) == bit) {
          return it;
        }
      }
      return _width;
    }
    
    BitString select(const BitString& then, const BitString& otherwise) const {
      if (_width != 1) {
        throw_error(Error, "Condition must be of width 1, but got BitString of width " << _width);
      }
      return at(0) ? then : otherwise;
    }
  };
  
  std::ostream& operator<<(std::ostream& stream, const BitString& bit_string) {
    bit_string.write(stream);
    return stream;
  }

  class PartialBitString {
  public:
    enum class Bool {
      False, True, Unknown
    };
  private:
    BitString _known;
    BitString _value;
  public:
    PartialBitString() {}
    explicit PartialBitString(size_t width): _known(width), _value(width) {}
    
    PartialBitString(const std::string& string):
        _known(string.size()), _value(string.size()) {
    
      for (size_t it = 0; it < string.size(); it++) {
        char chr = string[string.size() - it - 1];
        switch (chr) {
          case '0': _known.set(it, true); _value.set(it, false); break;
          case '1': _known.set(it, true); _value.set(it, true); break;
          case 'x':
          case 'X': _known.set(it, false); _value.set(it, false); break;
          default: throw_error(Error, "Invalid digit " << chr);
        }
      }
    }
    
    PartialBitString(const BitString& value):
      _known(~hdl::BitString(value.width())), _value(value) {}
    
    PartialBitString(const BitString& known, const BitString& value):
        _known(known), _value(value) {
      if (known.width() != value.width()) {
        throw_error(Error,
          "Width mismatch: known has width " << known.width() <<
          " while value has width " << value.width()
        );
      }
    }
    
    static PartialBitString from_bool(Bool value) {
      switch (value) {
        case Bool::False: return PartialBitString(BitString::from_bool(true), BitString::from_bool(false));
        case Bool::True: return PartialBitString(BitString::from_bool(true), BitString::from_bool(true));
        case Bool::Unknown: return PartialBitString(BitString::from_bool(false), BitString::from_bool(false));
      }
    }
    
    inline const size_t width() const { return _value.width(); }
    inline const BitString& known() const { return _known; }
    inline const BitString& value() const { return _value; }
    
    inline bool is_fully_known() const { return _known.is_all_ones(); }
    inline bool is_fully_unknown() const { return _known.is_zero(); }
    
    PartialBitString operator&(const PartialBitString& other) const {
      return PartialBitString(
        (_known & other._known) | (~_value & _known) | (~other._value & other._known),
        _value & other._value
      );
    }
    
    PartialBitString operator|(const PartialBitString& other) const {
      return PartialBitString(
        (_known & other._known) | (_value & _known) | (other._value & other._known),
        _value | other._value
      );
    }
    
    PartialBitString operator^(const PartialBitString& other) const {
      return PartialBitString(_known & other._known, _value ^ other._value);
    }
    
    PartialBitString operator~() const {
      return PartialBitString(_known, ~_value);
    }
    
    #define binop(name, RetType, impl, otherwise) \
      RetType name(const PartialBitString& other) const { \
        if (is_fully_known() && other.is_fully_known()) { \
          return impl; \
        } else { \
          return otherwise; \
        } \
      }
    
    binop(operator+, PartialBitString, _value + other._value, PartialBitString(width()))
    binop(operator-, PartialBitString, _value - other._value, PartialBitString(width()))
    binop(mul_u, PartialBitString, _value.mul_u(other._value), PartialBitString(width() + other.width()))
    
    #define cmp(op) binop(op, Bool, Bool(_value.op(other._value)), Bool::Unknown)
    
    cmp(eq)
    cmp(lt_u)
    cmp(lt_s)
    cmp(le_u)
    cmp(le_s)
    
    #undef cmp
    #undef binop
    
    PartialBitString concat(const PartialBitString& other) const {
      return PartialBitString(
        _known.concat(other._known),
        _value.concat(other._value)
      );
    }
    
    PartialBitString slice_width(size_t offset, size_t width) const {
      return PartialBitString(
        _known.slice_width(offset, width),
        _value.slice_width(offset, width)
      );
    }
    
    PartialBitString slice_width(const PartialBitString& offset, size_t width) const {
      if (offset.is_fully_known()) {
        return slice_width(offset.as_uint64(), width);
      } else {
        return PartialBitString(width);
      }
    }
    
    PartialBitString operator<<(size_t shift) const {
      return PartialBitString(
        _known << shift | (shift > 0 ? (~BitString(std::min(shift, width()))).zero_extend(width()) : BitString(width())),
        _value << shift
      );
    }
    
    PartialBitString shr_u(size_t shift) const {
      return PartialBitString(
        _known.shr_u(shift) | BitString::upper(width(), shift >= width() ? 0 : width() - shift),
        _value.shr_u(shift)
      );
    }
    
    PartialBitString shr_s(size_t shift) const {
      BitString upper = BitString::upper(width(), shift >= width() ? 0 : width() - shift);
      return PartialBitString(
        _known.shr_u(shift) | (_known.at(width() - 1) ? upper : BitString(width())),
        _value.shr_s(shift)
      );
    }
    
    PartialBitString operator<<(const PartialBitString& other) const {
      if (other.is_fully_known()) {
        return *this << other.as_uint64();
      } else {
        return PartialBitString(width()); 
      }
    }
    
    PartialBitString shr_u(const PartialBitString& other) const {
      if (other.is_fully_known()) {
        return shr_u(other.as_uint64());
      } else {
        return PartialBitString(width()); 
      }
    }
    
    PartialBitString shr_s(const PartialBitString& other) const {
      if (other.is_fully_known()) {
        return shr_s(other.as_uint64());
      } else {
        return PartialBitString(width()); 
      }
    }
    
    PartialBitString select(const PartialBitString& then, const PartialBitString& otherwise) const {
      if (width() != 1) {
        throw_error(Error, "Condition must be of width 1, but got PartialBitString of width " << width());
      }
      
      if (is_fully_known()) {
        if (_value[0]) {
          return then;
        } else {
          return otherwise;
        }
      } else {
        return then.merge(otherwise);
      }
    }
    
    PartialBitString merge(const PartialBitString& other) const {
      return PartialBitString(
        _known & other._known & ~(_value ^ other._value),
        _value
      );
    }
    
    bool merge_inplace(const PartialBitString& other) {
      if (other.width() != width()) {
        throw_error(Error, "PartialBitStrings must have the same width, but got " << width() << " and " << other.width());
      }
      bool changed = false;
      for (size_t it = 0; it < _known.data().size(); it++) {
        BitString::Word old = _known.data()[it];
        BitString::Word known = old & other._known.data()[it] & ~(_value.data()[it] ^ other._value.data()[it]);
        if (old != known) {
          changed = true;
          _known.data()[it] = known;
        }
      }
      return changed;
    }
    
    bool contains(const PartialBitString& other) const {
      return merge(other) == *this;
    }
    
    bool operator==(const PartialBitString& other) const {
      return _known == other._known &&
             (_value & _known) == (other._value & other._known);
    }
    
    inline bool operator!=(const PartialBitString& other) const {
      return !(*this == other);
    }
    
    size_t popcount_unknown() const {
      return (~_known).popcount();
    }
    
    uint64_t as_uint64() const {
      if (!is_fully_known()) {
        throw_error(Error, "PartialBitString is not fully known");
      }
      return _value.as_uint64();
    }
    
    hdl::BitString as_bit_string() const {
      if (!is_fully_known()) {
        throw_error(Error, "PartialBitString is not fully known");
      }
      return _value;
    }
    
    void write(std::ostream& stream) const {
      stream << width() << "'b";
      for (size_t it = width(); it-- > 0; ) {
        if (_known[it]) {
          stream << (_value[it] ? '1' : '0');
        } else {
          stream << 'x';
        }
      }
    }
  };
  
  std::ostream& operator<<(std::ostream& stream, const PartialBitString& partial_bit_string) {
    partial_bit_string.write(stream);
    return stream;
  }
}

template <>
struct std::hash<hdl::BitString> {
  std::size_t operator()(const hdl::BitString& bit_string) const {
    return bit_string.hash();
  }
};

#undef throw_error

#endif
