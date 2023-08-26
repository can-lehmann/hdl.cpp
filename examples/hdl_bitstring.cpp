// Copyright (c) 2023 Can Joshua Lehmann

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

