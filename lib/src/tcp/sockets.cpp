#include "tcp/sockets.hpp"
#include "tcp/packet.hpp"

#include <iomanip>

/* Visitor functors for Socket */

namespace tns {
namespace tcp {

// WriteInfo
void WriteInfo::operator()(const NormalSocket &sock, std::ostream &os) const
{
    using namespace std;
    const auto &sess = sock.tuple_;
    os  << setw(3)  << left  << sock.id_                   << " "   // SID
        << setw(15) << right << sess.local.toStringAddr()  << " "   // LAddr
        << setw(5)  << left  << sess.local.getPortHost()   << " "   // LPort
        << setw(15) << right << sess.remote.toStringAddr() << " "   // RAddr
        << setw(5)  << left  << sess.remote.getPortHost()  << " "   // RPort
        << setw(12) << right << sock.state_                << "\n"; // Status
}

void WriteInfo::operator()(const ListenSocket &lSock, std::ostream &os) const
{
    using namespace std;
    os  << setw(3)  << left  << lSock.id_    << " "   // SID
        << setw(15) << right << "0.0.0.0"    << " "   // LAddr
        << setw(5)  << left  << lSock.port_  << " "   // LPort
        << setw(15) << right << "0.0.0.0"    << " "   // RAddr
        << setw(5)  << left  << "*"          << " "   // RPort
        << setw(12) << right << lSock.state_ << "\n"; // Status
}

void ListenSocket::PendingSocks::onClose()
{
    // Make a copy of the pending sockets to avoid deadlock
    decltype(socks) sockcopies;
    {
        std::lock_guard lock(mutex);
        sockcopies = std::exchange(socks, {});
    }

    for (auto &[_, sock] : sockcopies)
        sock.get().vClose();  // Close all pending connections
}


auto ListenSocket::PendingSocks::add(const tcp::SessionTuple &sess, NormalSocketRef sock) -> std::optional<PendingSock>
{
    std::lock_guard lock(mutex);
    return socks.size() < MAX_PENDING_SOCKS 
            ? std::optional{ socks.emplace_back(sess, sock) }
            : std::nullopt;
}

auto ListenSocket::PendingSocks::remove(const tcp::SessionTuple &sess) -> std::optional<NormalSocketRef>
{
    std::lock_guard lock(mutex);
    auto it = std::ranges::find_if(socks, [&](auto &conn) { return conn.first == sess; });
    if (it == socks.end())
        return std::nullopt;
    const auto sock = it->second;
    socks.erase(it);
    return sock;
}

void ListenSocket::AcceptQueue::onClose()
{
    {
        std::lock_guard lock(queueMutex);
        closed = true;
        while (!queue.empty()) {
            auto sock = queue.front();
            queue.pop();
            sock.get().vClose();  // Close all sockets in the accept queue
        }
    }
    queueCondVar.notify_all();
}

void ListenSocket::AcceptQueue::pushAndNotify(NormalSocketRef sock)
{
    {
        std::lock_guard lock(queueMutex);
        queue.push(sock);
    }
    queueCondVar.notify_one();
}

auto ListenSocket::AcceptQueue::waitAndPop() -> tl::expected<NormalSocketRef, SocketError>
{
    std::unique_lock lock(queueMutex);
    queueCondVar.wait(lock, [this] { return !queue.empty() || closed; });
    if (closed)
        return tl::unexpected(SocketError::CLOSING);
    auto sock = queue.front();
    queue.pop();
    return sock;
}



} // namespace tcp
} // namespace tns
