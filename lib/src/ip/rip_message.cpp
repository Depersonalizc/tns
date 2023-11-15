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
    ::THROW_NO_IMPL();
}

RipMessage::RipMessage(PayloadView payload)  // TODO: This most certainly has memory leak hazards haha
{
    ::THROW_NO_IMPL();
}

} // namespace ip
} // namespace tns
