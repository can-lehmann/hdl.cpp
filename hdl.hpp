// Copyright (c) 2023 Can Joshua Lehmann

#ifndef HDL_HPP
#define HDL_HPP

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
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
    Value* initial = nullptr;
    Value* clock = nullptr;
    Value* next = nullptr;
    std::string name;
    
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
      Eq, LtU, LtS, LeU, LeS,
      Concat, Slice,
      Select
    };
    
    static constexpr const char* KIND_NAMES[] = {
      "And", "Or", "Xor", "Not",
      "Add", "Sub", "Mul",
      "Eq", "LtU", "LtS", "LeU", "LeS",
      "Concat", "Slice",
      "Select"
    };
    
    static constexpr const size_t MAX_ARG_COUNT = 3;
    
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
    
    BitString eval(const BitString** values) const {
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
        case Op::Kind::LtS: throw_error(Error, "Not implemented"); break;
        case Op::Kind::LeU: result = BitString::from_bool(arg(0).le_u(arg(1))); break;
        case Op::Kind::LeS: throw_error(Error, "Not implemented"); break;
        case Op::Kind::Concat: result = arg(0).concat(arg(1)); break;
        case Op::Kind::Slice: result = arg(0).slice_width(arg(1).as_uint64(), arg(2).as_uint64()); break;
        case Op::Kind::Select: result = (arg(0))[0] ? arg(1) : arg(2); break;
      }
      
      #undef binop
      #undef arg
      
      return result;
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
    
    Module(const Module& other) = delete;
    Module& operator=(const Module& other) = delete;
    
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
      reg->next = reg;
      _regs.push_back(reg);
      return reg;
    }
    
    Value* op(Op::Kind kind, const std::vector<Value*>& args) {
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
            } else if (Constant* constant = dynamic_cast<Constant*>(args[1])) {
              if (constant->value.is_zero()) {
                return constant;
              } else if (constant->value.is_all_ones()) {
                return args[0];
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
            } else if (Constant* constant = dynamic_cast<Constant*>(args[1])) {
              if (constant->value.is_zero()) {
                return args[0];
              } else if (constant->value.is_all_ones()) {
                return constant;
              }
            }
          break;
          case Op::Kind::Xor:
            if (args[0] == args[1]) {
              return constant(BitString(args[0]->width));
            }
          break;
          case Op::Kind::Not:
            if (const Op* arg = dynamic_cast<const Op*>(args[0])) {
              if (arg->kind == Op::Kind::Not) {
                return arg->args[0];
              }
            }
          break;
          // TODO
          case Op::Kind::Eq:
            if (args[0] == args[1]) {
              return constant(BitString::from_bool(true));
            }
          break;
          // TODO
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
        
        if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
          std::ostringstream expr;
          expr << constant->value;
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
            // TODO: Correctly count usages in Slice
            case Op::Kind::Slice: expr << args[0] << '[' << args[2] << '+' << args[1] << " - 1:" << args[1] << ']'; break;
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
        size_t id_count = 0;
        
        Context(std::ostream& _stream): stream(_stream) {}
        
        size_t alloc() { return id_count++; }
        size_t alloc(const Value* value) {
          size_t id = alloc();
          ids[value] = id;
          return id;
        }
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
          ctx.stream << "  n" << id << " [shape=none, label=\"" << constant->value.width() << "'b";
          size_t highest_significant_digit = constant->value.width() - 1;
          while (highest_significant_digit > 0 && !constant->value[highest_significant_digit]) {
            highest_significant_digit--;
          }
          for (size_t it = highest_significant_digit + 1; it-- > 0; ) {
            ctx.stream << (constant->value[it] ? '1' : '0');
          }
          ctx.stream << "\"];\n";
          return id;
        }
        
        if (ctx.ids.find(value) != ctx.ids.end()) {
          return ctx.ids.at(value);
        }
        
        size_t id = ctx.alloc(value);
        
        if (const Op* op = dynamic_cast<const Op*>(value)) {
          ctx.stream << "  n" << id << " [label=" << Op::KIND_NAMES[(size_t)op->kind] << "];\n";
        } else if (const Input* op = dynamic_cast<const Input*>(value)) {
          ctx.stream << "  n" << id << " [shape=box, label=\"" << op->name << "\"];\n";
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
        }
        
        return id;
      }
      
      void declare_regs(char prefix, Context& ctx) const {
        if (_split_regs) {
          ctx.stream << "  { rank=same;\n";
        }
        for (const Reg* reg : _module.regs()) {
          ctx.stream << "  " << prefix << ctx.ids.at(reg) << " [shape=box, label=\"";
          if (reg->name.size() == 0) {
            ctx.stream << "reg" << ctx.ids.at(reg);
          } else {
            ctx.stream << reg->name;
          }
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
        
        declare_regs('n', ctx);
        if (_split_regs) {
          declare_regs('r', ctx);
        }
        
        for (const Reg* reg : _module.regs()) {
          size_t initial_id = print(reg->initial, ctx);
          stream << "  n" << initial_id << " -> " << (_split_regs ? 'r' : 'n') << ctx.ids.at(reg) << " [label=initial];\n";
          if (_show_clocks) {
            size_t clock_id = print(reg->clock, ctx);
            stream << "  n" << clock_id << " -> " << (_split_regs ? 'r' : 'n') << ctx.ids.at(reg) << " [label=clock];\n";
          }
          if (reg->next != nullptr) {
            size_t next_id = print(reg->next, ctx);
            stream << "  n" << next_id << " -> " << (_split_regs ? 'r' : 'n') << ctx.ids.at(reg) << " [label=next];\n";
          }
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
          for (size_t it = 0; it < op->args.size(); it++) {
            eval(op->args[it], values);
          }
          
          const BitString* args[Op::MAX_ARG_COUNT] = {nullptr};
          for (size_t it = 0; it < op->args.size(); it++) {
            args[it] = &values.find(op->args[it])->second;
          }
          result = op->eval(args);
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
        if (inputs.size() != _module.inputs().size()) {
          throw_error(Error, "Module has " << _module.inputs().size() << " inputs, but simulation only got " << inputs.size() << " values.");
        }
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
