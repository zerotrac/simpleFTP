/*
 Author: zerotrac
 Date: 2017/11/01
*/

#ifndef USERDATA_H
#define USERDATA_H

#include "const.h"

struct User_data
{
    int monitor_pos;
    int login_status;
    char username[USERNAME_MAX];
};

void User_initialize(struct User_data* user_data, int _monitor_pos);

#endif
