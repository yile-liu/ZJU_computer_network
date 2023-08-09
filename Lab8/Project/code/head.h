#ifndef LAB8_HEAD

#define LAB8_HEAD

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <cstring>

const int BUFFER_SIZE = 2048;
const int MAX_CONNECTION = 100;
const int SOCK_PORT = 2708;

#endif
