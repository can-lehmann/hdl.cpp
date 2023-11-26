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

#ifndef HDL_PROOF
#define HDL_PROOF

#include <inttypes.h>
#include <vector>
#include <set>
#include <map>
#include <fstream>

#include "hdl.hpp"

#define throw_error(Error, msg) { \
  std::ostringstream error_message; \
  error_message << msg; \
  throw Error(error_message.str()); \
}

namespace hdl {
  namespace proof {
    class Cnf {
    public:
      struct Literal {
        int64_t id = 0;
        
        Literal() {}
        Literal(int64_t _id): id(_id) {}
        Literal operator!() const { return Literal(-id); }
        
        inline int64_t var() const { return (id < 0 ? -id : id) - 1; }
        inline bool is_positive() const { return id > 0; }
        inline bool is_negative() const { return id < 0; }
        inline bool is_valid() const { return id != 0; }
      };
    private:
      std::vector<Literal> _literals;
      std::vector<size_t> _clause_indices;
      int64_t _var_count = 0;
    public:
      Cnf() { }
      
      size_t var_count() const { return _var_count; }
      size_t clause_count() const { return _clause_indices.size(); }
      size_t size() const { return _literals.size(); }
      
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
      
      #undef binop
      
      // Simplify
      
      size_t clause_start_index(size_t clause_id) const {
        return clause_id == 0 ? 0 : _clause_indices[clause_id - 1];
      }
      
      size_t clause_end_index(size_t clause_id) const {
        return _clause_indices[clause_id];
      }
      
      Cnf simplify() const {
        struct Simplification {
          struct Uses {
            std::set<size_t> positive;
            std::set<size_t> negative;
            
            std::set<size_t>& operator[](bool polarity) {
              if (polarity) {
                return positive;
              } else {
                return negative;
              }
            }
            
            bool is_pure() const {
              return positive.size() == 0 || negative.size() == 0;
            }
          };
          
          const Cnf& cnf;
          std::vector<Uses> uses;
          std::vector<size_t> clause_sizes;
          std::map<int64_t, bool> assignments;
          std::set<size_t> inactive_clauses;
          std::vector<size_t> unit_clauses;
          bool is_unsat = false;
          
          Simplification(const Cnf& _cnf): cnf(_cnf), uses(_cnf._var_count), clause_sizes(_cnf._clause_indices.size()) {
            for (size_t clause_start = 0, clause_id = 0; clause_id < cnf._clause_indices.size(); clause_id++) {
              size_t clause_end = cnf._clause_indices[clause_id];
              for (size_t it = clause_start; it < clause_end; it++) {
                Literal lit = cnf._literals[it];
                uses[lit.var()][lit.is_positive()].insert(clause_id);
              }
              size_t clause_size = clause_end - clause_start;
              clause_sizes[clause_id] = clause_size;
              if (clause_size == 1) {
                unit_clauses.push_back(clause_id);
              } else if (clause_size == 0) {
                is_unsat = true;
              }
              clause_start = clause_end;
            }
          }
        
          bool is_active(size_t clause_id) const {
            return inactive_clauses.find(clause_id) == inactive_clauses.end();
          }
          
          bool is_assigned(int64_t var_id) const {
            return assignments.find(var_id) != assignments.end();
          }
        
        private:
          void deactivate_clause(size_t clause_id) {
            inactive_clauses.insert(clause_id);
            for (size_t it = cnf.clause_start_index(clause_id); it < cnf.clause_end_index(clause_id); it++) {
              Literal lit = cnf._literals[it];
              uses[lit.var()][lit.is_positive()].erase(clause_id);
            }
          }
        
        public:
          void assign(int64_t var_id, bool value) {
            if (is_unsat) { return; }
            if (is_assigned(var_id)) {
              if (assignments.at(var_id) != value) {
                is_unsat = true;
              }
              return;
            }
            
            assignments[var_id] = value;
            Uses var_uses = uses[var_id];
            for (size_t clause_id : var_uses[value]) {
              deactivate_clause(clause_id);
            }
            for (size_t clause_id : var_uses[!value]) {
              for (size_t it = cnf.clause_start_index(clause_id); it < cnf.clause_end_index(clause_id); it++) {
                if (cnf._literals[it].var() == var_id) {
                  clause_sizes[clause_id] -= 1;
                }
                if (clause_sizes[clause_id] == 1) {
                  unit_clauses.push_back(clause_id);
                } else if (clause_sizes[clause_id] == 0) {
                  is_unsat = true;
                  return;
                }
              }
            }
          }
          
          void unit_prop() {
            while (unit_clauses.size() > 0 && !is_unsat) {
              size_t clause_id = unit_clauses.back();
              unit_clauses.pop_back();
              
              if (!is_active(clause_id) || clause_sizes[clause_id] != 1) {
                continue;
              }
              
              for (size_t it = cnf.clause_start_index(clause_id); it < cnf.clause_end_index(clause_id); it++) {
                Literal lit = cnf._literals[it];
                if (!is_assigned(lit.var())) {
                  assign(lit.var(), lit.is_positive());
                  break;
                }
              }
            }
          }
          
          void assign_pure() {
            for (size_t var_id = 0; var_id < uses.size(); var_id++) {
              if (!is_assigned(var_id) && uses[var_id].is_pure()) {
                assign(var_id, uses[var_id].positive.size() > 0);
              }
            }
          }
          
          Cnf to_cnf() {
            Cnf result;
            if (is_unsat) {
              result.add_clause({});
              return result;
            }
            std::vector<Literal> vars(cnf._var_count);
            for (size_t clause_start = 0, clause_id = 0; clause_id < cnf._clause_indices.size(); clause_id++) {
              size_t clause_end = cnf._clause_indices[clause_id];
              
              if (inactive_clauses.find(clause_id) == inactive_clauses.end()) {
                std::vector<Literal> clause;
                for (size_t it = clause_start; it < clause_end; it++) {
                  if (assignments.find(cnf._literals[it].var()) != assignments.end()) {
                    continue;
                  }
                  if (!vars[cnf._literals[it].var()].is_valid()) {
                    vars[cnf._literals[it].var()] = result.var();
                  }
                  Literal lit = vars[cnf._literals[it].var()];
                  if (cnf._literals[it].is_negative()) {
                    lit = !lit;
                  }
                  clause.push_back(lit);
                }
                result.add_clause(clause);
              }
              
              clause_start = clause_end;
            }
            return result;
          }
        };
        
        Simplification simplification(*this);
        simplification.unit_prop();
        simplification.assign_pure();
        return simplification.to_cnf();
      }
      
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
      std::unordered_map<const Value*, Cnf::Literal> _values;
      
      void expect_bit(const Value* value) {
        if (value->width != 1) {
          throw_error(Error,
            "All values must have width 1, but got value of width " << value->width << ". " <<
            "Use hdl::flatten::Flattening to flatten circuit."
          );
        }
      }
    public:
      CnfBuilder() {}
      
      const Cnf& cnf() const { return _cnf; }
      
      void free(const Value* bit) {
        expect_bit(bit);
        _values[bit] = _cnf.var();
      }
      
      void free(const std::vector<Value*>& bits) {
        for (Value* bit : bits) {
          free(bit);
        }
      }
      
      void build(const Value* value) {
        expect_bit(value);
        
        if (_values.find(value) != _values.end()) {
          return;
        }
        
        Cnf::Literal result;
        
        if (const Constant* constant = dynamic_cast<const Constant*>(value)) {
          result = _cnf.f_const(constant->value[0]);
        } else if (const Op* op = dynamic_cast<const Op*>(value)) {
          for (const Value* arg : op->args) {
            build(arg);
          }
          
          #define arg(index) _values.at(op->args[index])
          
          switch (op->kind) {
            case Op::Kind::And: result = _cnf.f_and(arg(0), arg(1)); break;
            case Op::Kind::Or: result = _cnf.f_or(arg(0), arg(1)); break;
            case Op::Kind::Xor: result = _cnf.f_xor(arg(0), arg(1)); break;
            case Op::Kind::Not: result = _cnf.f_not(arg(0)); break;
            default: throw_error(Error, "Operator " << op->kind << " is not a gate");
          }
          
          #undef arg
        } else {
          throw Error("Unable to build value");
        }
        
        _values[value] = result;
      }
      
      void require(const std::vector<Value*> bits, const BitString& string) {
        if (bits.size() != string.width()) {
          throw_error(Error,
            "require expected BitString to be of the same width as value, but got " <<
            string.width() << " and " << bits.size()
          );
        }
        
        for (size_t it = 0; it < bits.size(); it++) {
          build(bits[it]);
          if (string[it]) {
            _cnf.add_clause({_values.at(bits[it])});
          } else {
            _cnf.add_clause({!_values.at(bits[it])});
          }
        }
      }
      
    };
  }
}

#undef throw_error

#endif
