#include "NetworkManager.hpp"
#include "RequestManager.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    NetworkManager NetMng;

    NetMng.Listen(3746);
    std::cout << "\033[31m[LOG] \033[0m"
              << "Web Server started. Waiting for request" << std::endl;

    /* Infinity loop on accepting request and handling */
    while (true) {
        int connfd;
        connfd = NetMng.Accept();
        RequestManager rm(connfd);
        std::cout << "\033[31m[LOG] \033[0m"
                  << "Connection Established, socket id = " << connfd << std::endl;
        rm.Start();
        NetMng.Close();
    }

    return 0;
}
