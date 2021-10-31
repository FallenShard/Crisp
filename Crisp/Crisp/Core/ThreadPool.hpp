#pragma once

#include <CrispCore/ChromeProfiler.hpp>

#include <condition_variable>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>

namespace crisp
{
    template <typename T>
    class ConcurrentQueue
    {
    public:

        void push(T&& item)
        {
            std::unique_lock lock(mutex);
            taskQueue.push(item);

            lock.unlock();
            condVar.notify_one();
        }

        T pop()
        {
            std::unique_lock lock(mutex);
            condVar.wait(lock, [this]() { return !taskQueue.empty(); });
            T item = taskQueue.front();
            taskQueue.pop();
            return item;
        }

    private:
        std::queue<std::function<void()>> taskQueue;
        std::mutex mutex;
        std::condition_variable condVar;
    };

    class ThreadPool
    {
    public:
        ThreadPool()
            : m_workers(std::thread::hardware_concurrency())
            , m_jobCount(0)
        {
            for (std::size_t i = 0; i < m_workers.size(); ++i)
            {
                auto& worker = m_workers[i];
                worker = std::thread([this, i]()
                    {
                    ChromeProfiler::setThreadName("Worker " + std::to_string(i));
                    while (true)
                    {
                        auto t = m_taskQueue.pop();
                        if (t == nullptr) {
                            m_taskQueue.push(nullptr);
                            break;
                        }

                        t();
                    }

                    ChromeProfiler::flushThreadBuffer();
                });
            }
        }

        ~ThreadPool()
        {
            m_taskQueue.push(nullptr);
            for (auto& w : m_workers)
                w.join();
        }


        template <typename F>
        void parallelFor(std::size_t iterationCount, F&& iterationCallback)
        {
            std::size_t jobsToSchedule = m_workers.size();
            std::size_t iterationsPerJob = iterationCount / jobsToSchedule;
            std::size_t remaining = iterationCount % jobsToSchedule;

            m_jobCount = jobsToSchedule;
            std::size_t start = 0;
            for (std::size_t i = 0; i < jobsToSchedule; ++i)
            {
                std::size_t iterCount = iterationsPerJob + (remaining > i ? 1 : 0);

                m_taskQueue.push([this, i, cb = std::move(iterationCallback), start, end = start + iterCount]{
                    for (std::size_t k = start; k < end; ++k)
                        cb(k, i);

                    std::unique_lock lock(m_jobMutex);
                    --m_jobCount;
                    if (m_jobCount == 0)
                    {
                        lock.unlock();
                        m_jobCondVar.notify_one();
                    }
                });

                start += iterCount;
            }

            std::unique_lock lock(m_jobMutex);
            m_jobCondVar.wait(lock, [this] {return m_jobCount == 0; });
        }

        template <typename F>
        void parallelJob(std::size_t iterationCount, F&& jobCallback)
        {
            std::size_t jobsToSchedule = m_workers.size();
            std::size_t iterationsPerJob = iterationCount / jobsToSchedule;
            std::size_t remaining = iterationCount % jobsToSchedule;

            m_jobCount = jobsToSchedule;
            std::size_t start = 0;
            for (std::size_t i = 0; i < jobsToSchedule; ++i)
            {
                std::size_t iterCount = iterationsPerJob + (remaining > i ? 1 : 0);

                m_taskQueue.push([this, i, cb = std::move(jobCallback), start, end = start + iterCount]{
                    cb(start, end, i);

                    std::unique_lock lock(m_jobMutex);
                    --m_jobCount;
                    if (m_jobCount == 0)
                    {
                        lock.unlock();
                        m_jobCondVar.notify_one();
                    }
                    });

                start += iterCount;
            }

            std::unique_lock lock(m_jobMutex);
            m_jobCondVar.wait(lock, [this] {return m_jobCount == 0; });
        }

    private:
        std::vector<std::thread> m_workers;
        ConcurrentQueue<std::function<void()>> m_taskQueue;

        std::size_t m_jobCount;
        std::mutex m_jobMutex;
        std::condition_variable m_jobCondVar;
    };
}
