#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template<typename T>
class SafeQueue
{
public:
    SafeQueue() = default;
    ~SafeQueue() = default;

    SafeQueue(const SafeQueue &) = delete;
    SafeQueue &operator=(const SafeQueue &) = delete;

    SafeQueue(SafeQueue &&other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        queue_ = std::move(other.queue_);
    }

    SafeQueue &operator=(SafeQueue &&other) noexcept
    {
        if (this != &other) {
            std::lock_guard<std::mutex> lock_this(mutex_);
            std::lock_guard<std::mutex> lock_other(other.mutex_);
            queue_ = std::move(other.queue_);
        }
        return *this;
    }

    // Добавить элемент по копированию
    void push(const T &item)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(item);
        }
        cond_var_.notify_one();
    }

    // Добавить элемент по перемещению
    void push(T &&item)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cond_var_.notify_one();
    }

    // Создание элемента на месте
    template<typename... Args>
    void emplace(Args &&...args)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.emplace(std::forward<Args>(args)...);
        }
        cond_var_.notify_one();
    }

    // Извлечение с таймаутом (в мс)
    std::optional<T> pop(int timeout_ms)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cond_var_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
                return !queue_.empty();
            })) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<T> queue_;
};
