#pragma once
#include <queue>
#include <mutex>
#include <string>

class EventQueueManager {
public:
    EventQueueManager();                          // Default max size = 5000
    ~EventQueueManager();

    void SetMaxSize(unsigned int newSize);        // Set max size from config
    void Push(const std::wstring& item);
    void Pop();
    std::wstring Peek();
    bool IsEmpty() const;
    unsigned int Size() const;

private:
    mutable std::mutex queueMutex_;
    std::queue<std::wstring> queue_;
    unsigned int maxSize_;
    unsigned int limitSize_ = 50000;
};


//
//#include <map>
//#include <string>
//#include <mutex>
//#include <utility> // For std::pair
//
//class EventQueueManager {
//public:
//    // Constructor and Destructor
//    EventQueueManager();
//    ~EventQueueManager();
//
//    // Method to push a key-value pair to the queue
//    void Push(const std::wstring& key, const std::wstring& value);
//
//    // Method to pop the first element in the queue
//    void Pop();
//
//    // Method to peek at the first element in the queue (returns both key and value)
//    std::pair<std::wstring, std::wstring> Peek();
//
//    // Method to check if the queue is empty
//    bool IsEmpty() const;
//
//private:
//    // The queue (a map where the key and value are both std::wstring)
//    mutable std::mutex queueMutex_;  // Mutex for thread safety
//    std::map<std::wstring, std::wstring> queue_;
//};