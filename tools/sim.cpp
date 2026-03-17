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
#include <optional>

#include "../hdl.hpp"
#include "../hdl_textir.hpp"

int main(int argc, char** argv) {
  std::optional<std::string> input_file;
  std::optional<std::string> output_file;
  std::optional<std::string> stop;
  std::optional<std::string> clock;
  std::optional<size_t> max_cycles;
  for (size_t it = 1; it < (size_t) argc; it++) {
    std::string arg = argv[it];

    #define opt(flag, var, value) \
      else if (arg == flag) { \
        it++; \
        if (it >= (size_t) argc) { \
          std::cerr << "Missing argument for " << flag << std::endl; \
          return 1; \
        } \
        arg = argv[it]; \
        var = value; \
      }

    if (false) {}
    opt("-o", output_file, arg)
    opt("--stop", stop, arg)
    opt("--clock", clock, arg)
    opt("--max-cycles", max_cycles, std::stoul(arg))
    else if (arg[0] == '-') {
      std::cerr << "Unknown argument: " << arg << std::endl;
      return 1;
    } else {
      input_file = arg;
    }
  }

  if (!input_file.has_value()) {
    std::cerr << "Missing input file." << std::endl;
    return 1;
  }

  hdl::Module module = hdl::textir::Reader::load_module(input_file.value().c_str());
  hdl::sim::Simulation sim(module);

  hdl::Value* clock_value = nullptr;
  if (clock.has_value()) {
    clock_value = module.find_input(clock.value());
  }

  hdl::Value* stop_value = nullptr;
  if (stop.has_value()) {
    stop_value = module.find_output(stop.value()).value;
  }

  hdl::sim::VCDWriter* vcd_writer = nullptr;
  if (output_file.has_value()) {
    std::ofstream* stream = new std::ofstream(output_file.value());
    if (!*stream) {
      std::cerr << "Failed to open output file: " << output_file.value() << std::endl;
      return 1;
    }
    vcd_writer = new hdl::sim::VCDWriter(*stream, module);
  }
  
  bool clock_state = false;
  size_t cycle = 0;

  while (true) {
    std::unordered_map<const hdl::Value*, hdl::BitString> inputs;
    if (clock_value) {
      inputs[clock_value] = hdl::BitString::from_bool(clock_state);
      if (clock_state) {
        cycle++;
      }
      clock_state = !clock_state;
    }
    if (inputs.size() < module.inputs().size()) {
      for (const hdl::Input* input : module.inputs()) {
        if (inputs.find(input) == inputs.end()) {
          std::cerr << "Missing value for input " << input->name << std::endl;
          return 1;
        }
      }
    }
    std::unordered_map<const hdl::Value*, hdl::BitString> values = sim.update(inputs);
    if (vcd_writer) {
      vcd_writer->write(values);
    }
    if (max_cycles.has_value() && cycle >= max_cycles.value()) {
      break;
    }
    if (stop_value && values[stop_value][0]) {
      break;
    }
  }

  std::cout << "Simulation finished after " << cycle << " cycles." << std::endl;

  return 0;
}
