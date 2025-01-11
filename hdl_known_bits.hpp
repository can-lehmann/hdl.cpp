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

#ifndef HDL_KNOWN_BITS_HPP
#define HDL_KNOWN_BITS_HPP

#include <vector>
#include <unordered_map>

#include "hdl.hpp"

#define throw_error(Error, msg) { \
  std::ostringstream error_message; \
  error_message << msg; \
  throw Error(error_message.str()); \
}

namespace hdl {
  namespace known_bits {
    class KnownBits {
    public:
      template <class T>
      struct Partial {
        T* known = nullptr;
        T* value = nullptr;
        
        Partial() {}
        Partial(T* _known, T* _value): 
          known(_known), value(_value) {}
      };
      
      using PartialValue = Partial<Value>;
      using PartialMemory = Partial<Memory>;
    private:
      Module& _module;
      
      std::unordered_map<Value*, PartialValue> _values;
      std::unordered_map<Memory*, PartialMemory> _memories;
      
      PartialValue merge(PartialValue a, PartialValue b) {
        return PartialValue(
          _module.op(Op::Kind::And, {
            _module.op(Op::Kind::And, {
              a.known, b.known
            }),
            _module.op(Op::Kind::Not, {_module.op(Op::Kind::Xor, {a.value, b.value})})
          }),
          a.value
        );
      }
      
      PartialValue select(Value* cond, PartialValue a, PartialValue b) {
        return PartialValue(
          _module.op(Op::Kind::Select, {cond, a.known, b.known}),
          _module.op(Op::Kind::Select, {cond, a.value, b.value})
        );
      }
      
      Value* is_fully_known(const std::vector<PartialValue>& args, size_t width) {
        Value* is_known = _module.constant(BitString::from_bool(true));
        for (PartialValue arg : args) {
          is_known = _module.op(Op::Kind::And, {
            is_known,
            _module.op(Op::Kind::Eq, {
              arg.known,
              _module.constant(~BitString(arg.known->width))
            })
          });
        }
        
        return _module.op(Op::Kind::Select, {
          is_known, 
          _module.constant(~BitString(width)),
          _module.constant(BitString(width))
        });
      }
    public:
      KnownBits(Module& module): _module(module) {}
      
      void define(Value* value, PartialValue partial) {
        _values[value] = partial;
      }
      
      void define_unknown(Value* value) {
        _values[value] = PartialValue(
          _module.constant(BitString(value->width)),
          _module.constant(BitString(value->width))
        );
      }
      
      void define(Memory* memory, PartialMemory partial) {
        _memories[memory] = partial;
      }
      
      PartialMemory lower(Memory* memory) {
        if (_memories.find(memory) != _memories.end()) {
          return _memories.at(memory);
        }
        
        PartialMemory partial;
        partial.known = _module.memory(memory->width, memory->size);
        partial.value = _module.memory(memory->width, memory->size);
        _memories[memory] = partial;
        return partial;
      }
      
      PartialValue lower(Value* value) {
        if (_values.find(value) != _values.end()) {
          return _values.at(value);
        }
        
        PartialValue partial;
        if (Constant* constant = dynamic_cast<Constant*>(value)) {
          partial.value = _module.constant(constant->value);
          partial.known = _module.constant(~BitString(constant->width));
        } else if (dynamic_cast<Unknown*>(value)) {
          partial.value = _module.constant(BitString(value->width));
          partial.known = _module.constant(BitString(value->width));
        } else if (Op* op = dynamic_cast<Op*>(value)) {
          std::vector<PartialValue> args;
          for (Value* arg : op->args) {
            args.push_back(lower(arg));
          }
          
          switch (op->kind) {
            case Op::Kind::And:
              partial = PartialValue(
                // TODO
                _module.op(Op::Kind::And, {args[0].known, args[1].known}),
                _module.op(Op::Kind::And, {args[0].value, args[1].value})
              );
            break;
            case Op::Kind::Or:
              partial = PartialValue(
                // TODO
                _module.op(Op::Kind::And, {args[0].known, args[1].known}),
                _module.op(Op::Kind::Or, {args[0].value, args[1].value})
              );
            break;
            case Op::Kind::Xor:
              partial = PartialValue(
                _module.op(Op::Kind::And, {args[0].known, args[1].known}),
                _module.op(Op::Kind::Xor, {args[0].value, args[1].value})
              );
            break;
            case Op::Kind::Not:
              partial = PartialValue(
                args[0].known,
                _module.op(Op::Kind::Not, {args[0].value})
              );
            break;
            case Op::Kind::Concat:
              partial = PartialValue(
                _module.op(Op::Kind::Concat, {args[0].known, args[0].known}),
                _module.op(Op::Kind::Concat, {args[0].value, args[0].value})
              );
            break;
            case Op::Kind::Slice:
              partial = PartialValue(
                _module.op(Op::Kind::Slice, {args[0].known, args[1].value, args[2].value}),
                _module.op(Op::Kind::Slice, {args[0].value, args[1].value, args[2].value})
              );
            break;
            case Op::Kind::Select:
              partial = select(
                args[0].known,
                select(args[0].value, args[1], args[2]),
                merge(args[1], args[2])
              );
            break;
            default: {
              std::vector<Value*> arg_values;
              for (PartialValue arg : args) {
                arg_values.push_back(arg.value);
              }
              
              partial = PartialValue(
                is_fully_known(args, op->width),
                _module.op(op->kind, arg_values)
              );
            }
            break;
          }
        } else if (Memory::Read* read = dynamic_cast<Memory::Read*>(value)) {
          PartialMemory memory = lower(read->memory);
          PartialValue address = lower(read->address);
          partial = PartialValue(
            _module.op(Op::Kind::And, {
              is_fully_known({address}, read->width),
              memory.known->read(address.value)
            }),
            memory.value->read(address.value)
          );
        } else {
          throw_error(Error, "Not supported");
        }
        
        _values[value] = partial;
        return partial;
      }
      
      void lower(Module& source_module,
                 const std::string& known_suffix = ".known",
                 const std::string& value_suffix = ".value") {
        
        for (Input* input : source_module.inputs()) {
          define(input, PartialValue(
            _module.input(input->name + known_suffix, input->width),
            _module.input(input->name + value_suffix, input->width)
          ));
        }
        
        for (const Output& output : source_module.outputs()) {
          PartialValue partial = lower(output.value);
          _module.output(output.name + known_suffix, partial.known);
          _module.output(output.name + value_suffix, partial.value);
        }
      }
    };
  }
}

#undef throw_error

#endif
