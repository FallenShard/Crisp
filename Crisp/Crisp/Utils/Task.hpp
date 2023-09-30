#pragma once

#include <Crisp/Core/Logger.hpp>

#include <atomic>
#include <coroutine>

namespace crisp::coro {
struct FireOnceEvent {
    void set() {
        m_flag.test_and_set();
        m_flag.notify_all();
    }

    void wait() {
        m_flag.wait(false);
    }

private:
    std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
};

template <typename T>
struct Task {
    struct promise_type {
        // Parent coroutine that initiated the current coroutine
        std::coroutine_handle<> precursor = std::noop_coroutine();

        T data;

        Task get_return_object() noexcept {
            return Task(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_always initial_suspend() const noexcept {
            return {};
        }

        auto final_suspend() const noexcept {
            struct FinalTaskAwaiter {
                bool await_ready() const noexcept {
                    return false;
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    return h.promise().precursor;
                }

                void await_resume() const noexcept {}
            };

            return FinalTaskAwaiter{};
        }

        template <std::convertible_to<T> From>
        void return_value(From value) noexcept {
            data = std::forward<From>(value);
        }

        void unhandled_exception() {}
    };

    bool await_ready() const noexcept {
        return !handle || handle.done();
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        handle.promise().precursor = coroutine;
        return handle;
    }

    T await_resume() const noexcept {
        return std::move(handle.promise().data);
    }

    T get() const noexcept {
        return std::move(handle.promise().data);
    }

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h)
        : handle(h) {}

    Task(const Task&) = delete;

    Task(Task&& other) noexcept
        : handle(std::exchange(other.handle, nullptr)) {}

    ~Task() {
        if (handle) {
            handle.destroy();
        }
    }
};

template <typename T = void>
struct SyncWaitTask {
    struct promise_type {
        std::suspend_always initial_suspend() const noexcept {
            return {};
        }

        auto final_suspend() const noexcept {
            struct awaiter {
                bool await_ready() const noexcept {
                    return false;
                }

                void await_suspend(std::coroutine_handle<promise_type> coro) const noexcept {
                    FireOnceEvent* const event = coro.promise().event;
                    if (event) {
                        event->set();
                    }
                }

                void await_resume() noexcept {}
            };

            return awaiter{};
        }

        FireOnceEvent* event = nullptr;

        SyncWaitTask get_return_object() noexcept {
            return SyncWaitTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void unhandled_exception() noexcept {
            std::terminate();
        }

        void return_void() {}
    };

    SyncWaitTask(std::coroutine_handle<promise_type> coro)
        : m_handle(coro) {}

    ~SyncWaitTask() {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    void run(FireOnceEvent& ev) {
        m_handle.promise().event = &ev;
        m_handle.resume();
    }

    std::coroutine_handle<promise_type> m_handle;
};

template <typename T>
SyncWaitTask<> makeSyncWaitTask(Task<T>& t) {
    co_await t;
}

template <typename T>
T syncWait(Task<T>& task) {
    FireOnceEvent ev;
    auto waitTask = makeSyncWaitTask(task);
    waitTask.run(ev);
    ev.wait();

    return task.get();
}

} // namespace crisp::coro