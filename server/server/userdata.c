/*
 Author: zerotrac
 Date: 2017/11/01
*/

#include "userdata.h"

void User_initialize(struct User_data* user_data, int _client_type, void* _connect_pt, int _fd)
{
    user_data->closed = 0;
    user_data->deleted = 0;
    
    user_data->client_type = _client_type;
    user_data->connection_type = CONNECTION_NONE;
    user_data->connect_pt = _connect_pt;
    
    user_data->login_status = LOGIN_NOUSERNAME;
    strcpy(user_data->relative_path, "/");
    
    user_data->fd = _fd;
    user_data->which_stream = STREAM_RETR;
    user_data->filefd = NULL;
}

void close_all(struct User_data* user_data)
{
    user_data->deleted = 1;
    user_data->closed = 1;
    close(user_data->fd);
    struct User_data* fa = user_data->connect_pt;
    if (fa->connection_type == CONNECTION_PASV)
    {
        fa->accept_pt->deleted = 1;
        fa->accept_pt->closed = 1;
        close(fa->accept_pt->fd);
    }
}
