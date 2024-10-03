#include <cstdio>
#include <coroutine>
#include <stdexcept>
#include <vector>
#include <string>
#include <new>
#include <typeinfo>

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
    static void reset() {
        if (living != 0) {
            errors.push_back("coro vars not destroyed");
        }
        living = 0;
    }
};
int dtor_guard::living = 0;

struct allocator {
    static void* state_buf;
    static bool allocated;

    static void* allocate(std::size_t size) {
        auto ret = std::malloc(size);
        if (state_buf) {
            errors.push_back("double state buf allocation");
        }
        else {
            state_buf = ret;
            allocated = true;
        }
        return ret;
    };

    static void deallocate(void* ptr) {
        if (ptr != state_buf) {
            errors.push_back("free unknown memory");
        }
        else if (!allocated) {
            errors.push_back("double free of state buf");
        }
        allocated = false;
    }

    static void reset() {
        if (allocated) {
            errors.push_back("state buf leak");
        }
        if (!state_buf) {
            errors.push_back("state buf allocation was elided (optimized out). Test is unreliable");
        }

        std::free(state_buf);
        state_buf = nullptr;
        allocated = false;
    }
};

void* allocator::state_buf = nullptr;
bool allocator::allocated = false;

struct error_guard {
    error_guard() {
        errors.clear();
    }
    ~error_guard() {
        dtor_guard::reset();
        allocator::reset();

        if (errors.empty()) {
            std::puts("    PASS");
            return;
        }

        for (const auto& err : errors) {
            std::printf("    %s\n", err.c_str());
        }

        std::puts("    FAIL");
    }
};

struct track_alloc {
    void* operator new(std::size_t size) {
        return allocator::allocate(size);
    }

    void operator delete(void* ptr) {
        allocator::deallocate(ptr);
    }
};

// this is what we should expect to work on all compilers all of the time
struct simple_wrapper {
    struct promise_type : public track_alloc {
        int last_yield = no_value;

        simple_wrapper get_return_object() {
            return simple_wrapper{ std::coroutine_handle<promise_type>::from_promise(*this) };
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

    explicit simple_wrapper(std::coroutine_handle<promise_type> h = nullptr) noexcept : handle(h) {}
    ~simple_wrapper() noexcept {
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

template <typename W>
W generator(int from, int to, int throw_on = -1) {
    dtor_guard dg;
    for (int i = from; i < to; ++i) {
        if (i == throw_on) {
            throw std::runtime_error(std::to_string(i));
        }
        co_yield i;
    }
}

template <typename W>
void no_throws() {
    auto gen = generator<W>(0, 10);
    for (int i = 0; i < 10; ++i) {
        std::printf("%d ", gen.get());
    }
}

template <typename W>
void eager_throw() {
    try {
        auto gen = generator<W>(0, 10, 0);
        for (int i = 0; i < 10; ++i) {
            std::printf("%d ", gen.get());
            errors.push_back("no exception thrown");
            return;
        }
    }
    catch (const std::runtime_error& e) {
        if (std::string("0") != e.what()) {
            errors.push_back("invalid exception thrown: " + std::string(e.what()));
        }
    }
}

template <typename W>
void post_yield_throw() {
    try {
        auto gen = generator<W>(0, 10, 5);
        for (int i = 0; i < 10; ++i) {
            std::printf("%d ", gen.get());
            if (i == 4) {
                errors.push_back("exception not thrown");
            }
        }
    }
    catch (const std::runtime_error& e) {
        if (std::string("5") != e.what()) {
            errors.push_back("invalid exception thrown: " + std::string(e.what()));
        }
    }
}

void run(void(*fn)(), const char* name) {
    error_guard eg;
    std::printf("  %s:\n    Output: ", name);
    fn();
    std::puts("");
}
template <typename W>
void run_all() {
    printf("%s:\n", typeid(W).name());

#define RUN(fn) run(fn<W>, #fn)
    RUN(no_throws);
    RUN(eager_throw);
    RUN(post_yield_throw);
}

int main() {
    run_all<simple_wrapper>();
    return 0;
}
