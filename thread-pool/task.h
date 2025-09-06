//
// Created by General Suslik on 06.09.2025.
//

#ifndef THREAD_POOL_TASK_H
#define THREAD_POOL_TASK_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>

namespace thread_pool {

    enum class TaskState {
        IDLE,
        PENDING,
        CANCELED,
        COMPLETED,
        FAILED,
        RUNNING,
    };

    class Task : public std::enable_shared_from_this<Task> {
        virtual ~Task() = default;

        virtual void Run() = 0;

        void AddDependency(std::shared_ptr<Task> dep);
        void AddTrigger(std::shared_ptr<Task> trigger);

        void SetTimeTrigger(std::chrono::system_clock::time_point at);
        auto GetTimeTrigger() -> std::optional<std::chrono::system_clock::time_point>;

        void Complete();

        bool IsCompleted();
        bool IsFailed();
        bool IsCanceled();
        bool IsFinished();

        bool CanBeExecuted();

        void SetError(std::exception_ptr error);
        auto GetError() -> std::exception_ptr;

        void Cancel();
        void Wait();

        void SetNotificationHandler(std::function<void(std::shared_ptr<Task>)> handler);
        void Notify();

    private:
        void AddSubscriber(std::shared_ptr<Task> subscriber);

        void NotifySubscribers(std::unique_lock<std::mutex>& lock);

    private:
        TaskState state_ = TaskState::IDLE;

        std::mutex mutex_;
        std::condition_variable cv_;

        std::vector<std::weak_ptr<Task>> dependencies_;
        std::vector<std::weak_ptr<Task>> subscribers_;
        std::vector<std::weak_ptr<Task>> triggers_;
        std::optional<std::chrono::system_clock::time_point> time_trigger_ = std::nullopt;

        std::optional<std::function<void(std::shared_ptr<Task>)>> notification_handler_ = std::nullopt;

        std::exception_ptr error_;

    };

} // namespace thread_pool

#endif //THREAD_POOL_TASK_H