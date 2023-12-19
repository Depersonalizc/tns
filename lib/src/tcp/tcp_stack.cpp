#include "tcp/tcp_stack.hpp"

namespace tns {
namespace tcp {

/******************************************** Normal Socket ********************************************/
// CAUTION: Do not modify the state of the sock before using the input state parameter.

// SYN-SENT ----SYN+ACK/ACK----> ESTABLISHED
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::SynSent &synSent, const events::GetSynAck &synAck)
{
    std::cout << "Normal socket " << sock.id_ 
              << " (SYN_SENT): Got SYN-ACK (seq=" << synAck.serverISN 
              << ", ack=" << synAck.ackNum
              << ", wnd=" << synAck.serverWND
              << ") from " << sock.tuple_.remote.toString() << "\n";

    // Check validity of ACK number
    const auto &[una, nxt] = sock.sendBuffer_.onAck(synAck.ackNum, synAck.serverWND);
    if (una != nxt) {
        // Unacceptable ACK, should send RST but we don't care about it for now
        std::stringstream ss;
        ss << "Normal socket " << sock.id_ << " (SYN_SENT): Got SYN-ACK with wrong ACK number. Expected " 
           << nxt << ", got " << synAck.ackNum << " instead\n";
        std::cout << ss.str();
        return;
    }

    // Sleep for a little while so the other side is less likely to miss the ACK packet
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto ack = synAck.serverISN + 1;    // will be RCV.NXT
    sock.recvBuffer_.setPointersNoLock(ack);

    // Send ACK packet to the remote
    const auto wnd = static_cast<std::uint16_t>(sock.recvBuffer_.getSizeFree());
    sendPacket(Packet::makeAckPacket(sock.tuple_, nxt, ack, wnd), sock.tuple_.remote);

    // Wake up the caller of connect()
    // Note that the caller will get a socket in state *SynSent*, not Established
    synSent.onSynAck();
}

// SYN-RECEIVED ----ACK/----> ESTABLISHED
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::SynReceived &synRecv, const events::GetAck &getAck)
{
    // Check validity of SEQ number (?)
    if (const auto recvNxt = sock.recvBuffer_.getNxt(); getAck.seqNum != recvNxt) {
        std::stringstream ss;
        ss << "Normal socket " << sock.id_ << " (SYN_RECEIVED): Got ACK with wrong SEQ number. Expected " 
           << recvNxt << ", got " << getAck.seqNum << " instead\n";
        std::cout << ss.str();
        return;
    }

    // Check validity of ACK number
    if (const auto &[una, nxt] = sock.sendBuffer_.onAck(getAck.ackNum, getAck.wndSize); una != nxt) {
        // Unacceptable ACK, should send RST but we don't care about it for now
        std::stringstream ss;
        ss << "Normal socket " << sock.id_ << " (SYN_RECEIVED): Got ACK with wrong ACK number. Expected " 
           << nxt << ", got " << getAck.ackNum << " instead\n";
        std::cout << ss.str();
        return;
    }

    // Potentially: increment SND.UNA, remove ack'd segments from retransmission queue
    // sock.sendBuffer_.onAck(getAck.ackNum, getAck.wndSize);

    std::cout << "Normal socket " << sock.id_ 
              << " (SYN_RECEIVED): Got ACK (seq=" << getAck.seqNum << ", ack=" << getAck.ackNum
              << ") from " << sock.tuple_.remote.toString() << ", connection established!\n";

    assert(sock.sendBuffer_.sanityCheckAtStart() && "FATAL: sendBuffer_ failed sanity check");
    assert(sock.recvBuffer_.sanityCheckAtStart() && "FATAL: recvBuffer_ failed sanity check");

    auto &lSock = synRecv.lSock.get();    // Copy the reference to the listen socket
    sock.state_ = states::Established{};  // Transition the socket state to ESTABLISHED (TODO: race condition?)

    [[maybe_unused]] auto ok = lSock.pendingSocks_.remove(sock.tuple_);
    assert(ok && "FATAL: Normal socket not found in pending list");
    lSock.acceptQ_.pushAndNotify(sock);
}

// ESTBALISHED ----ACK/(ACK)----> ESTABLISHED
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::Established &, const events::GetAck &getAck)
{
    // std::cout << "Normal socket " << sock.id_ 
    //           << ": Got ACK (seq=" << getAck.seqNum << ", ack=" << getAck.ackNum << ", data=" << getAck.payload.size()
    //           << ") from " << sock.tuple_.remote.toString() << "\n";

    // Potentially: increment SND.UNA, remove ack'd segments from retransmission queue
    const auto &[_, nxt] = sock.sendBuffer_.onAck(getAck.ackNum, getAck.wndSize);  // locks sendBuffer_

    if (getAck.payload.size() > 0) {
        // If there's data, reply ACK (no retransmission)
        const auto &[ack, wnd] = sock.recvBuffer_.onRecv(getAck.seqNum, getAck.payload);  // locks recvBuffer_
        sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));
    }
}

// ESTBALISHED ----SYN+ACK(Retransmitted)/ACK----> ESTABLISHED
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::Established &, const events::GetSynAck &synAck)
{
    // TODO: SEQ and ACK numbers could be wrong, check them?

    const auto &[___, nxt] = sock.sendBuffer_.onAck(synAck.ackNum, synAck.serverWND);  // TODO: Should we do this?
    const auto &[ack, wnd] = sock.recvBuffer_.getAckWnd();

    std::cout << "Normal socket " << sock.id_ 
              << " (ESTABLISHED): Got retransmitted SYN-ACK (seq=" << synAck.serverISN 
              << ", ack=" << synAck.ackNum
              << ", wnd=" << synAck.serverWND
              << ") from " << sock.tuple_.remote.toString() << "\n";
    std::cout << "Replying ACK (seq=" << nxt << ", ack=" << ack
              << ", wnd=" << wnd
              << ") to " << sock.tuple_.remote.toString() << "\n";

    sendPacket(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)), sock.tuple_.remote);
}

// ESTBALISHED ----FIN/ACK----> CLOSE_WAIT
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::Established &, const events::GetFin &getFin)
{
    // const auto seq = sock.sendBuffer_.getNxt();
    const auto &[___, nxt] = sock.sendBuffer_.onAck(0, getFin.wndSize);  // ACK nothing, just update window size
    const auto &[ack, wnd] = sock.recvBuffer_.onCtrl(getFin.seqNum);

    std::cout << "Normal socket " << sock.id_ 
              << " (ESTABLISHED): Got FIN (seq=" << getFin.seqNum
              << ") from " << sock.tuple_.remote.toString() << "\n";
    std::cout << "Replying ACK (seq=" << nxt << ", ack=" << ack
              << ", wnd=" << wnd << ") to " << sock.tuple_.remote.toString() << "\n";

    // Send ACK packet to the remote (No retransmission)
    sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));

    // Transition the socket state to CLOSE_WAIT, if the FIN is not an early arrival
    if (ack == getFin.seqNum + 1) {
        sock.shutdownRecv_();
        sock.state_ = states::CloseWait{};
        
        std::stringstream ss;
        ss << "Normal socket " << sock.id_ << ": Transitioned to CLOSE_WAIT\n";
        std::cout << ss.str();
    }
}

// ESTBALISHED ----FIN+ACK/ACK----> CLOSE_WAIT
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::Established &, const events::GetFinAck &finAck)
{
    const auto &[___, nxt] = sock.sendBuffer_.onAck(0, finAck.wndSize);  // ACK nothing, just update window size
    const auto &[ack, wnd] = sock.recvBuffer_.onCtrl(finAck.seqNum);

    std::cout << "Normal socket " << sock.id_ 
              << " (ESTABLISHED): Got FIN-ACK (seq=" << finAck.seqNum << ", ack=" << finAck.ackNum
              << ") from " << sock.tuple_.remote.toString() << "\n";
    std::cout << "Replying ACK (seq=" << nxt << ", ack=" << ack
              << ", wnd=" << wnd << ") to " << sock.tuple_.remote.toString() << "\n";

    // Send ACK packet to the remote (No retransmission)
    sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));

    // Transition the socket state to CLOSE_WAIT, if the FIN is not an early arrival
    if (ack == finAck.seqNum + 1) {
        sock.shutdownRecv_();
        sock.state_ = states::CloseWait{};
        
        std::stringstream ss;
        ss << "Normal socket " << sock.id_ << ": Transitioned to CLOSE_WAIT\n";
        std::cout << ss.str();
    }
}

// CLOSE-WAIT ----ACK/(ACK)----> CLOSE-WAIT   (data)
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::CloseWait &, const events::GetAck &getAck)
{
    // Potentially: increment SND.UNA, remove ack'd segments from retransmission queue
    std::cout << "Normal socket " << sock.id_
              << " (CLOSE_WAIT): Got ACK (seq=" << getAck.seqNum << ", ack=" << getAck.ackNum 
              << ", data=" << getAck.payload.size()
              << ") from " << sock.tuple_.remote.toString() << "\n";

    const auto &[_, nxt] = sock.sendBuffer_.onAck(getAck.ackNum, getAck.wndSize);  // locks sendBuffer_

    if (getAck.payload.size() > 0) {
        // If there's data, reply ACK (no retransmission)
        const auto &[ack, wnd] = sock.recvBuffer_.onRecv(getAck.seqNum, getAck.payload);  // locks recvBuffer_
        sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));
    }
}

// CLOSE-WAIT ----FIN(Retransmitted)/ACK----> CLOSE-WAIT
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::CloseWait &, const events::GetFin &getFin)
{
    // const auto seq = sock.sendBuffer_.getNxt();
    const auto &[___, nxt] = sock.sendBuffer_.onAck(0, getFin.wndSize);  // ACK nothing, just update window size
    const auto &[ack, wnd] = sock.recvBuffer_.onCtrl(getFin.seqNum);

    std::cout << "Normal socket " << sock.id_ 
              << " (CLOSE_WAIT): Got retransmitted FIN (seq=" << getFin.seqNum
              << ") from " << sock.tuple_.remote.toString() << "\n";
    std::cout << "Replying ACK (seq=" << nxt << ", ack=" << ack
              << ", wnd=" << wnd << ") to " << sock.tuple_.remote.toString() << "\n";

    // Send ACK packet to the remote (No retransmission)
    sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));
}



// FIN-WAIT-1 ----ACK/----> FIN-WAIT-2
// FIN-WAIT-1 ----ACK/(ACK)----> FIN-WAIT-1  (data)
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::FinWait1 &, const events::GetAck &getAck)
{
    // Potentially: increment SND.UNA, remove ack'd segments from retransmission queue
    const auto &[una, nxt] = sock.sendBuffer_.onAck(getAck.ackNum, getAck.wndSize);  // locks sendBuffer_

    if (getAck.payload.size() > 0) {
        // If there's data, reply ACK (no retransmission)
        const auto &[ack, wnd] = sock.recvBuffer_.onRecv(getAck.seqNum, getAck.payload);  // locks recvBuffer_
        sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));
        return;
    }

    // Check validity of SEQ number
    if (const auto recvNxt = sock.recvBuffer_.getNxt(); getAck.seqNum != recvNxt) {
        std::stringstream ss;
        ss << "Normal socket " << sock.id_ << " (FIN_WAIT_1): Got ACK with wrong SEQ number. Expected " 
           << recvNxt << ", got " << getAck.seqNum << " instead\n";
        std::cout << ss.str();
        return;
    }

    // Check validity of ACK number
    if (una != nxt) {
        // Unacceptable ACK
        std::stringstream ss;
        ss << "Normal socket " << sock.id_ << " (FIN_WAIT_1): Got ACK with wrong ACK number. Expected " 
           << nxt << ", got " << getAck.ackNum << " instead\n";
        std::cout << ss.str();
        return;
    }

    std::cout << "Normal socket " << sock.id_ 
              << " (FIN_WAIT_1): Got ACK (seq=" << getAck.seqNum << ", ack=" << getAck.ackNum
              << ") from " << sock.tuple_.remote.toString() << "\n";
    
    // Transition the socket state to FIN_WAIT_2
    sock.state_ = states::FinWait2{};

    std::cout << "Normal socket " << sock.id_ << ": Transitioned to FIN_WAIT_2\n";
}

// FIN-WAIT-2 ----FIN/ACK----> TIME-WAIT
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::FinWait2 &, const events::GetFin &getFin)
{
    const auto &[___, nxt] = sock.sendBuffer_.onAck(0, getFin.wndSize);  // ACK nothing, just update window size
    const auto &[ack, wnd] = sock.recvBuffer_.onCtrl(getFin.seqNum);

    std::cout << "Normal socket " << sock.id_
              << " (FIN_WAIT_2): Got FIN (seq=" << getFin.seqNum
              << ") from " << sock.tuple_.remote.toString() << "\n";
    std::cout << "Replying ACK (seq=" << nxt << ", ack=" << ack
              << ", wnd=" << wnd << ") to " << sock.tuple_.remote.toString() << "\n";

    // Send ACK packet to the remote (No retransmission)
    sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));

    // Transition the socket state to TIME_WAIT, if the FIN is not an early arrival
    if (ack == getFin.seqNum + 1) {
        sock.state_ = states::TimeWait{};
        std::cout << "Normal socket " << sock.id_ << ": Transitioned to TIME_WAIT\n";
    }
}

// FIN-WAIT-2 ----ACK/(ACK)----> FIN-WAIT-2
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::FinWait2 &, const events::GetAck &getAck)
{
    // Potentially: increment SND.UNA, remove ack'd segments from retransmission queue
    // std::cout << "Got ACK (seq=" << getAck.seqNum << ", ack=" << getAck.ackNum 
    //           << ", data=" << getAck.payload.size()
    //           << ") from " << sock.tuple_.remote.toString() << "\n";

    const auto &[_, nxt] = sock.sendBuffer_.onAck(getAck.ackNum, getAck.wndSize);  // locks sendBuffer_

    if (getAck.payload.size() > 0) {
        // If there's data, reply ACK (no retransmission)
        const auto &[ack, wnd] = sock.recvBuffer_.onRecv(getAck.seqNum, getAck.payload);  // locks recvBuffer_
        sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));
    }
}

// TIME-WAIT ----FIN(Retransmitted)/ACK----> TIME-WAIT
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::TimeWait &, const events::GetFin &getFin)
{
    // const auto nxt = sock.sendBuffer_.getNxt();
    const auto &[___, nxt] = sock.sendBuffer_.onAck(0, getFin.wndSize);  // ACK nothing, just update window size
    const auto &[ack, wnd] = sock.recvBuffer_.onCtrl(getFin.seqNum);

    std::cout << "Normal socket " << sock.id_ 
              << " (TIME_WAIT): Got retransmitted FIN (seq=" << getFin.seqNum
              << ") from " << sock.tuple_.remote.toString() << "\n";
    std::cout << "Replying ACK (seq=" << nxt << ", ack=" << ack
              << ", wnd=" << wnd << ") to " << sock.tuple_.remote.toString() << "\n";

    // Send ACK packet to the remote (No retransmission)
    sock.sendPacketNoRetransmit_(Packet::makeAckPacket(sock.tuple_, nxt, ack, static_cast<uint16_t>(wnd)));
}



// LAST-ACK ----ACK/----> CLOSED
void TcpStack::eventHandler_(
    NormalSocket &sock, const states::LastAck &, const events::GetAck &getAck)
{
    // Check validity of ACK number
    if (const auto &[una, nxt] = sock.sendBuffer_.onAck(getAck.ackNum, getAck.wndSize); una != nxt) {
        // Unacceptable ACK, should send RST but we don't care about it for now
        std::stringstream ss;
        ss << "Normal socket " << sock.id_ << " (LAST_ACK): Got ACK with wrong ACK number. Expected " 
           << nxt << ", got " << getAck.ackNum << " instead\n";
        std::cout << ss.str();
        return;
    }

    sock.shutdown_();
    sock.state_ = states::Closed{};

    std::cout << "Normal socket " << sock.id_ << ": Transitioned to CLOSED\n";
}


/********************************************* Listen Socket *********************************************/

// Handle a SYN packet event
void TcpStack::eventHandler_(
    ListenSocket &lSock, const states::Listen &, const events::GetSyn &getSyn)
{
    // Create a new normal socket.
    // This call will send a SYN-ACK and put the socket in SYN-RECEIVED state.
    // It then puts the socket in the pending connection list of the listener.
    auto sock = createPassiveConnection_(getSyn.session, getSyn.clientISN, getSyn.clientWND, lSock);

    if (sock) {
        std::cout << "Listener " << lSock.id_ << ": SYN request from " << getSyn.session.remote.toString() 
                  << " results in a new normal socket " << sock->get().id_ << "\n";
    } else {
        std::stringstream ss;
        ss << "Listen socket " << lSock.id_ << ": Failed to create a normal socket (ERROR: " << sock.error() << ")\n";
        std::cerr << ss.str();
    }
}

} // namespace tcp
} // namespace tns
