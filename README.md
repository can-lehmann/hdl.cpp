# hdl.cpp

hdl.cpp is an RTL intermediate representation for C++.
hdl.cpp targets code generation and analysis tasks.
It makes extensive use of hashconsing and on the fly simplifications.
Besides the core IR, the library provides the following features:

- Simulation
- Visualization
- Verilog Code Generation
- Verilog Frontend via a Yosys Plugin
- DSL for hardware description
- Theorem Proving with Z3 or bit-blasting

**Note:** hdl.cpp is currently highly unstable.

## Overview

`hdl::Module` is the core class of hdl.cpp's IR.
It represents a hardware description in register-transfer level.

Here is an example of a 4 bit counter:

```cpp
hdl::Module module("top");

hdl::Value* clock = module.input("clock", 1);
hdl::Reg* counter = module.reg(hdl::BitString("0000"), clock);

counter->next = module.op(hdl::Op::Kind::Add, {
  counter,
  module.constant(hdl::BitString("0001"))
});

module.output("counter", counter);
```

### Constants

The `Constant* Module::constant(const BitString& bit_string)` method is used to create a Constant from a BitString.
Constants are deduplicated using hashconsing.

Arbitrary bit values are represented by the `hdl::BitString` class.
The following constructors are available:

- `BitString(const std::string& string)` where `string` is a binary number
- `static BitString from_bool(bool value)` where the resulting BitString is of width 1
- `template <class T> static BitString from_uint(T value)` where the resulting BitString is of width `sizeof(T) * 8`

BitStrings do not record the signedness of their value.
Instead signedness is part of the operators applied on the BitString.

### Operators

Operators are created using the `Value* Module::op(Op::Kind kind, const std::vector<Value*>& args)` method.
Instantiated operators are deduplicated using hashconsing.

On the fly simplifications will also be applied.
If all arguments of an operator are constants, constant propagation is always applied.
Additionally simple local simplifications are applied.
E.g. `module.op(hdl::Op::Kind::Not, {module.op(hdl::Op::Kind::Not, {a})}) == a` is true.

The following operators are available for representing combinatorial logic.

| Operator | Type                       |
| -------- | -------------------------- |
| And      | `(a, a) -> a`              |
| Or       | `(a, a) -> a`              |
| Xor      | `(a, a) -> a`              |
| Not      | `a -> a`                   |
| Add      | `(a, a) -> a`              |
| Sub      | `(a, a) -> a`              |
| Mul      | `(a, b) -> a + b`          |
| Eq       | `(a, a) -> 1`              |
| LtU      | `(a, a) -> 1`              |
| LtS      | `(a, a) -> 1`              |
| LeU      | `(a, a) -> 1`              |
| LeS      | `(a, a) -> 1`              |
| Concat   | `(a, b) -> a + b`          |
| Slice    | `(a, b, Constant[w]) -> w` |
| Shl      | `(a, b) -> a`              |
| ShrU     | `(a, b) -> a`              |
| ShrS     | `(a, b) -> a`              |
| Select   | `(1, a, a) -> a`           |

### Memories

Memories can be created using the `Memory* Module::memory(size_t width, size_t size)` method.

### Simulation

Modules can be simulated using `hdl::sim::Simulation`.
The `hdl::sim::Simulation::update` method performs a simulation step with the given inputs.
Outputs can be read back using `hdl::sim::Simulation::outputs` or `hdl::sim::Simulation::find_output`.

```cpp
bool clock = false;
for (size_t step = 0; step < 32; step++) {
  simulation.update({hdl::BitString::from_bool(clock)});
  std::cout << simulation.outputs()[0] << std::endl;
  clock = !clock;
}
```

### Visualization

The IR can be visualized using GraphViz.

```cpp
hdl::graphviz::Printer gv_printer(module);
gv_printer.save("graph.gv");
```

### Code Generation

`hdl::verilog::Printer` is used to generate Verilog source code for a given module.

```cpp
hdl::verilog::Printer printer(module);
printer.print(std::cout);
```

### Theorem Proving

hdl.cpp supports theorem proving using Z3 and using generic SAT solvers using bit-blasting.
Check out the `hdl_proof_z3.cpp` and `hdl_proof.cpp` examples respectively.

### DSL

When writing test cases for analysis passes, it may be cumbersome to use the `hdl::Module` API.
This is why hdl.cpp provides a domain specific language for high level hardware description inside C++.
Check out the `hdl_dsl.cpp` example.

## License

Copyright 2023 Can Joshua Lehmann

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
