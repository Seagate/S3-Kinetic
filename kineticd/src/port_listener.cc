#include "port_listener.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/select.h>
#include <fcntl.h>

#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {

PortListener::PortListener(int receive_buffer_size)
    : receive_buffer_size_(receive_buffer_size) {}

void PortListener::MakeSocketNonBlocking(int sfd) {
    int flags, s;
    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        PLOG(ERROR) << "1. MakeSocketNonBlocking";//NO_SPELL
    }
    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        PLOG(ERROR) << "2. MakeSocketNonBlocking";//NO_SPELL
    }
}

bool PortListener::Listen(const std::string& listen_address, uint port, int *fd) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));

    // could be inet or inet6
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;

    struct addrinfo* result;

    string port_str = std::to_string(port);

    if (int res = getaddrinfo(listen_address.c_str(), port_str.c_str(), &hints, &result) != 0) {
        LOG(ERROR) << "Could not parse listen ip/port " << listen_address << "/" << port << ": "//NO_SPELL//NOLINT
                   << gai_strerror(res);
        return false;
    }

    struct addrinfo* ai;
    int socket_fd = -1;
    for (ai = result; ai != NULL; ai = ai->ai_next) {
        char host[NI_MAXHOST];
        char service[NI_MAXSERV];
        if (int res = getnameinfo(ai->ai_addr, ai->ai_addrlen, host, sizeof(host), service,
                sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
            LOG(ERROR) << "Could not get name info: " << gai_strerror(res);
            continue;
        } else {
            VLOG(1) << "Resolved " << listen_address << " port " << port << " to "
                    << string(host) << " port " << string(service) << "; trying to listen";
        }

        socket_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

        if (socket_fd == -1) {
            PLOG(WARNING) << "Could not open socket";
            continue;
        }

        // Set SO_REUSEADDR so that the port is not tied up if the program does not
        // exit cleanly
        int optval = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            PLOG(ERROR) << "Failed to set SO_REUSEADDR on socket";//NO_SPELL
            goto cleanup;
        }

        if (receive_buffer_size_ > 0) {
            VLOG(1) << "Setting the socket receive buffer size to " << receive_buffer_size_;
            if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &receive_buffer_size_,
                    sizeof(receive_buffer_size_)) == -1) {
                PLOG(ERROR) << "Failed to set the socket receive buffer size";
                goto cleanup;
            }
        } else {
            VLOG(1) << "Using the TCP stack's default socket receive buffer size";
        }

        // On linux, an ipv6 wildcard (::) will also listen on 0.0.0.0 (ipv4 wildcard). To make this
        // more understandable, we configure ipv6 sockets to only listen on v6.
        if (ai->ai_family == AF_INET6) {
            int on = 1;
            if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) == -1) {
                PLOG(ERROR) << "Could not configure ipv6 socket to only listen on v6";//NO_SPELL
                goto cleanup;
            }
        }

        if (bind(socket_fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            VLOG(1) << "Bound successfully";
            break;
        } else {
            PLOG(ERROR) << "Could not bind to " << string(host) << " on port " << string(service);
            goto cleanup;
        }

        cleanup:
        if (socket_fd != -1) {
            close(socket_fd);
        }
    }

    freeaddrinfo(result);

    if (ai == NULL) {
        // we went through all addresses without finding one we could bind to
        LOG(ERROR) << "Could not bind to any addresses for " << listen_address << " on port "
                << port;
        return false;
    }

    // Listen on the socket
    if (listen(socket_fd, SOMAXCONN) == -1) {
        PLOG(ERROR) << "Failed to listen on socket";
        close(socket_fd);
        return false;
    }

    int actual_receive_buffer;
    socklen_t option_size = sizeof(actual_receive_buffer);
    // To return correct results, this check must occur after the call to listen().
    if (getsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &actual_receive_buffer, &option_size) == -1) {
        PLOG(ERROR) << "Failed to check the socket receive buffer size";
        close(socket_fd);
        socket_fd = -1;
    } else {
        VLOG(1) << "Actual receive buffer size is " << actual_receive_buffer;
    }

    *fd = socket_fd;
    return (*fd >= 0);
}

bool PortListener::Close(int fd) {
    if (close(fd) == -1) {
        PLOG(ERROR) << "Failed to close socket file descriptor";
        return false;
    }
    return true;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
