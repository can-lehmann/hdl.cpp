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

#include <inttypes.h>

#include "../../unittest.cpp/unittest.hpp"
#include "../hdl_bitstring.hpp"

using Test = unittest::Test;
#define assert(cond) unittest_assert(cond)

int main() {
  using BitString = hdl::BitString;
  using PartialBitString = hdl::PartialBitString;
  
  Test("BitString::from_bool").run([](){
    assert(BitString::from_bool(false) == BitString(1));
    assert(BitString::from_bool(true) == ~BitString(1));
  });
  
  Test("BitString::from_uint").run([](){
    assert(BitString::from_uint(uint8_t(0)) == BitString("00000000"));
    assert(BitString::from_uint(uint8_t(1)) == BitString("00000001"));
    assert(BitString::from_uint(uint8_t(32)) == BitString("00100000"));
    assert(BitString::from_uint(uint8_t(127)) == BitString("01111111"));
    assert(BitString::from_uint(uint8_t(~0)) == BitString("11111111"));
    
    assert(BitString::from_uint(uint32_t(~0)) == BitString("11111111111111111111111111111111"));
    assert(BitString::from_uint(uint64_t(~0)) == BitString("1111111111111111111111111111111111111111111111111111111111111111"));
  });
  
  Test("BitString::from_base_log2").run([](){
    assert(BitString::from_base_log2(1, "000001010011100101110111") == BitString("000001010011100101110111"));
    assert(BitString::from_base_log2(2, "0123") == BitString("00011011"));
    assert(BitString::from_base_log2(3, "01234567") == BitString("000001010011100101110111"));
    assert(BitString::from_base_log2(4, "0123456789abcdef") == BitString("0000000100100011010001010110011110001001101010111100110111101111"));
    assert(BitString::from_base_log2(4, "0123456789ABCDEF") == BitString("0000000100100011010001010110011110001001101010111100110111101111"));
    assert(BitString::from_base_log2(5, "0") == BitString("00000"));
  });
  
  Test("BitString::from_oct").run([](){
    assert(BitString::from_oct("0") == BitString("000"));
    assert(BitString::from_oct("7") == BitString("111"));
    assert(BitString::from_oct("01234567") == BitString("000001010011100101110111"));
  });
  
  Test("BitString::from_hex").run([](){
    assert(BitString::from_hex("0") == BitString("0000"));
    assert(BitString::from_hex("A") == BitString("1010"));
    assert(BitString::from_hex("f") == BitString("1111"));
    assert(BitString::from_hex("Abc") == BitString("101010111100"));
    assert(BitString::from_hex("10") == BitString("00010000"));
    assert(BitString::from_hex("0123456789abcdef") == BitString("0000000100100011010001010110011110001001101010111100110111101111"));
    assert(BitString::from_hex("0123456789ABCDEF") == BitString("0000000100100011010001010110011110001001101010111100110111101111"));
  });
  
  Test("BitString::width").run([](){
    for (size_t width : {1, 8, 10, 16, 32, 63, 64, 100, 1000}) {
      assert(BitString(width).width() == width);
    }
    
    assert(BitString("1111").width() == 4);
  });
  
  Test("BitString::at").run([](){
    assert(!BitString("00100000").at(0));
    assert(BitString("00100000").at(5));
    assert(!BitString("00100000").at(7));
  });
  
  Test("BitString::set").run([](){
    BitString a(10);
    a.set(0, true);
    a.set(1, true);
    a.set(9, true);
    a.set(1, false);
    assert(a == BitString("1000000001"));
  });
  
  Test("BitString::operator&").run([](){
    assert((BitString("00111010") & BitString("10001011")) == BitString("00001010"));
  });
  
  Test("BitString::operator|").run([](){
    assert((BitString("00111010") | BitString("10001011")) == BitString("10111011"));
  });
  
  Test("BitString::operator^").run([](){
    assert((BitString("00111010") ^ BitString("10001011")) == BitString("10110001"));
  });
  
  Test("BitString::operator~").run([](){
    assert((~BitString("00111010")) == BitString("11000101"));
    assert((~BitString(100)).is_all_ones());
    assert(~(~BitString(200)) == BitString(200));
    assert(~BitString(3) == BitString("111"));
  });
  
  Test("BitString::operator+").run([](){
    assert(BitString::from_uint(uint64_t(123)) + BitString::from_uint(uint64_t(456)) == BitString::from_uint(uint64_t(579)));
    assert(BitString::from_uint(uint64_t(123)) + ~BitString(64) == BitString::from_uint(uint64_t(122)));
  });
  
  Test("BitString::operator-").run([](){
    assert(BitString::from_uint(uint64_t(456)) - BitString::from_uint(uint64_t(123)) == BitString::from_uint(uint64_t(333)));
    assert(BitString::from_uint(uint64_t(123)) - ~BitString(64) == BitString::from_uint(uint64_t(124)));
  });
  
  Test("BitString::operator<<").run([](){
    assert(BitString::from_uint(uint64_t(123)) << 1 == BitString::from_uint(uint64_t(246)));
    assert(BitString::from_uint(uint64_t(1)) << 32 == BitString::from_uint(uint64_t(1) << 32));
    assert(BitString("000000000001000000000000000000000000000000") << 1 == BitString("000000000010000000000000000000000000000000"));
    assert(BitString("000000000010000000000000000000000000000000") << 1 == BitString("000000000100000000000000000000000000000000"));
    assert(BitString("000000000010000000000000000000000000000000") << 10 == BitString("100000000000000000000000000000000000000000"));
  });
  
  Test("BitString::shr_u").run([](){
    assert(BitString("100").shr_u(1) == BitString("010"));
    assert(BitString("100").shr_u(2) == BitString("001"));
    assert(BitString("100").shr_u(3) == BitString("000"));
    assert(BitString::from_uint(uint64_t(123)).shr_u(1) == BitString::from_uint(uint64_t(123 / 2)));
    assert(BitString("100000000000000000000000000000000").shr_u(1) == BitString("010000000000000000000000000000000"));
    assert(BitString("100000000000000000000000000000000").shr_u(32) == BitString("000000000000000000000000000000001"));
  });
  
  Test("BitString::shr_s").run([](){
    assert(BitString("100").shr_s(1) == BitString("110"));
    assert(BitString("100").shr_s(2) == BitString("111"));
    assert(BitString::from_uint(uint64_t(123)).shr_s(1) == BitString::from_uint(uint64_t(123 / 2)));
    assert(BitString("100000000000000000000000000000000").shr_s(33) == BitString("111111111111111111111111111111111"));
    assert(BitString("010000000000000000000000000000000").shr_s(33) == BitString("000000000000000000000000000000000"));
    assert(BitString("100000000000000000000000000000000").shr_s(31) == BitString("111111111111111111111111111111110"));
    assert(BitString("100000000000000000000000000000000").shr_s(32) == BitString("111111111111111111111111111111111"));
  });
  
  Test("BitString::zero_extend").run([](){
    assert(BitString("100").zero_extend(10) == BitString("0000000100"));
    assert(BitString("10000000000000000000000000000000").zero_extend(32 + 10) == BitString("000000000010000000000000000000000000000000"));
  });
  
  Test("BitString::truncate").run([](){
    assert(BitString("001100000010000000000000000000000000000000").truncate(32) == BitString("10000000000000000000000000000000"));
    assert(BitString("001100000010000000000000000000000000000000").truncate(3) == BitString("000"));
  });
  
  Test("BitString::write").run([](){
    #define str(bitstring) ([](){ \
      std::ostringstream stream; \
      bitstring.write_short(stream); \
      return stream.str(); \
    })()
    
    assert(str(BitString("1000")) == "4'b1000");
    assert(str(BitString("1111011")) == "7'b1111011");
    
    #undef str
  });
  
  Test("BitString::write_short").run([](){
    #define str(bitstring) ([](){ \
      std::ostringstream stream; \
      bitstring.write_short(stream); \
      return stream.str(); \
    })()
    
    assert(str(BitString::from_uint(uint64_t(8))) == "64'b1000");
    assert(str(BitString::from_uint(uint64_t(123))) == "64'b1111011");
    
    #undef str
  });
  
  Test("BitString::is_zero").run([](){
    assert(BitString(100).is_zero());
    assert(!(~BitString(100)).is_zero());
  });
  
  Test("BitString::is_all_ones").run([](){
    assert((~BitString(100)).is_all_ones());
    assert(!BitString(100).is_all_ones());
  });
  
  Test("BitString::lt_u").run([](){
    assert(BitString::from_uint(uint64_t(3)).lt_u(BitString::from_uint(uint64_t(4))));
    assert(!(BitString::from_uint(uint64_t(4)).lt_u(BitString::from_uint(uint64_t(4)))));
    assert(!(BitString::from_uint(uint64_t(5)).lt_u(BitString::from_uint(uint64_t(4)))));
    assert(BitString::from_uint(uint64_t(100)).truncate(33).lt_u(BitString("100000000000000000000000000000000")));
    assert(BitString::from_uint(uint64_t(100)).truncate(33).lt_u(BitString("010000000000000000000000000000000")));
  });
  
  Test("BitString::le_u").run([](){
    assert(BitString::from_uint(uint64_t(3)).le_u(BitString::from_uint(uint64_t(4))));
    assert(BitString::from_uint(uint64_t(4)).le_u(BitString::from_uint(uint64_t(4))));
    assert(!(BitString::from_uint(uint64_t(5)).le_u(BitString::from_uint(uint64_t(4)))));
  });
  
  Test("BitString::lt_s").run([](){
    assert(BitString("00010").lt_s(BitString("00011")));
    
    assert(BitString("1111").lt_s(BitString("0000")));
    assert(BitString("1000").lt_s(BitString("0000")));
    assert(BitString("1000").lt_s(BitString("1001")));
    assert(BitString("1000").lt_s(BitString("1111")));
    
    assert(!BitString("0000").lt_s(BitString("1111")));
    assert(!BitString("0000").lt_s(BitString("1000")));
    assert(!BitString("1001").lt_s(BitString("1000")));
    assert(!BitString("1111").lt_s(BitString("1000")));
    assert(!BitString("1111").lt_s(BitString("1111")));
  });
  
  Test("BitString::le_s").run([](){
    assert(BitString("1111").le_s(BitString("0000")));
    assert(BitString("11111").le_s(BitString("11111")));
    assert(BitString("000000").le_s(BitString("000000")));
  });
  
  Test("BitString::concat").run([](){
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
  
  Test("BitString::slice_width").run([](){
    assert(BitString("1000110").slice_width(4, 3) == BitString("100"));
    assert(BitString("100000000000000000000000000000000").slice_width(32, 1) == BitString("1"));
    assert(BitString("100000000000000000000000000000000").slice_width(31, 2) == BitString("10"));
  });
  
  Test("BitString::as_uint64").run([](){
    assert(BitString("100").as_uint64() == 4);
    assert(BitString("10000000000000000000000000000000").as_uint64() == uint64_t(1) << 31);
    assert(BitString("100000000000000000000000000000000").as_uint64() == uint64_t(1) << 32);
  });
  
  Test("BitString::reverse_words").run([](){
    assert(BitString("011110").reverse_words(2) == BitString("101101"));
    assert(BitString("011110").reverse_words(3) == BitString("110011"));
    assert(BitString("011110").reverse_words(6) == BitString("011110"));
    assert(BitString::from_hex("abcdef").reverse_words(4) == BitString::from_hex("fedcba"));
  });
  
  Test("BitString::popcount").run([](){
    assert(BitString("0000").popcount() == 0);
    assert(BitString("0100").popcount() == 1);
    assert(BitString("1111").popcount() == 4);
    assert(BitString::from_hex("0123456789abcdef").popcount() == 0 + 1 + 1 + 2 + 1 + 2 + 2 + 3 + 1 + 2 + 2 + 3 + 2 + 3 + 3 + 4);
  });
  
  Test("PartialBitString::from_bool").run([](){
    assert(PartialBitString::from_bool(PartialBitString::Bool::False) == PartialBitString("0"));
    assert(PartialBitString::from_bool(PartialBitString::Bool::True) == PartialBitString("1"));
    assert(PartialBitString::from_bool(PartialBitString::Bool::Unknown) == PartialBitString("x"));
  });
  
  Test("PartialBitString::is_fully_known").run([](){
    assert(!PartialBitString("01xx").is_fully_known());
    assert(PartialBitString("0101").is_fully_known());
  });
  
  Test("PartialBitString::is_fully_unknown").run([](){
    assert(!PartialBitString("01xx").is_fully_unknown());
    assert(PartialBitString("xxxx").is_fully_unknown());
  });
  
  Test("PartialBitString::operator&").run([](){
    assert((PartialBitString("000111xxx") & PartialBitString("01x01x01x")) == PartialBitString("00001x0xx"));
  });
  
  Test("PartialBitString::operator|").run([](){
    assert((PartialBitString("000111xxx") | PartialBitString("01x01x01x")) == PartialBitString("01x111x1x"));
  });
  
  Test("PartialBitString::operator^").run([](){
    assert((PartialBitString("000111xxx") ^ PartialBitString("01x01x01x")) == PartialBitString("01x10xxxx"));
  });
  
  Test("PartialBitString::operator~").run([](){
    assert(~PartialBitString("01x") == PartialBitString("10x"));
  });
  
  Test("PartialBitString::merge").run([](){
    assert(PartialBitString("000111xxx").merge(PartialBitString("01x01x01x")) == PartialBitString("0xxx1xxxx"));
  });
  
  Test("PartialBitString::eq").run([](){
    assert(PartialBitString("0").eq(PartialBitString("0")) == PartialBitString::Bool::True);
    assert(PartialBitString("1").eq(PartialBitString("1")) == PartialBitString::Bool::True);
    assert(PartialBitString("1").eq(PartialBitString("0")) == PartialBitString::Bool::False);
    assert(PartialBitString("x").eq(PartialBitString("x")) == PartialBitString::Bool::Unknown);
    assert(PartialBitString("10x").eq(PartialBitString("10x")) == PartialBitString::Bool::Unknown);
  });
  
  Test("PartialBitString::operator==").run([](){
    assert(PartialBitString("0") == PartialBitString("0"));
    assert(PartialBitString("1") == PartialBitString("1"));
    assert(PartialBitString("x") == PartialBitString("x"));
    assert(PartialBitString("0") != PartialBitString("1"));
    assert(PartialBitString("0") != PartialBitString("x"));
    assert(PartialBitString("1") != PartialBitString("x"));
    assert(PartialBitString("01x") == PartialBitString("01x"));
    assert(PartialBitString("01x") != PartialBitString("x10"));
    assert(PartialBitString("01x") != PartialBitString("xxx"));
  });
  
  Test("PartialBitString::write").run([](){
    #define str(bitstring) ([](){ \
      std::ostringstream stream; \
      bitstring.write(stream); \
      return stream.str(); \
    })()
    
    assert(str(PartialBitString("01xx")) == "4'b01xx");
    assert(str(PartialBitString("0101")) == "4'b0101");
    assert(str(PartialBitString("00000000")) == "8'b00000000");
    
    #undef str
  });
  
  return 0;
}
