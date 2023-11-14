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
  HdlBackend(): Backend("hdl", "write design to hdl.cpp text IR") {}
  
  void execute(std::ostream* &file,
               std::string filename,
               std::vector<std::string> args,
               Design* design) {
    
    extra_args(file, filename, args, 1);
    
    for (RTLIL::Module* module : design->modules()) {
      hdl::yosys::Lowering lowering(module);
      hdl::Module hdl_module(RTLIL::id2cstr(module->name));
      lowering.into(hdl_module);
      
      hdl::textir::Printer printer(hdl_module);
      printer.print(*file);
    }
  }
  
} HdlBackend;

PRIVATE_NAMESPACE_END
