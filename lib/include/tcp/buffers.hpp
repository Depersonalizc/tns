#pragma once

#include <cassert>
#include <array>
#include <span>
#include <mutex>
#include <condition_variable>
#include <iostream>

#include "util/defines.hpp"
#include "util/tl/expected.hpp"
#include "tcp/socket_error.hpp"


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
    std::size_t write(const std::span<const T> data, const std::size_t at, const std::size_t last);

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
    std::size_t read(const std::span<T> buff, const std::size_t at, const std::size_t last) const;

    static constexpr std::size_t max_size() noexcept;

protected:
    std::array<T, N> buf_;
protected:
    static constexpr std::size_t idx(std::size_t seq) noexcept;
};


template <std::size_t N>
class SendBuffer : public RingBuffer<std::byte, N> {
public:
    using RB = RingBuffer<std::byte, N>;

    SendBuffer() = default;
    SendBuffer(std::uint32_t initSeqNum, std::uint32_t windowSize) noexcept;
    
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
    tl::expected<std::size_t, SocketError> write(const std::span<const T> data);

    /**
     * @brief Handle the acknowledgement of data.
     * 
     * This function is called when an acknowledgement (ACK) for a certain amount of data is received.
     * It updates the unacknowledged data pointer (una_) if the received ACK number is within the expected range.
     * After updating, it notifies all waiting (write) threads.
     * 
     * @param ackNum The number of the received acknowledgement
     * @param wndSize The window size advertised by the remote
     */
    void onAck(std::uint32_t ackNum, std::uint32_t wndSize);

    /**
     * @brief Send data from the send buffer to the network layer.
     * 
     * This function is called when the network layer is ready to send data.
     * It reads data from the send buffer into a byte buffer.
     * 
     * @param buff The buffer to write to
     * @param n The number of bytes to write
     * @return std::size_t The number of bytes actually written
     */
    std::size_t sendNBytes(const std::span<std::byte> buff, std::size_t n);

    auto sendReadyData(const std::span<std::byte> buff);

    auto sizeUnacked() const;
    auto sizeNotSent() const;
    auto sizeCanSend() const;
    auto sizeFree() const;

    auto getNxtNoLock() const;
    auto getNxt() const;
    void setWnd(std::uint32_t wnd);
    bool sanityCheckAtStart() const;

    // Dirty ad-hoc stuff for handling handshakes ...
    // because we didn't consider the buffers when coding the handshake
    void writeAndSendOneNoLock();
    void writeAndSendOne();

private:
    std::size_t sizeUnackedNoLock_() const noexcept;
    std::size_t sizeNotSentNoLock_() const noexcept;
    std::size_t sizeCanSendNoLock_() const noexcept;
    std::size_t sizeFreeNoLock_() const noexcept;

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

    // Update this when receiving ACKs (setWnd)
    std::uint32_t wnd_ = std::numeric_limits<std::uint32_t>::max();

    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
};


template <std::size_t N>
class RecvBuffer : public RingBuffer<std::byte, N> {
public:
    using RB = RingBuffer<std::byte, N>;

    RecvBuffer() = default;
    RecvBuffer(std::uint32_t initRecvNxt) noexcept;

    void setPointersNoLock(std::uint32_t initRecvNxt) noexcept;

    /**
     * @brief Read up to `n` bytes into the provided buffer.
     * 
     * Blocks if the buffer is empty.
     * 
     * @param buff The buffer to read into
     * @param n The maximum number of bytes to read
     * @return The number of bytes actually read, or a SocketError
     */
    tl::expected<std::size_t, SocketError> readAtMostNBytes(const std::span<std::byte> buff, std::size_t n);

    auto sizeToRead() const;
    auto sizeFree() const;

    auto getNxtNoLock() const;
    auto getNxt() const;

    bool sanityCheckAtStart() const;

private:
    std::size_t sizeToReadNoLock_() const noexcept;
    std::size_t sizeFreeNoLock_() const noexcept;  // The advertised "recv window size"

private:
    /**
     * [nbr_, nxt_) is the (acked) data available for reading. It is contiguous (in order).
     *  - Receiving in-order data causes nxt_ to shift right (more data available)
     *  - Reading data causes nbr_ to shift right (less data available)
    */
    std::uint32_t nbr_ = 0; // sequence number of next byte to read from recv buffer (one past LBR)
    std::uint32_t nxt_ = 0; // Next sequence number expected to receive

    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;

public:
    // Handles an incoming segment with sequence number `seqNum` and payload `payload`
    // Returns a pair of ACK, WND we should send back
    auto onRecv(std::size_t seqNum, PayloadView payload);

    auto getAckWnd() const;
};



} // namespace tcp
} // namespace tns
