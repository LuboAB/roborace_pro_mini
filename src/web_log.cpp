#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>
#include "web_log.h"

// WiFi 配置（如需改为 AP，仅修改 webTask 内的 WiFi 初始化逻辑）
static const char *WIFI_SSID = "NEUsmartcar";
static const char *WIFI_PWD = "NEUCARNB";

// WebServer 实例仅在本模块内可见
static WebServer server(80);

// 与主程序共享的舵机角度
volatile int g_servoPos = 90;

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
      "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'/>"
      "<title>Car Monitor</title>"
      "<style>"
      "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#111;color:#eee;}"
      "header{position:sticky;top:0;z-index:10;background:#20232a;padding:10px 12px;border-bottom:1px solid #333;}"
      "header h2{margin:0;font-size:18px;}"
      ".ip{font-size:12px;color:#aaa;margin-top:2px;}"
      "main{padding:10px 12px 80px 12px;}"
      "section{margin-bottom:14px;padding:10px;border-radius:8px;background:#1b1f24;border:1px solid #333;}"
      "section h3{margin:0 0 6px 0;font-size:15px;}"
      ".row{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-top:4px;flex-wrap:wrap;}"
      ".label{font-size:13px;color:#ccc;}"
      ".btn{padding:8px 14px;font-size:14px;border-radius:999px;border:none;cursor:pointer;outline:none;}"
      ".btn-toggle-on{background:#28a745;color:#fff;}"
      ".btn-toggle-off{background:#6c757d;color:#fff;}"
      ".pill{display:inline-block;padding:3px 10px;border-radius:999px;font-size:12px;margin-left:4px;}"
      ".pill-on{background:#28a74533;color:#7CFC00;border:1px solid #28a74599;}"
      ".pill-off{background:#ff000033;color:#ff8080;border:1px solid #ff000099;}"
      "#log{background:#000;color:#0f0;padding:8px;border-radius:6px;height:32vh;overflow:auto;font-size:11px;white-space:pre;}"
      ".dist-col{flex:1 1 0;min-width:0;text-align:center;}"
      ".dist-value{font-size:20px;font-weight:600;}"
      ".dist-label{font-size:11px;color:#999;}"
      ".state-label{font-size:11px;color:#888;margin-top:2px;}"
      ".footer-note{font-size:11px;color:#666;text-align:center;margin-top:6px;}"
      ".small{font-size:11px;color:#888;}"
      "</style>"
      "</head><body>"
      "<header>"
      "<h2>Car Monitor</h2>"
      "<div class='ip'>WiFi Monitor · Mobile Friendly</div>"
      "</header>"
      "<main>"

      // 距离 + 状态：横向 3 列
      "<section>"
      "<h3>距离传感器 (mm)</h3>"
      "<div class='row'>"
      "<div class='dist-col'>"
      "<div class='dist-label'>Left</div>"
      "<div class='dist-value'><span id='dist1'>--</span></div>"
      "<div class='state-label'>state_L: <span id='state1'>--</span></div>"
      "</div>"
      "<div class='dist-col'>"
      "<div class='dist-label'>Front</div>"
      "<div class='dist-value'><span id='dist0'>--</span></div>"
      "<div class='state-label'>state_F: <span id='state0'>--</span></div>"
      "</div>"
      "<div class='dist-col'>"
      "<div class='dist-label'>Right</div>"
      "<div class='dist-value'><span id='dist2'>--</span></div>"
      "<div class='state-label'>state_R: <span id='state2'>--</span></div>"
      "</div>"
      "</div>"
      "<div class='small'>回传周期：<span id='freqVal'>50</span> ms</div>"
      "</section>"

      "<section>"
      "<h3>启停控制</h3>"
      "<div class='row' style='margin-bottom:6px;'>"
      "<div class='label'>Motor"
      "<span id='motorPill' class='pill pill-off'>OFF</span>"
      "</div>"
      "<button id='motorBtn' class='btn btn-toggle-off' onclick='toggleMotor()'>convert</button>"
      "</div>"
      "<div class='row'>"
      "<div class='label'>Servo"
      "<span id='servoEPill' class='pill pill-off'>OFF</span>"
      "</div>"
      "<button id='servoEBtn' class='btn btn-toggle-off' onclick='toggleServoEnable()'>convert</button>"
      "</div>"
      "</section>"

      "<section>"
      "<h3>系统日志</h3>"
      "<pre id='log'></pre>"
      "<div class='footer-note'>日志每秒自动刷新，可用于调试和状态查看</div>"
      "</section>"

      "</main>"

      "<script>"
      "var distInterval = 50;"
      "document.addEventListener('DOMContentLoaded', function(){"
      "document.getElementById('freqVal').innerText = distInterval;"
      "});"
      "function updateToggleUI(id,val){"
      "var on = (val==='1');"
      "if(id==='motor'){"
      "motorPill.className = 'pill ' + (on?'pill-on':'pill-off');"
      "motorPill.textContent = on ? 'ON' : 'OFF';"
      "motorBtn.className = 'btn ' + (on?'btn-toggle-on':'btn-toggle-off');"
      "}else if(id==='servoE'){"
      "servoEPill.className = 'pill ' + (on?'pill-on':'pill-off');"
      "servoEPill.textContent = on ? 'ON' : 'OFF';"
      "servoEBtn.className = 'btn ' + (on?'btn-toggle-on':'btn-toggle-off');"
      "}"
      "}"
      "function pollLogs(){"
      "fetch('/logs').then(r=>r.text()).then(t=>{"
      "log.textContent=t;"
      "log.scrollTop=log.scrollHeight;"
      "}).catch(()=>{});"
      "}"
      // 这里假设你另外有 /state0 /state1 /state2 三个接口返回 state_F/L/R
      "function pollDist(){"
      "fetch('/distance0').then(r=>r.text()).then(t=>{dist0.innerText=t;}).catch(()=>{});"
      "fetch('/distance1').then(r=>r.text()).then(t=>{dist1.innerText=t;}).catch(()=>{});"
      "fetch('/distance2').then(r=>r.text()).then(t=>{dist2.innerText=t;}).catch(()=>{});"
      "fetch('/state0').then(r=>r.text()).then(t=>{state0.innerText=t;}).catch(()=>{});"
      "fetch('/state1').then(r=>r.text()).then(t=>{state1.innerText=t;}).catch(()=>{});"
      "fetch('/state2').then(r=>r.text()).then(t=>{state2.innerText=t;}).catch(()=>{});"
      "}"
      "function refreshStates(){"
      "fetch('/motor').then(r=>r.text()).then(t=>{updateToggleUI('motor',t.trim());}).catch(()=>{});"
      "fetch('/servo_enable').then(r=>r.text()).then(t=>{updateToggleUI('servoE',t.trim());}).catch(()=>{});"
      "}"
      "function toggleMotor(){"
      "fetch('/motor?toggle=1').then(()=>refreshStates()).catch(()=>{});"
      "}"
      "function toggleServoEnable(){"
      "fetch('/servo_enable?toggle=1').then(()=>refreshStates()).catch(()=>{});"
      "}"
      "setInterval(pollLogs,1000);"
      "setInterval(pollDist,distInterval);"
      "setInterval(refreshStates,2000);"
      "pollLogs();pollDist();refreshStates();"
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
    if (p < 10)
      p = 10;
    if (p > 125)
      p = 125;
    g_servoPos = p; // 仅更新变量
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
  server.send(200, "text/plain", String(distance_L));
}
static void handleDistance2()
{
  server.send(200, "text/plain", String(distance_R));
}
static void handleDistance0()
{
  server.send(200, "text/plain", String(distance_F));
}
static void handleState1()
{
  server.send(200, "text/plain", String(state_L));
}
static void handleState2()
{
  server.send(200, "text/plain", String(state_R));
}
static void handleState0()
{
  server.send(200, "text/plain", String(state_F));
}
static void handleSpeed()
{
  if (server.hasArg("val"))
  {
    float v = server.arg("val").toFloat();
    if (v > 1)
      v = 1;
    if (v < -1)
      v = -1;
    g_speed = v; // 仅更新变量
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
  server.on("/distance0", handleDistance0);
  server.on("/distance1", handleDistance1);
  server.on("/distance2", handleDistance2);
  server.on("/state0", handleState0);
  server.on("/state1", handleState1);
  server.on("/state2", handleState2);
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