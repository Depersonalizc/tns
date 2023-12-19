#include <router_node.hpp>

#include <string>
#include <sstream>

using namespace tns;

constexpr std::string_view HELP_TEXT =
"Available commands:"
"\n  exit"
"\n  help                      - Display this help message"
"\n  send <dest-ip> <message>  - Send a test message"
"\n  up <interface-name>       - Enable an interface"
"\n  down <interface-name>     - Disable an interface"
"\n  li                        - List interfaces"
"\n  ln                        - List neighbors"
"\n  lr                        - List routes"
"\n";

int main(int argc, char *argv[])
{
    if (argc != 3 || std::string_view(argv[1]) != "--config") {
        std::cerr << "Usage: " << argv[0] << " --config <lnx-file>\n";
        return EXIT_FAILURE;
    }

    RouterNode routerNode{ std::string_view{argv[2]} };

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
            else if (line.starts_with("send")) {  // Send a test message
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
                    std::cout << "ERROR: Command `send`: message is too long.\n";
                    continue;
                }

                std::cout << "Sending message \"" << message << "\" to " << destIP << "\n";
                routerNode.sendIpTest(destIP, message);
            }
            else if (line.starts_with("up")) {
                std::string interfaceName;

                ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                if (ss.eof()) {
                    std::cout << "ERROR: Command `up` is missing interface name.\n";
                    continue;
                }

                ss >> interfaceName;
                routerNode.enableInterface(interfaceName);
            }
            else if (line.starts_with("down")) {
                std::string interfaceName;

                ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                if (ss.eof()) {
                    std::cout << "ERROR: Command `down` is missing interface name.\n";
                    continue;
                }

                ss >> interfaceName;
                routerNode.disableInterface(interfaceName);
            }
            else if (line == "li") {
                routerNode.listInterfaces();
            }
            else if (line == "ln") {
                routerNode.listNeighbors();
            }
            else if (line == "lr") {
                routerNode.listRoutes();
            }
            else {
                std::cout << "ERROR: Unknown command. Type 'help' for a list of supported commands.\n";
            }
        } catch (const std::exception &e) {
            std::cerr << "ERROR: " << e.what() << "\n";
        }
    }

    std::cout << "BYE!\n";

    return EXIT_SUCCESS;
}
