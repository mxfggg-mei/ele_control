#ifndef __SERIAL_CMD_H__
#define __SERIAL_CMD_H__

#include "config.h"

/* 函数声明 */
void serial_cmd_init(void);
void serial_cmd_process(void);
void serial_cmd_print_help(void);

#endif /* __SERIAL_CMD_H__ */