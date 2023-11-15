#include "src/util/util.hpp"

#include <cerrno>        // errno
#include <cstdio>        // perror
#include <sys/socket.h>  // recv

#include <iostream>

namespace tns {

ssize_t util::io::recvAll(int sockfd, void *buf, std::size_t len, int flags)
{
    ::THROW_NO_IMPL();
}

} // namespace tns
