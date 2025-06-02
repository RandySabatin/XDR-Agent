#pragma once
#ifndef JSONQUEUE_H
#define JSONQUEUE_H

#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include "json.h"

class ThreadSafeQueue {
public:
    void push(const Json::Value& data);
    bool pop(Json::Value& data);
    bool isEmpty() const;

private:
    std::queue<Json::Value> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
};

#endif