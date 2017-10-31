/*
 Author: zerotrac
 Date: 2017/10/31
*/

#include <stdlib.h>

#include "logic.c"

int main()
{
    struct Server_logic* logic = (struct Server_logic*) malloc(sizeof(struct Server_logic));
    return execute(logic);
}
