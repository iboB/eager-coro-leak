#include <cstdio>
#include <coroutine>
#include <stdexcept>
#include <vector>
#include <string>

enum invalid_values : int {
    no_value = -1,
    no_handle = -2,
    handle_done = -3,
};

std::vector<std::string> errors;

struct dtor_guard {
    static int living;
    dtor_guard() noexcept { ++living; }
    ~dtor_guard() noexcept {
        if (living == 0) {
            errors.push_back("double destruction of coro vars");
        }
        --living;
    }
};
int dtor_guard::living = 0;

struct error_guard {
    error_guard() {
        errors.clear();
    }
    ~error_guard() {
        if (dtor_guard::living != 0) {
            errors.push_back("coro vars not destroyed");
        }

        if (errors.empty()) {
            std::puts("  PASS");
            return;
        }

        for (const auto& err : errors) {
            std::printf("  %s\n", err.c_str());
        }
        std::puts("  FAIL");
    }
};

struct wrapper {
    struct promise_type {
        int last_yield = no_value;

        wrapper get_return_object() {
            return wrapper{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; } // eager
        std::suspend_always final_suspend() noexcept { return {}; } // preserve the final yield

        std::suspend_always yield_value(int v) noexcept {
            last_yield = v;
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() {
            throw;
        }
    };

    std::coroutine_handle<promise_type> handle = nullptr;

    explicit wrapper(std::coroutine_handle<promise_type> h = nullptr) noexcept : handle(h) {}
    ~wrapper() noexcept {
        if (handle) {
            handle.destroy();
        }
    }

    int get() {
        if (!handle) return no_handle;
        if (handle.done()) return handle_done;
        auto ret = handle.promise().last_yield;
        handle.resume();
        return ret;
    }
};

wrapper generator(int from, int to, int throw_on = -1) {
    dtor_guard dg;
    for (int i = from; i < to; ++i) {
        if (i == throw_on) {
            throw std::runtime_error("throwing on " + std::to_string(i));
        }
        co_yield i;
    }
}

void no_throws() {
    auto gen = generator(0, 10);
    for (int i = 0; i < 10; ++i) {
        std::printf("%d ", gen.get());
    }
}

void run(void(*fn)(), const char* name) {
    error_guard eg;
    std::printf("%s:\n  ", name);
    fn();
    std::puts("");
}

#define RUN(fn) run(fn, #fn)

int main() {
    RUN(no_throws);
    return 0;
}
