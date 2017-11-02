/*
 Author: zerotrac
 Date: 2017/11/01
*/

#ifndef USERDATA_H
#define USERDATA_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "const.h"

struct User_data
{
    int closed;
    int deleted; // delete flag in order to remove the event from queue
    
    int client_type;
    int connection_type;
    struct User_data* transport_pt;
    struct User_data* accept_pt;
    struct User_data* connect_pt;
    
    
    int login_status;
    char username[USERNAME_MAX];
    char relative_path[RECEIVE_DATA_MAX];
    char file_path[RECEIVE_DATA_MAX];
    
    char host[20];
    int port;
    int fd;
    int pasvfd;
    int which_stream;
    FILE* filefd;
};

void User_initialize(struct User_data* user_data, int _client_type, void* _connect_pt, int fd);
void close_all(struct User_data* user_data);

#endif
