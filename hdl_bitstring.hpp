// Copyright (c) 2023 Can Joshua Lehmann

#ifndef HDL_BITSTRING_HPP
#define HDL_BITSTRING_HPP

#include <inttypes.h>
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
  private:
    using Word = uint32_t;
    using DoubleWord = uint64_t;
    static constexpr const size_t WORD_WIDTH = sizeof(Word) * 8;
    
    std::vector<Word> _data;
    size_t _width = 0;
    
    static size_t word_count(size_t width) {
      return width / WORD_WIDTH + (width % WORD_WIDTH == 0 ? 0 : 1);
    }
    
    static Word mask_lower(size_t bits) {
      return bits == WORD_WIDTH ? ~Word(0) : (Word(1) << bits) - 1;
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
    
    inline size_t width() const { return _width; }
    
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
    
    void write(std::ostream& stream) const {
      stream << _width << "'b";
      for (size_t it = _width; it-- > 0;) {
        stream << ((*this)[it] ? '1' : '0');
      }
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
      
      Word mask = mask_lower(_width % WORD_WIDTH);
      return (_data.back() & mask) == (other._data.back() & mask);
    }
    
    size_t hash() const {
      size_t value = std::hash<size_t>()(_width);
      for (size_t it = 0; it < _data.size(); it++) {
        value ^= std::hash<Word>()(_data[it]);
      }
      return value;
    }
    
    bool lt_u(const BitString& other) const {
      ensure_same_width(other);
      
      Word mask = mask_lower(_width % WORD_WIDTH);
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
    
  };
  
  std::ostream& operator<<(std::ostream& stream, const BitString& bit_string) {
    bit_string.write(stream);
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
