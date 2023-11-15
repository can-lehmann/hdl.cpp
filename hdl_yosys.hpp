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

#ifndef HDL_YOSYS_HPP
#define HDL_YOSYS_HPP

#include <vector>
#include <string>
#include <map>
#include <deque>
#include <unordered_set>

#include "kernel/yosys.h"
#include "kernel/sigtools.h"
#include "kernel/ffinit.h"

#include "hdl.hpp"

#define throw_error(Error, msg) { \
  std::ostringstream error_message; \
  error_message << msg; \
  throw Error(error_message.str()); \
}

namespace hdl {
  namespace yosys {
    namespace RTLIL = Yosys::RTLIL;
    
    class Lowering {
    private:
      RTLIL::Module* _ys_module = nullptr;
      Yosys::SigMap _sigmap;
      Yosys::FfInitVals _ff_init_vals;
      Yosys::dict<RTLIL::SigBit, std::pair<RTLIL::Cell*, RTLIL::IdString>> _drivers;
    public:
      Lowering(RTLIL::Module* ys_module):
          _ys_module(ys_module),
          _sigmap(ys_module),
          _ff_init_vals(&_sigmap, ys_module) {
        for (RTLIL::Cell* cell : ys_module->cells()) {
          for (auto [name, spec] : cell->connections()) {
            if (cell->output(name)) {
              for (RTLIL::SigBit bit : spec) {
                RTLIL::SigBit canonical = _sigmap(bit);
                if (_drivers.find(canonical) != _drivers.end()) {
                  throw_error(Error, "Multiple drivers for signal");
                }
                _drivers[canonical] = { cell, name };
              }
            }
          }
        }
      }
      
    private:
      struct Context {
        std::map<RTLIL::SigBit, Value*> values;
        std::map<RTLIL::IdString, Memory*> memories;
        std::unordered_set<RTLIL::Cell*> cells;
        std::deque<std::pair<Reg*, RTLIL::SigSpec>> open_regs;
        
        Module& module;
        Yosys::SigMap& sigmap;
        
        Context(Module& _module, Yosys::SigMap& _sigmap): module(_module), sigmap(_sigmap) {}
        
        void set(const RTLIL::SigSpec& spec, Value* value) {
          if (spec.size() != value->width) {
            throw_error(Error, "Width mismatch, expected " << spec.size() << " bits, but got " << value->width);
          }
          size_t it = 0;
          for (RTLIL::SigBit bit : spec.bits()) {
            values[sigmap(bit)] = module.op(Op::Kind::Slice, {
              value,
              module.constant(BitString::from_uint(it)),
              module.constant(BitString::from_uint(1))
            });
            it++;
          }
        }
        
        Value* operator[](const RTLIL::SigBit& bit) const {
          RTLIL::SigBit canonical = sigmap(bit);
          if (values.find(canonical) == values.end()) {
            throw_error(Error, "Bit not yet lowered");
          }
          return values.at(canonical);
        }
        
        bool has(const RTLIL::SigBit& bit) const {
          return values.find(sigmap(bit)) != values.end();
        }
        
        void queue_reg(Reg* reg, const RTLIL::SigSpec& next) {
          open_regs.emplace_back(reg, next);
        }
        
        bool has_open_regs() const {
          return !open_regs.empty();
        }
        
        std::pair<Reg*, RTLIL::SigSpec> pop_reg() {
          std::pair<Reg*, RTLIL::SigSpec> reg = open_regs.front();
          open_regs.pop_front();
          return reg;
        }
      };
      
      Value* extend(Value* value, size_t target, bool is_signed, Context& context) {
        if (target < value->width) {
          throw_error(Error, "Unable to shrink");
        } 
        
        if (target > value->width) {
          BitString zeros(target - value->width);
          hdl::Value* fill = context.module.constant(zeros);
          
          if (is_signed) {
            fill = context.module.op(Op::Kind::Select, {
              context.module.op(Op::Kind::Slice, {
                value,
                context.module.constant(BitString::from_uint(value->width - 1)),
                context.module.constant(BitString::from_uint(1))
              }),
              context.module.constant(~zeros),
              fill
            });
          }
          
          value = context.module.op(Op::Kind::Concat, {
            fill, value
          });
        }
        
        return value;
      }
      
      bool is_cmp(const RTLIL::IdString& type) const {
        return type.in(
          ID($eqx),
          ID($nex),
          ID($lt),
          ID($le),
          ID($gt),
          ID($ge),
          ID($eq),
          ID($ne)
        );
      }
      
      bool is_reduce(const RTLIL::IdString& type) const {
        return type.in(
          ID($reduce_and),
          ID($reduce_or),
          ID($reduce_xor),
          ID($reduce_xnor),
          ID($reduce_bool)
        );
      }
      
      bool is_binop(const RTLIL::IdString& type) const {
        return is_cmp(type) || type.in(
          ID($and),
          ID($or),
          ID($xor),
          ID($xnor),
          ID($shl),
          ID($shr),
          ID($sshl),
          ID($sshr),
          ID($logic_and),
          ID($logic_or),
          ID($add),
          ID($sub),
          ID($mul),
          ID($div),
          ID($mod),
          ID($divfloor),
          ID($modfloor)
        );
      }
      
      bool is_unop(const RTLIL::IdString& type) const {
        return is_reduce(type) || type.in(
          ID($not),
          ID($pos),
          ID($neg),
          ID($logic_not)
        );
      }
      
      void lower_unop(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec port_a = cell->getPort(RTLIL::ID::A);
        RTLIL::SigSpec port_y = cell->getPort(RTLIL::ID::Y);
        
        hdl::Value* a = lower(port_a, context);
        
        // Sign Extend
        
        bool a_signed = !cell->getParam(RTLIL::ID::A_SIGNED).is_fully_zero();
        
        size_t internal_width = port_y.size();
        if (cell->type == ID($logic_not) || is_reduce(cell->type)) {
          internal_width = port_a.size();
        }
        
        a = extend(a, internal_width, a_signed, context);
        
        // Eval
        
        Value* y = nullptr;
        
        if (cell->type == ID($not)) {
          y = context.module.op(Op::Kind::Not, {a});
        } else if (cell->type == ID($neg)) {
          y = context.module.op(Op::Kind::Sub, {
            context.module.constant(BitString(a->width)),
            a
          });
        } else if (cell->type == ID($pos)) {
          y = a;
        } else if (cell->type == ID($reduce_or) || cell->type == ID($reduce_bool)) {
          y = context.module.op(Op::Kind::Not, {
            context.module.op(Op::Kind::Eq, {
              a,
              context.module.constant(BitString(a->width))
            })
          });
        } else if (cell->type == ID($reduce_and)) {
          y = context.module.op(Op::Kind::Eq, {
            a,
            context.module.constant(~BitString(a->width))
          });
        } else if (cell->type == ID($reduce_xor) || cell->type == ID($reduce_xnor)) {
          y = context.module.constant(BitString::from_bool(false));
          for (size_t it = 0; it < a->width; it++) {
            Value* bit = context.module.op(Op::Kind::Slice, {
              a,
              context.module.constant(BitString::from_uint(it)),
              context.module.constant(BitString::from_uint(1))
            });
            
            y = context.module.op(Op::Kind::Xor, {y, bit});
          }
          
          if (cell->type == ID($reduce_xnor)) {
            y = context.module.op(Op::Kind::Not, {y});
          }
        } else if (cell->type == ID($logic_not)) {
          y = context.module.op(Op::Kind::Eq, {
            a,
            context.module.constant(BitString(a->width))
          });
        } else {
          throw_error(Error, "Lowering unary operator \"" << RTLIL::id2cstr(cell->type) << "\" is not implemented");
        }
        
        context.set(port_y, y);
      }
      
      void lower_binop(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec port_a = cell->getPort(RTLIL::ID::A);
        RTLIL::SigSpec port_b = cell->getPort(RTLIL::ID::B);
        RTLIL::SigSpec port_y = cell->getPort(RTLIL::ID::Y);
        
        hdl::Value* a = lower(port_a, context);
        hdl::Value* b = lower(port_b, context);
        
        // Sign Extend
        
        bool a_signed = !cell->getParam(RTLIL::ID::A_SIGNED).is_fully_zero();
        bool b_signed = !cell->getParam(RTLIL::ID::B_SIGNED).is_fully_zero();
        
        size_t internal_width = port_y.size();
        if (is_cmp(cell->type)) {
          internal_width = std::max(port_a.size(), port_b.size());
        }
        
        a = extend(a, internal_width, a_signed, context);
        b = extend(b, internal_width, b_signed, context);
        
        // Eval
        
        Value* y = nullptr;
        
        if (cell->type == ID($and)) {
          y = context.module.op(Op::Kind::And, { a, b });
        } else if (cell->type == ID($or)) {
          y = context.module.op(Op::Kind::Or, { a, b });
        } else if (cell->type == ID($xor)) {
          y = context.module.op(Op::Kind::Xor, { a, b });
        } else if (cell->type == ID($xnor)) {
          y = context.module.op(Op::Kind::Not, {
            context.module.op(Op::Kind::Xor, { a, b })
          });
        } else if (cell->type == ID($shl) || cell->type == ID($sshl)) {
          y = context.module.op(Op::Kind::Shl, { a, b });
        } else if (cell->type == ID($shr)) {
          y = context.module.op(Op::Kind::ShrU, { a, b });
        } else if (cell->type == ID($sshr)) {
          y = context.module.op(Op::Kind::ShrS, { a, b });
        } else if (cell->type == ID($eqx)) {
          y = context.module.op(Op::Kind::Eq, { a, b });
        } else if (cell->type == ID($nex)) {
          y = context.module.op(Op::Kind::Not, {
            context.module.op(Op::Kind::Eq, { a, b })
          });
        } else if (cell->type == ID($eq)) {
          y = context.module.op(Op::Kind::Eq, { a, b });
        } else if (cell->type == ID($ne)) {
          y = context.module.op(Op::Kind::Not, {
            context.module.op(Op::Kind::Eq, { a, b })
          });
        } else if (cell->type == ID($lt)) {
          y = context.module.op(a_signed && b_signed ? Op::Kind::LtS : Op::Kind::LtU, { a, b });
        } else if (cell->type == ID($le)) {
          y = context.module.op(a_signed && b_signed ? Op::Kind::LeS : Op::Kind::LeU, { a, b });
        } else if (cell->type == ID($gt)) {
          y = context.module.op(a_signed && b_signed ? Op::Kind::LtS : Op::Kind::LtU, { b, a });
        } else if (cell->type == ID($ge)) {
          y = context.module.op(a_signed && b_signed ? Op::Kind::LeS : Op::Kind::LeU, { b, a });
        } else if (cell->type == ID($add)) {
          y = context.module.op(Op::Kind::Add, { a, b });
        } else if (cell->type == ID($sub)) {
          y = context.module.op(Op::Kind::Sub, { a, b });
        } else if (cell->type == ID($mul)) {
          y = context.module.op(Op::Kind::Slice, {
            context.module.op(Op::Kind::Mul, { a, b }),
            context.module.constant(BitString::from_uint(0)),
            context.module.constant(BitString::from_uint(internal_width))
          });
        } else {
          throw_error(Error, "Lowering binary operator \"" << RTLIL::id2cstr(cell->type) << "\" is not implemented");
        }
        
        context.set(port_y, y);
      }
      
      bool lower(RTLIL::State state) {
        switch (state) {
          case RTLIL::S0: return false;
          case RTLIL::S1: return true;
          case RTLIL::Sx: return false; // TODO
          case RTLIL::Sz: throw_error(Error, "z state is not supported");
          default: throw_error(Error, "Unsupported state");
        }
      }
      
      BitString lower(const RTLIL::Const& constant, Context& context) {
        BitString bit_string(constant.size());
        for (size_t it = 0; it < bit_string.width(); it++) {
          bit_string.set(it, lower(constant[it]));
        }
        return bit_string;
      }
      
      void lower_dff(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec clk = cell->getPort(RTLIL::ID::CLK);
        RTLIL::SigSpec d = cell->getPort(RTLIL::ID::D);
        RTLIL::SigSpec q = cell->getPort(RTLIL::ID::Q);
        
        size_t width = cell->getParam(RTLIL::ID::WIDTH).as_int();
        
        BitString initial = lower(_ff_init_vals(q), context);
        if (initial.width() != width) {
          throw_error(Error, "Width mismatch");
        }
        Reg* reg = context.module.reg(initial, lower(clk, context));
        if (q.is_wire()) {
          reg->name = RTLIL::id2cstr(q.as_wire()->name);
        }
        context.set(q, reg);
        context.queue_reg(reg, d);
      }
      
      void lower_mux(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec port_s = cell->getPort(RTLIL::ID::S);
        RTLIL::SigSpec port_a = cell->getPort(RTLIL::ID::A);
        RTLIL::SigSpec port_b = cell->getPort(RTLIL::ID::B);
        RTLIL::SigSpec port_y = cell->getPort(RTLIL::ID::Y);
        
        context.set(port_y, context.module.op(Op::Kind::Select, {
          lower(port_s, context),
          lower(port_b, context),
          lower(port_a, context),
        }));
      }
      
      void lower_pmux(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec port_s = cell->getPort(RTLIL::ID::S);
        RTLIL::SigSpec port_a = cell->getPort(RTLIL::ID::A);
        RTLIL::SigSpec port_b = cell->getPort(RTLIL::ID::B);
        RTLIL::SigSpec port_y = cell->getPort(RTLIL::ID::Y);
        
        Value* result = lower(port_a, context);
        Value* cases = lower(port_b, context);
        Value* selector = lower(port_s, context);
        
        for (size_t it = 0; it < selector->width; it++) {
          result = context.module.op(Op::Kind::Select, {
            context.module.op(Op::Kind::Slice, {
              selector,
              context.module.constant(BitString::from_uint(it)),
              context.module.constant(BitString::from_uint(1))
            }),
            context.module.op(Op::Kind::Slice, {
              cases,
              context.module.constant(BitString::from_uint(result->width * it)),
              context.module.constant(BitString::from_uint(result->width))
            }),
            result
          });
        }
        
        context.set(port_y, result);
      }
      
      void lower_memrd(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec port_data = cell->getPort(RTLIL::ID::DATA);
        RTLIL::SigSpec port_addr = cell->getPort(RTLIL::ID::ADDR);
        RTLIL::SigSpec port_en = cell->getPort(RTLIL::ID::EN);
        RTLIL::SigSpec port_clk = cell->getPort(RTLIL::ID::CLK);
        
        RTLIL::IdString memid = cell->getParam(RTLIL::ID::MEMID).decode_string();
        bool clk_enable = !cell->getParam(RTLIL::ID::CLK_ENABLE).is_fully_zero();
        
        Memory* memory = context.memories.at(memid);
        
        Value* addr = lower(port_addr, context);
        Value* data = memory->read(addr);
        
        if (clk_enable) {
          Value* clk = lower(port_clk, context);
          Reg* reg = context.module.reg(BitString(port_data.size()), clk);
          reg->next = data;
          data = reg;
        }
        
        context.set(port_data, data);
      }
      
      void lower_memwr_v2(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec port_data = cell->getPort(RTLIL::ID::DATA);
        RTLIL::SigSpec port_addr = cell->getPort(RTLIL::ID::ADDR);
        RTLIL::SigSpec port_en = cell->getPort(RTLIL::ID::EN);
        RTLIL::SigSpec port_clk = cell->getPort(RTLIL::ID::CLK);
        
        RTLIL::IdString memid = cell->getParam(RTLIL::ID::MEMID).decode_string();
        bool clk_enable = !cell->getParam(RTLIL::ID::CLK_ENABLE).is_fully_zero();
        
        if (!clk_enable) {
          throw_error(Error, "Asynchronous memories are not supported");
        }
        
        Memory* memory = context.memories.at(memid);
        
        Value* addr = lower(port_addr, context);
        Value* data = lower(port_data, context);
        Value* en = lower(port_en, context);
        Value* clk = lower(port_clk, context);
        
        Value* any_en = context.module.op(Op::Kind::Not, {
          context.module.op(Op::Kind::Eq, {
            en,
            context.module.constant(BitString(en->width))
          })
        });
        
        memory->write(clk, addr, any_en,
          context.module.op(Op::Kind::Or, {
            context.module.op(Op::Kind::And, {
              memory->read(addr),
              context.module.op(Op::Kind::Not, {en})
            }),
            context.module.op(Op::Kind::And, {data, en})
          })
        );
      }
      
      void lower_bwmux(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec port_s = cell->getPort(RTLIL::ID::S);
        RTLIL::SigSpec port_a = cell->getPort(RTLIL::ID::A);
        RTLIL::SigSpec port_b = cell->getPort(RTLIL::ID::B);
        RTLIL::SigSpec port_y = cell->getPort(RTLIL::ID::Y);
        
        Value* select = lower(port_s, context);
        
        context.set(port_y,
          context.module.op(Op::Kind::Or, {
            context.module.op(Op::Kind::And, {
              select,
              lower(port_b, context)
            }),
            context.module.op(Op::Kind::And, {
              context.module.op(Op::Kind::Not, { select }),
              lower(port_a, context)
            })
          })
        );
      }
      
      
      void lower_bweqx(RTLIL::Cell* cell, Context& context) {
        RTLIL::SigSpec port_a = cell->getPort(RTLIL::ID::A);
        RTLIL::SigSpec port_b = cell->getPort(RTLIL::ID::B);
        RTLIL::SigSpec port_y = cell->getPort(RTLIL::ID::Y);
        
        context.set(port_y,
          context.module.op(Op::Kind::Not, {
            context.module.op(Op::Kind::Xor, {
              lower(port_a, context),
              lower(port_b, context)
            })
          })
        );
      }
      
      void lower(RTLIL::Cell* cell, Context& context) {
        if (context.cells.find(cell) != context.cells.end()) {
          return;
        }
        context.cells.insert(cell);
        
        std::cout << RTLIL::id2cstr(cell->name) << std::endl;
        std::cout << RTLIL::id2cstr(cell->type) << std::endl;
        for (const auto& [name, value] : cell->parameters) {
          std::cout << "  " << RTLIL::id2cstr(name) << " = " << value.as_string() << std::endl;
        }
        for (const auto& [name, spec] : cell->connections()) {
          std::cout << "  " << RTLIL::id2cstr(name) << " = ";
          RTLIL::Cell* prev = nullptr;
          bool is_first = true;
          for (const RTLIL::SigBit& bit : spec.bits()) {
            RTLIL::SigBit canonical = _sigmap(bit);
            if (canonical.wire == nullptr) {
              if (!is_first) { std::cout << ", "; }
              std::cout << int(canonical.data);
              is_first = false;
            } else if (_drivers.find(canonical) != _drivers.end()) {
              RTLIL::Cell* port_driver = _drivers.at(canonical).first;
              if (port_driver != prev) {
                if (!is_first) { std::cout << ", "; }
                std::cout << RTLIL::id2cstr(port_driver->name);
                prev = port_driver;
                is_first = false;
              }
            } else {
              if (!is_first) { std::cout << ", "; }
              std::cout << '?';
              is_first = false;
            }
          }
          std::cout << std::endl;
        }
        
        if (cell->type == ID($dff)) {
          lower_dff(cell, context);
        } else if (cell->type == ID($mux)) {
          lower_mux(cell, context);
        } else if (cell->type == ID($pmux)) {
          lower_pmux(cell, context);
        } else if (cell->type == ID($memrd)) {
          lower_memrd(cell, context);
        } else if (cell->type == ID($memwr_v2)) {
          lower_memwr_v2(cell, context);
        } else if (cell->type == ID($bweqx)) {
          lower_bweqx(cell, context);
        } else if (cell->type == ID($bwmux)) {
          lower_bwmux(cell, context);
        } else if (is_unop(cell->type)) {
          lower_unop(cell, context);
        } else if (is_binop(cell->type)) {
          lower_binop(cell, context);
        } else {
          throw_error(Error, "Unknown cell: " << RTLIL::id2cstr(cell->type));
        }
        
        std::cout << "finish: " << RTLIL::id2cstr(cell->name) << std::endl;
      }
      
      Value* lower(const RTLIL::SigBit& _bit, Context& context) {
        RTLIL::SigBit bit = _sigmap(_bit);
        if (context.has(bit)) {
          return context[bit];
        }
        
        if (bit.wire == nullptr) {
          return context.module.constant(BitString::from_bool(lower(bit.data)));
        }
        
        if (bit.wire->port_input) {
          context.set(bit.wire, context.module.input(RTLIL::id2cstr(bit.wire->name), bit.wire->width));
        } else if (_drivers.find(bit) != _drivers.end()) {
          auto [cell, output] = _drivers[bit];
          lower(cell, context);
        } else {
          throw_error(Error, "Signal " << Yosys::log_signal(bit) << " has no driver");
        }
        
        return context[bit];
      }
      
      Value* lower(const RTLIL::SigSpec& spec, Context& context) {
        std::vector<Value*> chunks;
        Value* chunk = nullptr;
        
        for (const RTLIL::SigBit& bit : spec.bits()) {
          Value* value = lower(bit, context);
          
          bool concat = false;
          if (Constant* constant = dynamic_cast<Constant*>(value)) {
            if (dynamic_cast<Constant*>(chunk)) {
              concat = true;
            }
          } else if (Op* op = dynamic_cast<Op*>(value)) {
            if (Op* chunk_op = dynamic_cast<Op*>(chunk)) {
              if (op->kind == Op::Kind::Slice &&
                  chunk_op->kind == Op::Kind::Slice &&
                  op->args[0] == chunk_op->args[0]) {
                concat = true;
              }
            }
          }
          
          if (concat) {
            chunk = context.module.op(hdl::Op::Kind::Concat, {
              value,
              chunk
            });
          } else {
            if (chunk != nullptr) {
              chunks.push_back(chunk);
            }
            chunk = value;
          }
        }
        
        if (chunk != nullptr) {
          chunks.push_back(chunk);
        }
        
        hdl::Value* result = nullptr;
        for (Value* chunk : chunks) {
          if (result == nullptr) {
            result = chunk;
          } else {
            result = context.module.op(hdl::Op::Kind::Concat, {
              chunk,
              result
            });
          }
        }
        
        if (result == nullptr) {
          throw Error("nullptr");
        }
        
        return result;
      }
      
    public:
      void into(Module& hdl_module) {
        Context context(hdl_module, _sigmap);
        
        for (const auto& [name, memory] : _ys_module->memories) {
          Memory* hdl_memory = hdl_module.memory(memory->width, memory->size);
          hdl_memory->name = RTLIL::id2cstr(memory->name);
          context.memories[name] = hdl_memory;
        }
        
        for (RTLIL::Wire* wire : _ys_module->wires()) {
          if (wire->port_output) {
            Value* value = lower(RTLIL::SigSpec(wire), context);
            hdl_module.output(RTLIL::id2cstr(wire->name), value);
          }
        }
        
        for (RTLIL::Cell* cell : _ys_module->cells()) {
          if (context.cells.find(cell) == context.cells.end()) {
            std::cout << RTLIL::id2cstr(cell->type) << '\t' << RTLIL::id2cstr(cell->name) << std::endl;
          }
          
          if (cell->type == ID($memwr_v2)) {
            lower(cell, context);
          }
        }
        
        while (context.has_open_regs()) {
          auto [reg, next] = context.pop_reg();
          reg->next = lower(next, context);
        }
      }
    };
    
  }
}

#undef throw_error

#endif
