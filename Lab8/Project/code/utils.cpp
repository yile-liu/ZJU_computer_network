//
// Created by Yile Liu on 2022/12/10.
//

#include "head.h"

using namespace std;

void printSplitLine() {
    cout << "\n**********************\n";
}

/**
 *
 * @param method
 * @param uri
 * @param version
 * @param header
 * @param fd
 *
 * @attention
 * Content will be read according to the Content-Length header.
 * Do not check the border of params.
 */
void parseHttpFromSock(char *method, char *uri, char *version, char *header, char *content, int fd) {
    char buffer[BUFFER_SIZE] = {};
    char ch;
    int res = -1, state = 0, content_len = 0;
    char *content_len_ptr = nullptr;

    // 获取并解析header
    // state==0: first line includes method, request-uri, http-version
    // state==1: header
    // state==2: blank line after header
    for (int n = 0; n < BUFFER_SIZE; n++) {
        res = read(fd, &ch, 1);
        if (res > 0) {
            buffer[n] = ch;
            if (ch == '\n') {
                if (state == 0) {
                    sscanf(buffer, "%s %s %s", method, uri, version);
                    memset(buffer, 0, BUFFER_SIZE);
                    n = -1;
                    state++;
                } else if (state == 1) {
                    // header一行结束，如果这一行是Content-Length则要把后面的数字留下
                    if (strstr(buffer, "Content-Length") != nullptr) {
                        sscanf(buffer, "Content-Length: %d", &content_len);
                    }
                    // 保存此行头部，清空buffer
                    strcat(header, buffer);
                    memset(buffer, 0, BUFFER_SIZE);
                    n = -1;
                    state++;
                } else if (state == 2) {
                    break;
                }
            } else {
                if (state == 2 && ch != '\r') {
                    // 不是连续的两个换行，说明还没到content
                    state--;
                }
            }
        }
    }
    // 获取content
    for (int i = 0; i < content_len; i++) {
        read(fd, &ch, 1);
        content[i] = ch;
    }
}

/**
 *
 * @param uri: file path
 *
 * @description
 * Add prefix "../files" to uri to translate it from request uri to actual file path on server.
 * The result will be copied back to param uri without checking the border.
 */
void translateFileName(char *uri) {
    char buffer[BUFFER_SIZE] = {};
    ::strcpy(buffer, "../files");
    ::strcat(buffer, uri);
    ::strcpy(uri, buffer);
}

/**
 *
 * @param fd
 * @param state_num
 * @param msg: available iff state_num is 200
 *
 * @description
 * Send http response to a socket with a certain state number.
 */
void clientSendMsg(int fd, char *state_num, char *msg) {
    char content[BUFFER_SIZE] = {};
    char buff[BUFFER_SIZE] = {};

    if (!::strcmp(state_num, "200")) {
        sprintf(content, "<html><body>%s</body></html>", msg);
    }

    sprintf(buff + ::strlen(buff), "HTTP/1.0 %s %s\r\n", state_num, msg);
    sprintf(buff + ::strlen(buff), "Content-type: text/html\r\n");
    sprintf(buff + ::strlen(buff), "Content-length: %d\r\n\r\n", (int) strlen(content));
    ::strcat(buff, content);
    printf("clientSendMsg:\n");
    printf("%s", buff);
    printSplitLine();

    write(fd, buff, ::strlen(buff));
}

void get_filetype(char *file_name, char *file_type) {
    if (strstr(file_name, ".html"))
        strcpy(file_type, "text/html");
    else if (strstr(file_name, ".jpg"))
        strcpy(file_type, "image/jpeg");
    else
        strcpy(file_type, "text/plain");
}

/**
 *
 * @param fd
 * @param uri
 * @param size
 *
 * @description
 * send http response to a socket with state 200 and the content of file corresponding to param uri.
 */
void clientSendFile(int fd, char *uri, off_t size) {
    char *mapped_fp, filetype[BUFFER_SIZE] = {}, buff[BUFFER_SIZE] = {};

    /* Send response headers to client */
    get_filetype(uri, filetype);
    sprintf(buff, "HTTP/1.0 200 OK\r\n");
    sprintf(buff, "%sServer: A Web Server\r\n", buff);
    sprintf(buff, "%sConnection: close\r\n", buff);
    sprintf(buff, "%sContent-length: %lld\r\n", buff, size);
    sprintf(buff, "%sContent-type: %s\r\n\r\n", buff, filetype);

    write(fd, buff, ::strlen(buff));
    printf("clientSendFile.header: \n");
    printf("%s", buff);
    printSplitLine();

    /* Send response body to client */
    int file_fd = open(uri, O_RDONLY, 0);
    mapped_fp = (char *) mmap(nullptr, size, PROT_READ, MAP_PRIVATE, file_fd, 0);

    write(fd, mapped_fp, size);
    printf("clientSendFile.content: \n");
    printf("%s", mapped_fp);
    printSplitLine();

    munmap(mapped_fp, size);
    close(fd);
}