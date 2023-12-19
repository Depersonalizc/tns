#pragma once

#include <array>
#include <cassert>
#include <condition_variable>
// #include <iostream>
#include <mutex>
#include <span>
// #include <experimental/memory>

#include "tcp/intervals.hpp"
#include "tcp/retransmission_queue.hpp"
#include "tcp/socket_error.hpp"
#include "util/defines.hpp"
#include "util/tl/expected.hpp"


namespace tns {
namespace tcp {

// A generic circular buffer for TCP
template <typename T, std::size_t N>
class RingBuffer {
public:
    RingBuffer() = default;

    /**
     * @brief Write data starting from buf_[idx(at)] until buf_[idx(last)] inclusive.
     * 
     * Returns the number of bytes actually written.
     * 
     * @param data The bytes to write
     * @param at The *absolute* index of first element to write, inclusive
     * @param last The *absolute* index of last (potential) element to write, inclusive
     */
    std::size_t write(const std::span<const T> data, const std::size_t at, const std::size_t last)
    {
        if (at >= last)
            return 0;

        assert(last - at < N && "Cannot write more than the buffer size");
        // Alternatively: last = std::min(last, at + N - 1);

        auto atIdx = idx(at);
        auto lastIdx = idx(last);
        auto lastIdxPlus1 = lastIdx + 1;

        if (atIdx <= lastIdx) {
            // [atIdx, lastIdx] is the available range for write
            const auto n = std::min(lastIdxPlus1 - atIdx, data.size());
            std::copy(data.begin(), data.begin() + n, buf_.begin() + atIdx);
            return n;
        }
        else {
            // [atIdx, N) and [0, lastIdx] are the available ranges for write
            auto nLeft = data.size();
            const auto n1 = N - atIdx;
            if (n1 >= nLeft) {
                // Copy to buf_[atIdx, atIdx + data.size())
                std::copy(data.begin(), data.begin() + nLeft, buf_.begin() + atIdx);
                return nLeft;
            }
            else {
                // Copy to buf_[atIdx, N)
                std::copy(data.begin(), data.begin() + n1, buf_.begin() + atIdx);
                nLeft -= n1;

                // Copy to buf_[0, std::min(lastIdx + 1, nLeft))
                const auto n2 = std::min(lastIdxPlus1, nLeft);
                std::copy(data.begin() + n1, data.begin() + n1 + n2, buf_.begin());

                return n1 + n2;
            }
        }
    }

    /**
     * @brief Read data starting from buf_[idx(at)] until buf_[idx(last)] inclusive 
     * into a provided buffer. Data is *truncated* if the provided buffer is too short.
     * 
     * Returns the number of bytes actually read.
     * 
     * @param buff The buffer to read into (Read max buff.size() bytes)
     * @param at The *absolute* index of first element to read, inclusive
     * @param last The *absolute* index of last (potential) element to read, inclusive
     */
    std::size_t read(const std::span<T> buff, const std::size_t at, const std::size_t last) const
    {
        if (at > last)
            return 0;

        // last = std::min(last, at + buff.size() - 1);

        assert(last - at < N && "Cannot read more than the buffer size");
        // Alternatively: last = std::min(last, at + N - 1);

        auto atIdx = idx(at);
        auto lastIdx = idx(last);
        auto lastIdxPlus1 = lastIdx + 1;

        if (atIdx <= lastIdx) {
            // [atIdx, lastIdx] is the available range for read
            const auto n = std::min(lastIdxPlus1 - atIdx, buff.size());
            std::copy(buf_.begin() + atIdx, buf_.begin() + atIdx + n, buff.begin());
            return n;
        }
        else {
            // [atIdx, N) and [0, lastIdx] are the available ranges for read
            auto nLeft = buff.size();
            const auto n1 = N - atIdx;
            if (n1 >= nLeft) {
                // Copy to buff[0, buff.size())
                std::copy(buf_.begin() + atIdx, buf_.begin() + atIdx + nLeft, buff.begin());
                return nLeft;
            }
            else {
                // Copy to buff[0, N - atIdx)
                std::copy(buf_.begin() + atIdx, buf_.begin() + atIdx + n1, buff.begin());
                nLeft -= n1;

                // Copy to buff[n1, std::min(lastIdx + 1, nLeft))
                const auto n2 = std::min(lastIdx + 1, nLeft);
                std::copy(buf_.begin(), buf_.begin() + n2, buff.begin() + n1);

                return n1 + n2;
            }
        }
    }

    const T& at(std::size_t seq) const { return buf_[idx(seq)]; }

    // // for DEBUG
    // void print() const noexcept
    // {
    //     for (const auto &elem : buf_)
    //         std::cout << elem << ", ";
    //     std::cout << "\n";
    // }

    static constexpr std::size_t max_size() noexcept { return N; }

protected:
    std::array<T, N> buf_;
protected:
    static constexpr std::size_t idx(std::size_t seq) noexcept { return seq % N; }
};


template <std::size_t N>
class SendBuffer : public RingBuffer<std::byte, N> {
public:
    using RB = RingBuffer<std::byte, N>;
    // using seq_t = std::uint32_t?

    SendBuffer() = default;
    SendBuffer(std::uint32_t initSeqNum, std::uint32_t windowSize) noexcept
        : una_(initSeqNum), nxt_(initSeqNum), nbw_(initSeqNum), wnd_(windowSize) 
    {}

    ~SendBuffer() { shutdown(); std::cout << "SendBuffer DESTRUCTED\n"; }

    /**
     * @brief Write data to the send buffer.
     * 
     * This function writes the given data to the buffer. It blocks until there is free space in the buffer.
     * After writing, it moves the next byte to write (nbw_) pointer to one past the last byte written.
     * 
     * @param data The data to write to the buffer
     * @return The number of bytes written or a SocketError
     */
    template <typename T>
    tl::expected<std::size_t, SocketError> write(const std::span<const T> data)
    {
        std::size_t n;
        for (auto bytesLeft = std::as_bytes(data); !bytesLeft.empty(); bytesLeft = bytesLeft.subspan(n)) 
        {
            {
                std::unique_lock lk(mutex_);

                // std::cout << "I have " << bytesLeft.size() << " bytes left to write, waiting for the buffer (szFree=" 
                //           << sizeFreeNoLock_() << ", wnd_=" << wnd_ << ")" << std::endl;

                cvWriter_.wait(lk, [this] { return sizeFreeNoLock_() > 0 || stopped_; });

                // std::cout << "Got the buffer (size free=" << sizeFreeNoLock_() << ", wnd_=" << wnd_ << ")" << std::endl;

                if (stopped_)
                    return tl::unexpected{SocketError::CLOSING};

                n = RB::write(bytesLeft, nbw_, nbw_ + sizeFreeNoLock_()-1);  // Write to [nbw_..una_ + N-1]            
                nbw_ += static_cast<decltype(nbw_)>(n);                      // Move nbw_ to the right

                // std::cout << "Wrote " << n << " bytes to the buffer, nbw_ = " << nbw_ << "\n";

                // Notify the single SENDER thread that there is new data to send out
                // std::cout << "sizeCanSend = " << sizeCanSendNoLock_() << ", notifying sender thread\n";
                if (sizeCanSendNoLock_() > 0)
                    cvSender_.notify_one();
            }
        }
        return data.size_bytes();
    }

    /**
     * @brief Handle the acknowledgement of data.
     * 
     * This function is called when an acknowledgement (ACK) for a certain amount of data is received.
     * It updates the unacknowledged data pointer (una_) if the received ACK number is within the expected range.
     * After updating, it notifies all waiting (write) threads.
     * 
     * @param ackNum The number of the received acknowledgement
     * @param wndSize The window size advertised by the remote
     * @return The new in-flight data window as a pair of (una_, nxt_)
     */
    std::pair<std::uint32_t, std::uint32_t>
    onAck(std::uint32_t ackNum, std::uint32_t wndSize)
    {
        std::pair<std::uint32_t, std::uint32_t> una_nxt{};
        {
            std::lock_guard lk(mutex_);

            una_nxt.first  = una_;
            una_nxt.second = nxt_;

            /**
             * In a connection with a one-way data flow, the window information will be carried in acknowledgment segments 
             * that all have the same sequence number, so there will be no way to reorder them if they arrive out of order. 
             * This is not a serious problem, but it will allow the window information to be on occasion temporarily based on 
             * old reports from the data receiver. A refinement to avoid this problem is to act on the window information from 
             * segments that carry the highest acknowledgment number (that is, segments with an acknowledgment number equal to 
             * or greater than the highest previously received).
             */
            if (ackNum >= una_) {
                // Update the window size
                if (wndSize > wnd_)
                    cvSender_.notify_one();  // Notify SENDER thread if window expands
                wnd_ = wndSize;

                // zwp_.onWnd(wndSize);
                switch (zwp_.state) {
                case 0:  // PAUSE
                    if (wnd_ == 0) {
                        std::cout << "Got zero window ack (" << ackNum << ") in state 0" << std::endl;
                        zwp_.cvOnPause.notify_one();
                    }
                    break;
                case 1:  // COUNTDOWN
                    // std::cout << "Got ack (" << ackNum << ") with WND = " << wnd_ << " in state 1" << std::endl;
                    zwp_.cvOnCountdown.notify_one();
                    break;
                case 2:  // WAITACK
                    std::cout << "Got ack (" << ackNum << ") with WND = " << wnd_ << " in state 2" << std::endl;
                    /**
                     * The transmitting host SHOULD send the first zero-window probe when a zero window has existed for the retransmission 
                     * timeout period (SHLD-29) (Section 3.8.1), and SHOULD increase exponentially the interval between successive probes (SHLD-30).
                    */
                    if (ackNum > zwp_.seq) {
                        // If the probe data is ACKed, go back to PAUSE (new probe)
                        zwp_.state = 0;
                        zwp_.cvOnCountdown.notify_one();
                    } else if (wnd_ > 0) {
                        retransmitQueue.resetZwpCounter();  // Restart the exponential countdown
                    }
                    break;
                }
            }
            

            // std::cerr << "onAck: una_ = " << una_ << "\n";
            // std::cerr << "onAck: nxt_ = " << nxt_ << "\n";
            // std::cerr << "onAck: nbw_ = " << nbw_ << "\n";

            // Unacceptable ACK
            if (ackNum <= una_ || ackNum > nxt_)
                return una_nxt;

            // Acceptable ACK
            una_ = una_nxt.first = ackNum;  // una_ is guaranteed to shift right -> notify writer threads

            // std::cout << "Got valid ACK: now una_ = " << ackNum << ", nxt_ = " << nxt_ << ", sizeFree = " 
            //           << sizeFreeNoLock_() << ", wnd_ = " << wnd_ << "\n";
            // std::cout << "Notifiying writer thread\n";
        }

        cvWriter_.notify_all();  // Acceptable ACK -> notify writers there's more space

        // Any segments on the retransmission queue that are thereby
        // *entirely* acknowledged are removed (3.10.7.4 RFC9293)
        retransmitQueue.onAck(ackNum);

        return una_nxt;
    }

    // TODO: This function currently copies the data from the sendbuffer to the provided buffer and is inefficient.
    // Instead we should return a locked range of the sendbuffer and expect the caller to release the lock after use.
    using SeqLenPair = std::pair<std::uint32_t, std::size_t>;
    auto sendReadyData(const std::span<std::byte> buff) -> tl::expected<SeqLenPair, SocketError>
    {
        std::unique_lock lk{mutex_};
        // if (sizeCanSendNoLock_() == 0) {
        //     std::cout << "sendReadyData(): waiting for data to send (sizeCanSend=0, wnd_=" << wnd_ << ")" << std::endl;
        // }
        cvSender_.wait(lk, [this] { return sizeCanSendNoLock_() > 0 || stopped_; });

        if (stopped_)
            return tl::unexpected{SocketError::CLOSING};

        const auto seq = nxt_;
        const auto n = std::min(sizeCanSendNoLock_(), buff.size());  // Send as many bytes as we can

        [[maybe_unused]] auto nRead = RB::read(buff, nxt_, nxt_ + n-1);
        assert(nRead == n && "Failed to read correct number of bytes from send buffer");

        wnd_ -= static_cast<decltype(wnd_)>(n);  // We shrink the window precautiously (before onAck) to avoid over-sending
        nxt_ += static_cast<decltype(nxt_)>(n);  // Move those bytes to `sent but un-acked`

        // std::cout << "sendReadyData(): sent " << n << " bytes, nxt_ = " << nxt_ 
        //           << ", una_ = " << una_ << ", sizeCanSend = " << sizeCanSendNoLock_() << "\n";

        // No need to notify anyone here, as the sent bytes are still in the queue (moved from ready to unacked)
        return std::make_pair(seq, n);
    }

    auto getSizeUnacked() const { std::lock_guard lk(mutex_); return sizeUnackedNoLock_(); }
    auto getSizeNotSent() const { std::lock_guard lk(mutex_); return sizeNotSentNoLock_(); }
    auto getSizeCanSend() const { std::lock_guard lk(mutex_); return sizeCanSendNoLock_(); }
    auto getSizeFree() const { std::lock_guard lk(mutex_); return sizeFreeNoLock_(); }
    auto getNxt() const { std::lock_guard lk(mutex_); return nxt_; }

    auto getWndEndExclusiveNoLock() const { return una_ + wnd_; }
    auto getWndEndExclusive() const { std::lock_guard lk(mutex_); return getWndEndExclusiveNoLock(); }

    // Dirty ad-hoc stuff for handling handshakes ...
    // because we didn't consider the buffers when coding the handshake
    void writeAndSendOneNoLock() { ++nbw_; ++nxt_; }
    void writeAndSendOne() { std::lock_guard lk(mutex_); writeAndSendOneNoLock(); }

    void shutdown()
    {
        // Notify all threads to exit
        {
            std::lock_guard<std::mutex> lk(mutex_);
            stopped_ = true;
        }
        cvWriter_.notify_all();
        cvSender_.notify_all();

        zwp_.cvOnPause.notify_one();
        zwp_.cvOnCountdown.notify_one();
    }

    struct LockedDataView {
        std::uint32_t seq;
        std::span<const std::byte> data;
        std::unique_lock<std::mutex> lock;

        auto empty() const noexcept { return data.empty(); }
        auto size() const noexcept { return data.size(); }
    };

    // Returns a locked (1 byte) range of the sendbuffer, or a SocketError
    auto zwpGetProbeByte() -> tl::expected<LockedDataView, SocketError>
    {
        static constexpr auto ZWP_TIMEOUT = RetransmissionQueue::RtoEstimator::MIN_RTO * 4;

        std::unique_lock lk{mutex_};

        // Wait until we get a WND = 0
        zwp_.state = 0;  // PAUSE
        zwp_.cvOnPause.wait(lk, [this] { return wnd_ == 0 || stopped_; });
        if (stopped_)
            return tl::unexpected{SocketError::CLOSING};

        // Wait until we get a WND > 0, or ZWP timeout
        // std::cout << "Got WND = 0; ZWP counting down..." << std::endl;
        zwp_.state = 1;  // COUNTDOWN
        const auto pred = zwp_.cvOnCountdown.wait_for(lk, ZWP_TIMEOUT, 
            [this] { return wnd_ > 0 || stopped_; });
        if (stopped_)
            return tl::unexpected{SocketError::CLOSING};

        // If pred, that means we got an ACK with WND > 0, so we go back to pause
        if (pred) {
            zwp_.state = 0;
            return LockedDataView{};
        }

        // Otherwise, WND is still 0 (but we timed out), so send a probe and wait for ACK
        std::cout << "ZWP timeout! Sending probe...\n";

        if (sizeNotSentNoLock_() == 0) {
            zwp_.state = 0;
            std::cout << "Ooops, no data to send, going back to begining\n";
            return LockedDataView{};
        }

        zwp_.state = 2;  // WAITACK

        // Send a probe with 1 byte of new data, if any
        zwp_.seq = nxt_;
        decltype(LockedDataView::data) data{ &RB::at(zwp_.seq), 1 };
        nxt_++;  // Move byte to `sent but un-acked`

        return LockedDataView{zwp_.seq, data, std::move(lk)};
    }

    auto zwpWaitAck(LockedDataView &&ldv) -> tl::expected<void, SocketError>
    {
        // Responds to any new advertised WND by going back to the begining of the loop
        zwp_.cvOnCountdown.wait(ldv.lock, [this] { return zwp_.state == 0 || stopped_; });
        if (stopped_)
            return tl::unexpected{SocketError::CLOSING};

        return {};
    }

    // void zwpRecordRetransmitEntry(RetransmissionQueue::Entry &entry)
    // {
    //     zwp_.retransmitEntry = std::experimental::make_observer(&entry);
    // }

    // void zwpResetRetransmitEntry()
    // {
    //     zwp_.retransmitEntry = nullptr;
    // }

    // void zwpFunction(std::function<void(Packet &&)> packetSender)
    // {
    //     static constexpr auto ZWP_TIMEOUT = std::chrono::seconds{3};
    //     while (true) {
    //         std::unique_lock lk{mutex_};

    //         // Wait until we get a WND=0
    //         zwp_.state = 0;
    //         zwp_.cvOnPause.wait(lk, [this] { return wnd_ == 0 || stopped_; });
    //         if (stopped_) break;

    //         // Wait until we get a WND > 0, or ZWP timeout
    //         std::cout << "ZWP counting down...\n";
    //         zwp_.state = 1;
    //         const auto pred = zwp_.cvOnCountdown.wait_for(lk, ZWP_TIMEOUT, 
    //             [this] { return wnd_ > 0 || stopped_; });
    //         if (stopped_) break;

    //         // If pred, that means we got an ACK with WND > 0, so we go back to pause
    //         // Otherwise, WND is still 0 (but we timed out), so send a probe and wait for ACK
    //         if (!pred) {
    //             std::cout << "ZWP timeout! Sending probe...\n";

    //             if (sizeNotSentNoLock_() == 0) {
    //                 std::cout << "Ooops, no data to send, going back to begining\n";
    //                 zwp_.state = 0;
    //                 continue;
    //             }

    //             // Send a probe with 1 byte of new data, if any
    //             std::byte probeData;
    //             [[maybe_unused]] auto nRead = RB::read(std::span(probeData), nxt_, nxt_);
    //             assert(nRead == 1 && "Failed to read correct number (1) of bytes from send buffer");
    //             nxt_++;  // Move byte to `sent but un-acked`

    //             packetSender(Packet::makeAckPacket(una_, nxt_, wnd_, probeData));

    //             // Responds to any new advertised WND by going back to the begining of the loop
    //             zwp_.state = 2;
    //             zwp_.cvOnCountdown.wait(lk, [this] { return zwp_.state == 0 || stopped_; });
    //             if (stopped_) break;

    //             std::cout << "Got ACK, going back to begining\n";
    //         }
    //     }
    // }

    bool sanityCheckAtStart() const { std::lock_guard lk(mutex_); return una_ == nxt_ && nxt_ == nbw_; }

public:
    RetransmissionQueue retransmitQueue;

private:
    std::size_t sizeUnackedNoLock_() const noexcept { return nxt_ - una_; }
    std::size_t sizeNotSentNoLock_() const noexcept { return nbw_ - nxt_; }
    std::size_t sizeCanSendNoLock_() const noexcept 
    {
        return wnd_ > sizeUnackedNoLock_()
             ? std::min(wnd_ - sizeUnackedNoLock_(), sizeNotSentNoLock_()) : 0;
    }
    std::size_t sizeFreeNoLock_() const noexcept { return N - (nbw_ - una_); }

private:
    /**
     *  [una_, nxt_) is the data *in flight* that might be retransmitted
     *    - Receiving ACKs (?) causes una_ to shift right : onAck()
     *    - Sending data causes nxt_ to shift right (new acks are expected)
     * 
     *  [nxt_, nbw_) is the data available for sending but not yet sent
     *    - Sending data causes nxt_ to shift right (less data available)
     *    - Writing from app causes lbw_ to shift right : write()
     * 
     *  Invariant: una_ <= nxt_ <= nbw_
    */
    std::uint32_t una_ = 0;  // oldest (sent but) unacknowledged sequence number
    std::uint32_t nxt_ = 0;  // next sequence number to be sent
    std::uint32_t nbw_ = 0;  // sequence number of next byte to write from app

    // The window size advertised by remote is the max [una_, nxt_) could get. 
    // Sender must block (cannot increment nxt_) if this size has been reached.

    // Update this when receiving ACKs
    std::uint32_t wnd_ = std::numeric_limits<std::uint32_t>::max();

    mutable std::mutex mutex_;
    mutable std::condition_variable cvSender_;
    mutable std::condition_variable cvWriter_;
    bool stopped_ = false;

    struct ZeroWindowProbing {
        // using RetransmitEntryPtr = 
        //     std::experimental::observer_ptr<RetransmissionQueue::Entry>;

        int state = 0;
        // std::mutex mutex;
        
        // RetransmitEntryPtr retransmitEntry;
        std::uint32_t seq;
        mutable std::condition_variable cvOnPause, cvOnCountdown;

        void onWnd(std::uint32_t wnd)
        {
            // switch (state) {
            // case 0:
            //     if (wnd == 0) {
            //         // std::cout << "Got zero window ack (" << ackNum << ") " << wnd_ << " in state 0\n";
            //         cvOnPause.notify_one();
            //     }
            //     break;
            // case 1:
            //     // std::cout << "Got ack (" << ackNum << ") with WND = " << wnd_ << " in state 1\n";
            //     cvOnCountdown.notify_one();
            //     break;
            // case 2:
            //     // std::cout << "Got ack (" << ackNum << ") with WND = " << wnd_ << " in state 2\n";
            //     state = 0;
            //     cvOnCountdown.notify_one();
            //     break;
            // }
        }
    } zwp_;
};


template <std::size_t N>
class RecvBuffer : public RingBuffer<std::byte, N> {
public:
    using RB = RingBuffer<std::byte, N>;

    RecvBuffer() = default;
    RecvBuffer(std::uint32_t initRecvNxt) noexcept
        : nbr_(initRecvNxt), nxt_(initRecvNxt) 
    {}

    ~RecvBuffer() { shutdown(); std::cout << "RecvBuffer DESTRUCTED\n"; }

    void setPointersNoLock(std::uint32_t initRecvNxt) noexcept
    {
        nbr_ = initRecvNxt;
        nxt_ = initRecvNxt;
    }

    /**
     * @brief Read up to `n` bytes into the provided buffer.
     * 
     * Blocks if the receive buffer is empty.
     * 
     * @param buff The buffer to read into
     * @param n The maximum number of bytes to read
     * @return The number of bytes actually read, or a SocketError
     */
    tl::expected<std::size_t, SocketError> readAtMostNBytes(const std::span<std::byte> buff, std::size_t n)
    {
        if (n == 0) return 0;

        std::unique_lock lk(mutex_);
        cv_.wait(lk, [this] { return sizeToReadNoLock_() > 0 || stopped_; });

        if (stopped_)
            return tl::unexpected{SocketError::CLOSING};
        
        // std::cout << "n = " << n << ", sizeToRead = " << sizeToReadNoLock_() << std::endl;
        n = std::min(n, sizeToReadNoLock_());

        [[maybe_unused]] const auto nRead = RB::read(buff, nbr_, nbr_ + n-1);
        assert(nRead == n && "Failed to read correct number of bytes from recv buffer");

        // std::cout << "old wnd = " << sizeFreeNoLock_() << std::endl;
        nbr_ += static_cast<decltype(nbr_)>(n);
        // std::cout << "new wnd = " << sizeFreeNoLock_() << std::endl;
        return n;
    }

    auto getSizeToRead() const { std::lock_guard lk(mutex_); return sizeToReadNoLock_(); }
    auto getSizeFree() const { std::lock_guard lk(mutex_); return sizeFreeNoLock_(); }
    auto getNxt() const { std::lock_guard lk(mutex_); return nxt_; }

    void shutdown()
    {
        // Notify all threads to exit
        {
            std::lock_guard<std::mutex> lk(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

    bool sanityCheckAtStart() const { std::lock_guard lk(mutex_); return nbr_ == nxt_; }

private:
    std::size_t sizeToReadNoLock_() const noexcept { return nxt_ - nbr_; }
    std::size_t sizeFreeNoLock_() const noexcept { return N - sizeToReadNoLock_(); }  // The advertised "recv window size"

private:
    /**
     * [nbr_, nxt_) is the (acked) data available for reading. It is contiguous (in order).
     *  - Receiving in-order data causes nxt_ to shift right (more data available)
     *  - Reading data causes nbr_ to shift right (less data available)
    */
    std::uint32_t nbr_ = 0; // sequence number of next byte to read from recv buffer (one past LBR)
    std::uint32_t nxt_ = 0; // Next sequence number expected to receive

    RightOpenIntervalSet<std::uint32_t> earlyArrivals_;

    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    bool stopped_ = false;

public:
    // Handles an incoming control segment (e.g., FIN) with sequence number `seqNum`
    // Returns a pair of RCV.NXT, RCV.WND (the ACK, WND we should ack back)
    auto onCtrl(std::uint32_t seqNum)
    {
        std::lock_guard lk(mutex_);
        if (seqNum == nxt_) nxt_++;
        return std::make_pair(nxt_, sizeFreeNoLock_());
    }

    // Handles an incoming segment with sequence number `seqNum` and payload `payload`
    // Returns a pair of RCV.NXT, RCV.WND (the ACK, WND we should ack back)
    auto onRecv(std::uint32_t seqNum, PayloadView payload)
    {
        assert(!payload.empty() && "Payload should not be empty");

        std::pair<decltype(nxt_), decltype(sizeFreeNoLock_())> ack_wnd{};
        auto &[ack, wnd] = ack_wnd;

        {
            std::lock_guard lk(mutex_);

            ack = nxt_;
            wnd = sizeFreeNoLock_();

            // Handle early arrival
            if (seqNum > nxt_) {
                const auto nWritten = static_cast<std::uint32_t>(
                    RB::write(payload, seqNum, nbr_ + N-1)
                );
                if (nWritten > 0) {
                    // Insert the new segment and merge all overlapping segments
                    earlyArrivals_.emplaceMerge({seqNum, seqNum + nWritten});
                }
                return ack_wnd;
            }

            // No new data
            auto offset = nxt_ - seqNum;
            if (offset >= payload.size())
                return ack_wnd;

            // Trim old data in payload, then write the rest to the buffer
            const auto nWritten = static_cast<std::uint32_t>(
                RB::write(payload.subspan(offset), nxt_, nbr_ + N-1)
            );

            if (nWritten == 0)
                return ack_wnd;
            
            // Merge and remove early arrival segments
            // ... returns one past last byte of the merged interval, which should be the new nxt_
            nxt_ = earlyArrivals_.mergeRemove({seqNum, seqNum + nWritten});

            ack  = nxt_;
            wnd -= nWritten;
        }

        cv_.notify_one();  // Tell waiting READER thread there is data to read

        return ack_wnd;
    }

    auto getAckWnd() const
    {
        std::lock_guard lk(mutex_);
        return std::make_pair(nxt_, sizeFreeNoLock_());
    }
};



} // namespace tcp
} // namespace tns
