#include "RequestManager.hpp"
#include "csapp.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#define LINE_MAX_LENGTH 8192
#define BUFFER_SIZE 8192

//=====================================
// Static functions
//=====================================
static ssize_t rio_read(rio_t* rp, char* usrbuf, size_t n);
static void MakeResponse_Post(int connfd, bool log);
static void HandlePost(int fd, char* uri, rio_t* rio);
static void HandleGet(int fd, char* uri, rio_t* rio);
void* Request_Handler(void* arg);
void iClient_error(int connfd, char* err, char* shortmsg, char* longmsg);

RequestManager::RequestManager(int connfd)
    : connfd(connfd)
{
}

void RequestManager::Start()
{
    pthread_t tid;
    if (pthread_create(&tid, NULL, Request_Handler, (void*)(&connfd)) == -1) {
        perror("Error creating new thread");
        return;
    }
    pthread_join(tid, NULL);
}

void* Request_Handler(void* arg)
{
    rio_t rio;

    int connfd = *(int*)arg;
    rio_readinitb(&rio, connfd);

    char Buffer[LINE_MAX_LENGTH], method[LINE_MAX_LENGTH], uri[LINE_MAX_LENGTH], version[LINE_MAX_LENGTH];
    rio_readlineb(&rio, Buffer, LINE_MAX_LENGTH);

    sscanf(Buffer, "%s %s %s", method, uri, version);

    std::cout << "\n------------------------------------------" << std::endl;
    std::cout << "\033[31mMethod:  \033[0m" << method << std::endl;
    std::cout << "\033[31mURI:     \033[0m" << uri << std::endl;
    std::cout << "\033[31mVERSION: \033[0m" << version << std::endl;
    if (!strcasecmp(method, "POST")) {
        HandlePost(connfd, uri, &rio);
        pthread_exit(NULL);
    } else if (!strcasecmp(method, "GET")) {
        HandleGet(connfd, uri, &rio);
        pthread_exit(NULL);
    } else {
        iClient_error(connfd, "501", "Not implemented", "This kind of request is not supported in this server!");
        pthread_exit(NULL);
    }
}

/* Read_RequestHeader：
	Does not use any information in the request header, only reads the header
*/
void Read_RequestHeader(rio_t* rp)
{
    char Buffer[LINE_MAX_LENGTH];

    rio_readlineb(rp, Buffer, LINE_MAX_LENGTH);
    while (strcmp(Buffer, "\r\n")) {
        // The empty text line in the header of the termination request is composed of a carriage return and a line feed
        rio_readlineb(rp, Buffer, LINE_MAX_LENGTH);
        printf("%s", Buffer);
    }
    return;
}

/* URI_Prase:
	Assume that the main directory of static content is the current directory; the main directory of the executable is ./cgi-bin/, and any URI containing the string is considered a request for dynamic content
Parse the URI into a file name and an optional CGI parameter string
*/
int URI_Prase(char* uri, char* filename, char* cgiargs)
{
    char* ptr;

    if (strstr(uri, "cgi-bin")) {
        // Dynamic content：Take all the CGi parameters and convert the rest of the URI to a corresponding unix file name
        ptr = index(uri, '?');
        if (!ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        } else {
            cgiargs[0] = '\0';
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    } else {
        // static content: Clear the CGI parameter string, and then convert the URI to a relative unix path name
        strcpy(filename, "."); // begin convert1
        strcat(filename, uri); // end convert1
        if (uri[strlen(uri) - 1] == '/') {
            // slash check：If ending with "/", append the default file name to the end
            strcat(filename, "home.html"); // append default
        }
        return 1;
    }
}

/* get_filetype:
	derive file type from file name
*/
void get_filetype(char* filename, char* filetype)
{
    if (!filename || !filetype)
        return;
    if (strstr(filename, ".html")) {
        strcpy(filetype, "text/html");
    } else if (strstr(filename, ".jpg")) {
        strcpy(filetype, "image/jpeg");
    } else if (strstr(filename, ".gif")) {
        strcpy(filetype, "image/gif");
    } else {
        strcpy(filetype, "text/plain");
    }
}

/* serve_static:
	Send an HTTP response with the body containing the contents of a local file
*/
void serve_static(int fd, char* filename, int filesize)
{
    char *srcp, filetype[LINE_MAX_LENGTH], buf[LINE_MAX_LENGTH];
    int srcfd;

    // send response headers to client
    get_filetype(filename, filetype); // get filetype

    sprintf(buf, "HTTP/1.0 200 OK\r\n Server: Tiny Web Server\r\n Content-length: %d\r\n Content-type: %s\r\n\r\n", filesize, filetype); // begin serve
    rio_writen(fd, buf, strlen(buf)); // end serve

    // send response body to client
    srcfd = Open(filename, O_RDONLY, 0); // open
    srcp = (char*)Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    // mmap: map the requested file to a virtual memory space,
    // Call mmap to map the first filesize bytes of file srcfd to a private read-only memory area starting at address srcp
    Close(srcfd);
    // Perform the actual file transfer to the client, copy the filesize bytes starting from the srcp position to the client's connected descriptor
    rio_writen(fd, srcp, filesize);
    // Frees mapped virtual memory areas to avoid potential memory leaks
    Munmap(srcp, filesize);
}

/* serve_dynamic:
	Forking a child process and running a CGI program in the context of the child process to provide various types of dynamic content
*/
void serve_dynamic(int fd, char* filename, char* cgiargs)
{
    char buf[LINE_MAX_LENGTH], *emptylist[] = { NULL };

    // return first part of HTTP response
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { // child
        // real server sould set all CGI vars here
        setenv("QUERY_STRING", cgiargs, 1); // setenv
        Dup2(fd, STDOUT_FILENO); // redirect stdout to client, dup2
        Execve(filename, emptylist, environ); // run CGI program, execve
    }
    Wait(NULL); // parent waits for and reaps chile
}

void HandleGet(int connfd, char* uri, rio_t* rio)
{
    int isStatic;
    char FileName[LINE_MAX_LENGTH], cgi_Args[LINE_MAX_LENGTH];
    struct stat sbuf;

    Read_RequestHeader(rio);
    // parse URI from GET request
    isStatic = URI_Prase(uri, FileName, cgi_Args);
    if (stat(FileName, &sbuf) < 0) {
        iClient_error(connfd, "404", "Not found", "Tiny could not find this file");
        return;
    }

    if (isStatic) {
        // static content：Verify that static content is a normal file with read permission
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            iClient_error(connfd, "403", "Forbidden", "Tiny could not read the file");
            return;
        }
        serve_static(connfd, FileName, sbuf.st_size);
    } else {
        // dynamic sontent：Dynamic content, verify that it is executable, and if so, provide dynamic content
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            iClient_error(connfd, "403", "Forbidden", "Tiny could not run the CGI program");
            return;
        }
        serve_dynamic(connfd, FileName, cgi_Args);
    }
}

void HandlePost(int connfd, char* uri, rio_t* rio)
{
    char Buffer[LINE_MAX_LENGTH];
    char login[LINE_MAX_LENGTH], pass[LINE_MAX_LENGTH];
    bool log = false;
    if (strcasecmp(uri, "/html/dopost") || strcasecmp(uri, "/dopost")) {
        int contentlength;
        rio_readlineb(rio, Buffer, LINE_MAX_LENGTH);
        std::cout << Buffer;

        /* Read till \r\n appear */
        while (strcmp(Buffer, "\r\n")) {
            if (strstr(Buffer, "Content-Length:"))
                sscanf(Buffer + strlen("Content-Length:"), "%d", &contentlength);
            rio_readlineb(rio, Buffer, LINE_MAX_LENGTH);
            std::cout << Buffer;
        }
        std::cout << "\033[31m[Finish reading header]\033[0m" << std::endl;
        /* Now we get the body */
        rio_readlineb(rio, Buffer, contentlength + 1);
        /* Output the body */
        std::cout << "Body: [[" << Buffer << "]]" << std::endl;
        if (strstr(Buffer, "login=") && strstr(Buffer, "pass=")) {
            char* p = strstr(Buffer, "login=");
            int li = 0, pi = 0;
            while (*p++ != '=')
                ;
            while (*p != '&') {
                login[li++] = *p++;
            }
            login[li] = 0;
            while (*p++ != '=')
                ;
            while (*p) {
                pass[pi++] = *p++;
            }
            pass[pi] = 0;
        }
        /* Login status check */
        std::cout << "Username: " << login << "\nPasswd: " << pass << std::endl;
        if (!strcmp(login, "3170103746") && !strcmp(pass, "12345"))
            log = true;
        else
            log = false;

        /* Return status */
        MakeResponse_Post(connfd, log);
        return;
    } else {
        iClient_error(connfd, "404", "Not found", "This kind of resource is not available in this server");
        return;
    }
}

void MakeResponse_Post(int connfd, bool log)
{
    char* failmsg = "<html><body>Fail</body></html>";
    char* successmsg = "<html><body>Success</body></html>";
    char Buffer[LINE_MAX_LENGTH];

    sprintf(Buffer, "HTTP/1.0 200 OK\r\nServer: Server Content-length: %d\r\nContent-type: text/html\r\n\r\n", log ? strlen(successmsg) : strlen(failmsg));

    /* Log in successful */

    rio_writen(connfd, Buffer, strlen(Buffer));
    if (log)
        rio_writen(connfd, successmsg, strlen(successmsg));
    else
        rio_writen(connfd, failmsg, strlen(failmsg));
}

void iClient_error(int connfd, char* err, char* shortmsg, char* longmsg)
{
    char Buffer[LINE_MAX_LENGTH], body[LINE_MAX_LENGTH];
    sprintf(body, "%s: %s\r\n%s\r\nError msg from Web Server\r\n", err, shortmsg, longmsg);

    sprintf(Buffer, "HTTP/1.0 %s %s\r\n", err, shortmsg);
    rio_writen(connfd, Buffer, strlen(Buffer));
    sprintf(Buffer, "Content-type: text/html\r\n");
    rio_writen(connfd, Buffer, strlen(Buffer));
    sprintf(Buffer, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(connfd, Buffer, strlen(Buffer));

    rio_writen(connfd, body, strlen(body));
}
