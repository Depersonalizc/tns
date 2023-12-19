#pragma once

#include "tcp/constants.hpp"
#include "tcp/packet.hpp"
#include "util/tl/expected.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <mutex>
#include <ranges>
#include <functional>


namespace tns {
namespace tcp {

class RetransmissionQueue {
public:
    using SC = std::chrono::steady_clock;
    struct Entry {
        Entry() = default;
        Entry(Packet packet, SC::time_point lastSent) noexcept : packet{std::move(packet)}, lastSent{lastSent} {}
        void resetCounter() noexcept { counter = 0; }
        void refreshTimestamp() noexcept { lastSent = SC::now(); }
        void refreshTimestamp(const auto &now) noexcept { lastSent = now; }

        auto getEndExclusive() const noexcept { return packet.getSeqNumHost() + packet.getPayloadSize(); }
        auto getRtt(const auto &now) const noexcept { return now - lastSent; }
        auto hasNotResent() const noexcept { return counter == 0; }
        auto hasExpired(const auto &now, const auto &rto) const noexcept { return getRtt(now) > rto; }
        auto hasExpiredExponential(const auto &now, const auto &rto) const noexcept { return getRtt(now) > rto * std::exp2(counter); }

        Packet packet;
        SC::time_point lastSent;
        std::size_t counter = 0;
    };

    using EntryRef = std::reference_wrapper<Entry>;
    using LockedEntryRef  = std::pair<std::unique_lock<std::mutex>, EntryRef>;
    using LockedEntryRefs = std::pair<std::unique_lock<std::mutex>, std::vector<EntryRef>>;



    // Remove all packets from the queue that are *entirely* acknowledged by the `ack` number.
    // i.e., those whose sequence number + payload size is <= `ack`.
    // i.e., those before the first packet whose sequence number + payload size is > `ack`.
    void onAck(std::uint32_t ack)
    {
        using namespace std;
        const auto now = SC::now();

        // cout << "onack!\n";
        lock_guard<mutex> lk(mutex_);

        // Regular entries
        const auto firstUnacked = ranges::upper_bound(deque_, ack, {}, &Entry::getEndExclusive);
        for (const auto &entry : ranges::subrange(deque_.begin(), firstUnacked)
                               | views::filter(&Entry::hasNotResent)) {
            rto.addRttSample(entry.getRtt(now));
        }

        deque_.erase(deque_.begin(), firstUnacked);
        // cout << "retransmission queue size after ACK: " << deque_.size() << '\n';

        // Zero Window Probe entry
        if (zwpEntry_ && zwpEntry_->getEndExclusive() <= ack) {
            zwpEntry_.reset();
        }
    }

    [[nodiscard]] LockedEntryRef enqueue(Packet packet)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        // Packet must be enqueued in order
        assert((deque_.empty() || packet.getSeqNumHost() >= deque_.back().getEndExclusive())
                && "Packet enqueued to the retransmission queue is out of order");

        // std::cout << "retransmission queue size before enqueue: " << deque_.size() << '\n';
        deque_.emplace_back(std::move(packet), SC::now());
        // std::cout << "retransmission queue size after enqueue: " << deque_.size() << '\n';

        return std::make_pair(std::move(lk), std::ref(deque_.back()));
    }

    [[nodiscard]] LockedEntryRef enqueueZwp(Packet packet)
    {
        std::unique_lock<std::mutex> lk(mutex_);
        assert(!zwpEntry_ && "ZWP already exists in the retransmission queue");

        zwpEntry_ = Entry{std::move(packet), SC::now()};

        return std::make_pair(std::move(lk), std::ref(*zwpEntry_));
    }

    [[nodiscard]] tl::expected<LockedEntryRefs, SocketError>
    getExpiredEntries(std::uint32_t rightWindowEdge = 0)
    {
        using namespace std;
        const auto now = SC::now();
        const auto rtoEst = rto.get();
        vector<EntryRef> expiredEntries;

        unique_lock<mutex> lk(mutex_);

        /**
         * If this (shrunk window, i.e., right window edge moved to the left) happens, the sender SHOULD NOT send new data (SHLD-15), 
         * but SHOULD retransmit normally the old unacknowledged data between SND.UNA and SND.UNA+SND.WND (SHLD-16). 
         * The sender MAY also retransmit old data beyond SND.UNA+SND.WND (MAY-7), but SHOULD NOT time out the 
         * connection if data beyond the right window edge is not acknowledged (SHLD-17). 
         * If the window shrinks to zero, the TCP implementation MUST probe it in the standard way (described below) (MUST-35).
        */
        auto expEntries = views::all(deque_)
                        | views::filter([&](auto &entry) {
                            return entry.hasExpired(now, rtoEst)
                                && entry.getEndExclusive() <= rightWindowEdge; });  // or should it be `<` ? Not sure.

        // Regular entries
        for (auto &expEntry : expEntries) {
            if (++expEntry.counter > MAX_RETRANSMISSIONS) {
                std::cout << "entry (seq=" << expEntry.packet.getSeqNumHost() << ", len=" << expEntry.packet.getPayloadSize()
                          << ") has been retransmitted " << MAX_RETRANSMISSIONS << " times, giving up\n";
                std::cout << "retransmission queue size: " << deque_.size() << '\n';
                return tl::unexpected(SocketError::TIMEOUT);
            }
            
            cout << "Retransmitting packet (seq: " << expEntry.packet.getSeqNumHost() << ", len: " << expEntry.packet.getPayloadSize()
                 << ", retry #" << expEntry.counter << ", RTO = " << rtoEst.count() << "ms)\n";
            expEntry.refreshTimestamp(now);
            expiredEntries.emplace_back(expEntry);
        }

        // Zero Window Probe entry
        if (zwpEntry_ && zwpEntry_->hasExpiredExponential(now, rtoEst)) {
            ++zwpEntry_->counter;

            cout << "Retransmitting ZWP (seq: " << zwpEntry_->packet.getSeqNumHost() 
                 << ", len: " << zwpEntry_->packet.getPayloadSize()
                 << ", retry #" << zwpEntry_->counter << ", RTO = " << rtoEst.count() << "ms)\n";
            zwpEntry_->refreshTimestamp(now);
            expiredEntries.emplace_back(*zwpEntry_);
        }

        if (expiredEntries.empty())
            return make_pair(decltype(lk){}, decltype(expiredEntries){});

        return make_pair(move(lk), move(expiredEntries));
    }

    void resetZwpCounter() noexcept
    {
        if (zwpEntry_)
            zwpEntry_->resetCounter();
    }

    // void refreshAll() noexcept
    // {
    //     const auto now = SC::now();
    //     std::lock_guard<std::mutex> lk(mutex_);
    //     for (auto &entry : deque_) {
    //         entry.refreshTimestamp(now);
    //         entry.resetCounter();
    //     }
    // }

    struct RtoEstimator {
    public:
        using msec = std::chrono::milliseconds;
        static constexpr auto MIN_RTO = msec{500};   // minimum RTO (500 ms)
        static constexpr auto MAX_RTO = msec{1000};  // maximum RTO (1 second)
        static constexpr auto ALPHA   = 0.875;       // "smoothing factor"
        static constexpr auto BETA    = 1.5;         // "delay factor"

        const msec &get() const noexcept
        {
            // return _rto_dbg_;  // hard coded 10s RTO for DEBUGGING EARLY ARRIVAL ONLY
            return rto_;
        }

        void addRttSample(const auto &rtt) noexcept
        {
            using std::chrono::duration_cast;
            srtt_ = duration_cast<msec>(ALPHA * srtt_ + (1 - ALPHA) * rtt);
            rto_  = std::clamp(duration_cast<msec>(BETA * srtt_), MIN_RTO, MAX_RTO);
            // std::cout << "added rtt sample " << duration_cast<msec>(rtt).count() << "ms, SRTT = " 
            //           << srtt_.count() << "ms, new RTO = " << rto_.count() << "ms\n";
        }

    private:
        msec srtt_{300};     // "smoothed round-trip time"
        msec  rto_{500};     // "retransmission timeout"
        // msec  _rto_dbg_{10000};  // hard coded 10s RTO for DEBUGGING EARLY ARRIVAL ONLY
    } rto;

private:
    std::deque<Entry> deque_;
    std::optional<Entry> zwpEntry_;
    std::mutex mutex_;  // Separate mutex for the retransmission queue
};


} // namespace tcp
} // namespace tns