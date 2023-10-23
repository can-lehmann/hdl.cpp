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
      std::vector<hdl::Value*> conditions;
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
      Val(Module& module, Value* value):
        _module(module), _value(value) {}
      
      void expect_same_module(const Val<Width>& other) const {
        if (&_module != &other._module) {
          throw Error("Not same module");
        }
      }
    public:
      Val(): _module(global_context.module()) {
        _value = _module.constant(BitString(Width));
      }
      
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
      
      template <size_t Width>
      Reg<T>& operator=(const Val<Width>& val) {
        hdl::Reg* reg = dynamic_cast<hdl::Reg*>(this->value());
        reg->clock = global_context.clock();
        reg->next = val.value();
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
    
    template <size_t Width>
    class U: public Val<Width> {
    private:
      static hdl::Value* create_constant(uint64_t constant) {
        BitString bit_string = BitString::from_uint(constant).truncate(Width);
        return global_context.module().constant(bit_string);
      }
    public:
      using Val<Width>::Val;
      // Why?
      using Val<Width>::module;
      using Val<Width>::value;
      
      U(uint64_t constant):
        Val<Width>(global_context.module(), create_constant(constant)) {}
      
      U<Width> operator+(const U<Width>& other) const {
        Val<Width>::expect_same_module(other);
        Value* result = module().op(Op::Kind::Add, {value(), other.value()});
        return U<Width>(module(), result);
      }
    };
    
    void on(const U<1>& clock, const std::function<void()>& body) {
      global_context.on(clock.value(), body);
    }
    
    void synth(Module& module, const std::function<void()>& body) {
      global_context.synth(module, body);
    }
    
  }
}

#endif
