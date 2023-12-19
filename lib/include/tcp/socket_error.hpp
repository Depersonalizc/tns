#pragma once

#include <ostream>

enum class SocketError {
    CLOSING,
    TIMEOUT,
    RESET,
    CONN_NOT_EXIST,
    DUPLICATE_SOCKET,
    NO_RESOURCES,
    OP_NOT_ALLOWED,
    NYI,
};

inline std::ostream &operator<<(std::ostream &os, SocketError err)
{
    switch (err) {
        case SocketError::CLOSING:          return os << "connection closing";
        case SocketError::TIMEOUT:          return os << "TIMEOUT";
        case SocketError::RESET:            return os << "connection reset";
        case SocketError::CONN_NOT_EXIST:   return os << "connection does not exist";
        case SocketError::DUPLICATE_SOCKET: return os << "connection already exists";
        case SocketError::NO_RESOURCES:     return os << "insufficient resources";
        case SocketError::OP_NOT_ALLOWED:   return os << "OP_NOT_ALLOWED";
        case SocketError::NYI:              return os << "not yet implemented";
        default:                            return os << "UNKNOWN";
    }
}
