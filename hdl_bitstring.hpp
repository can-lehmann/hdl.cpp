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
    
    #define BINOP(op) \
      BitString operator op(const BitString& other) const { \
        if (_width != other._width) { \
          throw_error(Error, "BitStrings must have the same width, but got " << _width << " and " << other._width); \
        } \
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
    
    void write(std::ostream& stream) const {
      stream << _width << "'b";
      for (size_t it = _width; it-- > 0;) {
        stream << ((*this)[it] ? '1' : '0');
      }
    }
    
    bool operator==(const BitString& other) {
      if (_width != other._width) {
        return false;
      }
      
      for (size_t it = 0; it + 1 < _data.size(); it++) {
        if (_data[it] != other._data[it]) {
          return false;
        }
      }
      
      Word mask = mask_lower(_width % WORD_WIDTH);
      return _data.back() == other._data.back();
    }
    
  };
  
  std::ostream& operator<<(std::ostream& stream, const BitString& bit_string) {
    bit_string.write(stream);
    return stream;
  }
}

#undef throw_error

#endif
