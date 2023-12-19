#include <host_node.hpp>

#include <string>
#include <sstream>
#include <thread>

using namespace tns;

constexpr std::string_view HELP_TEXT =
"Available commands:"
"\n  exit"
"\n  help                         - Display this help message"
"\n  send <dest-ip> <message>     - Send a test message"
"\n  up <interface-name>          - Enable an interface"
"\n  down <interface-name>        - Disable an interface"
"\n  li                           - List interfaces"
"\n  ln                           - List neighbors"
"\n  lr                           - List routes"
"\n  a <port>                     - Listen + Accept connections"
"\n  c <ip> <port>                - Connect to a remote host"
"\n  s <sid> <payload>            - Send a payload via TCP socket"
"\n  r <sid> <max-bytes>          - Receive bytes via TCP socket"
"\n  sf <file-path> <addr> <port> - Send a file via TCP"
"\n  rf <dest-file> <port>        - Receive a file via TCP"
"\n  cl <sid>                     - Close a TCP socket"
"\n  ls                           - List TCP sockets"
"\n";

int main(int argc, char *argv[])
{
    if (argc != 3 || std::string_view(argv[1]) != "--config") {
        std::cerr << "Usage: " << argv[0] << " --config <lnx-file>\n";
        return EXIT_FAILURE;
    }

    std::vector<std::jthread> acceptThreads;

    {
        HostNode hostNode{ std::string_view{argv[2]} };
        
        std::string line;
        std::stringstream ss;

        // REPL
        while (true) {
            try {
                std::cout << "> ";

                if (!std::getline(std::cin, line) || line == "exit")
                    break;

                if (line.empty())
                    continue;

                ss.clear();
                ss.str(line);

                if (line == "help") {
                    std::cout << ::HELP_TEXT;
                }
                else if (line.starts_with("send ")) {  // Send a test message
                    std::string destIP;
                    std::string_view message;

                    // Ignore "send"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `send` is missing destination IP address.\n";
                        continue;
                    }

                    // Extract destination IP address
                    ss >> destIP;
                    if (destIP.empty()) {
                        std::cout << "ERROR: Command `send` is missing destination IP address.\n";
                        continue;
                    }

                    // Skip whitespace and extract message
                    ss.get();
                    if (ss.eof() || ss.peek() == EOF) {
                        std::cout << "ERROR: Command `send` is missing message.\n";
                        continue;
                    }
                    message = {line.data() + ss.tellg(), line.size() - ss.tellg()};
                    if (message.size() > std::numeric_limits<uint16_t>::max() - 20) {
                        std::cout << "ERROR: Command `send` message is too long.\n";
                        continue;
                    }

                    std::cout << "Sending message \"" << message << "\" to " << destIP << "\n";
                    hostNode.sendIpTest(destIP, message);
                }
                else if (line.starts_with("up ")) {
                    std::string interfaceName;

                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `up` is missing interface name.\n";
                        continue;
                    }

                    ss >> interfaceName;
                    hostNode.enableInterface(interfaceName);
                }
                else if (line.starts_with("down ")) {
                    std::string interfaceName;

                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `down` is missing interface name.\n";
                        continue;
                    }

                    ss >> interfaceName;
                    hostNode.disableInterface(interfaceName);
                }
                else if (line.starts_with("a ")) {  // Listen + Accept incoming connections on <port>
                    std::string portstr;

                    // Ignore "a"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `a` is missing port.\n";
                        continue;
                    }

                    // Extract port
                    ss >> portstr;
                    if (portstr.empty()) {
                        std::cout << "ERROR: Command `a` is missing port.\n";
                        continue;
                    }

                    // TODO: Check input validity
                    // Create listen socket
                    auto port = std::stoi(portstr);
                    auto ls = hostNode.tcpListen(static_cast<in_port_t>(port));
                    if (!ls) {
                        std::cout << "ERROR: Failed to create a listen socket on port " << port
                                  << " (" << ls.error() << ")\n";
                        continue;
                    }

                    std::cout << "Listening on port " << port << " (SID = " << ls->get().getID() << ")\n";

                    // Spawn a thread to accept incoming connections indefinitely on the listen socket
                    acceptThreads.emplace_back(
                        [&listener = ls->get()] {
                            const auto sid = listener.getID();
                            while (true) {
                                auto sock = listener.vAccept();
                                std::stringstream ss;
                                if (!sock) {
                                    ss << "vAccept: Listen socket " << sid << ": " << sock.error() << "\n";
                                    std::cout << ss.str();
                                    return;
                                }
                                ss << "Accepted new connection -> Socket " << sock->get().getID() << "\n";
                                std::cout << ss.str();
                            }
                        }
                    );
                }
                else if (line.starts_with("c ")) {  // Connect to a remote host at <ip> <port>
                    std::string ip, port;

                    // Ignore "c"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `c` is missing remote address.\n";
                        continue;
                    }

                    // Extract remote IP address
                    ss >> ip;
                    if (ip.empty()) {
                        std::cout << "ERROR: Command `c` is missing remote address.\n";
                        continue;
                    }

                    // Extract remote port
                    ss >> port;
                    if (port.empty()) {
                        std::cout << "ERROR: Command `c` is missing remote port.\n";
                        continue;
                    }

                    // TODO: Check input validity
                    auto portNumber = static_cast<in_port_t>(std::stoi(port));

                    std::cout << "Connecting to " << ip << ":" << port << "\n";
                    // std::jthread th{[&]() {
                    //     hostNode.tcpConnect(ip::Ipv4Address{ip}, portNumber);
                    // }};
                    // th.detach();
                    hostNode.tcpConnect(ip::Ipv4Address{ip}, portNumber);
                }
                else if (line.starts_with("s ")) {  // Send a payload via TCP socket
                    std::string sid;
                    std::string_view message;

                    // Ignore "s"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `s` is missing socket ID.\n";
                        continue;
                    }

                    // Extract socket ID
                    ss >> sid;
                    if (sid.empty()) {
                        std::cout << "ERROR: Command `s` is missing socket ID.\n";
                        continue;
                    }

                    // Skip whitespace and extract message
                    ss.get();
                    if (ss.eof() || ss.peek() == EOF) {
                        std::cout << "ERROR: Command `s` is missing payload.\n";
                        continue;
                    }
                    message = {line.data() + ss.tellg(), line.size() - ss.tellg()};
                    if (message.size() > hostNode.tcpMaxPayloadSize()) {
                        std::cout << "ERROR: Command `s` payload is too long.\n";
                        continue;
                    }

                    auto res = hostNode.tcpSend(std::stoi(sid), std::span{message});
                    if (!res) {
                        std::cout << "ERROR: Failed to send data (" << res.error() << ")\n";
                        continue;
                    }
                    std::cout << "Sent " << res.value() << " bytes\n";
                }
                else if (line.starts_with("r ")) {  // recv bytes via TCP socket
                    std::string sid, maxBytes;

                    // Ignore "r"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `r` is missing socket ID.\n";
                        continue;
                    }

                    // Extract socket ID
                    ss >> sid;
                    if (sid.empty()) {
                        std::cout << "ERROR: Command `r` is missing socket ID.\n";
                        continue;
                    }

                    // Skip whitespace and extract maxBytes
                    ss >> maxBytes;
                    if (maxBytes.empty()) {
                        std::cout << "ERROR: Command `r` is missing number of bytes to read.\n";
                        continue;
                    }

                    std::vector<std::byte> buff(stoi(maxBytes));
                    const auto res = hostNode.tcpRecv(std::stoi(sid), std::span{buff});
                    if (!res) {
                        std::cout << "ERROR: Failed to read data (" << res.error() << ")\n";
                        continue;
                    }

                    const auto length = res.value();
                    std::cout << "Read " << length << " bytes: \n";
                    std::string_view sv{reinterpret_cast<const char *>(buff.data()), length};
                    std::cout << sv << "\n";
                }
                else if (line.starts_with("cl ")) {
                    std::string sid;

                    // Ignore "cl"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `cl` is missing socket ID.\n";
                        continue;
                    }

                    // Extract socket ID
                    ss >> sid;
                    if (sid.empty()) {
                        std::cout << "ERROR: Command `cl` is missing socket ID.\n";
                        continue;
                    }

                    if (auto ok = hostNode.tcpClose(std::stoi(sid)); ok)
                        std::cout << "Closed socket " << sid << "\n";
                    else
                        std::cout << "ERROR: Failed to close socket " << sid << " (" << ok.error() << ")\n";
                }
                else if (line.starts_with("ab ")) {
                    std::string sid;

                    // Ignore "ab"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `ab` is missing socket ID.\n";
                        continue;
                    }

                    // Extract socket ID
                    ss >> sid;
                    if (sid.empty()) {
                        std::cout << "ERROR: Command `ab` is missing socket ID.\n";
                        continue;
                    }

                    if (auto ok = hostNode.tcpAbort(std::stoi(sid)); ok)
                        std::cout << "Aborted socket " << sid << "\n";
                    else
                        std::cout << "ERROR: Failed to abort socket " << sid << " (" << ok.error() << ")\n";
                }
                else if (line.starts_with("sf ")) {
                    std::string filePath, ip, port;

                    // Ignore "sf"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `sf` is missing file path.\n";
                        continue;
                    }

                    // Extract file path
                    ss >> filePath;
                    if (filePath.empty()) {
                        std::cout << "ERROR: Command `sf` is missing file path.\n";
                        continue;
                    }

                    ss >> ip;
                    if (ip.empty()) {
                        std::cout << "ERROR: Command `sf` is missing destination IP.\n";
                        continue;
                    }

                    // Skip whitespace and extract destIP
                    ss >> port;
                    if (port.empty()) {
                        std::cout << "ERROR: Command `sf` is missing destination port.\n";
                        continue;
                    }

                    // TODO: Check input validity
                    auto portNumber = static_cast<in_port_t>(std::stoi(port));

                    std::cout << "Sending file " << filePath << " to " << ip << ":" << port << "\n";

                    std::jthread th{[&, filePath = std::string(filePath)]() {
                        auto nSentMaybe = hostNode.tcpSendFile(filePath, ip::Ipv4Address{ip}, portNumber);
                        if (!nSentMaybe)
                            std::cout << "ERROR: Failed to send file (" << nSentMaybe.error() << ")\n";
                    }};
                    th.detach();
                }
                else if (line.starts_with("rf ")) {
                    std::string filePath, port;

                    // Ignore "rf"
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    if (ss.eof()) {
                        std::cout << "ERROR: Command `rf` is missing destination file path.\n";
                        continue;
                    }

                    // Extract file path
                    ss >> filePath;
                    if (filePath.empty()) {
                        std::cout << "ERROR: Command `rf` is missing destination file path.\n";
                        continue;
                    }

                    // Extract port
                    ss >> port;
                    if (port.empty()) {
                        std::cout << "ERROR: Command `rf` is missing port.\n";
                        continue;
                    }

                    // TODO: Check input validity
                    auto portNumber = static_cast<in_port_t>(std::stoi(port));

                    std::cout << "Receiving file " << filePath << " from port " << port << "\n";

                    std::jthread th{[&, filePath = std::string(filePath)]() {
                        const auto nRecvMaybe = hostNode.tcpRecvFile(filePath, portNumber);
                        if (!nRecvMaybe)
                            std::cout << "ERROR: Failed to receive file (" << nRecvMaybe.error() << ")\n";
                        else
                            std::cout << "[SUCCESS] Received " << nRecvMaybe.value() << " bytes\n";
                    }};
                    th.detach();
                }
                else if (line == "ls") {
                    hostNode.tcpListSockets();
                }
                else if (line == "li") {
                    hostNode.listInterfaces();
                }
                else if (line == "ln") {
                    hostNode.listNeighbors();
                }
                else if (line == "lr") {
                    hostNode.listRoutes();
                }
                else {
                    std::cout << "ERROR: Unknown command. Type 'help' for a list of supported commands.\n";
                }
            } catch (const std::exception &e) {
                std::cerr << "ERROR: " << e.what() << "\n";
            }
        }
    }

    std::cout << "BYE!\n";

    return EXIT_SUCCESS;
}
