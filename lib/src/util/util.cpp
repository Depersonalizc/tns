#include "src/util/util.hpp"

#include <cerrno>        // errno
#include <cstdio>        // perror
#include <sys/socket.h>  // recv

#include <iostream>

namespace tns {

ssize_t util::io::recvAll(int sockfd, void *buf, std::size_t len, int flags)
{
    char *bufp = static_cast<char *>(buf);
    std::size_t nLeft = len;
    ssize_t n;
    while (nLeft > 0) {
        std::cout << "going to recv on socket " << sockfd << "\n";
        n = recv(sockfd, bufp, nLeft, flags);
        if (n == 0)
            break;                  // Disconnected
        if (n == -1) {
            if (errno == EINTR)     // Interrupted by signal
                continue;           // Try again
            perror("recv");
            return -1;
        }
        nLeft -= n;
        bufp += n;
    }
    return len - nLeft;
}

} // namespace tns
