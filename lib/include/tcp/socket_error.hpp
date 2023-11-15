#pragma once

#include <ostream>

enum class SocketError {
    TERMINATED,
    TIMEOUT,
    RESET,
    SOCKET_NOT_FOUND,
    DUPLICATE_SOCKET,
    NO_RESOURCES,
    OP_NOT_ALLOWED,
    BAD_STATE,
    GENERIC_ERROR
};

inline std::ostream &operator<<(std::ostream &os, SocketError err);
