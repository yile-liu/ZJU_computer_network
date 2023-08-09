#include "Global.h"
#include <arpa/inet.h>
#include <fstream>
#include <pthread.h>
#include <queue>
#include <vector>

using namespace std;

#define MAX_LISTEN_SIZE 10

const int keepAlive = 1;
const int keepIdle = 10000;
const int keepInterval = 1;
const int keepCount = 5;

char server_name[100] = { 0 };
void* clientThread(void*);

static int conn_fd;
static int listen_fd;

vector<client_info> client_list;

int main(int argc, char* argv[])
{
    gethostname(server_name, sizeof(server_name));
    int pId, portNo;
    pthread_t thread_pool[MAX_CLIENT_NUM];
    struct sockaddr_in server_address, client_address;
    socklen_t len = sizeof(client_address);

    if (argc < 2) {
        cerr << "\033[31mThere are too few arguments:\033[0m\n\t\033[1mSERVER_NAME <listen-port>\033[0m" << endl;
        return 0;
    }

    portNo = atoi(argv[1]);
    if ((portNo > 65535) || (portNo < 1025)) {
        cerr << "\033[31mPlease enter a port number between \033[1m1025 - 65535\033[0m" << endl;
        return 0;
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        cerr << "\033[31mCannot open socket\033[0m" << endl;
        return 0;
    }

    unsigned int timeout = 10;
#ifdef __APPLE__
    setsockopt(listen_fd, IPPROTO_TCP, TCP_CONNECTIONTIMEOUT, &timeout, sizeof(timeout));
    setsockopt(listen_fd, IPPROTO_TCP, TCP_KEEPALIVE, &keepIdle, sizeof(keepIdle));
#else
    setsockopt(listen_fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout));
    setsockopt(listen_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
#endif
    setsockopt(listen_fd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
    setsockopt(listen_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
    setsockopt(listen_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));

    memset((char*)&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(portNo);

//bind socket
#ifdef __APPLE__
    if (::bind(listen_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "\033[31mCannot bind\033[0m" << endl;
        return 0;
    }
#else
    if (bind(listen_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "\033[31mCannot bind\033[0m" << endl;
        return 0;
    }
#endif

    listen(listen_fd, MAX_LISTEN_SIZE);

    int thread_num = 0;
    while (thread_num < MAX_CLIENT_NUM) {
        cout << "=====================\nListening..." << endl;

        int conn_fd = accept(listen_fd, (struct sockaddr*)&client_address, &len);
        if (conn_fd < 0) {
            cerr << "\033[31m\033[1mCannot accept connection\033[0m" << endl;
            return 0;
        } else {
            cout << "\033[32m\033[1mConnection successful\033[0m" << endl;
        }

        cout << "\tIP address: \033[1m" << inet_ntoa(client_address.sin_addr) << "\033[0m" << endl;
        cout << "\tport: \033[1m" << (int)ntohs(client_address.sin_port) << "\033[0m\n"
             << endl;

        char* ip_address = inet_ntoa(client_address.sin_addr);
        client_info temp;
        temp.num = conn_fd;
        memset(temp.ip, 0, 15);
        strncpy(temp.ip, ip_address, strlen(ip_address));
        temp.port = (int)ntohs(client_address.sin_port);
        client_list.push_back(temp);

        int* args = &conn_fd;
        pthread_create(&thread_pool[thread_num], NULL, clientThread, (void*)args);

        thread_num++;
    }

    for (int i = 0; i < thread_num; i++) {
        pthread_join(thread_pool[i], NULL);
    }
    close(listen_fd);
}

void* clientThread(void* args)
{
    int conn_fd = *(int*)args;
    int cnt = 0;

    cout << "\033[34m[Thread No. " << pthread_self() << "]\033[0m"
         << "  Connect ID " << conn_fd << endl;

    while (1) {
        unsigned char test[READ_BUFFER_SIZE];
        memset(test, 0, READ_BUFFER_SIZE); // set read buffer

        // read from socket
        int packet_size = sizeof(Packet);
        int received = 0;
        int bytes = 0;
        do {
            bytes = recv(conn_fd, test + received, packet_size - received, 0);
            if (bytes < 0)
                cerr << "\033[31mERROR reading recv_buffer from socket\033[0m";
            if (bytes == 0)
                break;
            received += bytes;
        } while (received < packet_size);

        if (bytes == 0) // client shut down
        {
            cout << "\033[34mThe client is shut down.\033[0m\n"
                 << client_list.size() << endl;

            auto iter = client_list.begin();
            for (; iter != client_list.end();) {
                if (conn_fd == iter->num) {
                    iter = client_list.erase(iter);
                } else
                    iter++;
            }
            break;
        }
        Packet* phead = (Packet*)test;
        int data_length = phead->header.length - sizeof(PacketHeader);

        cout << "# Data: [[\033[33m\n"
             << phead->body.data << "\033[0m\n]]" << endl;
        if (data_length > 0 && data_length > sizeof(Packet) - sizeof(PacketHeader)) { // extra read
            received = 0;
            cout << "# Extra Read #" << endl;
            do {
                bytes = recv(conn_fd, test + packet_size + received, data_length - received, 0);

                if (bytes < 0)
                    cerr << "\033[31mERROR reading recv_buffer from socket.\033[0m";
                if (bytes == 0) //the server is closed
                {
                    break;
                }
                received += bytes;

            } while (received < data_length);
        }

        switch (phead->header.op) {
        case TIME: {
            cout << "[Get time request]" << endl;

            cnt++;
            time_t nowtime;
            nowtime = time(NULL);
            Packet to_send(listen_fd, conn_fd, sizeof(Packet), 0, TIME, nullptr);
            strncpy((char*)to_send.body.data, (const char*)&nowtime, sizeof(nowtime)); //copy data
            send(conn_fd, &to_send, sizeof(to_send), 0);

            cout << "\033[32m# Send to client success " << sizeof(to_send) << "\033[0m" << endl;
            cout << "\033[32m# Send Time to client success "
                 << "\033[0m" << endl;
            cout << "# Raw data: [[\033[33m\n"
                 << to_send.body.data << "\033[0m\n]]" << endl;
            cout << "# Data is: [[\033[33m\n"
                 << *((time_t*)to_send.body.data) << "\033[0m\n]]" << endl;
            cout << "[" << ctime(&nowtime) << "]" << endl;
            cout << " ---- cnt: " << cnt << "\n"
                 << endl;
            break;
        }
        case NAME: {
            cout << "[Get name request]\n";

            Packet to_send(listen_fd, conn_fd, sizeof(Packet), 0, NAME, nullptr);
            strncpy((char*)to_send.body.data, (const char*)server_name, strlen(server_name) + 1);
            send(conn_fd, &to_send, sizeof(to_send), 0);

            cnt++;
            cout << "\033[32m# Send to client success " << sizeof(to_send) << "\033[0m" << endl;
            cout << "\033[32m# Send name to client success "
                 << "\033[0m" << endl;
            cout << "# Raw data: [[\033[33m\n"
                 << (char*)to_send.body.data << "\033[0m\n]]" << endl;
            cout << ((char*)to_send.body.data) << endl;
            cout << " ---- cnt: " << cnt << "\n"
                 << endl;
            break;
        }
        case ACTIVE_LIST: {
            cout << "[Get active list request]\n";

            Packet to_send(listen_fd, conn_fd, sizeof(Packet), 0, ACTIVE_LIST, nullptr);
            for (int i = 0; i < client_list.size(); ++i) {
                to_send.body.list[i].port = client_list[i].port;
                to_send.body.list[i].num = client_list[i].num;
                strncpy(to_send.body.list[i].ip, client_list[i].ip, 15);
                if (conn_fd == to_send.body.list[i].num) {
                    to_send.body.list[i].isThisMyfd = 1;
                    cout << "...done" << endl;
                } else
                    to_send.body.list[i].isThisMyfd = 0;
            }
            int n = client_list.size();
            strncpy((char*)to_send.body.data, (const char*)&n, sizeof(int)); //copy data
            send(conn_fd, &to_send, sizeof(to_send), 0);
            cout << "\033[32m# Send to client success " << sizeof(to_send) << "\033[0m" << endl;
            cout << "\033[32m# Send name to client success " << sizeof(to_send) - sizeof(PacketHeader) << "\033[0m" << endl;
            cout << "# Raw data: [[\033[33m\n"
                 << (char*)to_send.body.data << "\033[0m\n]]" << endl;
            cout << "# num of data is : " << *(int*)to_send.body.data << endl;
            for (int i = 0; i < *(int*)to_send.body.data; ++i) {
                cout << "* " << to_send.body.list[i].ip;
                cout << ":" << to_send.body.list[i].port;
                cout << "\t\tNAME:\"" << sizeof(to_send.body.list[i].ip) << "\"" << endl;
            }
            cnt++;
            cout << " ---- cnt: " << cnt << "\n"
                 << endl;
            break;
        }
        case MESSAGE: {
            if (phead->header.type == 1) { // message request
                cout << "[Get message request]\n";

                Packet to_send = *phead;
                bool isExist = 0;
                for (auto i : client_list) {
                    if (i.num == to_send.header.destination) {
                        isExist = true;
                        break;
                    }
                }

                if (isExist == false) {
                    cerr << "No such client!" << endl;
                    Packet error_info(listen_fd, conn_fd, sizeof(Packet), 2, ERROR, nullptr);
                    send(conn_fd, &error_info, sizeof(error_info), 0);
                    cout << "\033[32m# Send error message to client success " << conn_fd << "\033[0m" << endl;
                } else {
                    cout << "req:" << to_send.header.source << " to " << to_send.header.destination << endl;
                    send(to_send.header.destination, &to_send, sizeof(to_send), 0);
                    cout << "\033[32m# Send message request to client success " << sizeof(to_send) << "\033[0m" << endl;
                    cout << "# Raw data: [[\033[33m\n"
                         << (char*)to_send.body.data << "\033[0m\n]]" << endl;
                    cout << "# fd is : " << to_send.header.destination << endl;
                }

                cnt++;
                cout << " ---- cnt: " << cnt << "\n"
                     << endl;
            } else {
                cout << "[Get message reply]\n";

                Packet to_send = *phead;
                cout << to_send.header.source << " send to:" << to_send.header.destination << endl;
                send(to_send.header.destination, &to_send, sizeof(to_send), 0); //maybe bug
                cout << "\033[32m# Send message reply to client success " << sizeof(to_send) << "\033[0m" << endl;
                cout << "# Raw data: [[\033[33m\n"
                     << (char*)to_send.body.data << "\033[0m\n]]" << endl;
                cout << "# num of data is : " << *(int*)to_send.body.data << endl;

                cnt++;
                cout << " ---- cnt: " << cnt << "\n"
                     << endl;
            }
            break;
        }
        default: {
            cout << "\033[31mRequest Error!\033[0m\n";
        }
        }
    }
}
