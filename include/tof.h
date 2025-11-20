#pragma once
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// 传感器数量（如需变更在 tof.cpp 同步修改）
size_t tof_count();

// 初始化专用 I2C 控制器与多颗 VL53L1X（含 XSHUT 上电与改地址、连续测距）
void tof_setup();

// 读取全部 ToF 距离（mm），out_len >= tof_count()
// 返回 true 表示全部成功（无超时）；若存在超时仍会填入读取值
bool tof_read_all(uint16_t* out, size_t out_len);

#ifdef __cplusplus
}
#endif