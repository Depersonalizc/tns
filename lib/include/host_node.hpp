#pragma once

#include <fstream>
#include <chrono>

#include "network_node.hpp"
#include "network_interface.hpp"
#include "tcp/tcp_stack.hpp"


namespace tns {

// This network node represents a host.
class HostNode : public NetworkNode {
public:
    HostNode() = default;
    HostNode(const std::string_view lnxFile);
    HostNode(const std::string &lnxFile) : HostNode(std::string_view(lnxFile)) {};
    HostNode(const HostNode&) = delete;
    HostNode(HostNode&&) = delete;

    ~HostNode() { std::cout << "HostNode::~HostNode() : DONE!\n"; };

    // Public API with TCP stack

    static constexpr auto tcpMaxPayloadSize() noexcept { return tcp::MAX_TCP_PAYLOAD_SIZE; }

    // Create a socket and connect to a remote host (Active Open)
    auto tcpConnect(const ip::Ipv4Address &remoteIP, in_port_t remotePort)
    {
        // Use address of the first interface as local
        return tcpStack_.vConnect(/*  local= */ interfaces_.front().ipAddress_,
                                  /* remote= */ {remoteIP.getAddrNetwork(), tns::util::hton(remotePort)});
    }
 
    // Create a listening socket bound to the given port (Passive Open)
    auto tcpListen(in_port_t port) { return tcpStack_.vListen(port); }

    // Send a stream of data via a TCP socket.
    template <typename T>
    auto tcpSend(int socketID, const std::span<const T> data) { return tcpStack_.vSend(socketID, data); }

    // Receive data from a TCP socket into a buffer.
    auto tcpRecv(int socketID, const std::span<std::byte> buff) { return tcpStack_.vRecv(socketID, buff); }

    // Send a file via a TCP socket.
    auto tcpSendFile(const std::string &filename, const ip::Ipv4Address &remoteIP, in_port_t remotePort) -> tl::expected<std::size_t, SocketError>
    {
        using namespace std::chrono;

        std::ifstream file(filename, std::ios::binary | std::ios::ate);

        // Check is file is opened and good
        if (!file) {
            std::cerr << "tcpSendFile(): Failed to open file " << filename << "\n";
            return tl::unexpected{SocketError::NO_RESOURCES};
        }

        const std::size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size == 0)
            return std::size_t{0};

        static constexpr std::size_t BUF_SIZE = 10 * 1024 * 1024;  // 10 MB buffer
        std::vector<char> buffer(BUF_SIZE);
    
        auto sockMaybe = tcpConnect(remoteIP, remotePort);
        if (!sockMaybe)
            return tl::unexpected{sockMaybe.error()};
        auto &sock = sockMaybe->get();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        const auto start = high_resolution_clock::now();

        std::size_t total = 0;
        while (total < size) {
            const auto n = std::min(BUF_SIZE, static_cast<std::size_t>(size - total));
            if (!file.read(buffer.data(), n))
                return tl::unexpected{SocketError::NO_RESOURCES};
            const auto nSent = sock.vSend(std::span<const char>{buffer.data(), n});
            if (!nSent)
                return tl::unexpected{nSent.error()};
            total += *nSent;
        }

        const auto duration = duration_cast<milliseconds>(high_resolution_clock::now() - start);

        std::cout << "Closing socket...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        sock.vClose();

        std::cout << "[SUCCESS] Sent " << total << " bytes in " << duration.count() << "ms.\n";

        return total;
    }

    // Receive a file from a TCP socket.
    auto tcpRecvFile(const std::string &filename, in_port_t localPort) -> tl::expected<std::size_t, SocketError>
    {
        std::cout << "tcpRecvFile(): Listening on port " << localPort << "\n";

        auto lSockMaybe = tcpListen(localPort);
        if (!lSockMaybe)
            return tl::unexpected{lSockMaybe.error()};
        auto &lSock = lSockMaybe->get();

        std::cout << "Created Listen socket " << lSock.getID() << "\n";

        std::cout << "tcpRecvFile(): Accepting connection...\n";

        auto sockMaybe = lSock.vAccept();
        if (!sockMaybe)
            return tl::unexpected{sockMaybe.error()};
        auto &sock = sockMaybe->get();

        std::cout << "Connection accepted. Receiving file on socket " << sock.getID() << "\n";

        static constexpr std::size_t MAXSIZE = 10 * 1024 * 1024;  // 10 MB buffer
        std::vector<std::byte> buf(MAXSIZE);
        std::size_t total = 0;
        while (true) {
            // std::this_thread::sleep_for(std::chrono::seconds(10000));
            // Recv starting from buf[total]
            const auto span   = std::span<std::byte>{buf.begin() + total, buf.end()};
            const auto nMaybe = sock.vRecv(span, span.size());
            if (!nMaybe) {
                if (nMaybe.error() == SocketError::CLOSING) {
                    std::cout << "tcpRecvFile(): Connection has been closed by sender.\n";
                    break;
                }
                return tl::unexpected{nMaybe.error()};
            }

            const auto n = nMaybe.value();

            total += n;

            // std::cout << "tcpRecvFile(): Received " << n << " new bytes. \tTotal " << total << "\n";

            // static constexpr std::size_t CHUNK_SIZE = 100 * 1024;  // 100 KB
            // if (chunk >= CHUNK_SIZE) {  // 100 KB
            //     std::stringstream ss;
            //     ss << "tcpRecvFile(): Received " << chunk << " new bytes. \tTotal " << total << "\r";
            //     std::cout << ss.str() << std::flush;
            //     chunk = 0;
            // }
        }

        std::cout << "\ntcpRecvFile(): Received " << total << " bytes in total.\n";
        std::cout << "Closing sockets...\n";

        sock.vClose();
        lSock.vClose();

        std::cout << "tcpRecvFile(): Writing to file " << filename << "...\n";

        std::ofstream file(filename, std::ios::binary);
        if (!file)
            return tl::unexpected{SocketError::NO_RESOURCES};
        
        file.write(reinterpret_cast<const char*>(buf.data()), total);

        std::cout << "tcpRecvFile(): File written.\n";

        return total;
    }

    // Close a TCP socket
    auto tcpClose(int socketID) { return tcpStack_.vClose(socketID); }

    // Abort a TCP socket
    auto tcpAbort(int socketID) { return tcpStack_.vAbort(socketID); }

    // List info of all sockets
    void tcpListSockets(std::ostream &os = std::cout) const { tcpStack_.listSockets(os); }


private:
    void datagramHandler_(DatagramPtr datagram, const ip::Ipv4Address &infaceAddr) const override;

private:
    tcp::TcpStack tcpStack_;
};

} // namespace tns
