/*
 * Copyright ©2026 Justin Hsia and Amber Hu. All rights reserved.
 * Permission is hereby granted to students registered for University of
 * Washington CSE 333 for use solely during Winter Quarter 2026 for
 * purposes of the course. No other use, copying, distribution, or
 * modification is permitted without prior written consent. Copyrights
 * for third-party components of this work must be honored. Instructors
 * interested in reusing these course materials should contact the
 * authors.
 */

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

using std::string;

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int* const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = ai_family;       // AF_INET, AF_INET6, or AF_UNSPEC
  hints.ai_socktype = SOCK_STREAM;   // TCP
  hints.ai_flags = AI_PASSIVE;       // bind to wildcard address
  hints.ai_flags |= AI_V4MAPPED;     // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  // convert port number to string for getaddrinfo
  char port_str[10];
  snprintf(port_str, sizeof(port_str), "%d", port_);

  struct addrinfo* result;
  int res = getaddrinfo(nullptr, port_str, &hints, &result);
  if (res != 0) {
    std::cerr << "getaddrinfo failed: " << gai_strerror(res) << std::endl;
    return false;
  }

  // iterate through results and try to bind
  int fd = -1;
  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd == -1) continue;

    // allow reuse of port
    int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;  // success

    close(fd);
    fd = -1;
  }
  freeaddrinfo(result);

  if (fd == -1) {
    std::cerr << "Could not bind to port " << port_ << std::endl;
    return false;
  }

  // start listening
  if (listen(fd, SOMAXCONN) != 0) {
    std::cerr << "listen() failed: " << strerror(errno) << std::endl;
    close(fd);
    return false;
  }

  listen_sock_fd_ = fd;
  *listen_fd = fd;


  return true;
}

bool ServerSocket::Accept(int* const accepted_fd,
                          string* const client_addr,
                          uint16_t* const client_port,
                          string* const client_dns_name,
                          string* const server_addr,
                          string* const server_dns_name) const {
  // STEP 2:
  struct sockaddr_storage caddr;
  socklen_t caddr_len = sizeof(caddr);
  int client_fd;

  // loop to handle EINTR
  while (true) {
    client_fd = accept(listen_sock_fd_,
                      reinterpret_cast<struct sockaddr*>(&caddr),
                      &caddr_len);
    if (client_fd < 0) {
      if (errno == EINTR) continue;
      std::cerr << "accept() failed: " << strerror(errno) << std::endl;
      return false;
    }
    break;
  }

  *accepted_fd = client_fd;

  // extract client IP address and port
  if (caddr.ss_family == AF_INET) {
    // IPv4
    char astring[INET_ADDRSTRLEN];
    struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&caddr);
    inet_ntop(AF_INET, &sa->sin_addr, astring, INET_ADDRSTRLEN);
    *client_addr = astring;
    *client_port = ntohs(sa->sin_port);
  } else {
    // IPv6
    char astring[INET6_ADDRSTRLEN];
    struct sockaddr_in6* sa = reinterpret_cast<struct sockaddr_in6*>(&caddr);
    inet_ntop(AF_INET6, &sa->sin6_addr, astring, INET6_ADDRSTRLEN);
    *client_addr = astring;
    *client_port = ntohs(sa->sin6_port);
  }

  // resolve client DNS name
  char client_hostname[NI_MAXHOST];
  if (getnameinfo(reinterpret_cast<struct sockaddr*>(&caddr),
                  caddr_len,
                  client_hostname, NI_MAXHOST,
                  nullptr, 0, 0) != 0) {
    snprintf(client_hostname, NI_MAXHOST, "%s", client_addr->c_str());
  }
  *client_dns_name = client_hostname;

  // get server address and DNS name
  char server_hostname[NI_MAXHOST];
  struct sockaddr_storage saddr;
  socklen_t saddr_len = sizeof(saddr);
  if (getsockname(client_fd,
                  reinterpret_cast<struct sockaddr*>(&saddr),
                  &saddr_len) != 0) {
    std::cerr << "getsockname() failed: " << strerror(errno) << std::endl;
    return false;
  }

  if (saddr.ss_family == AF_INET) {
    char astring[INET_ADDRSTRLEN];
    struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&saddr);
    inet_ntop(AF_INET, &sa->sin_addr, astring, INET_ADDRSTRLEN);
    *server_addr = astring;
  } else {
    char astring[INET6_ADDRSTRLEN];
    struct sockaddr_in6* sa = reinterpret_cast<struct sockaddr_in6*>(&saddr);
    inet_ntop(AF_INET6, &sa->sin6_addr, astring, INET6_ADDRSTRLEN);
    *server_addr = astring;
  }

  if (getnameinfo(reinterpret_cast<struct sockaddr*>(&saddr),
                  saddr_len,
                  server_hostname, NI_MAXHOST,
                  nullptr, 0, 0) != 0) {
    snprintf(server_hostname, NI_MAXHOST, "%s", server_addr->c_str());
  }
  *server_dns_name = server_hostname;

  return true;
}

}  // namespace hw4
