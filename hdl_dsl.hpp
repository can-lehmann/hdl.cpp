// Copyright (c) 2023 Can Joshua Lehmann

#ifndef HDL_DSL
#define HDL_DSL

#include <inttypes.h>
#include <functional>
#include <vector>

#include "hdl.hpp"

namespace hdl {
  namespace dsl {
    class GlobalContext {
    private:
      Module* _module = nullptr;
      Value* _clock = nullptr;
      std::vector<Value*> _conditions;
    public:
      GlobalContext() {}
      
      Module& module() {
        if (_module == nullptr) {
          throw Error("No module");
        }
        return *_module;
      }
      
      Value* clock() {
        if (_clock == nullptr) {
          throw Error("No clock");
        }
        return _clock;
      }
      
      Value* condition() {
        Value* condition = module().constant(BitString::from_bool(true));
        for (Value* cond : _conditions) {
          condition = module().op(Op::Kind::And, {
            condition, cond
          });
        }
        return condition;
      }
      
      void when(Value* cond,
                const std::function<void()>& then,
                const std::function<void()>& otherwise) {
        _conditions.push_back(cond);
        then();
        _conditions.pop_back();
        _conditions.push_back(_module->op(Op::Kind::Not, {cond}));
        otherwise();
        _conditions.pop_back();
      }
      
      void on(Value* clock, const std::function<void()>& body) {
        Value* old_clock = _clock;
        _clock = clock;
        body();
        _clock = old_clock;
      }
      
      void synth(Module& module, const std::function<void()>& body) {
        Module* old_module = _module;
        _module = &module;
        body();
        _module = old_module;
      }
    };
    
    GlobalContext global_context;
    
    
    template <size_t Width>
    class Val {
    private:
      Module& _module;
      Value* _value = nullptr;
    protected:
      void expect_same_module(const Val<Width>& other) const {
        if (&_module != &other._module) {
          throw Error("Not same module");
        }
      }
    public:
      Val(): _module(global_context.module()) {
        _value = _module.constant(BitString(Width));
      }
      
      Val(Module& module, Value* value):
        _module(module), _value(value) {}
      
      Module& module() const { return _module; }
      Value* value() const { return _value; }
    };
    
    template <class T>
    class Reg: public T {
    private:
      static hdl::Value* create_register(const T& initial) {
        return initial.module().reg(initial.value(), nullptr);
      }
      
      static hdl::Value* create_register() {
        T initial;
        return create_register(initial);
      }
    public:
      Reg(): T(global_context.module(), create_register()) {}
      Reg(const T& initial): T(initial.module(), create_register(initial)) {}
      
      Reg<T>& operator=(const T& val) {
        hdl::Reg* reg = dynamic_cast<hdl::Reg*>(this->value());
        reg->clock = global_context.clock();
        reg->next = global_context.module().op(Op::Kind::Select, {
          global_context.condition(),
          val.value(),
          reg->next
        });
        return *this;
      }
    };
    
    template <class T>
    class Input: public T {
    private:
      template <size_t Width>
      static hdl::Value* create_input(const char* name, const Val<Width>&) {
        return global_context.module().input(name, Width);
      }
    public:
      Input(const char* name): T(global_context.module(), create_input(name, T())) {}
    };
    
    class Bool: public Val<1> {
    private:
      static Value* create_bool(bool value) {
        return global_context.module().constant(BitString::from_bool(value));
      }
    public:
      using Val<1>::Val;
      using Val<1>::module;
      using Val<1>::value;
      
      Bool(bool value): Val<1>(global_context.module(), create_bool(value)) {}
      
      template <class T>
      T select(const T& then, const T& otherwise) {
        Value* result = Val<1>::module().op(Op::Kind::Select, {
          value(),
          then.value(),
          otherwise.value()
        });
        return T(Val<1>::module(), result);
      }
      
      #define binop(op_name, kind, invert) \
        Bool operator op_name(const Bool& other) const { \
          Val<1>::expect_same_module(other); \
          Value* result = module().op(Op::Kind::kind, {value(), other.value()}); \
          if (invert) { \
            result = module().op(Op::Kind::Not, {result}); \
          } \
          return Bool(module(), result); \
        }
      
      binop(&, And, false);
      binop(|, Or, false);
      binop(&&, And, false);
      binop(||, Or, false);
      binop(^, Xor, false);
      
      binop(==, Eq, false);
      binop(!=, Eq, true);
      
      #undef binop
      
      Bool operator!() const {
        Value* result = module().op(Op::Kind::Not, {value()});
        return Bool(module(), result);
      }
      
      Bool operator~() const {
        return !(*this);
      }
    };
    
    template <size_t Width>
    class U: public Val<Width> {
    private:
      static hdl::Value* create_constant(uint64_t constant) {
        BitString bit_string = BitString::from_uint(constant).truncate(Width);
        return global_context.module().constant(bit_string);
      }
    public:
      using Val<Width>::Val;
      using Val<Width>::module;
      using Val<Width>::value;
      
      U(uint64_t constant):
        Val<Width>(global_context.module(), create_constant(constant)) {}
      
      #define binop(op_name, kind) \
        U<Width> operator op_name(const U<Width>& other) const { \
          Val<Width>::expect_same_module(other); \
          Value* result = module().op(Op::Kind::kind, {value(), other.value()}); \
          return U<Width>(module(), result); \
        }
      
      binop(&, And)
      binop(|, Or)
      binop(^, Xor)
      
      binop(+, Add)
      binop(-, Sub)
      binop(<<, Shl)
      binop(>>, ShrU)
      
      #undef binop
      
      #define cmp(op_name, kind, invert) \
        Bool operator op_name(const U<Width>& other) const { \
          Val<Width>::expect_same_module(other); \
          Value* result = module().op(Op::Kind::kind, {value(), other.value()}); \
          if (invert) { \
            result = module().op(Op::Kind::Not, {result}); \
          } \
          return Bool(module(), result); \
        }
      
      cmp(==, Eq, false)
      cmp(!=, Eq, true)
      cmp(<, LtU, false)
      cmp(>=, LtU, true)
      cmp(<=, LeU, false)
      cmp(>, LeU, true)
      
      #undef cmp
    };
    
    template <class T, size_t Size>
    class Mem {
    private:
      Module& _module;
      Memory* _memory = nullptr;
      
      template <size_t Width>
      static Memory* create_memory(const Val<Width>&) {
        return global_context.module().memory(Width, Size, nullptr);
      }
    public:
      Mem(): _module(global_context.module()), _memory(create_memory(T())) {}
      
      template <size_t AddressWidth>
      T operator[](const U<AddressWidth>& address) {
        return T(_module, _memory->read(address.value()));
      }
      
      template <size_t AddressWidth>
      void write(const U<AddressWidth>& address, const T& value) {
        _memory->clock = global_context.clock();
        _memory->write(address.value(), global_context.condition(), value.value());
      }
    };
    
    void on(const Bool& clock, const std::function<void()>& body) {
      global_context.on(clock.value(), body);
    }
    
    void when(const Bool& cond,
              const std::function<void()>& then,
              const std::function<void()>& otherwise) {
      global_context.when(cond.value(), then, otherwise);
    }
    
    void when(const Bool& cond,
              const std::function<void()>& then) {
      when(cond, then, [](){});
    }
    
    void synth(Module& module, const std::function<void()>& body) {
      global_context.synth(module, body);
    }
    
  }
}

#define $ [&]()

#endif
