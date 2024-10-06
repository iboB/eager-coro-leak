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
        if (living > 0) {
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
            std::puts("    \033[01;32mPASS\033[m");
            return;
        }

        for (const auto& err : errors) {
            std::printf("    \033[31mERROR: %s\n", err.c_str());
        }

        std::puts("    FAIL\033[m");
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
// as of this writing it only works on:
// * gcc trunk (latest stable is 14.2)
// * clang 17 and later
struct simple_wrapper {
    struct promise_type : public track_alloc {
        int last_yield = no_value;

        simple_wrapper get_return_object() {
            return simple_wrapper{std::coroutine_handle<promise_type>::from_promise(*this)};
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

struct throwing_eager_coro_promise_type_helper {
protected:
    std::exception_ptr m_exception;
    bool m_has_been_suspended = false;
    bool m_has_exception_before_first_suspend = false;
public:
    void unhandled_exception() {
        if (m_has_been_suspended) {
            m_exception = std::current_exception();
        }
        else {
            m_has_exception_before_first_suspend = true;
            throw;
        }
    }

    void on_suspend() noexcept {
        m_has_been_suspended = true;
    }

    void rethrow_if_exception() {
        if (m_exception) {
            std::rethrow_exception(m_exception);
        }
    }

    template <typename PT>
    static void safe_destroy_handle(const std::coroutine_handle<PT>& h) noexcept {
        static_assert(std::is_base_of_v<throwing_eager_coro_promise_type_helper, PT>);
        if (h && !h.promise().m_has_exception_before_first_suspend) {
            h.destroy();
        }
    }
};

// this is our attempt at having something work, dealing with specific compiler quirks
struct workaround_wrapper {
    struct promise_type : public throwing_eager_coro_promise_type_helper, public track_alloc {
        int last_yield = no_value;

        workaround_wrapper get_return_object() {
            return workaround_wrapper{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; } // eager
        std::suspend_always final_suspend() noexcept { return {}; } // preserve the final yield

        std::suspend_always yield_value(int v) noexcept {
            last_yield = v;
            on_suspend();
            return {};
        }

        void return_void() noexcept {}
    };

    std::coroutine_handle<promise_type> handle = nullptr;

    explicit workaround_wrapper(std::coroutine_handle<promise_type> h = nullptr) noexcept : handle(h) {}
    ~workaround_wrapper() noexcept {
        throwing_eager_coro_promise_type_helper::safe_destroy_handle(handle);
    }

    int get() {
        if (!handle) return no_handle;
        if (handle.done()) return handle_done;
        auto ret = handle.promise().last_yield;
        handle.resume();
        handle.promise().rethrow_if_exception();
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
        errors.push_back("no exception thrown");
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
    run_all<workaround_wrapper>();
    return 0;
}
