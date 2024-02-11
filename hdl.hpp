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

#ifndef HDL_HPP
#define HDL_HPP

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <sstream>
#include <fstream>

#include "hdl_bitstring.hpp"

#define throw_error(Error, msg) { \
  std::ostringstream error_message; \
  error_message << msg; \
  throw Error(error_message.str()); \
}

namespace hdl {
  class Module;
  
  struct Value {
    const size_t width = 0;
    
    Value(size_t _width): width(_width) {}
    virtual ~Value() {}
  };
  
  struct Input: public Value {
    std::string name;
    
    Input(const std::string& _name, size_t _width):
      Value(_width), name(_name) {}
  };
  
  struct Reg: public Value {
    BitString initial;
    Value* clock = nullptr;
    Value* next = nullptr;
    std::string name;
    
    Reg(const BitString& _initial, Value* _clock):
      Value(_initial.width()), initial(_initial), clock(_clock) {}
  };
  
  struct Comb: public Value { using Value::Value; };
  
  struct Constant: public Comb {
    const BitString value;
    
    Constant(const BitString& _value):
      Comb(_value.width()), value(_value) {}
    
    bool hashcons_equal(const Constant& other) const {
      return value == other.value;
    }
  };
  
  struct Unknown: public Comb {
    Unknown(size_t _width): Comb(_width) {}
  };
  
  struct Op: public Comb {
    enum class Kind {
      And, Or, Xor, Not,
      Add, Sub, Mul,
      Eq, LtU, LtS, LeU, LeS,
      Concat, Slice,
      Shl, ShrU, ShrS,
      Select
    };
    
    static constexpr const size_t KIND_COUNT = size_t(Kind::Select) + 1;
    static const char* KIND_NAMES[];
    
    static constexpr const size_t MAX_ARG_COUNT = 3;
    
    static bool is_commutative(Kind kind) {
      return kind == Kind::And ||
             kind == Kind::Or ||
             kind == Kind::Xor ||
             kind == Kind::Add ||
             kind == Kind::Eq;
    }
    
    const Kind kind;
    const std::vector<Value*> args;
    
  private:
    static void expect_arg_count(Kind& kind, const std::vector<Value*>& args, size_t count) {
      if (args.size() != count) {
        throw_error(Error,
          "Operator " << KIND_NAMES[(size_t)kind] <<
          " expected " << count << " arguments, but got " << args.size()
        );
      }
    }
    
    static void expect_equal_width(Kind& kind, const std::vector<Value*>& args, size_t a, size_t b) {
      if (args[a]->width != args[b]->width) {
        throw_error(Error,
          "Operator " << KIND_NAMES[(size_t)kind] <<
          " expected arguments " << a << " and " << b <<
          " to have equal bit width, but got arguments of widths " << 
          args[a]->width << " and " << args[b]->width
        );
      }
    }
    
    static size_t infer_width(Kind& kind, const std::vector<Value*>& args) {
      switch (kind) {
        case Kind::Not:
          expect_arg_count(kind, args, 1);
          return args[0]->width;
        case Kind::And:
        case Kind::Or:
        case Kind::Xor:
        case Kind::Add:
        case Kind::Sub:
          expect_arg_count(kind, args, 2);
          expect_equal_width(kind, args, 0, 1);
          return args[0]->width;
        case Kind::Mul:
          expect_arg_count(kind, args, 2);
          return args[0]->width + args[1]->width;
        case Kind::Eq:
        case Kind::LtU:
        case Kind::LtS:
        case Kind::LeU:
        case Kind::LeS:
          expect_arg_count(kind, args, 2);
          expect_equal_width(kind, args, 0, 1);
          return 1;
        case Kind::Concat:
          expect_arg_count(kind, args, 2);
          return args[0]->width + args[1]->width;
        case Kind::Slice:
          expect_arg_count(kind, args, 3);
          if (const Constant* constant = dynamic_cast<const Constant*>(args[2])) {
            size_t width = (size_t)constant->value.as_uint64();
            return width;
          } else {
            throw_error(Error,
              "Third argument of " << KIND_NAMES[(size_t)kind] <<
              " operator must be constant."
            );
          }
        break;
        case Kind::Shl:
        case Kind::ShrU:
        case Kind::ShrS:
          expect_arg_count(kind, args, 2);
          return args[0]->width;
        case Kind::Select:
          expect_arg_count(kind, args, 3);
          expect_equal_width(kind, args, 1, 2);
          return args[1]->width;
      }
    }
  public:
    
    Op(Kind _kind, const std::vector<Value*>& _args):
      Comb(infer_width(_kind, _args)), kind(_kind), args(_args) {}
    
    bool hashcons_equal(const Op& other) const {
      return kind == other.kind && args == other.args;
    }
    
    BitString eval(BitString const** values) const {
      #define arg(index) (*(values[(index)]))
      #define binop(op) result = arg(0) op arg(1);
      
      BitString result;
      switch (kind) {
        case Op::Kind::And: binop(&); break;
        case Op::Kind::Or: binop(|); break;
        case Op::Kind::Xor: binop(^); break;
        case Op::Kind::Not: result = ~arg(0); break;
        case Op::Kind::Add: binop(+); break;
        case Op::Kind::Sub: binop(-); break;
        case Op::Kind::Mul: result = arg(0).mul_u(arg(1)); break;
        case Op::Kind::Eq: result = BitString::from_bool(arg(0) == arg(1)); break;
        case Op::Kind::LtU: result = BitString::from_bool(arg(0).lt_u(arg(1))); break;
        case Op::Kind::LtS: result = BitString::from_bool(arg(0).lt_s(arg(1))); break;
        case Op::Kind::LeU: result = BitString::from_bool(arg(0).le_u(arg(1))); break;
        case Op::Kind::LeS: result = BitString::from_bool(arg(0).le_s(arg(1))); break;
        case Op::Kind::Concat: result = arg(0).concat(arg(1)); break;
        case Op::Kind::Slice: result = arg(0).slice_width(arg(1).as_uint64(), arg(2).as_uint64()); break;
        case Kind::Shl: result = arg(0) << arg(1).as_uint64(); break;
        case Kind::ShrU: result = arg(0).shr_u(arg(1).as_uint64()); break;
        case Kind::ShrS: result = arg(0).shr_s(arg(1).as_uint64()); break;
        case Op::Kind::Select: result = (arg(0))[0] ? arg(1) : arg(2); break;
      }
      
      #undef binop
      #undef arg
      
      return result;
    }
    
    BitString eval(const std::vector<BitString>& values) const {
      if (values.size() != args.size()) {
        throw_error(Error,
          "Operator " << KIND_NAMES[size_t(kind)] <<
          " expects " << args.size() << " arguments, but " <<
          "eval(const std::vector<BitString>&) got " << values.size()
        );
      }
      
      BitString const* value_ptrs[MAX_ARG_COUNT] = {nullptr};
      for (size_t it = 0; it < values.size(); it++) {
        value_ptrs[it] = &values[it];
      }
      return eval(value_ptrs);
    }
  };
  
  const char* Op::KIND_NAMES[] = {
    "And", "Or", "Xor", "Not",
    "Add", "Sub", "Mul",
    "Eq", "LtU", "LtS", "LeU", "LeS",
    "Concat", "Slice",
    "Shl", "ShrU", "ShrS",
    "Select"
  };
  
  std::ostream& operator<<(std::ostream& stream, const Op::Kind& kind) {
    stream << Op::KIND_NAMES[(size_t)kind];
    return stream;
  }
}

template <>
struct std::hash<hdl::Constant> {
  std::size_t operator()(const hdl::Constant& constant) const {
    return std::hash<hdl::BitString>()(constant.value);
  }
};

template <>
struct std::hash<hdl::Op> {
  std::size_t operator()(const hdl::Op& op) const {
    size_t hash = std::hash<size_t>()((size_t)op.kind);
    size_t offset = 3;
    for (const hdl::Value* arg : op.args) {
      hash ^= std::hash<const hdl::Value*>()(arg) << offset;
      offset++;
    }
    return hash;
  }
};

namespace hdl {
  struct Memory {
    struct Write {
      Value* clock = nullptr;
      Value* address = nullptr;
      Value* enable = nullptr;
      Value* value = nullptr;
      
      Write(Value* _clock, Value* _address, Value* _enable, Value* _value):
        clock(_clock), address(_address), enable(_enable), value(_value) {}
    };
    
    struct Read : public Comb {
      Memory* memory = nullptr;
      Value* address = nullptr;
      
      Read(Memory* _memory, Value* _address):
        Comb(_memory->width), memory(_memory), address(_address) {}
    };
    
    const size_t width = 0;
    const size_t size = 0;
    std::unordered_map<uint64_t, BitString> initial;
    std::vector<Write> writes;
    std::unordered_map<Value*, Read*> reads;
    std::string name;
    
    Memory(size_t _width, size_t _size):
      width(_width), size(_size) {}
    
    Memory(const Memory& other) = delete;
    Memory& operator=(const Memory& other) = delete;
    
    ~Memory() {
      for (const auto& [address, read] : reads) {
        delete read;
      }
    }
    
    void write(Value* clock, Value* address, Value* enable, Value* value) {
      if (clock->width != 1) {
        throw_error(Error,
          "The memory write clock signal must have width " << 1 <<
          " but got value of width " << clock->width
        );
      }
      if (value->width != width) {
        throw_error(Error,
          "Unable to write value of width " << value->width <<
          " to memory of width " << width
        );
      }
      if (enable->width != 1) {
        throw_error(Error,
          "The memory write enable signal must have width " << 1 <<
          " but got value of width " << enable->width
        );
      }
      if (Constant* constant = dynamic_cast<Constant*>(enable)) {
        if (constant->value.is_zero()) {
          return;
        }
      }
      writes.emplace_back(clock, address, enable, value);
    }
    
    Read* read(Value* address) {
      if (reads.find(address) != reads.end()) {
        return reads.at(address);
      }
      Read* read = new Read(this, address);
      reads[address] = read;
      return read;
    }
    
    void init(uint64_t address, const BitString& value) {
      if (value.width() != width) {
        throw_error(Error,
          "Unable to initialize memory of width " << width <<
          " with value of width " << value.width()
        );
      }
      
      if (value.is_zero()) {
        initial.erase(address);
      } else {
        initial[address] = value;
      }
    }
    
    void init(uint64_t address,
              const BitString& enable,
              const BitString& value) {
      if (value.width() != width) {
        throw_error(Error,
          "Unable to initialize memory of width " << width <<
          " with value of width " << value.width()
        );
      }
      
      BitString word = value & enable;
      if (initial.find(address) != initial.end()) {
        word = word | (initial.at(address) & ~enable);
      }
      init(address, word);
    }
  };
  
  struct Output {
    std::string name;
    Value* value = nullptr;
    
    Output(const std::string& _name, Value* _value):
      name(_name), value(_value) {}
  };
  
  class Module {
  private:
    template <class T>
    class Hashcons {
    private:
      struct Cell {
        T* node = nullptr;
        
        Cell(T* _node): node(_node) {}
        
        bool operator==(const Cell& other) const {
          return node->hashcons_equal(*other.node);
        }
      };
      
      struct CellHasher {
        size_t operator()(const Cell& cell) const {
          return std::hash<T>()(*cell.node);
        }
      };
      
      std::unordered_set<Cell, CellHasher> _nodes;
    public:
      Hashcons() {}
      
      Hashcons(const Hashcons<T>& other) = delete;
      Hashcons& operator=(const Hashcons<T>& other) = delete;
      
      Hashcons(Hashcons<T>&& other):
        _nodes(std::move(other._nodes)) {}
      
      ~Hashcons() {
        for (const Cell& cell : _nodes) {
          delete cell.node;
        }
      }
      
      T* operator[](const T& node) {
        Cell cell((T*)&node);
        if (_nodes.find(cell) == _nodes.end()) {
          Cell cell(new T(node));
          _nodes.insert(cell);
          return cell.node;
        } else {
          return _nodes.find(cell)->node;
        }
      }
      
      void gc(std::set<void*> reached) {
        std::unordered_set<Cell, CellHasher> nodes;
        for (const Cell& cell : _nodes) {
          if (reached.find(cell.node) == reached.end()) {
            delete cell.node;
          } else {
            nodes.insert(cell);
          }
        }
        _nodes = nodes;
      }
    };
    
    std::string _name;
    Hashcons<Constant> _constants;
    Hashcons<Op> _ops;
    std::vector<Reg*> _regs;
    std::vector<Memory*> _memories;
    std::vector<Input*> _inputs;
    std::vector<Output> _outputs;
    std::vector<Unknown*> _unknowns;
  public:
    Module(const std::string& name): _name(name) {}
    
    Module(const Module& other) = delete;
    Module& operator=(const Module& other) = delete;
    
    Module(Module&& other):
      _name(std::move(other._name)),
      _constants(std::move(other._constants)),
      _ops(std::move(other._ops)),
      _regs(std::move(other._regs)),
      _memories(std::move(other._memories)),
      _inputs(std::move(other._inputs)),
      _outputs(std::move(other._outputs)) {}
    
    inline const std::string& name() const { return _name; }
    inline const std::vector<Reg*> regs() const { return _regs; }
    inline const std::vector<Memory*> memories() const { return _memories; }
    inline const std::vector<Input*> inputs() const { return _inputs; }
    inline const std::vector<Output> outputs() const { return _outputs; }
    
    const Output& find_output(const std::string& name) const {
      for (const Output& output : _outputs) {
        if (output.name == name) {
          return output;
        }
      }
      throw_error(Error, "Unable to find output \"" << name << "\"");
    }
  
  private:
    template <class T>
    T* try_find(const std::vector<T*>& values, const std::string& name) const {
      for (T* value : values) {
        if (value->name == name) {
          return value;
        }
      }
      return nullptr;
    }
    
    template <class T>
    T* find(const std::vector<T*>& values, const std::string& name) const {
      if (T* value = try_find(values, name)) {
        return value;
      } else {
        throw_error(Error, "Unable to find \"" << name << "\"");
      }
    }
    
  public:
    Reg* try_find_reg(const std::string& name) const { return try_find(_regs, name); }
    Input* try_find_input(const std::string& name) const { return try_find(_inputs, name); }
    Memory* try_find_memory(const std::string& name) const { return try_find(_memories, name); }
    
    Reg* find_reg(const std::string& name) const { return find(_regs, name); }
    Input* find_input(const std::string& name) const { return find(_inputs, name); }
    Memory* find_memory(const std::string& name) const { return find(_memories, name); }
    
    Input* input(const std::string& name, size_t width) {
      Input* input = new Input(name, width);
      _inputs.push_back(input);
      return input;
    }
    
    void output(const std::string& name, Value* value) {
      _outputs.push_back(Output { name, value });
    }
    
    Reg* reg(const BitString& initial, Value* clock) {
      Reg* reg = new Reg(initial, clock);
      reg->next = reg;
      _regs.push_back(reg);
      return reg;
    }
    
    Memory* memory(size_t width, size_t size) {
      Memory* memory = new Memory(width, size);
      _memories.push_back(memory);
      return memory;
    }
    
    Value* op(Op::Kind kind, std::vector<Value*> args) {
      if (Op::is_commutative(kind)) {
        bool lhs_const = dynamic_cast<Constant*>(args[0]) != nullptr;
        bool rhs_const = dynamic_cast<Constant*>(args[1]) != nullptr;
        if (lhs_const == rhs_const) {
          if (args[0] > args[1]) {
            std::swap(args[0], args[1]);
          }
        } else if (rhs_const) {
          std::swap(args[0], args[1]);
        }
      }
      
      Op op(kind, args);
      bool is_constant = true;
      for (const Value* arg : args) {
        if (!dynamic_cast<const Constant*>(arg)) {
          is_constant = false;
          break;
        }
      }
      
      if (is_constant) {
        const BitString* arg_values[Op::MAX_ARG_COUNT] = {nullptr};
        for (size_t it = 0; it < args.size(); it++) {
          arg_values[it] = &dynamic_cast<const Constant*>(args[it])->value;
        }
        return constant(op.eval(arg_values));
      } else {
        // For the commutative operations, we only need to check if the left hand side
        // is constant, as constants will always be moved to the left hand side of the operator.
        switch (kind) {
          case Op::Kind::And:
            if (args[0] == args[1]) {
              return args[0];
            } else if (Constant* constant = dynamic_cast<Constant*>(args[0])) {
              if (constant->value.is_zero()) {
                return constant;
              } else if (constant->value.is_all_ones()) {
                return args[1];
              }
            }
          break;
          case Op::Kind::Or:
            if (args[0] == args[1]) {
              return args[0];
            } else if (Constant* constant = dynamic_cast<Constant*>(args[0])) {
              if (constant->value.is_zero()) {
                return args[1];
              } else if (constant->value.is_all_ones()) {
                return constant;
              }
            }
          break;
          case Op::Kind::Xor:
            if (args[0] == args[1]) {
              return constant(BitString(args[0]->width));
            } else if (Constant* constant = dynamic_cast<Constant*>(args[0])) {
              if (constant->value.is_zero()) {
                return args[1];
              } else if (constant->value.is_all_ones()) {
                return this->op(Op::Kind::Not, {args[1]});
              }
            }
          break;
          case Op::Kind::Not:
            if (const Op* arg = dynamic_cast<const Op*>(args[0])) {
              if (arg->kind == Op::Kind::Not) {
                return arg->args[0];
              }
            }
          break;
          case Op::Kind::Add:
            if (Constant* constant = dynamic_cast<Constant*>(args[0])) {
              if (constant->value.is_zero()) {
                return args[1];
              }
            }
          break;
          case Op::Kind::Sub:
            if (args[0] == args[1]) {
              return constant(BitString(args[0]->width));
            }
            if (Constant* constant = dynamic_cast<Constant*>(args[1])) {
              if (constant->value.is_zero()) {
                return args[0];
              }
            }
          break;
          case Op::Kind::Mul: break;
          case Op::Kind::Eq:
            if (args[0] == args[1]) {
              return constant(BitString::from_bool(true));
            }
            if (args[1]->width == 1) {
              if (Constant* constant = dynamic_cast<Constant*>(args[0])) {
                if (constant->value.is_zero()) {
                  return this->op(Op::Kind::Not, { args[1] });
                } else {
                  return args[1];
                }
              }
            }
          break;
          case Op::Kind::LtU:
            if (args[0] == args[1]) {
              return constant(BitString::from_bool(false));
            }
            if (Constant* constant_b = dynamic_cast<Constant*>(args[1])) {
              if (constant_b->value.is_zero()) {
                return constant(BitString::from_bool(false));
              }
            }
          break;
          case Op::Kind::LtS:
            if (args[0] == args[1]) {
              return constant(BitString::from_bool(false));
            }
          break;
          case Op::Kind::LeU:
            if (args[0] == args[1]) {
              return constant(BitString::from_bool(true));
            }
            if (Constant* constant_a = dynamic_cast<Constant*>(args[0])) {
              if (constant_a->value.is_zero()) {
                return constant(BitString::from_bool(true));
              }
            }
          break;
          case Op::Kind::LeS:
            if (args[0] == args[1]) {
              return constant(BitString::from_bool(true));
            }
          break;
          case Op::Kind::Concat: {
            Op* op_high = dynamic_cast<Op*>(args[0]);
            Op* op_low = dynamic_cast<Op*>(args[1]);
            
            if (op_low != nullptr &&
                op_high != nullptr &&
                op_high->kind == Op::Kind::Slice &&
                op_low->kind == Op::Kind::Slice &&
                op_high->args[0] == op_low->args[0]) {
              
              Constant* const_low_offset = dynamic_cast<Constant*>(op_low->args[1]);
              Constant* const_high_offset = dynamic_cast<Constant*>(op_high->args[1]);
              size_t low_width = dynamic_cast<Constant*>(op_low->args[2])->value.as_uint64();
              size_t high_width = dynamic_cast<Constant*>(op_high->args[2])->value.as_uint64();
              
              bool is_contiguous =
                const_low_offset != nullptr &&
                const_high_offset != nullptr &&
                const_low_offset->value.as_uint64() + low_width == const_high_offset->value.as_uint64();
              
              if (is_contiguous) {
                return this->op(Op::Kind::Slice, {
                  op_low->args[0],
                  op_low->args[1],
                  constant(BitString::from_uint(high_width + low_width))
                });
              }
            }
          }
          break;
          case Op::Kind::Slice: {
            Constant* constant_offset = dynamic_cast<Constant*>(args[1]);
            size_t width = dynamic_cast<Constant*>(args[2])->value.as_uint64();
            if (constant_offset != nullptr &&
                constant_offset->value.is_zero() &&
                width == args[0]->width) {
              return args[0];
            } else if (Op* op = dynamic_cast<Op*>(args[0])) {
              switch (op->kind) {
                case Op::Kind::Concat:
                  if (constant_offset != nullptr) {
                    size_t offset = constant_offset->value.as_uint64();
                    if (offset + width <= op->args[1]->width) {
                      return this->op(Op::Kind::Slice, {
                        op->args[1],
                        args[1],
                        args[2]
                      });
                    } else if (offset >= op->args[1]->width) {
                      return this->op(Op::Kind::Slice, {
                        op->args[0],
                        this->constant(BitString::from_uint(offset - op->args[1]->width)),
                        args[2]
                      });
                    }
                  }
                break;
                case Op::Kind::Slice:
                  if (Constant* inner_constant_offset = dynamic_cast<Constant*>(op->args[1])) {
                    size_t offset = constant_offset->value.as_uint64();
                    size_t inner_offset = inner_constant_offset->value.as_uint64();
                    
                    return this->op(Op::Kind::Slice, {
                      op->args[0],
                      this->constant(BitString::from_uint(offset + inner_offset)),
                      args[2]
                    });
                  }
                break;
                default: break;
              }
            }
          }
          break;
          case Op::Kind::Shl:
            if (Constant* constant = dynamic_cast<Constant*>(args[0])) {
              if (constant->value.is_zero()) {
                return args[0];
              }
            }
            if (Constant* constant = dynamic_cast<Constant*>(args[1])) {
              if (constant->value.is_zero()) {
                return args[0];
              }
            }
          break;
          case Op::Kind::ShrU:
            if (Constant* constant = dynamic_cast<Constant*>(args[0])) {
              if (constant->value.is_zero()) {
                return args[0];
              }
            }
            if (Constant* constant = dynamic_cast<Constant*>(args[1])) {
              if (constant->value.is_zero()) {
                return args[0];
              }
            }
          break;
          case Op::Kind::ShrS:
            if (Constant* constant = dynamic_cast<Constant*>(args[0])) {
              if (constant->value.is_zero()) {
                return args[0];
              } else if (constant->value.is_all_ones()) {
                return args[0];
              }
            }
            if (Constant* constant = dynamic_cast<Constant*>(args[1])) {
              if (constant->value.is_zero()) {
                return args[0];
              }
            }
          break;
          case Op::Kind::Select:
            if (args[1] == args[2]) {
              return args[1];
            } else if (const Constant* constant = dynamic_cast<const Constant*>(args[0])) {
              if (constant->value[0]) {
                return args[1];
              } else {
                return args[2];
              }
            }
          break;
        }
      }
      
      return _ops[op];
    }
    
    Constant* constant(const BitString& bit_string) {
      return _constants[Constant(bit_string)];
    }
    
    Unknown* unknown(size_t width) {
      Unknown* unknown = new Unknown(width);
      _unknowns.push_back(unknown);
      return unknown;
    }
    
  private:
    void trace(Memory* memory, std::set<void*>& reached) {
      if (reached.find(memory) == reached.end()) {
        reached.insert(memory);
        
        for (const Memory::Write& write : memory->writes) {
          trace(write.clock, reached);
          trace(write.address, reached);
          trace(write.enable, reached);
          trace(write.value, reached);
        }
      }
    }
    
    void trace(Value* value, std::set<void*>& reached) {
      if (reached.find(value) == reached.end()) {
        reached.insert(value);
        
        if (Op* op = dynamic_cast<Op*>(value)) {
          for (Value* arg : op->args) {
            trace(arg, reached);
          }
        } else if (Reg* reg = dynamic_cast<Reg*>(value)) {
          trace(reg->clock, reached);
          trace(reg->next, reached);
        } else if (Memory::Read* read = dynamic_cast<Memory::Read*>(value)) {
          trace(read->memory, reached);
          trace(read->address, reached);
        }
      }
    }
  public:
    void gc() {
      std::set<void*> reached;
      for (Output& output : _outputs) {
        trace(output.value, reached);
      }
      
      std::vector<Reg*> regs;
      for (Reg* reg : _regs) {
        if (reached.find(reg) == reached.end()) {
          delete reg;
        } else {
          regs.push_back(reg);
        }
      }
      _regs = regs;
      
      std::vector<Memory*> memories;
      for (Memory* memory : _memories) {
        if (reached.find(memory) == reached.end()) {
          delete memory;
        } else {
          memories.push_back(memory);
        }
      }
      _memories = memories;
      
      std::vector<Unknown*> unknowns;
      for (Unknown* unknown : _unknowns) {
        if (reached.find(unknown) == reached.end()) {
          delete unknown;
        } else {
          unknowns.push_back(unknown);
        }
      }
      _unknowns = unknowns;
      
      _ops.gc(reached);
      _constants.gc(reached);
    }
  };
  
  namespace verilog {
    struct Width {
      size_t width = 0;
      
      Width(size_t _width): width(_width) {}
    };
    
    std::ostream& operator<<(std::ostream& stream, const Width& width) {
      if (width.width != 1)  {
        stream << '[' << (width.width - 1) << ":0] ";
      }
      return stream;
    }
    
    class Printer {
    private:
      Module& _module;
      std::unordered_map<const Value*, std::string> _names;
      std::unordered_map<const Memory*, std::string> _memory_names;
      std::unordered_map<const Value*, size_t> _counts;
      
      void count_usages(const Value* value) {
        if (_counts.find(value) != _counts.end()) {
          _counts[value] += 1;
          return;
        }
        
        _counts[value] = 1;
        if (const Op* op = dynamic_cast<const Op*>(value)) {
          if (op->kind == Op::Kind::Slice) {
            // Slices can only be applied to wires, not expressions
            count_usages(op->args[0]);
          }
          for (const Value* arg : op->args) {
            count_usages(arg);
          }
          if (op->kind == Op::Kind::Slice) {
            count_usages(op->args[1]);
          }
        } else if (const Memory::Read* read = dynamic_cast<const Memory::Read*>(value)) {
          count_usages(read->address);
        }
      }
      
      void count_usages() {
        for (const Reg* reg : _module.regs()) {
          count_usages(reg->clock);
          count_usages(reg->next);
        }
        
        for (const Memory* memory : _module.memories()) {
          if (memory->writes.size() > 0) {
            for (const Memory::Write& write : memory->writes) {
              count_usages(write.clock);
              count_usages(write.address);
              count_usages(write.value);
              count_usages(write.enable);
            }
          }
        }
        
        for (const Output& output : _module.outputs()) {
          count_usages(output.value);
        }
      }
      
      std::string print(std::ostream& stream, const Value* value, std::unordered_set<const Value*>& closed) const {
        if (closed.find(value) != closed.end()) {
          if (_names.find(value) == _names.end()) {
            throw Error("Unreachable");
          }
          return _names.at(value);
        }
        
        if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
          std::ostringstream expr;
          expr << constant->value;
          return expr.str();
        } else if (const Unknown* unknown = dynamic_cast<const Unknown*>(value)) {
          std::ostringstream expr;
          expr << unknown->width << "'bx";
          return expr.str();
        }
        
        closed.insert(value);
        
        std::ostringstream expr;
        if (const Op* op = dynamic_cast<const Op*>(value)) {
          std::vector<std::string> args;
          for (const Value* arg : op->args) {
            args.emplace_back(print(stream, arg, closed));
          }
          
          expr << '(';
          switch (op->kind) {
            case Op::Kind::And: expr << args[0] << " & " << args[1]; break;
            case Op::Kind::Or: expr << args[0] << " | " << args[1]; break;
            case Op::Kind::Xor: expr << args[0] << " ^ " << args[1]; break;
            case Op::Kind::Not: expr << "~" << args[0]; break;
            case Op::Kind::Add: expr << args[0] << " + " << args[1]; break;
            case Op::Kind::Sub: expr << args[0] << " - " << args[1]; break;
            case Op::Kind::Mul: expr << args[0] << " * " << args[1]; break;
            case Op::Kind::Eq: expr << args[0] << " == " << args[1]; break;
            case Op::Kind::LtU: expr << "$unsigned(" << args[0] << ") < $unsigned(" << args[1] << ")"; break;
            case Op::Kind::LtS: expr << "$signed(" << args[0] << ") < $signed(" << args[1] << ")"; break;
            case Op::Kind::LeU: expr << "$unsigned(" << args[0] << ") <= $unsigned(" << args[1] << ")"; break;
            case Op::Kind::LeS: expr << "$signed(" << args[0] << ") <= $signed(" << args[1] << ")"; break;
            case Op::Kind::Concat: expr << '{' << args[0] << ',' << args[1] << '}'; break;
            case Op::Kind::Slice:
              expr << args[0] << '[';
              if (const Constant* const_offset = dynamic_cast<const Constant*>(op->args[1])) {
                size_t offset = const_offset->value.as_uint64();
                expr << (offset + value->width - 1) << ':' << offset;
              } else {
                expr << args[2] << '+' << args[1] << " - 1:" << args[1];
              }
              expr << ']';
            break;
            case Op::Kind::Shl: expr << args[0] << " << " << args[1]; break;
            case Op::Kind::ShrU: expr << args[0] << " >> " << args[1]; break;
            case Op::Kind::ShrS: expr << args[0] << " >>> " << args[1]; break;
            case Op::Kind::Select: expr << args[0] << " ? " << args[1] << " : " << args[2]; break;
          }
          expr << ')';
        } else if (const Memory::Read* read = dynamic_cast<const Memory::Read*>(value)) {
          std::string address = print(stream, read->address, closed);
          expr << '(' << _memory_names.at(read->memory) << '[' << address << "])";
        } else {
          std::cout << value << std::endl;
          throw Error("Unreachable: Invalid value");
        }
        
        if (_names.find(value) == _names.end()) {
          return expr.str();
        } else {
          const std::string& name = _names.at(value);
          stream << "  wire " << Width(value->width) << name << ";\n";
          stream << "  assign " << name << " = " << expr.str() << ";\n";
          return name;
        }
      }
    public:
      Printer(Module& module): _module(module) {
        for (const Reg* reg : _module.regs()) {
          _names[reg] = "reg" + std::to_string(_names.size());
        }
        
        for (const Memory* memory : _module.memories()) {
          _memory_names[memory] = "memory" + std::to_string(_names.size());
        }
        
        for (const Input* input : _module.inputs()) {
          _names[input] = input->name;
        }
        
        count_usages();
        for (const auto& [value, count] : _counts) {
          if (count > 1 &&
              _names.find(value) == _names.end() &&
              dynamic_cast<const Constant*>(value) == nullptr) {
            _names[value] = "value" + std::to_string(_names.size());
          }
        }
      }
      
      void print(std::ostream& stream) const {
        stream << "module " << _module.name() << '(';
        bool is_first = true;
        for (const Input* input : _module.inputs()) {
          if (!is_first) { stream << ", "; }
          stream << "input " << Width(input->width) << input->name;
          is_first = false;
        }
        for (const Output& output : _module.outputs()) {
          if (!is_first) { stream << ", "; }
          stream << "output " << Width(output.value->width) << output.name;
          is_first = false;
        }
        stream << ");\n";
        
        std::unordered_set<const Value*> closed;
        
        for (const Input* input : _module.inputs()) {
          closed.insert(input);
        }
        
        for (const Reg* reg : _module.regs()) {
          stream << "  reg " << Width(reg->width) << _names.at(reg) << " = " << reg->initial << ";\n";
          closed.insert(reg);
        }
        
        for (const Memory* memory : _module.memories()) {
          const std::string& name = _memory_names.at(memory);
          stream << "  reg" << Width(memory->width) << name << " [" << memory->size << "];\n";
          if (memory->initial.size() > 0) {
            stream << "  initial begin\n";
            for (const auto& [address, value] : memory->initial) {
              stream << "    " << name << "[" << address << "] = " << value << ";\n";
            }
            stream << "  end\n";
          }
        }
        
        for (const Output& output : _module.outputs()) {
          std::string value = print(stream, output.value, closed);
          stream << "  assign " << output.name << " = " << value << ";\n";
        }
        
        for (const Reg* reg : _module.regs()) {
          std::string clock = print(stream, reg->clock, closed);
          std::string next = print(stream, reg->next, closed);
          const std::string& name = _names.at(reg);
          
          stream << "  always @(posedge " << clock << ")\n";
          stream << "    " << name << " <= " << next << ";\n";
        }
        
        for (const Memory* memory : _module.memories()) {
          const std::string& name = _memory_names.at(memory);
          
          for (const Memory::Write& write : memory->writes) {
            std::string clock = print(stream, write.clock, closed);
            std::string enable = print(stream, write.enable, closed);
            std::string address = print(stream, write.address, closed);
            std::string value = print(stream, write.value, closed);
            
            stream << "  always @(posedge " << clock << ")\n";
            stream << "    if (" << enable << ")\n";
            stream << "      " << name << "[" << address << "] <= " << value << ";\n";
          }
        }
        
        stream << "\n";
        
        stream << "endmodule\n";
      }
      
      void save(const char* path) const {
        std::ofstream file;
        file.open(path);
        print(file);
      }
    };
  }
  
  namespace graphviz {
    class Printer {
    private:
      Module& _module;
      bool _show_clocks = false;
      bool _split_regs = true;
      
      struct Context {
        std::ostream& stream;
        std::unordered_map<const Value*, size_t> ids;
        std::unordered_map<const Memory*, size_t> memory_ids;
        size_t id_count = 0;
        
        Context(std::ostream& _stream): stream(_stream) {}
        
        size_t alloc() { return id_count++; }
        
        size_t alloc(const Value* value) {
          size_t id = alloc();
          ids[value] = id;
          return id;
        }
        
        size_t alloc(const Memory* value) {
          size_t id = alloc();
          memory_ids[value] = id;
          return id;
        }
        
        size_t operator[](const Value* value) const { return ids.at(value); }
        size_t operator[](const Memory* memory) const { return memory_ids.at(memory); }
      };
      
      static const char** const arg_names(Op::Kind kind) {
        #define names(...) { \
          static const char* array[] = {__VA_ARGS__}; \
          return array; \
        }
        
        switch (kind) {
          case Op::Kind::Sub:
          case Op::Kind::LtU:
          case Op::Kind::LtS:
          case Op::Kind::LeU:
          case Op::Kind::LeS: names("a", "b");
          case Op::Kind::Concat: names("high", "low");
          case Op::Kind::Slice: names("value", "offset", "width");
          case Op::Kind::Select: names("cond", "a", "b");
          default: return nullptr;
        }
        
        #undef names
      }
      
      size_t print(const Value* value, Context& ctx) const {
        if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
          size_t id = ctx.alloc();
          ctx.stream << "  n" << id << " [shape=none, label=\"";
          constant->value.write_short(ctx.stream);
          ctx.stream << "\"];\n";
          return id;
        } else if (const Unknown* unknown = dynamic_cast<const Unknown*>(value)) {
          size_t id = ctx.alloc();
          ctx.stream << "  n" << id << " [shape=none, label=\"";
          ctx.stream << unknown->width << "'bx";
          ctx.stream << "\"];\n";
          return id;
        }
        
        if (ctx.ids.find(value) != ctx.ids.end()) {
          return ctx[value];
        }
        
        size_t id = ctx.alloc(value);
        
        if (const Op* op = dynamic_cast<const Op*>(value)) {
          ctx.stream << "  n" << id << " [label=" << Op::KIND_NAMES[(size_t)op->kind] << "];\n";
        } else if (const Input* op = dynamic_cast<const Input*>(value)) {
          ctx.stream << "  n" << id << " [shape=box, label=\"" << op->name << "\"];\n";
        } else if (const Memory::Read* read = dynamic_cast<const Memory::Read*>(value)) {
          ctx.stream << "  n" << id << " [label=Read];\n";
        } else {
          ctx.stream << "  n" << id << ";\n";
        }
        
        if (const Op* op = dynamic_cast<const Op*>(value)) {
          const char** names = arg_names(op->kind);
          size_t it = 0;
          for (const Value* arg : op->args) {
            size_t arg_id = print(arg, ctx);
            ctx.stream << "  n" << arg_id << " -> n" << id;
            if (names != nullptr) {
              ctx.stream << " [label=" << names[it] << "]";
            }
            ctx.stream << ";\n";
            it++;
          }
        } else if (const Memory::Read* read = dynamic_cast<const Memory::Read*>(value)) {
          size_t address_id = print(read->address, ctx);
          ctx.stream << "  n" << address_id << " -> n" << id << ";\n";
          ctx.stream << "  n" << ctx[read->memory] << " -> n" << id << ";\n";
        }
        
        return id;
      }
      
      void write_name(const Reg* reg, Context& ctx) const {
        if (reg->name.size() == 0) {
          ctx.stream << "reg" << ctx[reg];
        } else {
          ctx.stream << reg->name;
        }
      }
      
      void write_name(const Memory* memory, Context& ctx) const {
        if (memory->name.size() == 0) {
          ctx.stream << "memory" << ctx[memory];
        } else {
          ctx.stream << memory->name;
        }
        ctx.stream << "[" << memory->size << "]";
      }
      
      template <class T>
      void declare(char prefix, const std::vector<T*>& objects, Context& ctx) const {
        if (_split_regs) {
          ctx.stream << "  { rank=same;\n";
        }
        for (const T* object : objects) {
          ctx.stream << "  " << prefix << ctx[object] << " [shape=box, label=\"";
          write_name(object, ctx);
          ctx.stream << "\"];\n";
        }
        if (_split_regs) {
          ctx.stream << "  }\n";
        }
      }
      
    public:
      Printer(Module& module): _module(module) {}
      
      void print(std::ostream& stream) const {
        stream << "digraph {\n";
        
        Context ctx(stream);
        
        for (const Reg* reg : _module.regs()) {
          ctx.alloc(reg);
        }
        
        for (const Memory* memory : _module.memories()) {
          ctx.alloc(memory);
        }
        
        declare('n', _module.regs(), ctx);
        declare('n', _module.memories(), ctx);
        if (_split_regs) {
          declare('r', _module.regs(), ctx);
          declare('m', _module.memories(), ctx);
        }
        
        for (const Reg* reg : _module.regs()) {
          if (_show_clocks) {
            size_t clock_id = print(reg->clock, ctx);
            stream << "  n" << clock_id << " -> " << (_split_regs ? 'r' : 'n') << ctx[reg] << " [label=clock];\n";
          }
          if (reg->next != nullptr) {
            size_t next_id = print(reg->next, ctx);
            stream << "  n" << next_id << " -> " << (_split_regs ? 'r' : 'n') << ctx[reg] << " [label=next];\n";
          }
        }
        
        for (const Memory* memory : _module.memories()) {
          for (const Memory::Write& write : memory->writes) {
            size_t write_id = ctx.alloc();
            ctx.stream << "  w" << write_id << " [label=Write];\n";
            
            if (_show_clocks) {
              size_t clock_id = print(write.clock, ctx);
              stream << "  n" << clock_id << " -> w" << write_id << " [label=clock];\n";
            }
            
            size_t address_id = print(write.address, ctx);
            stream << "  n" << address_id << " -> w" << write_id << " [label=address];\n";
            size_t value_id = print(write.value, ctx);
            stream << "  n" << value_id << " -> w" << write_id << " [label=value];\n";
            size_t enable_id = print(write.enable, ctx);
            stream << "  n" << enable_id << " -> w" << write_id << " [label=enable];\n";
            
            stream << "  w" << write_id << " -> " << (_split_regs ? 'm' : 'n') << ctx[memory] << ";\n";
          }
        }
        
        for (const Output& output : _module.outputs()) {
          print(output.value, ctx);
        }
        
        stream << "}\n";
      }
      
      void save(const char* path) const {
        std::ofstream file;
        file.open(path);
        print(file);
      }
    };
  }
  
  namespace sim {
    class Simulation {
    public:
      using Values = std::unordered_map<const Value*, BitString>;
      
      struct MemoryData {
        const Memory* memory = nullptr;
        std::unordered_map<uint64_t, BitString> data;
        
        MemoryData() {}
        MemoryData(const Memory* _memory):
          memory(_memory), data(_memory->initial) {}
        
        BitString& operator[](uint64_t address) {
          if (address >= memory->size) {
            //throw_error(Error,
            //  "Memory access out of bounds: Attempt to access address " << address <<
            //  " in memory of size " << memory->size
            //);
            address %= memory->size;
          }
          if (data.find(address) == data.end()) {
            data[address] = BitString(memory->width);
          }
          return data[address];
        }
      };
    private:
      Module& _module;
      std::unordered_map<const Value*, bool> _prev_clocks;
      std::vector<BitString> _regs;
      std::unordered_map<const Memory*, MemoryData> _memories;
      std::vector<BitString> _outputs;
      
      BitString eval(const Value* value, Values& values) {
        if (values.find(value) != values.end()) {
          return values.at(value);
        }
        
        BitString result;
        if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
          result = constant->value;
        } else if (const Unknown* unknown = dynamic_cast<const Unknown*>(value)) {
          throw_error(Error, "Unable to simulate with unknown values");
        } else if (const Op* op = dynamic_cast<const Op*>(value)) {
          if (op->kind == Op::Kind::Select) {
            if (eval(op->args[0], values).at(0)) {
              result = eval(op->args[1], values);
            } else {
              result = eval(op->args[2], values);
            }
          } else {
            for (size_t it = 0; it < op->args.size(); it++) {
              eval(op->args[it], values);
            }
            
            const BitString* args[Op::MAX_ARG_COUNT] = {nullptr};
            for (size_t it = 0; it < op->args.size(); it++) {
              args[it] = &values.at(op->args[it]);
            }
            result = op->eval(args);
          }
        } else if (const Memory::Read* read = dynamic_cast<const Memory::Read*>(value)) {
          BitString address = eval(read->address, values);
          result = _memories.at(read->memory)[address.as_uint64()];
        } else {
          throw Error("");
        }
        
        if (result.width() != value->width) {
          std::string name = "value";
          if (const Op* op = dynamic_cast<const Op*>(value)) {
            name = Op::KIND_NAMES[size_t(op->kind)];
          } else if (const Memory::Read* read = dynamic_cast<const Memory::Read*>(value)) {
            name = "read";
          }
          throw_error(Error, "Width mismatch: " << name << " returned BitString of width " << result.width() << ", but expected width " << value->width);
        }
        
        values[value] = result;
        return result;
      }
    public:
      Simulation(Module& module):
          _module(module),
          _regs(module.regs().size()),
          _outputs(module.outputs().size()) {

        size_t it = 0;
        for (const Reg* reg : _module.regs()) {
          _prev_clocks[reg->clock] = false;
          _regs[it++] = BitString(reg->width);
        }
        
        it = 0;
        for (const Memory* memory : _module.memories()) {
          _memories[memory] = MemoryData(memory);
          for (const Memory::Write& write : memory->writes) {
            _prev_clocks[write.clock] = false;
          }
        }
        
        reset();
      }
      
      const std::vector<BitString>& regs() const { return _regs; };
      const std::unordered_map<const Memory*, MemoryData>& memories() const { return _memories; };
      const std::vector<BitString>& outputs() const { return _outputs; };
      
      const BitString& find_output(const std::string& name) const {
        for (size_t it = 0; it < _outputs.size(); it++) {
          if (_module.outputs()[it].name == name) {
            return _outputs[it];
          }
        }
        
        throw_error(Error, "Output " << name << " not found");
      }
      
      const BitString& find_reg(const std::string& name) const {
        for (size_t it = 0; it < _regs.size(); it++) {
          if (_module.regs()[it]->name == name) {
            return _regs[it];
          }
        }
        
        throw_error(Error, "Reg " << name << " not found");
      }
      
      void reset() {
        size_t it = 0;
        for (const Reg* reg : _module.regs()) {
          _regs[it++] = reg->initial;
        }
        
        for (const Memory* memory : _module.memories()) {
          _memories[memory] = MemoryData(memory);
        }
      }
      
      Values update(const std::vector<BitString>& inputs) {
        if (inputs.size() != _module.inputs().size()) {
          throw_error(Error, "Module has " << _module.inputs().size() << " inputs, but simulation only got " << inputs.size() << " values.");
        }
        
        Values values;
        for (size_t it = 0; it < inputs.size(); it++) {
          values[_module.inputs()[it]] = inputs[it];
        }
        update(values);
        return values;
      }
      
      Values update(const std::unordered_map<std::string, BitString>& inputs) {
        if (inputs.size() != _module.inputs().size()) {
          throw_error(Error, "Module has " << _module.inputs().size() << " inputs, but simulation only got " << inputs.size() << " values.");
        }
        
        Values values;
        for (hdl::Input* input : _module.inputs()) {
          values[input] = inputs.at(input->name);
        }
        values = update(values);
        return values;
      }
      
      Values update(const Values& initial) {
        while (true) {
          Values values = initial;
          if (!update_step(values)) {
            return values;
          }
        }
      }
      
      bool update_step(Values& values) {
        size_t it = 0;
        for (const Reg* reg : _module.regs()) {
          values[reg] = _regs[it++];
        }
        
        it = 0;
        for (const Output& output : _module.outputs()) {
          _outputs[it++] = eval(output.value, values);
        }
        
        bool changed = false;
        
        it = 0;
        for (const Reg* reg : _module.regs()) {
          bool clock = eval(reg->clock, values)[0];
          if (clock && !_prev_clocks.at(reg->clock)) {
            _regs[it] = eval(reg->next, values);
            changed = true;
          }
          it++;
        }
        
        for (const Memory* memory : _module.memories()) {
          for (const Memory::Write& write : memory->writes) {
            bool clock = eval(write.clock, values)[0];
            if (clock && !_prev_clocks.at(write.clock)) {
              bool enable = eval(write.enable, values)[0];
              if (enable) {
                uint64_t address = eval(write.address, values).as_uint64();
                BitString value = eval(write.value, values);
                _memories[memory][address] = value;
                changed = true;
              }
            }
          }
        }
        
        for (auto& [value, prev] : _prev_clocks) {
          prev = eval(value, values)[0];
        }
        
        return changed;
      }
    };
    
    class VCDWriter {
    private:
      std::ostream& _stream;
      Module& _module;
      std::string _timescale = "1ps";
      size_t _timestamp = 0;
      bool _header_written = false;
      
      std::unordered_map<const Value*, std::string> _name_overrides;
      std::unordered_map<const Value*, BitString> _prev;
      std::unordered_map<const Value*, size_t> _ids;
      
      std::string name(const Value* value) {
        if (_name_overrides.find(value) != _name_overrides.end()) {
          return _name_overrides.at(value);
        } else if (const Input* input = dynamic_cast<const Input*>(value)) {
          if (input->name.size() > 0) {
            return input->name;
          }
        } else if (const Reg* reg = dynamic_cast<const Reg*>(value)) {
          if (reg->name.size() > 0) {
            return reg->name;
          }
        }
        
        size_t id = _ids.at(value);
        return std::string("v") + std::to_string(id);
      }
      
      void print_id(size_t id) {
        if (id == 0) {
          _stream << '!';
          return;
        }
        
        constexpr char MIN_PRINTABLE = 33;
        constexpr char MAX_PRINTABLE = 126;
        constexpr char PRINTABLE_COUNT = (MAX_PRINTABLE - MIN_PRINTABLE + 1);
        
        while (id > 0) {
          _stream << char((id % PRINTABLE_COUNT) + MIN_PRINTABLE);
          id /= PRINTABLE_COUNT;
        }
      }
      
      void dump(size_t id, const BitString& value) {
        if (value.width() == 1) {
          _stream << (value[0] ? '1' : '0');
        } else {
          _stream << 'b';
          for (size_t it = value.width(); it-- > 0; ) {
            _stream << (value[it] ? '1' : '0');
          }
          _stream << ' ';
        }
        
        print_id(id);
        _stream << std::endl;
      }
    public:
      VCDWriter(std::ostream& stream, Module& module):
          _stream(stream),
          _module(module),
          _prev(module.regs().size()) {
        
        for (const Reg* reg : _module.regs()) {
          probe(reg);
        }
        
        for (const Input* input : _module.inputs()) {
          probe(input);
        }
        
        for (const Output& output : _module.outputs()) {
          probe(output.value, output.name);
        }
      }
      
      inline const std::string& timescale() const { return _timescale; }
      inline void set_timescale(const std::string& timescale) {
        if (_header_written) {
          throw_error(Error, "Unable to change timescale after writing to VCD file");
        }
        _timescale = timescale;
      }
      
      inline size_t timestamp() const { return _timestamp; }
      inline void set_timestamp(size_t timestamp) { _timestamp = timestamp; }
      
      void probe(const Value* value) {
        if (_header_written) {
          throw_error(Error, "Unable to add probe after writing to VCD file");
        }
        _ids.insert({value, _ids.size()});
      }
      
      void probe(const Value* value, const std::string& name) {
        probe(value);
        _name_overrides.insert({value, name});
      }
      
      void write_header() {
        _stream << "$timescale " << _timescale << " $end" << std::endl;
        _stream << "$scope module " << _module.name() << " $end" << std::endl;
        for (const auto& [value, id] : _ids) {
          const Reg* reg = dynamic_cast<const Reg*>(value);
          
          _stream << "$var " << (reg ? "reg" : "wire") << " " << value->width << " ";
          print_id(id);
          _stream << " " << name(value) << " $end" << std::endl;
        }
        _stream << "$upscope $end" << std::endl;
        _stream << "$enddefinitions $end" << std::endl;
        _stream << "$dumpvars" << std::endl;
        
        for (const auto& [value, id] : _ids) {
          if (const Reg* reg = dynamic_cast<const Reg*>(value)) {
            _prev[value] = reg->initial;
          } else {
            _prev[value] = BitString(value->width);
          }
          
          dump(id, _prev.at(value));
        }
        
        _stream << "$end" << std::endl;
        _header_written = true;
      }
      
      void write(const std::unordered_map<const Value*, BitString>& values) {
        if (!_header_written) {
          write_header();
        }
        
        _stream << "#" << _timestamp << std::endl;
        
        for (const auto& [value, id] : _ids) {
          const BitString& current = values.at(value);
          if (current != _prev.at(value)) {
            dump(id, current);
            _prev[value] = current;
          }
        }
        
        _timestamp++;
      }
      
    };
  }
}

#undef throw_error

#endif
