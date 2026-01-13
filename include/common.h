#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

#define SOCKET_PATH "/tmp/disk_analyzer.sock"


typedef enum{
    CMD_ADD, 
    CMD_SUSPEND,
    CMD_RESUME,
    CMD_REMOVE, 
    CMD_INFO, 
    CMD_LIST, 
    CMD_PRINT
} command_t;


typedef struct{
    command_t type;
    char path[256];
    int priority;
    int id;
} request_t;


typedef struct{
    int status_code;
    char message[8192];
} response_t;

#endif 