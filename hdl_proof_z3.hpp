// Copyright (c) 2023 Can Joshua Lehmann

#ifndef HDL_PROOF
#define HDL_PROOF

#include <vector>
#include <map>
#include <optional>
#include <string>
#include <sstream>

#include <z3++.h>

#include "hdl.hpp"

namespace hdl {
  namespace proof {
    namespace z3 {
      
      class Builder {
      private:
        ::z3::context context;
        ::z3::solver solver;
        std::map<const Value*, ::z3::expr> values;
        
        ::z3::expr bv_val(const BitString& bit_string) {
          bool bits[bit_string.width()];
          for (size_t it = 0; it < bit_string.width(); it++) {
            bits[it] = bit_string[it];
          }
          return context.bv_val(bit_string.width(), &bits[0]);
        }
        
        ::z3::expr bool2bv(::z3::expr expr) {
          return ::z3::ite(expr,
            bv_val(BitString::from_bool(true)),
            bv_val(BitString::from_bool(false))
          );
        }
      public:
        Builder(): context(), solver(context) {}
        
        void free(const Value* value) {
          std::ostringstream name;
          name << "value" << values.size();
          values.emplace(value, context.bv_const(name.str().c_str(), value->width));
        }
        
        void build(const Value* value) {
          if (values.find(value) != values.end()) {
            return;
          }
          
          std::optional<::z3::expr> expr;
          if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
            expr = bv_val(constant->value);
          } else if (const Op* op = dynamic_cast<const Op*>(value)) {
            for (const Value* arg : op->args) {
              build(arg);
            }
            
            #define arg(index) values.at(op->args[index])
            
            switch (op->kind) {
              case Op::Kind::And: expr = arg(0) & arg(1); break;
              case Op::Kind::Or: expr = arg(0) | arg(1); break;
              case Op::Kind::Xor: expr = arg(0) ^ arg(1); break;
              case Op::Kind::Not: expr = ~arg(0); break;
              case Op::Kind::Add: expr = arg(0) + arg(1); break;
              case Op::Kind::Sub: expr = arg(0) - arg(1); break;
              case Op::Kind::Mul: expr = arg(0) * arg(1); break; // TODO
              case Op::Kind::Eq: expr = bool2bv(arg(0) == arg(1)); break;
              case Op::Kind::LtU: expr = bool2bv(::z3::ult(arg(0), arg(1))); break;
              case Op::Kind::LtS: expr = bool2bv(::z3::slt(arg(0), arg(1))); break;
              case Op::Kind::LeU: expr = bool2bv(::z3::ule(arg(0), arg(1))); break;
              case Op::Kind::LeS: expr = bool2bv(::z3::sle(arg(0), arg(1))); break;
              case Op::Kind::Shl: expr = ::z3::shl(arg(0), arg(1)); break;
              case Op::Kind::ShrU: expr = ::z3::lshr(arg(0), arg(1)); break;
              case Op::Kind::ShrS: expr = ::z3::ashr(arg(0), arg(1)); break;
              case Op::Kind::Select: expr = ::z3::ite(arg(0).bit2bool(0), arg(1), arg(2)); break;
              default:
                throw Error("");
            }
            
            #undef arg
          } else {
            throw Error("");
          }
          
          values.emplace(value, expr.value());
        }
        
        void require(const Value* value, const BitString& string) {
          build(value);
          
          solver.add(values.at(value) == bv_val(string));
        }
        
        bool satisfiable() {
          return solver.check() != ::z3::unsat;
        }
        
        BitString interp(const Value* value) {
          auto model = solver.get_model();
          auto numeral = model.eval(values.at(value), true);
          BitString bit_string(numeral.get_sort().bv_size());
          for (size_t it = 0; it < bit_string.width(); it++) {
            switch (model.eval(numeral.bit2bool(0), true).bool_value()) {
              case Z3_L_TRUE: bit_string.set(it, true); break;
              case Z3_L_FALSE: bit_string.set(it, false); break;
              default: throw Error("");
            }
          }
          return bit_string;
        }
        
        std::string to_smt2() {
          return solver.to_smt2();
        }
      };
      
    }
  }
}

#endif
