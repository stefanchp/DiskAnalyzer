#include "common.h"
#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>


int create_server_socket(void)
{
    int fd;
    struct sockaddr_un addr;
    fd = socket(AF_UNIX, SOCK_STREAM, 0);

    unlink(SOCKET_PATH);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(fd, 10);
    chmod(SOCKET_PATH, 0666);
    return fd;
}