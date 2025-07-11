/* Copyright (c) 2011-2013, 2015, 2020 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation, nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * ​​​​​Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __MSG_TASK__
#define __MSG_TASK__

#include <mutex>
#include <list>
#include <functional>
#include <LocThread.h>
#include <LocTimer.h>

using std::list;
using std::mutex;

namespace loc_util {

class MsgTimer;

struct LocMsg {
    inline LocMsg() {}
    inline virtual ~LocMsg() {}
    virtual void proc() const = 0;
    inline virtual void log() const {}
};

class MsgTask {
    class MsgTimer : public LocTimer {
        MsgTask& mMsgTask;
        LocMsg* mMsg;
    public:
        inline MsgTimer(MsgTask& msgTask, const LocMsg* msg, uint32_t delayInMs) :
                LocTimer(), mMsgTask(msgTask), mMsg((LocMsg*)msg) {
            start(delayInMs, false/* wakeOnExpire */);
        }
        virtual ~MsgTimer();
       inline void detachMsg() { mMsg = nullptr; }
        virtual void timeOutCallback() override;
    };
    friend class MsgTimer;
    const void* mQ;
    LocThread mThread;
    mutable mutex mMutex;
    mutable list<MsgTimer> mAllMsgTimers;
public:
    ~MsgTask();
    MsgTask(const char* threadName = NULL);
    void sendMsg(const LocMsg* msg, uint32_t delayInMs = 0) const ;
    void sendMsg(const std::function<void()> runnable, uint32_t delayInMs = 0) const;
};

} //

#endif //__MSG_TASK__
