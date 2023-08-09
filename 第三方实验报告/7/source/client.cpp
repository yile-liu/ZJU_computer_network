#include "Global.h"
#include <errno.h>
#include <queue>
#include <vector>

using namespace std;

const int keepalive = 1;
const int keepidle = 10;
const int keepinterval = 1;
const int keepcount = 5;

unsigned char recv_buffer[READ_BUFFER_SIZE];
client_info list[MAX_CLIENT_NUM];

int client_fd;

void getTime(int state, int source_fd, int dest_fd);
void getName(int, int, int);
void getList(int state, int fd, int i);
void sendMessage(int state, int source_fd, int dest_fd, int tr_fd);

void* threadFunction(void* args);

deque<Packet> packet_queue;

int main(int argc, char const* argv[])
{
    int port_num;
    int state = 0;
    struct sockaddr_in server_address;
    struct hostent* server;

    if (argc < 3) {
        cerr << "Syntax : ./client <host name> <port>" << endl;
        return 0;
    }

    port_num = atoi(argv[2]);

    if ((port_num > 65535) || (port_num < 2000)) {
        cerr << "Please enter port number between 2000 - 65535" << endl;
        return 0;
    }

    server = gethostbyname(argv[1]);

    if (server == nullptr) {
        cerr << "Host does not exist" << endl;
        return 0;
    }
    memset((char*)&server_address, 0, sizeof(server_address)); //initial
    server_address.sin_family = AF_INET;
    memmove((char*)&server_address.sin_addr.s_addr, (char*)server->h_addr, server->h_length);

    server_address.sin_port = htons(port_num);
    cout << "+----------------------------------+\n";
    cout << "|   Welcome to use this client!    |\n";
    cout << "+----------------------------------+\n";
    cout << "| 1 : connect to server            |\n";
    cout << "| 2 : disconnect from server       |\n";
    cout << "| 3 : get time                     |\n";
    cout << "| 4 : get server name              |\n";
    cout << "| 5 : get client list              |\n";
    cout << "| 6 : send message                 |\n";
    cout << "| 7 : exit                         |\n";
    cout << "| 8 : send 100 times               |\n";
    cout << "+----------------------------------+\n";

    pthread_t childthread;

    int command = 0;
    int interperterOn = 1;
    while (interperterOn) {
        cout << "Please input your command:\n";
        scanf("%d", &command);
        switch (command) {
        case 1: { //connect
            //create client socket
            client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            // configure the socket
            setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
#ifdef __APPLE__
            setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPALIVE, &keepidle, sizeof(keepidle));
#else
            setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
#endif
            setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepinterval, sizeof(keepinterval));
            setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcount, sizeof(keepcount));

            if (client_fd < 0) {
                cerr << "Cannot open socket" << endl;
                return 0;
            }

            // connect(client_fd, server_address);
            int check = connect(client_fd, (struct sockaddr*)&server_address, sizeof(server_address));
            if (0 == check) {
                // success
                state = 1;
                // start the background thread
                pthread_create(&childthread, nullptr, threadFunction, (void*)&client_fd);
                cout << "Connection success" << endl;
                break;
            } else {
                cerr << "Connection failed" << endl;
                break;
            }
        }
        case 2: { //disconnect
            if (state == 0)
                cout << "No connection\n";
            else {
                close(client_fd);
                pthread_cancel(childthread);
                cout << "Close socket success\n";
            }
            break;
        }
        case 3: { //get time
            getTime(state, client_fd, -1);

            while (packet_queue.empty())
                ;

            if (packet_queue.empty()) {
                cout << "no data" << endl;
            }

            time_t valread = -1;

            for (auto iter = packet_queue.begin(); iter != packet_queue.end();) {
                if (iter->header.op == TIME) {
                    valread = *(time_t*)(iter->body.data);
                    packet_queue.erase(iter);
                    break;
                } else {
                    iter++;
                }
            }

            if (valread == -1) {
                cout << "No time packet received" << endl;
            } else {
                cout << ctime(&valread) << endl;
            }
            break;
        }
        case 4: { //get name
            getName(state, client_fd, -1);

            while (packet_queue.empty())
                ;

            if (packet_queue.empty()) {
                cout << "No data" << endl;
            }
            string name = "";
            auto iter = packet_queue.begin();
            for (; iter != packet_queue.end();) {
                if (iter->header.op == NAME) {
                    name = (char*)(iter->body.data);
                    packet_queue.erase(iter);
                    break;
                } else {
                    iter++;
                }
            }

            if (name == "")
                cout << "No name packet received or name is empty" << endl;
            else
                cout << "name is: " << name << endl;
            break;
        }
        case 5: { //get client list
            getList(state, client_fd, -1);

            while (packet_queue.empty())
                ;

            if (packet_queue.empty()) {
                cout << "no data" << endl;
            }
            int n = -1;
            cout << "+---------------------+\n";
            cout << "|    Active clients   |\n";
            cout << "+---------------------+\n";
            for (auto iter = packet_queue.begin(); iter != packet_queue.end();) {
                if (iter->header.op == ACTIVE_LIST) {
                    n = *(int*)(iter->body.data);
                    for (int i = 0; i < n; ++i) {
                        if (iter->body.list[i].isThisMyfd == 1)
                            cout << "num: " << iter->body.list[i].num << " (me)" << endl;
                        else
                            cout << "num: " << iter->body.list[i].num << endl;
                        cout << "ip: " << iter->body.list[i].ip << endl;
                        cout << "port: " << iter->body.list[i].port << endl;
                        cout << "+---------------------+\n";
                    }
                    packet_queue.erase(iter);
                    break;
                } else {
                    iter++;
                }
            }

            if (n == -1) {
                cout << "No name packet received or name is empty" << endl;
            } else {
                cout << "Total number of client is: " << n << endl;
            }
            break;
        }
        case 6: { //send message
            cout << "Please input your fd and destination fd:\n";
            int tr_fd = -1;
            int dest_fd = -1;

            scanf("%d %d", &tr_fd, &dest_fd);
            sendMessage(state, client_fd, dest_fd, tr_fd);

            // block wait
            while (packet_queue.empty())
                ;

            if (packet_queue.empty()) {
                cout << "no data" << endl;
            }
            string isOk = "";

            for (auto iter = packet_queue.begin(); iter != packet_queue.end();) {
                if (iter->header.op == MESSAGE && iter->header.type == 0) {
                    isOk = "ok";
                    if (iter->header.source == dest_fd && isOk == "ok")
                        cout << "Get message done!" << endl;

                    packet_queue.erase(iter);
                    break;
                } else if (iter->header.type == 2) {
                    isOk = "error";
                    cout << "No such client linked" << endl;
                    packet_queue.erase(iter);
                    break;
                } else {
                    iter++;
                }
            }
            if (isOk == "") {
                cout << "No send packet received " << endl;
            } else if (isOk == "error") {
                cout << "Use command 5 to find the linked client " << endl;
            } else {
                cout << "Get a reply. Send success." << endl;
            }
            break;
        }
        case 7: { //exit
            close(client_fd);
            pthread_cancel(childthread);
            cout << "Exit success.\n";
            interperterOn = 0;
            break;
        }
        case 8: {
            for (int i = 0; i < 100; i++) {
                getTime(state, client_fd, -1);
            }
            cout << "Send 100 time request\n";
            int count = 0;
            for (int i = 0; i < 100; i++) {

                if (packet_queue.empty()) {
                    cout << "no data" << endl;
                }
                time_t valread = -1;

                for (auto iter = packet_queue.begin(); iter != packet_queue.end();) {
                    if (iter->header.op == TIME) {
                        valread = *(time_t*)(iter->body.data);
                        count++;
                        packet_queue.erase(iter);
                        break;
                    } else {
                        iter++;
                    }
                }

                if (valread == -1) {
                    cout << "No time packet received" << endl;
                } else {
                    cout << ctime(&valread) << endl;
                }
            }

            cout << "Got " << count << " time reply." << endl;

            break;
        }

        default: {
            cout << "invalid command!\n";
            break;
        }
        }
        if (interperterOn == 0)
            break;
    }
    return 0;
}

void* threadFunction(void* args)
{
    while (1) {
        int flag = 1;
        memset(recv_buffer, 0, READ_BUFFER_SIZE);

        int head_total = sizeof(Packet);
        int received = 0;
        int bytes = 0;
        int sockfd = *(int*)args;
        Packet temp;
        do {
            bytes = read(sockfd, &temp, head_total - received);
            if (bytes < 0) {
                perror("ERROR reading recv_buffer from socket");
                flag = 0;
                break;
            }
            if (bytes == 0) {
                //the server is closed
                cout << "the server is closed\n";
                flag = 0;
                break;
            }
            received += bytes;
        } while (received < head_total);

        Packet* phead = (Packet*)recv_buffer;
        int data_length = phead->header.length - sizeof(PacketHeader);

        if (data_length > 0 && data_length > sizeof(Packet) - sizeof(PacketHeader)) {
            //need extra read
            received = 0;
            do {
                bytes = read(sockfd, recv_buffer + head_total + received, data_length - received);

                if (bytes < 0)
                    perror("ERROR reading recv_buffer from socket");
                if (bytes == 0) //the server is closed
                {
                    break;
                }
                received += bytes;

            } while (received < data_length);
        }

        packet_queue.push_front(temp);
        if (packet_queue.front().header.op == MESSAGE && packet_queue.front().header.type == 1) {
            Packet to_send(packet_queue.front().header.destination, packet_queue.front().header.source, sizeof(Packet), 0, MESSAGE, nullptr);
            strncpy((char*)to_send.body.data, "ok", strlen("ok") + 1); //copy data
            send(client_fd, &to_send, sizeof(Packet), 0);
            cout << "destination:" << to_send.header.destination << "src " << to_send.header.source << " reply succuess\n";
            packet_queue.pop_front();
        }
        if (!flag) {
            break;
        }
    }
}

void getTime(int state, int source_fd, int dest_fd)
{
    if (state == 0) {
        perror("Connect first!\n");
        return;
    }

    Packet to_send(source_fd, dest_fd, sizeof(Packet), 1, TIME, nullptr);
    send(source_fd, &to_send, sizeof(Packet), 0);
}

void sendMessage(int state, int source_fd, int dest_fd, int true_s_fd)
{
    if (state == 0) {
        perror("Connect first!\n");
        return;
    }

    Packet to_send(true_s_fd, dest_fd, sizeof(Packet), 1, MESSAGE, nullptr);
    send(source_fd, &to_send, sizeof(Packet), 0);
}

void getList(int state, int source_fd, int dest_fd)
{
    if (state == 0) {
        perror("Connect first!\n");
        return;
    }

    Packet to_send(source_fd, dest_fd, sizeof(Packet), 1, ACTIVE_LIST, nullptr);
    send(source_fd, &to_send, sizeof(Packet), 0);
}

void getName(int state, int source_fd, int dest_fd)
{
    if (state == 0) {
        perror("Connect first!\n");
        return;
    }
    Packet to_send(source_fd, dest_fd, sizeof(Packet), 1, NAME, nullptr);
    send(source_fd, &to_send, sizeof(Packet), 0);
}
