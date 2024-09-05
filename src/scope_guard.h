#pragma once
#include <type_traits>

namespace ucpp {

template<typename Callable>
struct scope_guard {
    scope_guard(Callable&& f) : _f(std::forward<Callable>(f)) {}
    ~scope_guard() {
        if (should_call()) {
            _f();
        }
    };

    scope_guard(const scope_guard&) = delete;
    scope_guard& operator=(const scope_guard&) = delete;

#if __cplusplus >= 201703L || __cpp_guaranteed_copy_elision > 0

private:
    bool should_call() const {
        return true;
    }

#else

    scope_guard(scope_guard&& other) : _f(std::move(other._f)) {
        other._ignore = true;
    }

    scope_guard& operator=(scope_guard&& other) {
        _ignore = false;
        _f = std::move(other._f);

        other._ignore = true;
    }

private:
    bool _ignore = false;
    bool should_call() const {
        return _ignore == false;
    }

#endif
    Callable _f;
};

template<typename Callable>
scope_guard<Callable> make_scope_guard(Callable&& f) {
    return scope_guard<Callable>(std::forward<Callable>(f));
}

}
