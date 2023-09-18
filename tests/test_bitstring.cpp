// Copyright (c) 2023 Can Joshua Lehmann

#include <inttypes.h>

#include "../../unittest.cpp/unittest.hpp"
#include "../hdl_bitstring.hpp"

using Test = unittest::Test;
#define assert(cond) unittest_assert(cond)

int main() {
  using BitString = hdl::BitString;
  
  Test("from_bool", [](){
    assert(BitString::from_bool(false) == BitString(1));
    assert(BitString::from_bool(true) == ~BitString(1));
  });
  
  Test("from_uint", [](){
    assert(BitString::from_uint(uint8_t(0)) == BitString("00000000"));
    assert(BitString::from_uint(uint8_t(1)) == BitString("00000001"));
    assert(BitString::from_uint(uint8_t(32)) == BitString("00100000"));
    assert(BitString::from_uint(uint8_t(127)) == BitString("01111111"));
    assert(BitString::from_uint(uint8_t(~0)) == BitString("11111111"));
    
    assert(BitString::from_uint(uint32_t(~0)) == BitString("11111111111111111111111111111111"));
    assert(BitString::from_uint(uint64_t(~0)) == BitString("1111111111111111111111111111111111111111111111111111111111111111"));
  });
  
  Test("width", [](){
    for (size_t width : {1, 8, 10, 16, 32, 63, 64, 100, 1000}) {
      assert(BitString(width).width() == width);
    }
    
    assert(BitString("1111").width() == 4);
  });
  
  Test("at", [](){
    assert(!BitString("00100000").at(0));
    assert(BitString("00100000").at(5));
    assert(!BitString("00100000").at(7));
  });
  
  Test("set", [](){
    BitString a(10);
    a.set(0, true);
    a.set(1, true);
    a.set(9, true);
    a.set(1, false);
    assert(a == BitString("1000000001"));
  });
  
  Test("operator&", [](){
    assert((BitString("00111010") & BitString("10001011")) == BitString("00001010"));
  });
  
  Test("operator|", [](){
    assert((BitString("00111010") | BitString("10001011")) == BitString("10111011"));
  });
  
  Test("operator^", [](){
    assert((BitString("00111010") ^ BitString("10001011")) == BitString("10110001"));
  });
  
  Test("operator~", [](){
    assert((~BitString("00111010")) == BitString("11000101"));
    assert((~BitString(100)).is_all_ones());
    assert(~(~BitString(200)) == BitString(200));
    assert(~BitString(3) == BitString("111"));
  });
  
  Test("operator+", [](){
    assert(BitString::from_uint(uint64_t(123)) + BitString::from_uint(uint64_t(456)) == BitString::from_uint(uint64_t(579)));
    assert(BitString::from_uint(uint64_t(123)) + ~BitString(64) == BitString::from_uint(uint64_t(122)));
  });
  
  Test("operator-", [](){
    assert(BitString::from_uint(uint64_t(456)) - BitString::from_uint(uint64_t(123)) == BitString::from_uint(uint64_t(333)));
    assert(BitString::from_uint(uint64_t(123)) - ~BitString(64) == BitString::from_uint(uint64_t(124)));
  });
  
  Test("operator<<", [](){
    assert(BitString::from_uint(uint64_t(123)) << 1 == BitString::from_uint(uint64_t(246)));
    assert(BitString::from_uint(uint64_t(1)) << 32 == BitString::from_uint(uint64_t(1) << 32));
    assert(BitString("000000000001000000000000000000000000000000") << 1 == BitString("000000000010000000000000000000000000000000"));
    assert(BitString("000000000010000000000000000000000000000000") << 1 == BitString("000000000100000000000000000000000000000000"));
    assert(BitString("000000000010000000000000000000000000000000") << 10 == BitString("100000000000000000000000000000000000000000"));
  });
  
  Test("shr_u", [](){
    assert(BitString("100").shr_u(1) == BitString("010"));
    assert(BitString("100").shr_u(2) == BitString("001"));
    assert(BitString("100").shr_u(3) == BitString("000"));
    assert(BitString::from_uint(uint64_t(123)).shr_u(1) == BitString::from_uint(uint64_t(123 / 2)));
    assert(BitString("100000000000000000000000000000000").shr_u(1) == BitString("010000000000000000000000000000000"));
    assert(BitString("100000000000000000000000000000000").shr_u(32) == BitString("000000000000000000000000000000001"));
  });
  
  Test("shr_s", [](){
    assert(BitString("100").shr_s(1) == BitString("110"));
    assert(BitString("100").shr_s(2) == BitString("111"));
    assert(BitString::from_uint(uint64_t(123)).shr_s(1) == BitString::from_uint(uint64_t(123 / 2)));
    assert(BitString("100000000000000000000000000000000").shr_s(33) == BitString("111111111111111111111111111111111"));
    assert(BitString("010000000000000000000000000000000").shr_s(33) == BitString("000000000000000000000000000000000"));
    assert(BitString("100000000000000000000000000000000").shr_s(31) == BitString("111111111111111111111111111111110"));
    assert(BitString("100000000000000000000000000000000").shr_s(32) == BitString("111111111111111111111111111111111"));
  });
  
  Test("zero_extend", [](){
    assert(BitString("100").zero_extend(10) == BitString("0000000100"));
    assert(BitString("10000000000000000000000000000000").zero_extend(32 + 10) == BitString("000000000010000000000000000000000000000000"));
  });
  
  Test("truncate", [](){
    assert(BitString("001100000010000000000000000000000000000000").truncate(32) == BitString("10000000000000000000000000000000"));
    assert(BitString("001100000010000000000000000000000000000000").truncate(3) == BitString("000"));
  });
  
  Test("write", [](){
    #define str(bitstring) ([](){ \
      std::ostringstream stream; \
      bitstring.write_short(stream); \
      return stream.str(); \
    })()
    
    assert(str(BitString("1000")) == "4'b1000");
    assert(str(BitString("1111011")) == "7'b1111011");
    
    #undef str
  });
  
  Test("write_short", [](){
    #define str(bitstring) ([](){ \
      std::ostringstream stream; \
      bitstring.write_short(stream); \
      return stream.str(); \
    })()
    
    assert(str(BitString::from_uint(uint64_t(8))) == "64'b1000");
    assert(str(BitString::from_uint(uint64_t(123))) == "64'b1111011");
    
    #undef str
  });
  
  Test("is_zero", [](){
    assert(BitString(100).is_zero());
    assert(!(~BitString(100)).is_zero());
  });
  
  Test("is_all_ones", [](){
    assert((~BitString(100)).is_all_ones());
    assert(!BitString(100).is_all_ones());
  });
  
  Test("lt_u", [](){
    assert(BitString::from_uint(uint64_t(3)).lt_u(BitString::from_uint(uint64_t(4))));
    assert(!(BitString::from_uint(uint64_t(4)).lt_u(BitString::from_uint(uint64_t(4)))));
    assert(!(BitString::from_uint(uint64_t(5)).lt_u(BitString::from_uint(uint64_t(4)))));
    assert(BitString::from_uint(uint64_t(100)).truncate(33).lt_u(BitString("100000000000000000000000000000000")));
    assert(BitString::from_uint(uint64_t(100)).truncate(33).lt_u(BitString("010000000000000000000000000000000")));
  });
  
  Test("le_u", [](){
    assert(BitString::from_uint(uint64_t(3)).le_u(BitString::from_uint(uint64_t(4))));
    assert(BitString::from_uint(uint64_t(4)).le_u(BitString::from_uint(uint64_t(4))));
    assert(!(BitString::from_uint(uint64_t(5)).le_u(BitString::from_uint(uint64_t(4)))));
  });
  
  Test("concat", [](){
    assert(BitString("100").concat(BitString("0110")) == BitString("1000110"));
    assert(
      BitString("10000000000000000000000000000000").concat(BitString("10000000000000000000000000000000"))
      == BitString("1000000000000000000000000000000010000000000000000000000000000000")
    );
    assert(
      BitString("10000000000000000000000000000000").concat(BitString("1000000000000000000000"))
      == BitString("100000000000000000000000000000001000000000000000000000")
    );
  });
  
  Test("slice_width", [](){
    assert(BitString("1000110").slice_width(4, 3) == BitString("100"));
    assert(BitString("100000000000000000000000000000000").slice_width(32, 1) == BitString("1"));
    assert(BitString("100000000000000000000000000000000").slice_width(31, 2) == BitString("10"));
  });
  
  Test("as_uint64", [](){
    assert(BitString("100").as_uint64() == 4);
    assert(BitString("10000000000000000000000000000000").as_uint64() == uint64_t(1) << 31);
    assert(BitString("100000000000000000000000000000000").as_uint64() == uint64_t(1) << 32);
  });
  
  
  return 0;
}
