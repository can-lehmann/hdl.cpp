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

#include "kernel/yosys.h"

#include "../hdl.hpp"
#include "../hdl_yosys.hpp"
#include "../hdl_textir.hpp"

USING_YOSYS_NAMESPACE

PRIVATE_NAMESPACE_BEGIN

struct HdlBackend : public Backend {
  enum class Mode {
    TextIr, Verilog, Show
  };

  HdlBackend(): Backend("hdl", "write design to hdl.cpp text IR") {}
  
  void execute(std::ostream* &file,
               std::string filename,
               std::vector<std::string> args,
               Design* design) {
    
    RTLIL::Module* module = design->top_module();
    Mode mode = Mode::TextIr;
    bool adff_to_sdff = false;
    bool output_named_wires = false;
    
    size_t it = 1;
    while (it < args.size()) {
      if (args[it] == "-top") {
        it++;
        module = design->module(RTLIL::escape_id(args[it]));
        if (module == nullptr) {
          throw hdl::Error("Module does not exist");
        }
        it++;
      } else if (args[it] == "-textir") {
        mode = Mode::TextIr;
        it++;
      } else if (args[it] == "-show") {
        mode = Mode::Show;
        it++;
      } else if (args[it] == "-verilog") {
        mode = Mode::Verilog;
        it++;
      } else if (args[it] == "-adff2sdff") {
        adff_to_sdff = true;
        it++;
      } else if (args[it] == "-output-named-wires") {
        output_named_wires = true;
        it++;
      } else {
        break;
      }
    }
    
    if (module == nullptr) {
      throw hdl::Error("Design does not have top module, specify manually using -top.");
    }
    
    extra_args(file, filename, args, it);
    
    hdl::yosys::Lowering lowering(module);
    lowering.set_adff_to_sdff(adff_to_sdff);
    lowering.set_output_named_wires(output_named_wires);
    hdl::Module hdl_module(RTLIL::id2cstr(module->name));
    lowering.into(hdl_module);
    
    switch (mode) {
      case Mode::TextIr: {
        hdl::textir::Printer printer(hdl_module);
        printer.print(*file);
      }
      break;
      case Mode::Show: {
        hdl::graphviz::Printer printer(hdl_module);
        printer.print(*file);
      }
      break;
      case Mode::Verilog: {
        hdl::verilog::Printer printer(hdl_module);
        printer.print(*file);
      }
      break;
    }
  }
  
} HdlBackend;

struct HdlFrontend : public Frontend {
  HdlFrontend(): Frontend("hdl", "read module from hdl.cpp text IR") {}
  
  void execute(std::istream*& file,
               std::string filename,
               std::vector<std::string> args,
               RTLIL::Design* design) {
    
    extra_args(file, filename, args, 1);
    
    hdl::Module hdl_module = hdl::textir::Reader::read_module(*file);
    
    RTLIL::Module* ys_module = new RTLIL::Module();
    ys_module->name = RTLIL::escape_id(hdl_module.name().c_str());
    design->add(ys_module);
    
    hdl::yosys::Builder builder(ys_module);
    builder.build(hdl_module);
  }
  
} HdlFrontend;

PRIVATE_NAMESPACE_END
