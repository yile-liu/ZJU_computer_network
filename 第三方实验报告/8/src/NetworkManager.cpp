#include "NetworkManager.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#define MAX_CONN 1024

static int listen_port(int port);
static int net_accept(int socket, struct sockaddr* addr, socklen_t* addrlen);

NetworkManager::NetworkManager()
    : listen_fd(-1)
    , conn_fd(-1)
{
}
void NetworkManager::Listen(int port)
{
    int rc;
    rc = listen_port(port);
    if (rc < 0)
        perror("Listen error");
    else
        listen_fd = rc;
}

int NetworkManager::Accept()
{
    int clientlen;
    struct sockaddr_in clientaddr;
    clientlen = sizeof(clientaddr);
    conn_fd = net_accept(listen_fd, (sockaddr*)&clientaddr, (socklen_t*)&clientlen);
    if (conn_fd < 0) {
        perror("Accept error");
        return -1;
    } else
        return conn_fd;
}

void NetworkManager::Close()
{
    int rc;
    rc = close(conn_fd);
    if (rc < 0)
        perror("Close error");
    else
        return;
}

int listen_port(int port)
{
    // Reference: CSAPP
    struct sockaddr_in serveraddr;
    int listenfd, optval;

    /* Create a socket */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminate "Address already in use" */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
            (const void*)&optval, sizeof(int))
        < 0)
        return -1;

    /* Set up the address */
    bzero((char*)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Listen to the socket */
    if (listen(listenfd, MAX_CONN) < 0)
        return -1;

    /* Return the socket*/
    return listenfd;
}

int net_accept(int socket, struct sockaddr* addr, socklen_t* addrlen)
{
    int rc;
    rc = accept(socket, addr, addrlen);
    if (rc < 0) {
        perror("Accept error");
        return -1;
    } else
        return rc;
}
