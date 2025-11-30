#ifndef _WEB_LOG_H
#define _WEB_LOG_H

#include <Arduino.h>
#include "motor.h"
// 在另一核心启动 Web 服务任务
void web_log_start();

// 供全局使用的日志接口
void addLog(const String& m);
String buildLogs();

// 舵机角度与速度（-1~1）仅由网页写，主循环读后执行
extern volatile int g_servoPos;
extern volatile float g_speed;      // 当前速度 -1~1

extern volatile bool g_motorEnabled;
extern volatile bool g_servoEnabled;

#endif