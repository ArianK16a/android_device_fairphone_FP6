/* Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
Changes from Qualcomm Innovation Center are provided under the following license:

Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netdb.h>
#include <loc_misc_utils.h>
#include <log_util.h>
#include <LocIpc.h>
#include <algorithm>
#include <unordered_map>

using namespace std;

namespace loc_util {

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "LocSvc_LocIpc"

#define SOCK_OP_AND_LOG(buf, length, opable, rtv, exe)  \
    if (nullptr == (buf) || 0 == (length)) { \
        LOC_LOGe("Invalid inputs: buf - %p, length - %u", (buf), (length)); \
    } else if (!(opable)) {                                             \
        LOC_LOGe("Invalid object: operable - %d", (opable));            \
    } else { \
        rtv = (exe); \
        if (-1 == rtv) { \
            LOC_LOGw("failed reason: %s", strerror(errno)); \
        } \
    }

std::unordered_map<std::string, std::pair<int32_t, std::string>> sSockToPayloadMap;

const char Sock::MSG_ABORT[] = "LocIpc::Sock::ABORT";
bool Sock::sRandSeeded = false;

Sock::Sock(int sid, const uint32_t maxTxSize) :
        mMaxTxSize(maxTxSize - sizeof(LOC_IPC_HEAD)), mSid(sid) {
    if (!sRandSeeded) {
        srand48(time(NULL));
        sRandSeeded = true;
    }
    memset(LOC_IPC_HEAD, 0, sizeof(LOC_IPC_HEAD));
    snprintf(LOC_IPC_HEAD, sizeof(LOC_IPC_HEAD), "$MSG_CONCAT_HDR$%16.16lX$", lrand48());
}

ssize_t Sock::send(const void *buf, uint32_t len, int flags, const struct sockaddr *destAddr,
                          socklen_t addrlen) const {
    ssize_t rtv = -1;
    SOCK_OP_AND_LOG(buf, len, isValid(), rtv, sendto(buf, len, flags, destAddr, addrlen));
    return rtv;
}
ssize_t Sock::recv(const LocIpcRecver& recver, const shared_ptr<ILocIpcListener>& dataCb, int flags,
                   struct sockaddr *srcAddr, socklen_t *addrlen, int sid) const {
    ssize_t rtv = -1;
    if (-1 == sid) {
        sid = mSid;
    } // else it sid would be connection based socket id for recv
    SOCK_OP_AND_LOG(dataCb.get(), mMaxTxSize, isValid(), rtv,
                    recvfrom(recver, dataCb, sid, flags, srcAddr, addrlen));
    return rtv;
}
ssize_t Sock::sendto(const void *buf, size_t len, int flags, const struct sockaddr *destAddr,
                     socklen_t addrlen) const {
    ssize_t rtv = -1;
    if (len <= mMaxTxSize) {
        rtv = ::sendto(mSid, buf, len, flags, destAddr, addrlen);
    } else {
        char* tempBuf = new char[sizeof(LOC_IPC_HEAD) + mMaxTxSize];
        if (nullptr != tempBuf) {
            rtv = 1;
            memcpy(tempBuf, LOC_IPC_HEAD, sizeof(LOC_IPC_HEAD) - 9);
            /** Writting 10 instead of 9 bytes into tempBuf to prevent overwritting of "$" by null
                character at last index of LOC_IPC_HEAD. tempBuff is large enough to prevent any
                overflow */
            snprintf(tempBuf + 33, sizeof(LOC_IPC_HEAD) - 32, "%8.8X$", (unsigned int)len);
            for (size_t offset = 0; offset < len && rtv > 0; offset += rtv - sizeof(LOC_IPC_HEAD)) {
                size_t thisLen = min(len - offset, (size_t)mMaxTxSize);
                memcpy(tempBuf+sizeof(LOC_IPC_HEAD), (char*)buf + offset, thisLen);
                rtv = ::sendto(mSid, tempBuf, thisLen + sizeof(LOC_IPC_HEAD), flags, destAddr,
                               addrlen);
                if (rtv == -1 || rtv <= sizeof(LOC_IPC_HEAD)) {
                    LOC_LOGw("sendto failed, return %zu, reason: %s", rtv, strerror(errno));
                    break;
                }
            }
            rtv = (rtv > sizeof(LOC_IPC_HEAD)) ? (len) : -1;
            delete[] tempBuf;
        }
    }
    return rtv;
}
ssize_t Sock::recvfrom(const LocIpcRecver& recver, const shared_ptr<ILocIpcListener>& dataCb,
                       int sid, int flags, struct sockaddr *srcAddr, socklen_t *addrlen) const  {
    std::string msg(mMaxTxSize + sizeof(LOC_IPC_HEAD), 0);
    ssize_t nBytes = ::recvfrom(sid, (void*)msg.data(), msg.size(), flags, srcAddr, addrlen);
    if (nBytes > 0) {
        if (strncmp(msg.data(), MSG_ABORT, sizeof(MSG_ABORT)) == 0) {
            LOC_LOGi("recvd abort msg.data %s", msg.data());
            nBytes = -100;
        } else if (nBytes <= sizeof(LOC_IPC_HEAD) || strncmp(msg.data(), LOC_IPC_HEAD, 16) ||
            msg.data()[32] != '$' || msg.data()[41] != '$') {
            // short message
            msg.resize(nBytes);
            dataCb->onReceive(msg.data(), nBytes, &recver);
        } else {
            // long message
            std::string key(msg.data(), sizeof(LOC_IPC_HEAD));
            auto iter = sSockToPayloadMap.find(key);
            size_t payLoadSize = nBytes - sizeof(LOC_IPC_HEAD);
            if (iter == sSockToPayloadMap.end()) {
                size_t totalSize = 0;
                sscanf(msg.data() + sizeof(LOC_IPC_HEAD) - 9, "%zx", &totalSize);
                sSockToPayloadMap[key] = std::make_pair(totalSize - payLoadSize,
                                                        string(totalSize, 0));
                memcpy((char*) sSockToPayloadMap[key].second.data(), (char *)msg.data()+
                        sizeof(LOC_IPC_HEAD), payLoadSize);
            } else {
                memcpy((char*) iter->second.second.data() +
                        (iter->second.second.size() - iter->second.first),
                        msg.data()+sizeof(LOC_IPC_HEAD), payLoadSize);
                iter->second.first -= payLoadSize;

                if (0 == iter->second.first) {
                    dataCb->onReceive(iter->second.second.data(),
                            iter->second.second.size(), &recver);
                    sSockToPayloadMap.erase(iter);
                }
            }
        }
    }
    return nBytes;
}
ssize_t Sock::sendAbort(int flags, const struct sockaddr *destAddr, socklen_t addrlen) {
    return send(MSG_ABORT, sizeof(MSG_ABORT), flags, destAddr, addrlen);
}

class LocIpcLocalSender : public LocIpcSender {
protected:
    shared_ptr<Sock> mSock;
    struct sockaddr_un mAddr;
    inline virtual bool isOperable() const override { return mSock != nullptr && mSock->isValid(); }
    inline virtual ssize_t send(const uint8_t data[], uint32_t length, int32_t /* msgId */) const {
        return mSock->send(data, length, 0, (struct sockaddr*)&mAddr, sizeof(mAddr));
    }
public:
    inline LocIpcLocalSender(const char* name) : LocIpcSender(),
            mSock(nullptr),
            mAddr({.sun_family = AF_UNIX, {}}) {

        int fd = -1;
        if (nullptr != name) {
            fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
            if (fd >= 0) {
                timeval timeout;
                timeout.tv_sec = 2;
                timeout.tv_usec = 0;
                setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            }
            mSock.reset(new Sock(fd));
            if (mSock != nullptr && mSock->isValid()) {
                snprintf(mAddr.sun_path, sizeof(mAddr.sun_path), "%s", name);
            }
        }
    }
};

class LocIpcLocalRecver : public LocIpcLocalSender, public LocIpcRecver {
protected:
    inline virtual ssize_t recv() const override {
        socklen_t size = sizeof(mAddr);
        return mSock->recv(*this, mDataCb, 0, (struct sockaddr*)&mAddr, &size);
    }
public:
    inline LocIpcLocalRecver(const shared_ptr<ILocIpcListener>& listener, const char* name) :
            LocIpcLocalSender(name), LocIpcRecver(listener, *this) {

        if ((unlink(mAddr.sun_path) < 0) && (errno != ENOENT)) {
            LOC_LOGw("unlink socket error. reason:%s", strerror(errno));
        }

        umask(0157);
        if (mSock->isValid() && ::bind(mSock->mSid, (struct sockaddr*)&mAddr, sizeof(mAddr)) < 0) {
            LOC_LOGe("bind socket error. sock fd: %d: %s, reason: %s", mSock->mSid,
                    mAddr.sun_path, strerror(errno));
            mSock->close();
        }
    }
    inline virtual ~LocIpcLocalRecver() { unlink(mAddr.sun_path); }
    inline virtual const char* getName() const override { return mAddr.sun_path; };
    inline virtual void abort() const override {
        if (isSendable()) {
            mSock->sendAbort(0, (struct sockaddr*)&mAddr, sizeof(mAddr));
        }
    }
};

class LocIpcInetSender : public LocIpcSender {
protected:
    int mSockType;
    shared_ptr<Sock> mSock;
    const string mName;
    sockaddr_in mAddr;
    inline virtual bool isOperable() const override { return mSock != nullptr && mSock->isValid(); }
    virtual ssize_t send(const uint8_t data[], uint32_t length, int32_t /* msgId */) const {
        return mSock->send(data, length, 0, (struct sockaddr*)&mAddr, sizeof(mAddr));
    }
public:
    inline LocIpcInetSender(const LocIpcInetSender& sender) :
            mSockType(sender.mSockType), mSock(sender.mSock),
            mName(sender.mName), mAddr(sender.mAddr) {
    }
    inline LocIpcInetSender(const char* name, int32_t port, int sockType) : LocIpcSender(),
            mSockType(sockType),
            mSock(make_shared<Sock>((nullptr == name) ? -1 : (::socket(AF_INET, mSockType, 0)))),
            mName((nullptr == name) ? "" : name),
            mAddr({.sin_family = AF_INET, .sin_port = htons(port),
                    .sin_addr = {htonl(INADDR_ANY)}}) {
        if (mSock != nullptr && mSock->isValid() && nullptr != name) {
            struct hostent* hp = gethostbyname(name);
            if (nullptr != hp) {
                memcpy((char*)&(mAddr.sin_addr.s_addr), hp->h_addr_list[0], hp->h_length);
            }
        }
    }

    unique_ptr<LocIpcRecver> getRecver(const shared_ptr<ILocIpcListener>& listener) override {
        return make_unique<SockRecver>(listener, *this, mSock);
    }
};

class LocIpcInetTcpSender : public LocIpcInetSender {
protected:
    mutable bool mFirstTime;

    virtual ssize_t send(const uint8_t data[], uint32_t length, int32_t /* msgId */) const {
        int connStatus = 0;
        if (mFirstTime) {
            connStatus = ::connect(mSock->mSid, (const struct sockaddr*)&mAddr, sizeof(mAddr));
            if (0 == connStatus) {
                mFirstTime = false;
            }
        }

        if (0 == connStatus) {
            return mSock->send(data, length, 0, (struct sockaddr*)&mAddr, sizeof(mAddr));
        }

        return 0;
    }

public:
    inline LocIpcInetTcpSender(const char* name, int32_t port) :
            LocIpcInetSender(name, port, SOCK_STREAM),
            mFirstTime(true) {}
};

class LocIpcInetRecver : public LocIpcInetSender, public LocIpcRecver {
     int32_t mPort;
protected:
     virtual ssize_t recv() const = 0;
public:
    inline LocIpcInetRecver(const shared_ptr<ILocIpcListener>& listener, const char* name,
                               int32_t port, int sockType) :
            LocIpcInetSender(name, port, sockType), LocIpcRecver(listener, *this),
            mPort(port) {
        if (mSock->isValid() && ::bind(mSock->mSid, (struct sockaddr*)&mAddr, sizeof(mAddr)) < 0) {
            LOC_LOGe("bind socket error. sock fd: %d, reason: %s", mSock->mSid, strerror(errno));
            mSock->close();
        }
    }
    inline virtual ~LocIpcInetRecver() {}
    inline virtual const char* getName() const override { return mName.data(); };
    inline virtual void abort() const override {
        if (isSendable()) {
            mSock->sendAbort(0, (struct sockaddr*)&mAddr, sizeof(mAddr));
        }
    }
    inline virtual unique_ptr<LocIpcSender> getLastSender() const override {
        return make_unique<LocIpcInetSender>(static_cast<const LocIpcInetSender&>(*this));
    }
};

class LocIpcInetTcpRecver : public LocIpcInetRecver {
    mutable int32_t mConnFd;
protected:
    inline virtual ssize_t recv() const override {
        socklen_t size = sizeof(mAddr);
        if (-1 == mConnFd && mSock->isValid()) {
            if (::listen(mSock->mSid, 3) < 0 ||
                (mConnFd = accept(mSock->mSid, (struct sockaddr*)&mAddr, &size)) < 0) {
                mSock->close();
                mConnFd = -1;
            }
        }
        ssize_t nBytes = mSock->recv(*this, mDataCb, 0, (struct sockaddr*)&mAddr, &size, mConnFd);
        if (0 == nBytes) {
            // tcp connection closed, accept new connection for next recv
            // But do not exit the receiver thread by returning 0 bytes.
            mConnFd = -1;
            nBytes = 100;
        }
        return nBytes;
    }
public:
    inline LocIpcInetTcpRecver(const shared_ptr<ILocIpcListener>& listener, const char* name,
                               int32_t port) :
            LocIpcInetRecver(listener, name, port, SOCK_STREAM), mConnFd(-1) {}
    inline virtual ~LocIpcInetTcpRecver() { if (-1 != mConnFd) ::close(mConnFd);}
};

class LocIpcInetUdpRecver : public LocIpcInetRecver {
protected:
    inline virtual ssize_t recv() const override {
        socklen_t size = sizeof(mAddr);
        return mSock->recv(*this, mDataCb, 0, (struct sockaddr*)&mAddr, &size);
    }
public:
    inline LocIpcInetUdpRecver(const shared_ptr<ILocIpcListener>& listener, const char* name,
                                int32_t port) :
            LocIpcInetRecver(listener, name, port, SOCK_DGRAM) {}

    inline virtual ~LocIpcInetUdpRecver() {}
};

class LocIpcRunnable : public LocRunnable {
    bool mAbortCalled;
    LocIpc& mLocIpc;
    unique_ptr<LocIpcRecver> mIpcRecver;
public:
    inline LocIpcRunnable(LocIpc& locIpc, unique_ptr<LocIpcRecver>& ipcRecver) :
            mAbortCalled(false),
            mLocIpc(locIpc),
            mIpcRecver(move(ipcRecver)) {}
    inline virtual bool run() override {
        if (mIpcRecver != nullptr) {
            mLocIpc.startBlockingListening(*(mIpcRecver.get()));
            if (!mAbortCalled) {
                LOC_LOGw("startListeningBlocking() returned w/o stopBlockingListening() called");
            }
        }
        // return false so the calling thread exits while loop
        return false;
    }
    inline virtual void interrupt() override {
        mAbortCalled = true;
        if (mIpcRecver != nullptr) {
            mIpcRecver->abort();
        }
    }
};

bool LocIpc::startNonBlockingListening(unique_ptr<LocIpcRecver>& ipcRecver) {
    if (ipcRecver != nullptr && ipcRecver->isRecvable()) {
        std::string threadName = generateThreadName(ipcRecver->getName());
        return mThread.start(threadName.c_str(), make_shared<LocIpcRunnable>(*this, ipcRecver));
    } else {
        LOC_LOGe("ipcRecver is null OR ipcRecver->recvable() is fasle");
        return false;
    }
}

bool LocIpc::startBlockingListening(LocIpcRecver& ipcRecver) {
    if (ipcRecver.isRecvable()) {
        // inform that the socket is ready to receive message
        ipcRecver.onListenerReady();
        while (ipcRecver.recvData());
        return true;
    } else  {
        LOC_LOGe("ipcRecver is null OR ipcRecver->recvable() is fasle");
        return false;
    }
}

void LocIpc::stopNonBlockingListening() {
    mThread.stop();
}

void LocIpc::stopBlockingListening(LocIpcRecver& ipcRecver) {
    if (ipcRecver.isRecvable()) {
        ipcRecver.abort();
    }
}

bool LocIpc::send(LocIpcSender& sender, const uint8_t data[], uint32_t length, int32_t msgId) {
    return sender.sendData(data, length, msgId);
}

std::string LocIpc::generateThreadName(const std::string& recverName) {
    std::string threadName("Ipc-");
    int recverNameLen = recverName.length();
    // Total thread name length is 16 letters.
    // Concatenate with up to 11 chars starting from the last char of the ipc name.
    threadName.append(recverName.substr(recverNameLen <= 11 ? 0 : (recverNameLen - 11)));
    return threadName;
}

shared_ptr<LocIpcSender> LocIpc::getLocIpcLocalSender(const char* localSockName) {
    return make_shared<LocIpcLocalSender>(localSockName);
}
unique_ptr<LocIpcRecver> LocIpc::getLocIpcLocalRecver(const shared_ptr<ILocIpcListener>& listener,
                                                      const char* localSockName) {
    return make_unique<LocIpcLocalRecver>(listener, localSockName);
}
static void* sLibQrtrHandle = nullptr;
static const char* sLibQrtrName = "libloc_socket.so";
shared_ptr<LocIpcSender> LocIpc::getLocIpcQrtrSender(int service, int instance) {
    typedef shared_ptr<LocIpcSender> (*creator_t) (int, int);
    static creator_t creator = (creator_t)dlGetSymFromLib(sLibQrtrHandle, sLibQrtrName,
            "_ZN8loc_util22createLocIpcQrtrSenderEii");
    return (nullptr == creator) ? nullptr : creator(service, instance);
}
unique_ptr<LocIpcRecver> LocIpc::getLocIpcQrtrRecver(const shared_ptr<ILocIpcListener>& listener,
                                                     int service, int instance,
                                                     const shared_ptr<LocIpcQrtrWatcher>& watcher) {
    typedef unique_ptr<LocIpcRecver> (*creator_t)(const shared_ptr<ILocIpcListener>&, int, int,
                                                  const shared_ptr<LocIpcQrtrWatcher>& watcher);
    static creator_t creator = (creator_t)dlGetSymFromLib(sLibQrtrHandle, sLibQrtrName,
#ifdef USE_GLIB
            "_ZN8loc_util22createLocIpcQrtrRecverERKSt10shared_ptrINS_15ILocIpcListenerEEiiRKS0_INS_17LocIpcQrtrWatcherEE");
#else
            "_ZN8loc_util22createLocIpcQrtrRecverERKNSt3__110shared_ptrINS_15ILocIpcListenerEEEiiRKNS1_INS_17LocIpcQrtrWatcherEEE");
#endif
    return (nullptr == creator) ? nullptr : creator(listener, service, instance, watcher);
}
shared_ptr<LocIpcSender> LocIpc::getLocIpcInetTcpSender(const char* serverName, int32_t port) {
    return make_shared<LocIpcInetTcpSender>(serverName, port);
}
unique_ptr<LocIpcRecver> LocIpc::getLocIpcInetTcpRecver(const shared_ptr<ILocIpcListener>& listener,
                                                            const char* serverName, int32_t port) {
    return make_unique<LocIpcInetTcpRecver>(listener, serverName, port);
}
shared_ptr<LocIpcSender> LocIpc::getLocIpcInetUdpSender(const char* serverName, int32_t port) {
    return make_shared<LocIpcInetSender>(serverName, port, SOCK_DGRAM);
}
unique_ptr<LocIpcRecver> LocIpc::getLocIpcInetUdpRecver(const shared_ptr<ILocIpcListener>& listener,
                                                             const char* serverName, int32_t port) {
    return make_unique<LocIpcInetUdpRecver>(listener, serverName, port);
}
pair<shared_ptr<LocIpcSender>, unique_ptr<LocIpcRecver>>
        LocIpc::getLocIpcQmiLocServiceSenderRecverPair(const shared_ptr<ILocIpcListener>& listener, int instance) {
    typedef pair<shared_ptr<LocIpcSender>, unique_ptr<LocIpcRecver>> (*creator_t)(const shared_ptr<ILocIpcListener>&, int);
    static void* sLibEmuHandle = nullptr;
    static creator_t creator = (creator_t)dlGetSymFromLib(sLibEmuHandle, "libloc_emu.so",
#ifdef USE_GLIB
            "_ZN13QmiLocService41createLocIpcQmiLocServiceSenderRecverPair"\
            "ERKSt10shared_ptrIN8loc_util15ILocIpcListenerEEi");
#else
            "_ZN13QmiLocService41createLocIpcQmiLocServiceSenderRecverPair"\
            "ERKNSt3__110shared_ptrIN8loc_util15ILocIpcListenerEEEi");
#endif
    return (nullptr == creator) ?
            make_pair<shared_ptr<LocIpcSender>, unique_ptr<LocIpcRecver>>(nullptr, nullptr) :
            creator(listener, instance);
}

}
