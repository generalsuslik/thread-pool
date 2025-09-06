//
// Created by General Suslik on 06.09.2025.
//

#include "task.h"

#include <algorithm>

namespace thread_pool {

    void Task::AddDependency(std::shared_ptr<Task> dep) {
        std::lock_guard lock{mutex_};

        dep->AddSubscriber(shared_from_this());
        dependencies_.emplace_back(std::move(dep));
    }

    void Task::AddTrigger(std::shared_ptr<Task> trigger) {
        std::lock_guard lock{mutex_};

        trigger->AddSubscriber(shared_from_this());
        triggers_.emplace_back(std::move(trigger));
    }

    void Task::AddSubscriber(std::shared_ptr<Task> subscriber) {
        std::lock_guard lock{mutex_};

        subscribers_.emplace_back(std::move(subscriber));
    }

    void Task::SetTimeTrigger(std::chrono::system_clock::time_point at) {
        std::lock_guard lock{mutex_};

        time_trigger_ = at;
    }

    auto Task::GetTimeTrigger() -> std::optional<std::chrono::system_clock::time_point> {
        std::lock_guard lock{mutex_};

        return time_trigger_;
    }

    bool Task::Capture() {
        std::lock_guard lock{mutex_};

        switch (state_) {
            case TaskState::PENDING:
                state_ = TaskState::RUNNING;
                return true;
            default:
                return false;
        }
    }

    void Task::Pend() {
        std::lock_guard lock{mutex_};

        state_ = TaskState::PENDING;
    }

    void Task::Complete() {
        std::unique_lock lock{mutex_};

        state_ = TaskState::COMPLETED;
        NotifySubscribers(lock);
        cv_.notify_all();
    }

    bool Task::IsCompleted() {
        std::lock_guard lock{mutex_};

        return state_ == TaskState::COMPLETED;
    }

    bool Task::IsFailed() {
        std::lock_guard lock{mutex_};

        return state_ == TaskState::FAILED;
    }

    bool Task::IsCanceled() {
        std::lock_guard lock{mutex_};

        return state_ == TaskState::CANCELED;
    }

    bool Task::IsFinished() {
        std::lock_guard lock{mutex_};

        return IsCompleted() || IsFailed() || IsCanceled();
    }

    bool Task::CanBeExecuted() {
        std::lock_guard lock{mutex_};

        if (triggers_.empty() && dependencies_.empty() && !time_trigger_.has_value()) {
            return true;
        }

        if (!time_trigger_.has_value() || std::chrono::system_clock::now() < time_trigger_.value()) {
            const bool trigger_ready = std::ranges::any_of(triggers_, [](const auto& trigger) {
                return trigger.lock()->IsFinished();
            });
            if (trigger_ready) {
                return true;
            }

            const bool deps_ready = std::ranges::all_of(dependencies_, [](const auto& dep) {
                return dep.lock()->IsFinished();
            });
            if (!deps_ready) {
                return false;
            }

            // no triggers, no time_trigger, deps are empty => cant execute
            // 'cause of the time trigger
            return !dependencies_.empty();
        }

        return true;
    }

    void Task::SetError(std::exception_ptr error) {
        std::unique_lock lock{mutex_};

        state_ = TaskState::FAILED;
        error_ = std::move(error);
        NotifySubscribers(lock);
        cv_.notify_all();
    }

    auto Task::GetError() -> std::exception_ptr {
        std::lock_guard lock{mutex_};

        return error_;
    }

    void Task::Cancel() {
        std::unique_lock lock{mutex_};

        switch (state_) {
            case TaskState::IDLE:
            case TaskState::PENDING:
            case TaskState::RUNNING:
                state_ = TaskState::CANCELED;
                NotifySubscribers(lock);
                cv_.notify_all();
                break;
            case TaskState::COMPLETED:
            case TaskState::FAILED:
            case TaskState::CANCELED:
                break;
        }
    }

    void Task::Wait() {
        std::unique_lock lock{mutex_};

        switch (state_) {
            case TaskState::IDLE:
            case TaskState::PENDING:
            case TaskState::RUNNING:
                cv_.wait(lock, [this] { return IsFinished(); });
                break;
            case TaskState::COMPLETED:
            case TaskState::FAILED:
            case TaskState::CANCELED:
                break;
        }
    }

    void Task::SetNotificationHandler(std::function<void(std::shared_ptr<Task>)> handler) {
        std::lock_guard lock{mutex_};

        notification_handler_.emplace(std::move(handler));
    }

    void Task::Notify() {
        std::unique_lock lock{mutex_};

        if (notification_handler_.has_value()) {
            const auto handler = notification_handler_.value();
            lock.unlock();
            handler(shared_from_this());
        }
    }

    void Task::NotifySubscribers(std::unique_lock<std::mutex>& lock) {
        auto subs = std::move(subscribers_);
        subscribers_.clear();
        lock.unlock();

        for (const auto& sub : subs) {
            if (auto task = sub.lock()) {
                if (this != task.get()) {
                    task->Notify();
                }
            }
        }
        lock.lock();
    }

} //  namespace thread_pool
