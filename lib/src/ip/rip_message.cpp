#include "ip/rip_message.hpp"
#include "ip/datagram.hpp"
#include "ip/address.hpp"
#include "util/util.hpp"

#include <limits>


namespace tns {
namespace ip {

RipMessage::RipMessage(Command command, RipMessage::Entries entries, OptionalAddresses learnedFrom)
    : command_(command), learnedFrom_(std::move(learnedFrom))
{
    // Check if the number of entries is less than limit of uint16_t
    if (entries.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("RipMessage::RipMessage() : "
                                    "Number of entries is greater than limit of uint16_t");
    }

    if (entries.size() != learnedFrom_.size()) {
        throw std::invalid_argument("RipMessage::RipMessage() : "
                                    "entries.size() != learnedFrom.size()");
    }

    numEntries_ = static_cast<std::uint16_t>(entries.size());
    entries_ = std::move(entries);
}

RipMessage::RipMessage(PayloadView payload)  // TODO: This most certainly has memory leak hazards haha
{
    using tns::util::ntoh;

    auto p = payload.data();

    // The first 2 bytes are the command
    command_ = Command{ ntoh(*reinterpret_cast<const std::uint16_t *>(p)) };
    p += sizeof(std::uint16_t);

    // The next 2 bytes are the number of entries
    numEntries_ = ntoh(*reinterpret_cast<const std::uint16_t *>(p));
    p += sizeof(std::uint16_t);

    // std::cout << "++++++++++++++++++++++++++++\n";
    // std::cout << "RipMessage: Command = " << static_cast<uint16_t>(command_) << ", numEntries = " << numEntries_ << "\n";

    // The rest are the entries
    for (std::uint16_t i = 0; i < numEntries_; i++, p += sizeof(RipMessage::Entry)) {
        auto newCost = ntoh(*reinterpret_cast<const std::uint32_t *>(p)) + 1;
        entries_.emplace_back(
            std::min(newCost, INFINITY),  // cost
            ntoh(*reinterpret_cast<const std::uint32_t *>(p + sizeof(std::uint32_t) * 1)),  // address
            ntoh(*reinterpret_cast<const std::uint32_t *>(p + sizeof(std::uint32_t) * 2))   // mask
        );

        // auto et = entries_.back();
        // std::cout << "RipMessage: Entry " << i 
        //           << ": cost = " << et.cost 
        //           << ", address = " << Ipv4Address(util::hton<std::uint32_t>(et.address)) 
        //           << ", mask len = " << util::ip::subnetMaskLength(et.mask) << "\n";
    }

    // std::cout << "++++++++++  RipMessage constructed!  +++++++++++\n";
}

} // namespace ip
} // namespace tns
