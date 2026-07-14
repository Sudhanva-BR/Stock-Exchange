#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>

namespace miniexchange {

    // Thread-safe FIFO queue using a mutex + condition variable.
    //
    // Producer calls push().
    // Consumer calls waitAndPop() (blocking) or tryPop() (non-blocking).
    // Call shutdown() to unblock all waiting consumers gracefully.
    template<typename T>
    class ThreadSafeQueue {
    public:
        ThreadSafeQueue() = default;

        // Non-copyable, non-movable (mutex and cv are not movable).
        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue(ThreadSafeQueue&&) = delete;
        ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;

        // Push an item; notifies one waiting consumer.
        void push(T item) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.push(std::move(item));
            }
            cv_.notify_one();
        }

        // Non-blocking pop.  Returns true and sets out if an item was available.
        bool tryPop(T& out) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) {
                return false;
            }
            out = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        // Blocking pop.  Blocks until an item is available or shutdown() is called.
        // Returns true and sets out on success; returns false when the queue is
        // shut down and empty (signal to the consumer to exit its loop).
        bool waitAndPop(T& out) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return !queue_.empty() || shutdown_.load(std::memory_order_relaxed);
            });

            if (queue_.empty()) {
                return false;  // woke up because of shutdown, nothing left
            }
            out = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        // Signal consumers to stop blocking once the queue is drained.
        void shutdown() {
            shutdown_.store(true, std::memory_order_relaxed);
            cv_.notify_all();
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

        std::size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }

    private:
        mutable std::mutex              mutex_;
        std::condition_variable         cv_;
        std::queue<T>                   queue_;
        std::atomic<bool>               shutdown_{false};
    };

} // namespace miniexchange
