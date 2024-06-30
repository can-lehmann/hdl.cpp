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
#include "../hdl_analysis.hpp"

using Test = unittest::Test;
#define assert(cond) unittest_assert(cond)

int main() {
  using Interval = hdl::analysis::Interval;
  
  Test("Interval::Interval").run([](){
    assert(
      Interval(hdl::PartialBitString::Bool::False) ==
      Interval(hdl::BitString::from_bool(false))
    );
    
    assert(
      Interval(hdl::PartialBitString::Bool::True) ==
      Interval(hdl::BitString::from_bool(true))
    );
    
    assert(
      Interval(hdl::PartialBitString::Bool::Unknown) ==
      Interval(hdl::BitString::from_bool(false), hdl::BitString::from_bool(true))
    );
    
    assert(
      Interval(hdl::PartialBitString("0xx1x00")) ==
      Interval(
        hdl::BitString("0001000"),
        hdl::BitString("0111100")
      )
    );
    
    assert(
      Interval(
        hdl::BitString("000"),
        hdl::BitString("111")
      ) ==
      Interval(
        hdl::BitString("001"),
        hdl::BitString("000")
      )
    );
  });
  
  Test("Interval::width").run([](){
    assert(Interval(hdl::BitString("100"), hdl::BitString("001")).width() == 3);
  });
  
  Test("Interval::has_unsigned_wrap").run([](){
    assert(!Interval(hdl::BitString("100"), hdl::BitString("100")).has_unsigned_wrap());
    assert(!Interval(hdl::BitString("100"), hdl::BitString("110")).has_unsigned_wrap());
    assert(Interval(hdl::BitString("100"), hdl::BitString("001")).has_unsigned_wrap());
    assert(Interval(hdl::BitString("111"), hdl::BitString("000")).has_unsigned_wrap());
  });
  
  Test("Interval::merge (size 1)").run([](){
    assert(
      Interval(hdl::BitString("101")).merge(hdl::BitString("111")) ==
      Interval(hdl::BitString("101"), hdl::BitString("111"))
    );
    
    assert(
      Interval(hdl::BitString("001")).merge(hdl::BitString("111")) ==
      Interval(hdl::BitString("111"), hdl::BitString("001"))
    );
    
    assert(
      Interval(hdl::BitString("000")).merge(hdl::BitString("111")) ==
      Interval(hdl::BitString("111"), hdl::BitString("000"))
    );
    
    assert(
      Interval(hdl::BitString("001")).merge(hdl::BitString("110")) ==
      Interval(hdl::BitString("110"), hdl::BitString("001"))
    );
    
    assert(
      Interval(hdl::BitString("011")).merge(hdl::BitString("110")) ==
      Interval(hdl::BitString("011"), hdl::BitString("110"))
    );
    
    assert(
      Interval(hdl::BitString("0")).merge(hdl::BitString("1")) ==
      Interval(hdl::BitString("0"), hdl::BitString("1"))
    );
  });
  
  Test("Interval::merge (size > 1)").run([](){
    // Extend
    //   --[--]------
    //   ----[--]----
    assert(
      Interval(hdl::BitString("0010"), hdl::BitString("0100"))
      .merge(Interval(hdl::BitString("0011"), hdl::BitString("0110"))) ==
      Interval(hdl::BitString("0010"), hdl::BitString("0110"))
    );
    
    // Disjoint
    //   --[--]------
    //   -------[--]-
    assert(
      Interval(hdl::BitString("0010"), hdl::BitString("0011"))
      .merge(Interval(hdl::BitString("0100"), hdl::BitString("0110"))) ==
      Interval(hdl::BitString("0010"), hdl::BitString("0110"))
    );
    
    // Contained
    //   --[-----]---
    //   ---[--]-----
    assert(
      Interval(hdl::BitString("0010"), hdl::BitString("0110"))
      .merge(Interval(hdl::BitString("0011"), hdl::BitString("0100"))) ==
      Interval(hdl::BitString("0010"), hdl::BitString("0110"))
    );
  });
  
  Test("Interval::merge (size > 1, unsigned wrapping)").run([](){
    // Bridge
    //   ----[----]----
    //   ------]-[-----
    assert(
      Interval(hdl::BitString("1000"), hdl::BitString("0110"))
      .merge(Interval(hdl::BitString("0011"), hdl::BitString("1001"))) ==
      Interval(hdl::BitString("0000"), hdl::BitString("1111"))
    );
    
    // Extend
    //   ----[-]-------
    //   -----]-------[
    assert(
      Interval(hdl::BitString("0010"), hdl::BitString("0101"))
      .merge(Interval(hdl::BitString("1111"), hdl::BitString("0011"))) ==
      Interval(hdl::BitString("1111"), hdl::BitString("0101"))
    );
    
    // Disjoint
    //   ----[-]-------
    //   -]-----------[
    assert(
      Interval(hdl::BitString("0010"), hdl::BitString("0101"))
      .merge(Interval(hdl::BitString("1111"), hdl::BitString("0000"))) ==
      Interval(hdl::BitString("1111"), hdl::BitString("0101"))
    );
    
    // Disjoint 2
    //   ---------[-]--
    //   -]-----------[
    assert(
      Interval(hdl::BitString("1010"), hdl::BitString("1011"))
      .merge(Interval(hdl::BitString("1111"), hdl::BitString("0000"))) ==
      Interval(hdl::BitString("1010"), hdl::BitString("0000"))
    );
  });
  
  Test("Interval::operator~").run([](){
    assert(
      ~Interval(hdl::BitString("101")) ==
      Interval(hdl::BitString("010"))
    );
    
    assert(
      ~Interval(hdl::BitString("001"), hdl::BitString("010")) ==
      Interval(hdl::BitString("101"), hdl::BitString("110"))
    );
    
    assert(
      ~Interval(hdl::BitString("111"), hdl::BitString("000")) ==
      Interval(hdl::BitString("111"), hdl::BitString("000"))
    );
    
    assert(
      ~Interval(hdl::BitString("110"), hdl::BitString("000")) ==
      Interval(hdl::BitString("111"), hdl::BitString("001"))
    );
    
    assert(
      ~Interval(hdl::BitString("000"), hdl::BitString("111")) ==
      Interval(hdl::BitString("000"), hdl::BitString("111"))
    );
  });
  
  Test("Interval::operator+").run([](){
    // Size 1
    
    assert(
      Interval(hdl::BitString("0010")) +
      Interval(hdl::BitString("0011")) ==
      Interval(hdl::BitString("0101"))
    );
    
    assert(
      Interval(hdl::BitString("1111")) +
      Interval(hdl::BitString("1110")) ==
      Interval(hdl::BitString("1101"))
    );
    
    // Size > 1
    assert(
      Interval(hdl::BitString("0000"), hdl::BitString("0111")) +
      Interval(hdl::BitString("0000"), hdl::BitString("1000")) ==
      Interval(hdl::BitString("0000"), hdl::BitString("1111"))
    );
    
    assert(
      Interval(hdl::BitString("0000"), hdl::BitString("1000")) +
      Interval(hdl::BitString("0000"), hdl::BitString("1000")) ==
      Interval(hdl::BitString("0000"), hdl::BitString("1111"))
    );
    
    assert(
      Interval(hdl::BitString("0000"), hdl::BitString("0100")) +
      Interval(hdl::BitString("0000"), hdl::BitString("0100")) ==
      Interval(hdl::BitString("0000"), hdl::BitString("1000"))
    );
    
    assert(
      Interval(hdl::BitString("1110"), hdl::BitString("0000")) +
      Interval(hdl::BitString("0000"), hdl::BitString("0100")) ==
      Interval(hdl::BitString("1110"), hdl::BitString("0100"))
    );
    
    // [0, MAX] interval
    assert(
      Interval(hdl::BitString("0000"), hdl::BitString("1111")) +
      Interval(hdl::BitString("0000"), hdl::BitString("1111")) ==
      Interval(hdl::BitString("0000"), hdl::BitString("1111"))
    );
    
    assert(
      Interval(hdl::BitString("0000"), hdl::BitString("1111")) +
      Interval(hdl::BitString("0001")) ==
      Interval(hdl::BitString("0000"), hdl::BitString("1111"))
    );
  });
  
  Test("Interval::operator-").run([](){
    assert(
      Interval(hdl::BitString("0010")) -
      Interval(hdl::BitString("0011")) ==
      Interval(hdl::BitString("1111"))
    );
    
    // [0, MAX] interval
    assert(
      Interval(hdl::BitString("0000"), hdl::BitString("1111")) -
      Interval(hdl::BitString("0000"), hdl::BitString("1111")) ==
      Interval(hdl::BitString("0000"), hdl::BitString("1111"))
    );
    
    assert(
      Interval(hdl::BitString("0000"), hdl::BitString("1111")) -
      Interval(hdl::BitString("0001")) ==
      Interval(hdl::BitString("0000"), hdl::BitString("1111"))
    );
  });
  
  Test("Interval::select").run([](){
    assert(
      Interval(hdl::PartialBitString::Bool::Unknown).select(
        hdl::BitString("0000"), hdl::BitString("0010")
      ) ==
      Interval(hdl::BitString("0000"), hdl::BitString("0010"))
    );
    
    assert(
      Interval(hdl::PartialBitString::Bool::True).select(
        hdl::BitString("0000"), hdl::BitString("0010")
      ) ==
      Interval(hdl::BitString("0000"))
    );
    
    assert(
      Interval(hdl::PartialBitString::Bool::False).select(
        hdl::BitString("0000"), hdl::BitString("0010")
      ) ==
      Interval(hdl::BitString("0010"))
    );
  });
  
  Test("Interval::as_partial_bit_string").run([](){
    assert(
      Interval(hdl::PartialBitString("101x0xx")).as_partial_bit_string() ==
      hdl::PartialBitString("101xxxx")
    );
    
    assert(
      Interval(hdl::BitString("0000"), hdl::BitString("1111")).as_partial_bit_string() ==
      hdl::PartialBitString("xxxx")
    );
    
    assert(
      Interval(hdl::BitString("0010"), hdl::BitString("0110")).as_partial_bit_string() ==
      hdl::PartialBitString("0xxx")
    );
  });
  
  return 0;
}
