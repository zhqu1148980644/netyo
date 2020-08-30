/**
 *
 *  TimingWheel.cc
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

// modified version

#pragma once
#include "../utils/Common.h"
#include <unordered_set>
#include "Buffer.h"
#include <memory>
#include <functional>
#include <atomic>
#include "eventloop/ThreadingEventLoop.h"
#include <chrono>

using namespace std;
using namespace chrono;

class TimingWheel : public NoCopyble {
protected:
    using SPEntry = shared_ptr<void>;
    using EntryBucket = FixedRingBuffer<unordered_set<SPEntry>>;
    EventLoop * loop = nullptr;
    vector<EntryBucket> wheels;
    atomic<size_t> curtick {0};
    
    size_t tick_interval = 1, nwheel = 1, bucket_size = 100;

    class CallBackEntry {
    private:
        using CallBack = function<void()>;
        CallBack cb;
    public:
        CallBackEntry(CallBack cb) : cb(move(cb)) {}
        ~CallBackEntry() { cb(); }
    };

public:
    TimingWheel(EventLoop * loop, size_t max_time, size_t tick_interval = 1) 
        : loop(loop), 
          tick_interval(tick_interval) {
        
        int num_tick = max_time / tick_interval + 1;
        int cur_num_tick = bucket_size;
        while (cur_num_tick <= num_tick) {
            nwheel++;
            cur_num_tick *= bucket_size;
        }
        // initialize timing wheels
        wheels = vector<EntryBucket>(nwheel, EntryBucket(bucket_size));

        loop->call_every([=]() {
            ++curtick;
            size_t t = curtick.load();
            size_t base = 1;
            for (size_t i = 0; i < nwheel; ++i) {
                if (t % base == 0) {
                    wheels[i].push_back(unordered_set<SPEntry>());
                }
                base *= bucket_size;
            }
        },
        seconds(tick_interval));
    }

    // ~TimingWheell()      default is fine
    void insert_in_loop(size_t delay, SPEntry spentry) {
        loop->assert_within_self_thread();
        size_t ntick = delay / tick_interval + 1;
        size_t base = 1;
        for (size_t i = 0; i < nwheel && delay > 0; ++i) {
            if (delay <=  bucket_size) {
                wheels[i][delay - 1].insert(spentry);
                break;
            }
            else if (i < nwheel - 1) {
                size_t pos = (delay - 1) % bucket_size;
                spentry = make_shared<CallBackEntry>([=](){
                    wheels[i][(delay - 1) % bucket_size].insert(spentry);
                });
                delay -= pos * base;
                base *= bucket_size;
            }
            else {
                wheels[nwheel - 1][bucket_size - 1].insert(spentry);
                return;
            }
        }
    }

    void insert(size_t delay, SPEntry spentry) {
        if (loop->within_self_thread()) {
            insert_in_loop(delay, move(spentry));
        }
        else {
            loop->call_soon([=](){ insert_in_loop(delay, spentry); });
        }
    }
};