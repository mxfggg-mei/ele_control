/**
 ****************************************************************************************************
 * @file        oled.h
 * @author      md
 * @version     V1.0
 * @date        2026-06-09
 * @brief       OLED 显示驱动模块 - 独立子模块
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#ifndef __OLED_H
#define __OLED_H

#include <Arduino.h>

/* 函数声明 */
void oled_init(void);           /* 初始化 OLED 并显示开机画面 */
void oled_update(void);         /* 更新 OLED 显示内容（非阻塞，按间隔刷新） */
void oled_force_update(void);   /* 强制下一次更新（忽略刷新间隔） */

#endif /* __OLED_H */