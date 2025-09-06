//
// Created by General Suslik on 06.09.2025.
//

#ifndef THREAD_POOL_EXECUTOR_H
#define THREAD_POOL_EXECUTOR_H

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "thread-pool/task.h"

namespace thread_pool {

    class Executor : public std::enable_shared_from_this<Executor> {
    public:
        explicit Executor(std::uint32_t thread_count = std::thread::hardware_concurrency());

        ~Executor();

        void Submit(TaskPtr task);

    private:
        void StartShutdown();
        void WaitShutdown();

        void RunWorker();

    private:
        std::vector<std::thread> workers_;
        std::deque<TaskPtr> tasks_;

        std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<bool> is_finished_{false};
    };

} // namespace thread_pool

#endif //THREAD_POOL_EXECUTOR_H