#include "tcp/states.hpp"
#include "tcp/packet.hpp"
#include "tcp/session_tuple.hpp"


namespace tns {
namespace tcp {

tl::expected<events::Variant, std::string>
events::fromPacket(const Packet &packet, const SessionTuple &session)
{
    const auto seq = packet.getSeqNumHost();
    const auto wnd = packet.getWndSizeHost();
    switch (packet.getFlags()) {
        case TH_SYN:
            return GetSyn{ session, seq, packet.getWndSizeHost() };
        case TH_SYN | TH_ACK:
            return GetSynAck{ seq, packet.getAckNumHost(), wnd };
        case TH_ACK:
            return GetAck{ seq, packet.getAckNumHost(),
                           wnd, packet.getPayloadView() };
        case TH_FIN:
            return GetFin{ seq, wnd };
        case TH_FIN | TH_ACK:
            return GetFinAck{ seq, packet.getAckNumHost(), wnd };
        default:
            return tl::unexpected("Unsupported packet flags");
    }
}


} // namespace tcp
} // namespace tns