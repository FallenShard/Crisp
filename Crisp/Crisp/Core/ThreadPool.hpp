#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include <rigtorp/MPMCQueue.h>

#include <Crisp/Core/ChromeProfiler.hpp>

namespace crisp {

template <typename T>
class ConcurrentQueue {
public:
    void push(T&& item) {
        std::unique_lock lock(mutex);
        taskQueue.push(item);

        lock.unlock();
        condVar.notify_one();
    }

    T pop() {
        std::unique_lock lock(mutex);
        condVar.wait(lock, [this]() { return !taskQueue.empty(); });
        T item = taskQueue.front();
        taskQueue.pop();
        return item;
    }

    bool tryPop(T& item) {
        std::unique_lock lock(mutex);
        if (taskQueue.empty()) {
            return false;
        }

        item = taskQueue.front();
        taskQueue.pop();
        return true;
    }

private:
    std::queue<T> taskQueue;
    std::mutex mutex;
    std::condition_variable condVar;
};

class ThreadPool {
public:
    using TaskType = std::function<void()>;

    explicit ThreadPool(std::optional<uint32_t> threadCount = std::nullopt)
        : m_taskQueue()
        , m_workers(threadCount ? *threadCount : std::thread::hardware_concurrency()) {
        for (std::size_t i = 0; i < m_workers.size(); ++i) {
            auto& worker = m_workers[i];
            worker.index = static_cast<int32_t>(i);
            worker.thread = std::thread([this, i]() {
                auto& context = m_workers.at(i);
                ChromeProfiler::setThreadName("Worker " + std::to_string(context.index));
                while (true) {
                    TaskType task = m_taskQueue.pop();
                    if (task == nullptr) {
                        m_taskQueue.push(nullptr);
                        break;
                    }

                    task();
                    // if (m_taskQueue.try_pop(task))
                    //{
                    //     if (task == nullptr)
                    //     {
                    //         m_taskQueue.push(nullptr);
                    //         break;
                    //     }

                    //    task();
                    //    context.popAttempts = 0;
                    //}
                    // else
                    //{
                    //    ++context.popAttempts;

                    //    std::this_thread::yield();
                    //    /*if (context.popAttempts == 1)
                    //    {

                    //        std::this_thread::yield();
                    //    }
                    //    else
                    //    {
                    //        m_jobCondVar std::this_thread::sleep_for(st)
                    //    }*/
                    //}
                }

                ChromeProfiler::flushThreadBuffer();
            });
        }
    }

    ~ThreadPool() {
        m_taskQueue.push(nullptr);
        for (auto& w : m_workers) {
            w.thread.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    std::size_t getThreadCount() const {
        return m_workers.size();
    }

    void schedule(TaskType&& taskType) {
        m_taskQueue.push(std::move(taskType));
    }

    template <typename F>
    void parallelFor(std::size_t iterationCount, F&& iterationCallback) {
        const std::size_t jobsToSchedule = m_workers.size();
        const std::size_t iterationsPerJob = iterationCount / jobsToSchedule;
        const std::size_t remaining = iterationCount % jobsToSchedule;

        m_jobCount = jobsToSchedule;
        std::size_t start = 0;
        for (std::size_t i = 0; i < jobsToSchedule; ++i) {
            const std::size_t iterCount = iterationsPerJob + (remaining > i ? 1 : 0);

            m_taskQueue.push([this, i, cb = std::forward<F>(iterationCallback), start, end = start + iterCount] {
                for (std::size_t k = start; k < end; ++k) {
                    cb(k, i);
                }

                std::unique_lock lock(m_jobMutex);
                --m_jobCount;
                if (m_jobCount == 0) {
                    lock.unlock();
                    m_jobCondVar.notify_one();
                }
            });

            start += iterCount;
        }

        std::unique_lock lock(m_jobMutex);
        m_jobCondVar.wait(lock, [this] { return m_jobCount == 0; });
    }

    template <typename F>
    void parallelJob(std::size_t iterationCount, F&& jobCallback) {
        std::size_t jobsToSchedule = m_workers.size();
        std::size_t iterationsPerJob = iterationCount / jobsToSchedule;
        std::size_t remaining = iterationCount % jobsToSchedule;

        m_jobCount = jobsToSchedule;
        std::size_t start = 0;
        for (std::size_t i = 0; i < jobsToSchedule; ++i) {
            std::size_t iterCount = iterationsPerJob + (remaining > i ? 1 : 0);

            m_taskQueue.push([this, i, cb = std::forward<F>(jobCallback), start, end = start + iterCount] {
                cb(start, end, i);

                std::unique_lock lock(m_jobMutex);
                --m_jobCount;
                if (m_jobCount == 0) {
                    lock.unlock();
                    m_jobCondVar.notify_one();
                }
            });

            start += iterCount;
        }

        std::unique_lock lock(m_jobMutex);
        m_jobCondVar.wait(lock, [this] { return m_jobCount == 0; });
    }

private:
    struct Worker {
        alignas(std::hardware_destructive_interference_size) int32_t index{0};
        uint32_t popAttempts{0};

        std::thread thread;
    };

    ConcurrentQueue<TaskType> m_taskQueue;
    // rigtorp::MPMCQueue<TaskType> m_taskQueue;

    std::vector<Worker> m_workers;

    std::size_t m_jobCount{0};
    std::mutex m_jobMutex;
    std::condition_variable m_jobCondVar;
};
} // namespace crisp
