#ifndef CMD_MANAGER_H
#define CMD_MANAGER_H
#include "stdint.h"
#include "frame.h"

void Cmd_Dispatch(const Frame_t *frame);
#endif