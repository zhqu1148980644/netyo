/**
 *
 *  LockFreeQueue.h
 *  An Tao
 *
 *  Public header file in trantor lib.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#pragma once
#include "../../utils/Common.h"
#include <assert.h>

#include <atomic>
#include <memory>
#include <type_traits>

/// A lock-free multiple producers single consumer queue
template <typename T>
class MpscQueue : public NoCopyble {
public:
    MpscQueue()
        : head_(new BufferNode), tail_(head_.load(std::memory_order_relaxed)) {}

    ~MpscQueue() {
        T output;
        while (this->dequeue(output)) {
        }
        BufferNode *front = head_.load(std::memory_order_relaxed);
        delete front;
    }
    template <typename X>
    void enqueue(X &&input) {
        BufferNode *node{new BufferNode(std::forward<X>(input))};
        BufferNode *prevhead{head_.exchange(node, std::memory_order_acq_rel)};
        prevhead->next_.store(node, std::memory_order_release);
    }

    bool dequeue(T &output) {
        BufferNode *tail = tail_.load(std::memory_order_relaxed);
        BufferNode *next = tail->next_.load(std::memory_order_acquire);

        if (next == nullptr) {
            return false;
        }
        output = std::move(*(next->dataPtr_));
        delete next->dataPtr_;
        tail_.store(next, std::memory_order_release);
        delete tail;
        return true;
    }

private:
    struct BufferNode {
        BufferNode() = default;
        BufferNode(const T &data) : dataPtr_(new T(data)) {}
        BufferNode(T &&data) : dataPtr_(new T(std::move(data))) {}
        T *dataPtr_;
        std::atomic<BufferNode *> next_{nullptr};
    };

    std::atomic<BufferNode *> head_;
    std::atomic<BufferNode *> tail_;
};
