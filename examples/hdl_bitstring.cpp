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

#include <iostream>

#include "../hdl_bitstring.hpp"

int main() {
  hdl::BitString a(10);
  std::cout << "a \t= " << a << std::endl;
  std::cout << "~a \t= " << ~a << std::endl;
  
  hdl::BitString b("0011100101");
  std::cout << "b \t= " << b << std::endl;
  std::cout << "~b \t= " << ~b << std::endl;
  
  hdl::BitString c("0110001010");
  std::cout << "c \t= " << c << std::endl;
  
  std::cout << "b & c \t= " << (b & c) << std::endl;
  std::cout << "b | c \t= " << (b | c) << std::endl;
  std::cout << "b ^ c \t= " << (b ^ c) << std::endl;
  
  std::cout << "c << 3 \t= " << (c << 3) << std::endl;
  
  std::cout << "b * c \t= " << (b.mul_u(c)) << std::endl;
  
  return 0;
}

