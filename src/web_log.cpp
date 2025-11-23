#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>
#include "web_log.h"

// WiFi 配置（如需改为 AP，仅修改 webTask 内的 WiFi 初始化逻辑）
static const char* WIFI_SSID = "NEUsmartcar";
static const char* WIFI_PWD  = "NEUCARNB";

// WebServer 实例仅在本模块内可见
static WebServer server(80);

// 与主程序共享的舵机角度
volatile int g_servoPos = 90;

// 距离数值
volatile uint16_t g_distance1 = 0;
volatile uint16_t g_distance2 = 0;

// 当前速度 -1~1
volatile float g_speed = 0.0f;

// 新增: 电机与舵机启用状态变量
volatile bool g_motorEnabled = false;
volatile bool g_servoEnabled = false;

// 日志缓冲
#define LOG_CAP 100
static String g_logs[LOG_CAP];
static volatile int g_logIdx = 0;
static portMUX_TYPE g_logMux = portMUX_INITIALIZER_UNLOCKED;

void addLog(const String &m)
{
    portENTER_CRITICAL(&g_logMux);
    g_logs[g_logIdx] = m;
    g_logIdx = (g_logIdx + 1) % LOG_CAP;
    portEXIT_CRITICAL(&g_logMux);
    Serial.println(m);
}

String buildLogs()
{
    String out;
    portENTER_CRITICAL(&g_logMux);
    int idx = g_logIdx;
    for (int i = 0; i < LOG_CAP; ++i)
    {
        int pos = (idx + i) % LOG_CAP;
        if (g_logs[pos].length())
        {
            out += g_logs[pos] + "\n";
        }
    }
    portEXIT_CRITICAL(&g_logMux);
    return out;
}

// 新增: 电机启用接口
static void handleMotor()
{
    if (server.hasArg("toggle"))
    {
        g_motorEnabled = !g_motorEnabled;
        addLog(String("MotorToggle -> ") + (g_motorEnabled ? "ON" : "OFF"));
    }
    else if (server.hasArg("set"))
    {
        int v = server.arg("set").toInt();
        g_motorEnabled = (v != 0);
        addLog(String("MotorSet -> ") + (g_motorEnabled ? "ON" : "OFF"));
    }
    server.send(200, "text/plain", String(g_motorEnabled ? "1" : "0"));
}

// 新增: 舵机启用接口
static void handleServoEnable()
{
    if (server.hasArg("toggle"))
    {
        g_servoEnabled = !g_servoEnabled;
        addLog(String("ServoEnableToggle -> ") + (g_servoEnabled ? "ON" : "OFF"));
    }
    else if (server.hasArg("set"))
    {
        int v = server.arg("set").toInt();
        g_servoEnabled = (v != 0);
        addLog(String("ServoEnableSet -> ") + (g_servoEnabled ? "ON" : "OFF"));
    }
    server.send(200, "text/plain", String(g_servoEnabled ? "1" : "0"));
}

static void handleRoot()
{
    String page =
    "<!DOCTYPE html><html><head><meta charset='utf-8'/>"
    "<title>Control</title></head><body>"
    "<h3>Servo</h3>"
    "<input type='range' min='10' max='125' value='" + String(g_servoPos) + "' id='servo' oninput='setServo(this.value)'/>"
    "<p id='servoVal'>" + String(g_servoPos) + "</p>"
    "<h3>Speed (-1..1)</h3>"
    "<input type='range' min='-1' max='1' step='0.01' value='" + String(g_speed, 2) + "' id='speed' oninput='setSpeed(this.value)'/>"
    "<p id='speedVal'>" + String(g_speed, 2) + "</p>"
    "<h3>Distance</h3>"
    "<div>D1: <span id='dist1'>--</span> mm</div>"  // 传感器1距离
    "<div>D2: <span id='dist2'>--</span> mm</div>"  // 传感器2距离
    "<h3>Motor / Servo Enable</h3>"
    "<div>Motor: <span id='motorState'>--</span> <button onclick='toggleMotor()'>Toggle</button></div>"
    "<div>Servo: <span id='servoState'>--</span> <button onclick='toggleServoEnable()'>Toggle</button></div>"
    "<h3>Logs</h3><pre id='log' style='background:#111;color:#0f0;padding:8px;height:200px;overflow:auto;font-size:12px;'></pre>"
    "<script>"
    "function setServo(v){servoVal.innerText=v;fetch('/servo?pos='+v);}"
    "function setSpeed(v){speedVal.innerText=v;fetch('/speed?val='+v);}"
    "function pollLogs(){fetch('/logs').then(r=>r.text()).then(t=>{log.textContent=t;log.scrollTop=log.scrollHeight;});}"
    "function pollDist(){fetch('/distance1').then(r=>r.text()).then(t=>{dist1.innerText=t;});fetch('/distance2').then(r=>r.text()).then(t=>{dist2.innerText=t;});}"
    "function refreshStates(){fetch('/motor').then(r=>r.text()).then(t=>{motorState.innerText=t=='1'?'ON':'OFF';});fetch('/servo_enable').then(r=>r.text()).then(t=>{servoState.innerText=t=='1'?'ON':'OFF';});}"
    "function toggleMotor(){fetch('/motor?toggle=1').then(_=>refreshStates());}"
    "function toggleServoEnable(){fetch('/servo_enable?toggle=1').then(_=>refreshStates());}"
    "function syncServo(){fetch('/servo').then(r=>r.text()).then(t=>{servo.value=t;servoVal.innerText=t;});}"
    "function syncSpeed(){fetch('/speed').then(r=>r.text()).then(t=>{speed.value=t;speedVal.innerText=t;});}"
    "setInterval(pollLogs,1000);setInterval(pollDist,300);setInterval(syncServo,1500);setInterval(syncSpeed,1500);setInterval(refreshStates,2000);"
    "pollLogs();pollDist();syncServo();syncSpeed();refreshStates();"
    "</script></body></html>";
    server.send(200, "text/html", page);
}

static void handleLogs()
{
    server.send(200, "text/plain", buildLogs());
}

static void handleServo()
{
    if (server.hasArg("pos"))
    {
        int p = server.arg("pos").toInt();
        if (p < 10) p = 10;
        if (p > 125) p = 125;
        g_servoPos = p;        // 仅更新变量
        addLog("ServoReq " + String(p));
        server.send(200, "text/plain", String(p));
    }
    else
    {
        server.send(200, "text/plain", String(g_servoPos)); // 支持 GET 当前值
    }
}

static void handleDistance1()
{
    server.send(200, "text/plain", String(g_distance1));
    
}
static void handleDistance2()
{
server.send(200, "text/plain", String(g_distance2));
}


static void handleSpeed()
{
    if (server.hasArg("val"))
    {
        float v = server.arg("val").toFloat();
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        g_speed = v;           // 仅更新变量
        addLog("SpeedReq " + String(v, 2));
        server.send(200, "text/plain", String(v, 2));
    }
    else
    {
        server.send(200, "text/plain", String(g_speed, 2));
    }
}

static void webTask(void *pv)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    while (WiFi.status() != WL_CONNECTED)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    addLog("WiFi connected, IP: " + WiFi.localIP().toString());

    server.on("/", handleRoot);
    server.on("/servo", handleServo);
    server.on("/logs", handleLogs);
    server.on("/distance1", handleDistance1);
    server.on("/distance2", handleDistance2);
    server.on("/speed", handleSpeed);
    server.on("/motor", handleMotor);
    server.on("/servo_enable", handleServoEnable);
    server.begin();
    addLog("HTTP server started");

    for (;;)
    {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void web_log_start()
{
    // 固定到 core1 运行 Web 服务
    xTaskCreatePinnedToCore(webTask, "webTask", 4096, nullptr, 1, nullptr, 1);
}