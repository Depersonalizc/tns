#include "tcp/sockets.hpp"
#include "tcp/packet.hpp"

#include <iomanip>

/* Visitor functors for Socket */

namespace tns {
namespace tcp {

// WriteInfo
void WriteInfo::operator()(const NormalSocket &sock, std::ostream &os) const
{
    ::THROW_NO_IMPL();
}

void WriteInfo::operator()(const ListenSocket &lSock, std::ostream &os) const
{
    ::THROW_NO_IMPL();
}

} // namespace tcp
} // namespace tns
