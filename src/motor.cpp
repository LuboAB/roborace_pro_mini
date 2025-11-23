#include "motor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

ESP32Encoder g_encoder;
volatile int64_t g_encoderCount = 0;        // 最近一次(2ms)采样脉冲数
static volatile int64_t g_encoderTotal = 0; // 累积总数（可选）
static TaskHandle_t s_encTask = nullptr;
volatile float target, adjust;
float speed_setting = 0;
float kp = 2.0f;
float ki = 0.1f;
float kd = 0.0f;
float last_error = 0.0f;
float last_last_error = 0.0f;
// 增量式 PID 控制计算
float pid_incremental(float target, float current, float *last_error, float *last_last_error,
                      float kp, float ki, float kd)
{
    float error = target - current;
    float delta_u = kp * (error - (*last_error)) + ki * error + kd * (error - 2 * (*last_error) + (*last_last_error));
    *last_last_error = *last_error;
    *last_error = error;
    return delta_u;
}
float kp_turn = 0.1f;
float ki_turn = 0.0f;
float kd_turn = 0.05f;
float last_error_turn = 0.0f;
float last_last_error_turn = 0.0f;
volatile int F, R, L;
float pid_position(float target, float current, float *last_error,
                   float kp, float ki, float kd)
{
    float error = target - current;
    float u = kp * error + ki * error + kd * (error - (*last_error));
    *last_error = error;
    return u;
}

float culc_speed(float F)
{
    if (F >= 800)
        return 100;
    else
        return 100 - 150 / 800 * (800 - F);
}
// 200 Hz 编码器处理任务：每 5 ms 读取增量后清零
extern Servo myservo;
extern volatile float servoPos;
static void encoder_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(5); // 5 ms -> 200 Hz
    for (;;)
    {
        int64_t delta = g_encoder.getCount(); // 读取自上次清零以来的增量

        uint16_t dis[3];
        uint8_t status[3];
        tof_read_all_with_status(dis, status, 3);

        L = dis[0];
        // if (L == 0 || status[0] == 2)
        // {
        //     L = 2000;
        // }
        // R = dis[1];
        // if (R == 0 || status[1] == 2)
        // {
        //     R = 2000;
        // }
        // F = dis[2];
        // if (F == 0 || status[2] == 2)
        // {
        //     F = 2000;
        // }
        if (L == 0 || status[0] != 0)
        {
            L = 2000;
        }
        R = dis[1];
        if (R == 0 || status[1] != 0)
        {
            R = 2000;
        }
        F = dis[2];
        if (F == 0 || status[2] != 0)
        {
            F = 2000;
        }
        float servoPos = 80 + pid_position(0, R - L, &last_error_turn, kp_turn, ki_turn, kd_turn);
        if (!g_servoEnabled)
        {
            servoPos = 90;
        }

        if (servoPos < 40)
            servoPos = 40;
        if (servoPos > 135)
            servoPos = 135;
        target = culc_speed(F);

        myservo.write(servoPos);
        // target = g_speed * 500;
        adjust = pid_incremental(target, (float)delta, &last_error, &last_last_error, kp, ki, kd);

        Serial.println(L);
        speed_setting += adjust / 500;
        if (!g_motorEnabled)
        {
            speed_setting = 0;
        }
        set_speed(speed_setting);
        g_encoderCount = delta;  // 保存本周期增量
        g_encoderTotal += delta; // 累积（需要可用，不要可忽略）
        g_encoder.clearCount();  // 清零以便下一周期增量
        vTaskDelayUntil(&last, period);
    }
}

// 获取最近一次周期脉冲数
int64_t motor_get_last_delta() { return g_encoderCount; }
// 获取累积总数
int64_t motor_get_total() { return g_encoderTotal; }

void motor_init()
{
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, MOTOR_PWM_PIN);
    mcpwm_config_t cfg = {};
    cfg.frequency = 10000; // 10 kHz
    cfg.cmpr_a = 0;        // 初始占空比 0%
    cfg.counter_mode = MCPWM_UP_COUNTER;
    cfg.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);

    g_encoder.attachSingleEdge(47, 45);
    g_encoder.clearCount(); // 计数归零

    pinMode(13, OUTPUT);
    if (s_encTask == nullptr)
    {
        xTaskCreatePinnedToCore(
            encoder_task, "enc500Hz",
            2048, nullptr,
            3, &s_encTask,
            0 // 选定核心，可按需要调整
        );
    }
}
float max_duty = 1.0f; // 最大占空比限制，防止过载
void pwm_set_duty(float duty)
{
    if (duty < 0)
        duty = 0;
    // if (duty > 1)
    //     duty = 1;
    if (duty > max_duty)
        duty = max_duty;
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty * 100.0f);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
}

void set_speed(float speed)
{
    if (speed > 0)
    {
        float duty = speed;
        pwm_set_duty(duty);
        digitalWrite(13, HIGH); // 正转
    }
    else
    {
        float duty = -speed;
        pwm_set_duty(duty);
        digitalWrite(13, LOW); // 反转
    }
}