#include "tcp/states.hpp"
#include "tcp/packet.hpp"
#include "tcp/session_tuple.hpp"


namespace tns {
namespace tcp {

tl::expected<events::Variant, std::string>
events::fromPacket(const Packet &packet, const SessionTuple &session)
{
    ::THROW_NO_IMPL();
}


} // namespace tcp
} // namespace tns