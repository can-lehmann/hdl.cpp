// Copyright (c) 2023 Can Joshua Lehmann

#ifndef HDL_PROOF
#define HDL_PROOF

#include <inttypes.h>
#include <vector>
#include <fstream>

#include "hdl.hpp"

namespace hdl {
  namespace proof {
    class Cnf {
    public:
      struct Literal {
        int64_t id = 0;
        
        Literal() {}
        Literal(int64_t _id): id(_id) {}
        Literal operator!() const { return Literal(-id); }
      };
    private:
      std::vector<Literal> _literals;
      std::vector<size_t> _clause_indices;
      int64_t _var_count = 0;
    public:
      Cnf() { }
      
      size_t size() const { return _clause_indices.size(); }
      
      Literal var() {
        return Literal(++_var_count);
      }
      
      void add_clause(const std::vector<Literal>& clause) {
        _literals.insert(_literals.end(), clause.begin(), clause.end());
        _clause_indices.push_back(_literals.size());
      }
      
      // Relations
      
      void r_and(Literal a, Literal b, Literal c) {
        // a && b <=> c
        // (a && b => c) && (c => a && b)
        // (!(a && b) || c) && (!c || (a && b))
        // (!a || !b || c) && (!c || a) && (!c || b)
        add_clause({!a, !b, c});
        add_clause({!c, a});
        add_clause({!c, b});
      }
      
      void r_or(Literal a, Literal b, Literal c) {
        // a || b <=> c
        // (a || b => c) && (c => a || b)
        // (!(a || b) || c) && (!c || a || b)
        // (!a || c) && (!b || c) && (!c || a || b)
        add_clause({!a, c});
        add_clause({!b, c});
        add_clause({!c, a, b});
      }
      
      void r_xor(Literal a, Literal b, Literal c) {
        // (a </> b) <=> c
        // ((a </> b) => c) && (c => (a </> b))
        // ((a && b) || (!a && !b) || c) && (!c || (!a && b) || (a && !b))
        // (a || !b || c) && (b || !a || c) && (!c || !a || !b) && (!c || b || a)
        add_clause({a, !b, c});
        add_clause({b, !a, c});
        add_clause({!b, !a, !c});
        add_clause({b, a, !c});
      }
      
      void r_eq(Literal a, Literal b, Literal c) {
        // (a <=> b) <=> c
        // ((a <=> b) => c) && (c => (a <=> b))
        // ((a </> b) || c) && (!c || (a <=> b))
        // ((a && !b) || (!a && b) || c) && (!c || (a && b) || (!a && !b))
        // (a || b || c) && (!b || !a || c) && (!c || a || !b) && (!c || b || !a)
        add_clause({a, b, c});
        add_clause({!a, !b, c});
        add_clause({a, !b, !c});
        add_clause({!a, b, !c});
      }
      
      void r_not(Literal a, Literal b) {
        // !a <=> b
        // (!a => b) && (b => !a)
        // (a || b) && (!b || !a)
        add_clause({a, b});
        add_clause({!a, !b});
      }
      
      void r_select(Literal cond, Literal a, Literal b, Literal c) {
        // (cond ? a : b) <=> c
        // (cond ? a : b) => c && c => (cond ? a : b)
        // (!((cond && a) || (!cond && b)) || c) && (!c || (cond && a) || (!cond && b))
        // (((!cond || !a) && (cond || !b)) || c) && (!c || (cond && a) || (!cond && b))
        
        // (!cond || !a || c) && (cond || !b || c) &&
        // (!c || a || !cond) && (!c || cond || b) && (!c || a || b)
        
        add_clause({!cond, !a, c});
        add_clause({cond, !b, c});
        add_clause({!c, a, !cond});
        add_clause({!c, cond, b});
        add_clause({!c, a, b});
      }
      
      // Functional API
      
      #define binop(name, relation) \
        Literal name(Literal a, Literal b) { \
          Literal c = var(); \
          relation(a, b, c); \
          return c; \
        }
      
      binop(f_and, r_and);
      binop(f_or, r_or);
      binop(f_xor, r_xor);
      binop(f_eq, r_eq);
      
      Literal f_not(Literal x) const { return !x; }
      Literal f_select(Literal cond, Literal a, Literal b) {
        Literal c = var();
        r_select(cond, a, b, c);
        return c;
      }
      
      Literal f_const(bool value) {
        Literal lit = var();
        if (value) {
          add_clause({lit});
        } else {
          add_clause({!lit});
        }
        return lit;
      }
      
      std::vector<Literal> f_not(const std::vector<Literal>& a) {
        std::vector<Literal> result(a.size());
        for (size_t it = 0; it < a.size(); it++) {
          result[it] = !a[it];
        }
        return result;
      }
      
      Literal f_eq(const std::vector<Literal>& a, const std::vector<Literal>& b) {
        if (a.size() != b.size()) {
          throw Error("");
        }
        Literal result = f_const(true);
        for (size_t it = 0; it < a.size(); it++) {
          result = f_and(result, f_eq(a[it], b[it]));
        }
        return result;
      }
      
      Literal f_lt_u(const std::vector<Literal>& a, const std::vector<Literal>& b) {
        if (a.size() != b.size()) {
          throw Error("");
        }
        
        Literal active = f_const(true);
        Literal result = f_const(false);
        for (size_t it = a.size(); it-- > 0; ) {
          result = f_or(result, f_and(active, f_and(!a[it], b[it])));
          active = f_and(active, !f_and(a[it], !b[it]));
        }
        
        return result;
      }
      
      std::vector<Literal> f_add_carry(const std::vector<Literal>& a, const std::vector<Literal>& b, Literal carry) {
        if (a.size() != b.size()) {
          throw Error("");
        }
        
        std::vector<Literal> sum(a.size());
        for (size_t it = 0; it < a.size(); it++) {
          sum[it] = f_xor(carry, f_xor(a[it], b[it]));
          carry = f_or(f_or(f_and(carry, b[it]), f_and(a[it], carry)), f_and(a[it], b[it]));
        }
        return sum;
      }
      
      std::vector<Literal> f_add(const std::vector<Literal>& a, const std::vector<Literal>& b) {
        return f_add_carry(a, b, f_const(false));
      }
      
      std::vector<Literal> f_sub(const std::vector<Literal>& a, const std::vector<Literal>& b) {
        return f_add_carry(a, f_not(b), f_const(true));
      }
      
      #undef binop
      
      // I/O
      
      void write(std::ostream& stream) const {
        stream << "p cnf " << _var_count << ' ' << _clause_indices.size() << '\n';
        size_t clause_start = 0;
        for (size_t clause_end : _clause_indices) {
          for (size_t it = clause_start; it < clause_end; it++) {
            if (it != clause_start) { stream << ' '; }
            stream << _literals[it].id;
          }
          stream << " 0\n";
          clause_start = clause_end;
        }
      }
      
      void save(const char* path) const {
        std::ofstream stream;
        stream.open(path);
        write(stream);
      }
    };
    
    class CnfBuilder {
    private:
      Cnf _cnf;
      std::unordered_map<const Value*, std::vector<Cnf::Literal>> _values;
    public:
      CnfBuilder() {}
      
      const Cnf& cnf() const { return _cnf; }
      
      void free(const Value* value) {
        std::vector<Cnf::Literal> result(value->width);
        for (Cnf::Literal& lit : result) {
          lit = _cnf.var();
        }
        _values[value] = result;
      }
      
      void build(const Value* value) {
        if (_values.find(value) != _values.end()) {
          return;
        }
        
        std::vector<Cnf::Literal> result(value->width);
        
        if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
          for (size_t it = 0; it < constant->value.width(); it++) {
            result[it] = _cnf.f_const(constant->value[it]);
          }
        } else if (const Op* op = dynamic_cast<const Op*>(value)) {
          for (const Value* arg : op->args) {
            build(arg);
          }
          
          #define arg(index) _values.at(op->args[index])

          #define elementwise(build_expr) \
            for (size_t it = 0; it < op->width; it++) { \
              result[it] = _cnf.build_expr; \
            }
          
          switch (op->kind) {
            case Op::Kind::And: elementwise(f_and(arg(0)[it], arg(1)[it])); break;
            case Op::Kind::Or: elementwise(f_or(arg(0)[it], arg(1)[it])); break;
            case Op::Kind::Xor: elementwise(f_xor(arg(0)[it], arg(1)[it])); break;
            case Op::Kind::Not: elementwise(f_not(arg(0)[it])); break;
            case Op::Kind::Add: result = _cnf.f_add(arg(0), arg(1)); break;
            case Op::Kind::Sub: result = _cnf.f_sub(arg(0), arg(1)); break;
            
            case Op::Kind::Eq: result[0] = _cnf.f_eq(arg(0), arg(1)); break;
            case Op::Kind::LtU: result[0] = _cnf.f_lt_u(arg(0), arg(1)); break;
            
            case Op::Kind::LeU: result[0] = _cnf.f_or(_cnf.f_lt_u(arg(0), arg(1)), _cnf.f_eq(arg(0), arg(1))); break;
            
            case Op::Kind::Select: elementwise(f_select(arg(0)[0], arg(1)[it], arg(2)[it])); break;
            
            default: throw Error("Op not implemented");
          }
          
          #undef elementwise
          #undef arg
        } else {
          throw Error("");
        }
        
        _values[value] = result;
      }
      
      void require(const Value* value, const BitString& string) {
        if (value->width != string.width()) {
          throw Error("");
        }
        
        build(value);
        const std::vector<Cnf::Literal>& literals = _values.at(value);
        for (size_t it = 0; it < value->width; it++) {
          if (string[it]) {
            _cnf.add_clause({literals[it]});
          } else {
            _cnf.add_clause({!literals[it]});
          }
        }
      }
      
    };
  }
}

#endif
