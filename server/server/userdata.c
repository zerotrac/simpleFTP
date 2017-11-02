/*
 Author: zerotrac
 Date: 2017/11/01
*/

#include "userdata.h"

void User_initialize(struct User_data* user_data, int _monitor_pos)
{
    user_data->monitor_pos = _monitor_pos;
    user_data->login_status = LOGIN_NOUSERNAME;
}
