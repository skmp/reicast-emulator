#pragma once

// Template magic for STATIC_FORWARD
template <class T>
class FI {

};

template<typename R, typename C, typename... Args>
struct FI<R(C::*)(Args...)> {
	using r_type = R;
	using c_type = C;

	static R(*forward(R(*fn)(void* context, Args...)))(void* ctx, Args...) {
		return fn;
	}
};

#define STATIC_FORWARD(klass, function)  FI<decltype(&klass::function)>::forward([](void* ctx, auto... args) { \
                                auto that = reinterpret_cast<klass*>(ctx); return that->function(args...); \
                            })