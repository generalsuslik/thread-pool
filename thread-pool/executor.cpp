//
// Created by General Suslik on 06.09.2025.
//

#include "executor.h"

namespace thread_pool {

    Executor::Executor(const std::uint32_t thread_count) {
        workers_.reserve(thread_count);
        for (std::uint32_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] {
                RunWorker();
            });
        }
    }

    Executor::~Executor() {
        StartShutdown();
        WaitShutdown();
    }

    void Executor::Submit(TaskPtr task) {
        std::lock_guard lock{mutex_};

        if (!is_finished_ && !task->IsFinished()) {
            task->Pend();
            task->SetNotificationHandler([weak_self = weak_from_this()] (std::shared_ptr<Task> sub) {
                if (auto self = weak_self.lock()) {
                    self->Submit(std::move(sub));
                }
            });
            tasks_.emplace_back(std::move(task));
            cv_.notify_one();
        }
    }

    void Executor::RunWorker() {
        for (;;) {
            std::unique_lock lock{mutex_};

            cv_.wait(lock, [this] { return !tasks_.empty() || is_finished_; });
            if (is_finished_) {
                return;
            }

            auto task = std::move(tasks_.front());
            tasks_.pop_front();
            if (!task->CanBeExecuted()) {
                if (task->GetTimeTrigger().has_value()) {
                    tasks_.push_back(std::move(task));
                }
                continue;
            }
            lock.unlock();

            if (task->Capture()) {
                try {
                    task->Run();
                    task->Complete();
                } catch (...) {
                    task->SetError(std::current_exception());
                }
            }
        }
    }

    void Executor::StartShutdown() {
        std::lock_guard lock{mutex_};

        is_finished_ = true;
        tasks_.clear();
        cv_.notify_all();
    }

    void Executor::WaitShutdown() {
        for (auto& worker : workers_) {
            worker.join();
        }
    }

} // namespace thread_pool

