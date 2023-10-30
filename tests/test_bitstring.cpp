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
  
  Test("from_bool").run([](){
    assert(BitString::from_bool(false) == BitString(1));
    assert(BitString::from_bool(true) == ~BitString(1));
  });
  
  Test("from_uint").run([](){
    assert(BitString::from_uint(uint8_t(0)) == BitString("00000000"));
    assert(BitString::from_uint(uint8_t(1)) == BitString("00000001"));
    assert(BitString::from_uint(uint8_t(32)) == BitString("00100000"));
    assert(BitString::from_uint(uint8_t(127)) == BitString("01111111"));
    assert(BitString::from_uint(uint8_t(~0)) == BitString("11111111"));
    
    assert(BitString::from_uint(uint32_t(~0)) == BitString("11111111111111111111111111111111"));
    assert(BitString::from_uint(uint64_t(~0)) == BitString("1111111111111111111111111111111111111111111111111111111111111111"));
  });
  
  Test("width").run([](){
    for (size_t width : {1, 8, 10, 16, 32, 63, 64, 100, 1000}) {
      assert(BitString(width).width() == width);
    }
    
    assert(BitString("1111").width() == 4);
  });
  
  Test("at").run([](){
    assert(!BitString("00100000").at(0));
    assert(BitString("00100000").at(5));
    assert(!BitString("00100000").at(7));
  });
  
  Test("set").run([](){
    BitString a(10);
    a.set(0, true);
    a.set(1, true);
    a.set(9, true);
    a.set(1, false);
    assert(a == BitString("1000000001"));
  });
  
  Test("operator&").run([](){
    assert((BitString("00111010") & BitString("10001011")) == BitString("00001010"));
  });
  
  Test("operator|").run([](){
    assert((BitString("00111010") | BitString("10001011")) == BitString("10111011"));
  });
  
  Test("operator^").run([](){
    assert((BitString("00111010") ^ BitString("10001011")) == BitString("10110001"));
  });
  
  Test("operator~").run([](){
    assert((~BitString("00111010")) == BitString("11000101"));
    assert((~BitString(100)).is_all_ones());
    assert(~(~BitString(200)) == BitString(200));
    assert(~BitString(3) == BitString("111"));
  });
  
  Test("operator+").run([](){
    assert(BitString::from_uint(uint64_t(123)) + BitString::from_uint(uint64_t(456)) == BitString::from_uint(uint64_t(579)));
    assert(BitString::from_uint(uint64_t(123)) + ~BitString(64) == BitString::from_uint(uint64_t(122)));
  });
  
  Test("operator-").run([](){
    assert(BitString::from_uint(uint64_t(456)) - BitString::from_uint(uint64_t(123)) == BitString::from_uint(uint64_t(333)));
    assert(BitString::from_uint(uint64_t(123)) - ~BitString(64) == BitString::from_uint(uint64_t(124)));
  });
  
  Test("operator<<").run([](){
    assert(BitString::from_uint(uint64_t(123)) << 1 == BitString::from_uint(uint64_t(246)));
    assert(BitString::from_uint(uint64_t(1)) << 32 == BitString::from_uint(uint64_t(1) << 32));
    assert(BitString("000000000001000000000000000000000000000000") << 1 == BitString("000000000010000000000000000000000000000000"));
    assert(BitString("000000000010000000000000000000000000000000") << 1 == BitString("000000000100000000000000000000000000000000"));
    assert(BitString("000000000010000000000000000000000000000000") << 10 == BitString("100000000000000000000000000000000000000000"));
  });
  
  Test("shr_u").run([](){
    assert(BitString("100").shr_u(1) == BitString("010"));
    assert(BitString("100").shr_u(2) == BitString("001"));
    assert(BitString("100").shr_u(3) == BitString("000"));
    assert(BitString::from_uint(uint64_t(123)).shr_u(1) == BitString::from_uint(uint64_t(123 / 2)));
    assert(BitString("100000000000000000000000000000000").shr_u(1) == BitString("010000000000000000000000000000000"));
    assert(BitString("100000000000000000000000000000000").shr_u(32) == BitString("000000000000000000000000000000001"));
  });
  
  Test("shr_s").run([](){
    assert(BitString("100").shr_s(1) == BitString("110"));
    assert(BitString("100").shr_s(2) == BitString("111"));
    assert(BitString::from_uint(uint64_t(123)).shr_s(1) == BitString::from_uint(uint64_t(123 / 2)));
    assert(BitString("100000000000000000000000000000000").shr_s(33) == BitString("111111111111111111111111111111111"));
    assert(BitString("010000000000000000000000000000000").shr_s(33) == BitString("000000000000000000000000000000000"));
    assert(BitString("100000000000000000000000000000000").shr_s(31) == BitString("111111111111111111111111111111110"));
    assert(BitString("100000000000000000000000000000000").shr_s(32) == BitString("111111111111111111111111111111111"));
  });
  
  Test("zero_extend").run([](){
    assert(BitString("100").zero_extend(10) == BitString("0000000100"));
    assert(BitString("10000000000000000000000000000000").zero_extend(32 + 10) == BitString("000000000010000000000000000000000000000000"));
  });
  
  Test("truncate").run([](){
    assert(BitString("001100000010000000000000000000000000000000").truncate(32) == BitString("10000000000000000000000000000000"));
    assert(BitString("001100000010000000000000000000000000000000").truncate(3) == BitString("000"));
  });
  
  Test("write").run([](){
    #define str(bitstring) ([](){ \
      std::ostringstream stream; \
      bitstring.write_short(stream); \
      return stream.str(); \
    })()
    
    assert(str(BitString("1000")) == "4'b1000");
    assert(str(BitString("1111011")) == "7'b1111011");
    
    #undef str
  });
  
  Test("write_short").run([](){
    #define str(bitstring) ([](){ \
      std::ostringstream stream; \
      bitstring.write_short(stream); \
      return stream.str(); \
    })()
    
    assert(str(BitString::from_uint(uint64_t(8))) == "64'b1000");
    assert(str(BitString::from_uint(uint64_t(123))) == "64'b1111011");
    
    #undef str
  });
  
  Test("is_zero").run([](){
    assert(BitString(100).is_zero());
    assert(!(~BitString(100)).is_zero());
  });
  
  Test("is_all_ones").run([](){
    assert((~BitString(100)).is_all_ones());
    assert(!BitString(100).is_all_ones());
  });
  
  Test("lt_u").run([](){
    assert(BitString::from_uint(uint64_t(3)).lt_u(BitString::from_uint(uint64_t(4))));
    assert(!(BitString::from_uint(uint64_t(4)).lt_u(BitString::from_uint(uint64_t(4)))));
    assert(!(BitString::from_uint(uint64_t(5)).lt_u(BitString::from_uint(uint64_t(4)))));
    assert(BitString::from_uint(uint64_t(100)).truncate(33).lt_u(BitString("100000000000000000000000000000000")));
    assert(BitString::from_uint(uint64_t(100)).truncate(33).lt_u(BitString("010000000000000000000000000000000")));
  });
  
  Test("le_u").run([](){
    assert(BitString::from_uint(uint64_t(3)).le_u(BitString::from_uint(uint64_t(4))));
    assert(BitString::from_uint(uint64_t(4)).le_u(BitString::from_uint(uint64_t(4))));
    assert(!(BitString::from_uint(uint64_t(5)).le_u(BitString::from_uint(uint64_t(4)))));
  });
  
  Test("lt_s").run([](){
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
  
  Test("le_s").run([](){
    assert(BitString("1111").le_s(BitString("0000")));
    assert(BitString("11111").le_s(BitString("11111")));
    assert(BitString("000000").le_s(BitString("000000")));
  });
  
  Test("concat").run([](){
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
  
  Test("slice_width").run([](){
    assert(BitString("1000110").slice_width(4, 3) == BitString("100"));
    assert(BitString("100000000000000000000000000000000").slice_width(32, 1) == BitString("1"));
    assert(BitString("100000000000000000000000000000000").slice_width(31, 2) == BitString("10"));
  });
  
  Test("as_uint64").run([](){
    assert(BitString("100").as_uint64() == 4);
    assert(BitString("10000000000000000000000000000000").as_uint64() == uint64_t(1) << 31);
    assert(BitString("100000000000000000000000000000000").as_uint64() == uint64_t(1) << 32);
  });
  
  
  return 0;
}
