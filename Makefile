all: tests/test_hdl tests/test_bitstring tests/test_textir examples/hdl examples/hdl_bitstring examples/hdl_dsl examples/hdl_proof examples/hdl_proof_z3 tools/hdl.so

test: tests/test_hdl tests/test_bitstring tests/test_textir tests/test_flatten
	./tests/test_bitstring
	./tests/test_hdl
	./tests/test_textir
	./tests/test_flatten

tools/hdl.so: tools/yosys_plugin.cpp hdl.hpp hdl_bitstring.hpp hdl_textir.hpp hdl_yosys.hpp
	cd tools; yosys-config --build hdl.so yosys_plugin.cpp

tests/test_hdl: tests/test_hdl.cpp hdl.hpp hdl_bitstring.hpp
	clang++ tests/test_hdl.cpp -o tests/test_hdl

tests/test_bitstring: tests/test_bitstring.cpp hdl_bitstring.hpp
	clang++ tests/test_bitstring.cpp -o tests/test_bitstring

tests/test_textir: tests/test_textir.cpp hdl.hpp hdl_bitstring.hpp hdl_textir.hpp
	clang++ tests/test_textir.cpp -o tests/test_textir

tests/test_flatten: tests/test_flatten.cpp hdl.hpp hdl_bitstring.hpp hdl_flatten.hpp
	clang++ tests/test_flatten.cpp -o tests/test_flatten

examples/hdl: examples/hdl.cpp hdl.hpp hdl_bitstring.hpp
	clang++ examples/hdl.cpp -o examples/hdl

examples/hdl_bitstring: examples/hdl_bitstring.cpp hdl_bitstring.hpp
	clang++ examples/hdl_bitstring.cpp -o examples/hdl_bitstring

examples/hdl_dsl: examples/hdl_dsl.cpp hdl.hpp hdl_bitstring.hpp hdl_dsl.hpp
	clang++ examples/hdl_dsl.cpp -o examples/hdl_dsl

examples/hdl_proof: examples/hdl_proof.cpp hdl.hpp hdl_bitstring.hpp hdl_proof.hpp
	clang++ examples/hdl_proof.cpp -o examples/hdl_proof

examples/hdl_proof_z3: examples/hdl_proof_z3.cpp hdl.hpp hdl_bitstring.hpp hdl_proof_z3.hpp
	clang++ -I/usr/include/z3 -lz3 examples/hdl_proof_z3.cpp -o examples/hdl_proof_z3


