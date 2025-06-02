#include "ThreadSafeQueue.h"

void ThreadSafeQueue::push(const Json::Value& data) {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push(data);
    cv_.notify_one();  // Notify the worker thread to process
}

bool ThreadSafeQueue::pop(Json::Value& data) {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] { return !queue_.empty(); });  // Wait until the queue has data
    data = queue_.front();
    queue_.pop();
    return true;
}

bool ThreadSafeQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return queue_.empty();
}