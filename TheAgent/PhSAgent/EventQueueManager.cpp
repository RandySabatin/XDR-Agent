#include "pch.h"
#include "EventQueueManager.h"

EventQueueManager::EventQueueManager()
    : maxSize_(10000) {}  // Default max size

EventQueueManager::~EventQueueManager() {}

void EventQueueManager::SetMaxSize(unsigned int newSize) {
    std::lock_guard<std::mutex> lock(queueMutex_);
	if (newSize > limitSize_) {
		// If the new size is invalid, set it to the default max size
		newSize = limitSize_;
	}
    maxSize_ = newSize;

    LOGW(Logger::INFO, Utility::FormatString(L"Max queue events size. Max queue events size: %u", maxSize_));

    // Trim the queue if it's too large
    while (queue_.size() > maxSize_) {
        queue_.pop();
    }
}

void EventQueueManager::Push(const std::wstring& item) {
    std::lock_guard<std::mutex> lock(queueMutex_);

    if (queue_.size() >= maxSize_) {
        queue_.pop();  // Remove oldest to make room
    }

    queue_.push(item);
}

void EventQueueManager::Pop() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (!queue_.empty()) {
        queue_.pop();
    }
}

std::wstring EventQueueManager::Peek() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (queue_.empty()) return L"";

    return queue_.front();
}

bool EventQueueManager::IsEmpty() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return queue_.empty();
}

unsigned int EventQueueManager::Size() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return static_cast<unsigned int>(queue_.size());
}



//#include "EventQueueManager.h"
//
//EventQueueManager::EventQueueManager() {}
//
//EventQueueManager::~EventQueueManager() {}
//
//void EventQueueManager::Push(const std::wstring& key, const std::wstring& value) {
//    std::lock_guard<std::mutex> lock(queueMutex_);
//    queue_[key] = value; // Insert or update the key-value pair in the map
//}
//
//void EventQueueManager::Pop() {
//    std::lock_guard<std::mutex> lock(queueMutex_);
//    if (!queue_.empty()) {
//        // Pop the first element (though maps don't have the concept of "popping" as a queue does)
//        queue_.erase(queue_.begin());
//    }
//}
//
//std::pair<std::wstring, std::wstring> EventQueueManager::Peek() {
//    std::lock_guard<std::mutex> lock(queueMutex_);
//    if (queue_.empty()) return { L"", L"" }; // Return a pair of empty strings if the map is empty
//
//    // Get the first element (which is ordered by the key)
//    auto it = queue_.begin();
//    return { it->first, it->second }; // Return both the key and the value as a pair
//}
//
//bool EventQueueManager::IsEmpty() const {
//    std::lock_guard<std::mutex> lock(queueMutex_);
//    return queue_.empty(); // Return true if the map is empty
//}
