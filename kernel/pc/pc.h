#ifndef _PC_H
#define _PC_H

#include <xsu/pc.h>

void pc_create_union(int asid, void (*func)(), unsigned int init_gp, char* name);

#endif