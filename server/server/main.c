/*
 Author: zerotrac
 Date: 2017/10/31
*/

#include <stdlib.h>

#include "logic.h"

int main(int argc, char* argv[])
{
    struct Server_logic* logic = (struct Server_logic*) malloc(sizeof(struct Server_logic));
    if (set_default(logic, argc, argv) == 1) return 1;
    return execute(logic);
}
