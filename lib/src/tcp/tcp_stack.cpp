#include "tcp/tcp_stack.hpp"

namespace tns {
namespace tcp {

/******************************************** Normal Socket ********************************************/
// CAUTION: Do not modify the state of the sock before using the input state parameter.
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::SynSent &synSent, const events::GetSynAck &getSynAck)
{
    ::THROW_NO_IMPL();
}

void TcpStack::eventHandler_(
    NormalSocket &sock, const states::SynReceived &synRecv, const events::GetAck &getAck)
{
    ::THROW_NO_IMPL();
}

void TcpStack::eventHandler_(
    NormalSocket &sock, const states::Established &/*estab*/, const events::GetAck &getAck)    // Process incoming segment
{
    ::THROW_NO_IMPL();
}


/********************************************* Listen Socket *********************************************/

// Handle a SYN packet event
void TcpStack::eventHandler_(ListenSocket &lSock, const events::GetSyn &getSyn)
{
    ::THROW_NO_IMPL();
}

} // namespace tcp
} // namespace tns
