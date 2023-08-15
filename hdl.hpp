// Copyright (c) 2023 Can Joshua Lehmann

#ifndef HDL_HPP
#define HDL_HPP

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <sstream>

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
    Value* initial = nullptr;
    Value* clock = nullptr;
    Value* next = nullptr;
    
    Reg(Value* _initial, Value* _clock):
      Value(_initial->width), initial(_initial), clock(_clock) {}
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
  
  struct Op: public Comb {
    enum class Kind {
      And, Or, Xor, Not,
      Add, Sub, Mul,
      Eq, LtU, LtS,
      Select
    };
    
    const Kind kind;
    const std::vector<Value*> args;
    
  private:
    static void expect_arg_count(const std::vector<Value*> args, size_t count) {
    }
    
    static void expect_equal_width(const std::vector<Value*> args, size_t a, size_t b) {
    }
    
    static size_t infer_width(Kind& kind, const std::vector<Value*> args) {
      switch (kind) {
        case Kind::Not:
          expect_arg_count(args, 1);
          return args[0]->width;
        case Kind::And:
        case Kind::Or:
        case Kind::Xor:
        case Kind::Add:
        case Kind::Sub:
          expect_arg_count(args, 2);
          expect_equal_width(args, 0, 1);
          return args[0]->width;
        case Kind::Mul:
          expect_arg_count(args, 2);
          return args[0]->width + args[1]->width;
        case Kind::Eq:
        case Kind::LtU:
        case Kind::LtS:
          expect_arg_count(args, 2);
          expect_equal_width(args, 0, 1);
          return 1;
        case Kind::Select:
          expect_arg_count(args, 3);
          expect_equal_width(args, 1, 2);
          return args[1]->width;
      }
    }
  public:
    
    Op(Kind _kind, const std::vector<Value*>& _args):
      Comb(infer_width(_kind, _args)), kind(_kind), args(_args) {}
    
    bool hashcons_equal(const Op& other) const {
      return kind == other.kind && args == other.args;
    }
  };
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
    };
    
    std::string _name;
    Hashcons<Constant> _constants;
    Hashcons<Op> _ops;
    std::vector<Reg*> _regs;
    std::vector<Input*> _inputs;
    std::vector<Output> _outputs;
  public:
    Module(const std::string& name): _name(name) {}
    
    inline const std::string& name() const { return _name; }
    inline const std::vector<Reg*> regs() const { return _regs; }
    inline const std::vector<Input*> inputs() const { return _inputs; }
    inline const std::vector<Output> outputs() const { return _outputs; }
    
    Input* input(const std::string& name, size_t width) {
      Input* input = new Input(name, width);
      _inputs.push_back(input);
      return input;
    }
    
    void output(const std::string& name, Value* value) {
      _outputs.push_back(Output { name, value });
    }
    
    Reg* reg(Value* initial, Value* clock) {
      Reg* reg = new Reg(initial, clock);
      _regs.push_back(reg);
      return reg;
    }
    
    Op* op(Op::Kind kind, const std::vector<Value*>& args) {
      return _ops[Op(kind, args)];
    }
    
    Constant* constant(const BitString& bit_string) {
      return _constants[Constant(bit_string)];
    }
    
    void usages(const Value* value, std::unordered_map<const Value*, size_t>& counts) const {
      if (counts.find(value) != counts.end()) {
        counts[value] += 1;
        return;
      }
      
      counts[value] = 1;
      if (const Op* op = dynamic_cast<const Op*>(value)) {
        for (const Value* arg : op->args) {
          usages(arg, counts);
        }
      }
    }
    
    std::unordered_map<const Value*, size_t> usages() const {
      std::unordered_map<const Value*, size_t> counts;
      for (const Reg* reg : _regs) {
        usages(reg->initial, counts);
        usages(reg->clock, counts);
        usages(reg->next, counts);
      }
      
      for (const Output& output : _outputs) {
        usages(output.value, counts);
      }
      
      return counts;
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
      
      std::string print(std::ostream& stream, const Value* value, std::unordered_set<const Value*>& closed) const {
        if (closed.find(value) != closed.end()) {
          if (_names.find(value) == _names.end()) {
            throw Error("Unreachable");
          }
          return _names.at(value);
        }
        closed.insert(value);
        
        std::ostringstream expr;
        if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
          expr << constant->value;
        } else if (const Op* op = dynamic_cast<const Op*>(value)) {
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
            case Op::Kind::Select: expr << args[0] << " ? " << args[1] << " : " << args[2]; break;
          }
          expr << ')';
        } else {
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
        
        for (const Input* input : _module.inputs()) {
          _names[input] = input->name;
        }
        
        std::unordered_map<const Value*, size_t> counts = _module.usages();
        for (const auto& [value, count] : counts) {
          if (count > 1 && _names.find(value) == _names.end()) {
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
          stream << "  reg " << Width(reg->width) << _names.at(reg) << ";\n";
          closed.insert(reg);
        }
        
        for (const Output& output : _module.outputs()) {
          std::string value = print(stream, output.value, closed);
          stream << "  assign " << output.name << " = " << value << ";\n";
        }
        
        for (const Reg* reg : _module.regs()) {
          std::string initial = print(stream, reg->initial, closed);
          std::string clock = print(stream, reg->clock, closed);
          std::string next = print(stream, reg->next, closed);
          const std::string& name = _names.at(reg);
          
          stream << "  initial " << name << " = " << initial << ";\n";
          stream << "  always @(posedge " << clock << ")\n";
          stream << "    " << name << " <= " << next << ";\n";
        }
        
        stream << "\n";
        
        stream << "endmodule\n";
      }
    };
  }
  
  namespace sim {
    class Simulation {
    private:
      using Values = std::unordered_map<const Value*, BitString>;
      
      Module& _module;
      std::vector<BitString> _inputs;
      std::vector<BitString> _regs;
      std::vector<bool> _prev_clocks;
      std::vector<BitString> _outputs;
      
      BitString eval(const Value* value, Values& values) {
        if (values.find(value) != values.end()) {
          return values.at(value);
        }
        
        BitString result;
        if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
          result = constant->value;
        } else if (const Op* op = dynamic_cast<const Op*>(value)) {
          #define arg(index) eval(op->args[index], values)
          #define binop(op) result = arg(0) op arg(1);
          
          switch (op->kind) {
            case Op::Kind::And: binop(&); break;
            case Op::Kind::Or: binop(|); break;
            case Op::Kind::Xor: binop(^); break;
            case Op::Kind::Not: result = ~arg(0); break;
            case Op::Kind::Add: binop(+); break;
            case Op::Kind::Sub: binop(-); break;
            case Op::Kind::Mul: throw_error(Error, "Not implemented"); break;
            case Op::Kind::Eq: result = BitString::from_bool(arg(0) == arg(1)); break;
            case Op::Kind::LtU: result = BitString::from_bool(arg(0).lt_u(arg(1))); break;
            case Op::Kind::LtS: throw_error(Error, "Not implemented"); break;
            case Op::Kind::Select: result = arg(0)[0] ? arg(1) : arg(2); break;
          }
          
          #undef binop
          #undef arg
        } else {
          throw Error("");
        }
        
        values[value] = result;
        return result;
      }
      
      Values eval() {
        Values values;
        
        size_t it = 0;
        for (const Reg* reg : _module.regs()) {
          values[reg] = _regs[it++];
        }
        
        it = 0;
        for (const Input* input : _module.inputs()) {
          values[input] = _inputs[it++];
        }
        
        it = 0;
        for (const Output& output : _module.outputs()) {
          _outputs[it++] = eval(output.value, values);
        }
        
        return values;
      }
    public:
      Simulation(Module& module):
          _module(module),
          _inputs(module.inputs().size()),
          _regs(module.regs().size()),
          _prev_clocks(module.regs().size()),
          _outputs(module.outputs().size()) {
        size_t it = 0;
        for (const Input* input : _module.inputs()) {
          _inputs[it++] = BitString(input->width);
        }
        
        it = 0;
        for (const Reg* reg : _module.regs()) {
          _prev_clocks[it] = false;
          _regs[it++] = BitString(reg->width);
        }
        reset();
      }
      
      const std::vector<BitString>& inputs() const { return _inputs; };
      const std::vector<BitString>& regs() const { return _regs; };
      const std::vector<BitString>& outputs() const { return _outputs; };
      
      void reset() {
        Values values = eval();
        
        size_t it = 0;
        for (const Reg* reg : _module.regs()) {
          _regs[it++] = eval(reg->initial, values);
        }
      }
      
      void update(const std::vector<BitString>& inputs) {
        _inputs = inputs;
        
        Values values = eval();
        size_t it = 0;
        for (const Reg* reg : _module.regs()) {
          bool clock = eval(reg->clock, values)[0];
          if (clock && !_prev_clocks[it]) {
            _regs[it] = eval(reg->next, values);
          }
          _prev_clocks[it] = clock;
          it++;
        }
      }
    };
  }
}

#undef throw_error

#endif
