/*
 * micro-ROS 双话题示例 for ESP32 —— 一收一发
 *
 * 发布：/esp32_counter  (std_msgs/Int32, 每秒递增)
 * 订阅：/esp32_led      (std_msgs/Int32, 0=关LED, 非0=开LED)
 *
 * 架构：
 *   ESP32 (micro-ROS client) --[WiFi/UDP]--> micro-ROS Agent (PC) --[DDS]--> ROS2 网络
 */

#include <micro_ros_arduino.h>

#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/int32.h>

// =========================== 配置区 ===========================
#define WIFI_SSID   "ysdlab"
#define WIFI_PASS   "ysdlab123456"
#define AGENT_IP    "192.168.3.104"
#define AGENT_PORT  8888

#define PUB_TOPIC   "esp32_counter"
#define SUB_TOPIC   "esp32_led"
#define NODE_NAME   "esp32_node"
#define PUB_PERIOD  1000              // 发布周期 (ms)
// ==============================================================

// --- 发布者 ---
rcl_publisher_t   publisher;
std_msgs__msg__Int32 pub_msg;

// --- 订阅者 ---
rcl_subscription_t  subscriber;
std_msgs__msg__Int32 sub_msg;

// --- 核心 ---
rclc_support_t   support;
rcl_allocator_t  allocator;
rcl_node_t       node;
rcl_timer_t      timer;
rclc_executor_t  executor;

#define LED_PIN 2   // ESP32 板载 LED

// ---------- 容错宏 ----------
#define RCCHECK(fn)  { rcl_ret_t rc = fn; if (rc != RCL_RET_OK) { error_loop(); }}
#define RCSOFT(fn)   { fn; }

void error_loop() {
  while (1) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(150);
  }
}

// ---------- 订阅回调：接收 /esp32_led 控制板载 LED ----------
void sub_callback(const void *msgin) {
  const std_msgs__msg__Int32 *msg = (const std_msgs__msg__Int32 *)msgin;
  if (msg->data == 0) {
    digitalWrite(LED_PIN, LOW);   // 关
    Serial.println("[SUB] LED OFF");
  } else {
    digitalWrite(LED_PIN, HIGH);  // 开
    Serial.println("[SUB] LED ON");
  }
}

// ---------- 定时发布回调：每秒发布递增计数器 ----------
void timer_callback(rcl_timer_t *timer, int64_t last_call_time) {
  (void)last_call_time;
  if (timer != NULL) {
    RCSOFT(rcl_publish(&publisher, &pub_msg, NULL));
    pub_msg.data++;
  }
}

// =========================== setup ===========================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.println("[micro-ROS] Starting...");

  // WiFi + UDP 传输
  Serial.printf("[micro-ROS] WiFi: %s -> Agent: %s:%d\n",
                WIFI_SSID, AGENT_IP, AGENT_PORT);
  set_microros_wifi_transports((char*)WIFI_SSID, (char*)WIFI_PASS,
                               (char*)AGENT_IP, AGENT_PORT);
  delay(2000);
  digitalWrite(LED_PIN, HIGH);
  Serial.println("[micro-ROS] WiFi connected");

  // 初始化 micro-ROS
  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, NODE_NAME, "", &support));

  // 发布者：/esp32_counter
  RCCHECK(rclc_publisher_init_best_effort(
    &publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    PUB_TOPIC));

  // 订阅者：/esp32_led
  RCCHECK(rclc_subscription_init_best_effort(
    &subscriber, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    SUB_TOPIC));

  // 定时器
  RCCHECK(rclc_timer_init_default(
    &timer, &support,
    RCL_MS_TO_NS(PUB_PERIOD), timer_callback));

  // 执行器：同时处理 1 个定时器 + 1 个订阅 = 2 handles
  RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  RCCHECK(rclc_executor_add_subscription(
    &executor, &subscriber, &sub_msg, &sub_callback, ON_NEW_DATA));

  pub_msg.data = 0;
  Serial.println("[micro-ROS] Ready! PUB=/esp32_counter  SUB=/esp32_led");
}

// =========================== loop ===========================
void loop() {
  rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
}
