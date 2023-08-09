//
// Created by Yile Liu on 2022/12/10.
//

#include "utils.cpp"

using namespace std;

class Server {
public:
    Server();

    ~Server();

    int startAcceptClnt();

private:
    pthread_t tid;
    int serv_sock = -1;
    struct sockaddr_in serv_addr;
};

Server::Server() {
    // socket()
    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    cout << "socket()完成 服务程序句柄: " << serv_sock << endl;

    // bind()
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY即本机IP
    serv_addr.sin_port = htons(SOCK_PORT);

    int res = ::bind(serv_sock, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    if (res != -1) {
        cout << "bind()完成 绑定IP: " << htonl(INADDR_ANY) << endl;
    } else {
        cout << "bind()发生错误: " << errno << endl;
        exit(errno);
    }

    // listen()
    res = listen(serv_sock, MAX_CONNECTION);
    if (res != -1) {
        cout << "listen()就绪" << endl;
    } else {
        cout << "listen()发生错误: " << errno << endl;
        exit(errno);
    }
    printSplitLine();
}

Server::~Server() {
    close(serv_sock);
}

void *handleRequest(void *arg) {
    int sock = *(int *) arg;

    char method[BUFFER_SIZE];
    char uri[BUFFER_SIZE];
    char ver[BUFFER_SIZE];
    char header[BUFFER_SIZE];
    char content[BUFFER_SIZE];
    struct stat file_stat;

    parseHttpFromSock(method, uri, ver, header, content, sock);

    if (!::strcasecmp(method, "GET")) {
        translateFileName(uri);
        if (stat(uri, &file_stat) < 0) {
            // cannot find the file
            clientSendMsg(sock, "404", "Not found");
        } else if ((S_IRUSR & file_stat.st_mode) == 0) {
            // have no auth. to open the file
            clientSendMsg(sock, "403", "Forbidden");
        } else {
            //response
            clientSendFile(sock, uri, file_stat.st_size);
        }
    }
    else if (!::strcasecmp(method, "POST")) {
        if (strcmp(uri, "/html/dopost")) {
            // if the uri is not /dopost
            clientSendMsg(sock, "404", "Not found");
        } else if (!strcmp(content, "login=3200102708&pass=2708")) {
            // content correct
            clientSendMsg(sock, "200", "登录成功");
        } else {
            // content incorrect
            clientSendMsg(sock, "200", "登录失败");
        }
    }
    pthread_exit(nullptr);
}


int Server::startAcceptClnt() {
    int clnt_sock = -1;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    while (true) {
        clnt_sock = accept(serv_sock, (struct sockaddr *) (&clnt_addr), &clnt_addr_size);
        if (clnt_sock == -1) {
            cout << "无法建立连接" << endl;
        } else {
            cout << "成功建立连接" << endl
                 << "客户端IP: " << clnt_addr.sin_addr.s_addr << endl
                 << "客户端端口号: " << ntohs(clnt_addr.sin_port) << endl;
            // 新建服务线程
            pthread_t tid;
            pthread_create(&tid, NULL, &handleRequest, (void *) (&clnt_sock));
        }
        printSplitLine();
    }

    return 0;
}

int main() {
    Server *myServer = new Server();

    myServer->startAcceptClnt();
    return 0;
}
