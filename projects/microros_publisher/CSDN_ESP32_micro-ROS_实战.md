# ESP32 通过 micro-ROS 接入 ROS2 双工通信：从零到双话题收发（附踩坑全指南）

> 本文记录了在 ESP32 上运行 micro-ROS，实现 ROS2 话题**一收一发**双工通信的完整过程，包含环境搭建、代码详解、Agent 桥接原理，以及踩过的所有坑和解决方案。

---

## 一、背景

[ROS2](https://docs.ros.org/en/humble/) 是机器人领域最主流的中间件框架，但其设计面向 Linux 主机，微控制器很难直接运行完整的 ROS2 协议栈。**micro-ROS** 是 ROS2 官方推出的轻量方案，专为 MCU（ESP32、STM32 等）设计，通过以下架构桥接：

```
┌──────────┐   WiFi/UDP    ┌──────────────────┐    DDS     ┌──────────────┐
│  ESP32   │ ────────────▶ │ micro-ROS Agent  │ ─────────▶ │  ROS2 网络   │
│ (Client) │ ◀──────────── │   (PC 上运行)     │ ◀───────── │ (ros2 topic) │
└──────────┘               └──────────────────┘            └──────────────┘
```

- **ESP32 端**：跑 `micro_ros_arduino` 库，通过 UDP 与 Agent 通信
- **Agent 端**：跑在 PC 上，将 micro-ROS 协议翻译为 DDS 标准协议，接入 ROS2 网络
- **通信链路**：ESP32 → WiFi → UDP → Agent → DDS → ROS2

本文实现一个双话题示例：

| 话题 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `/esp32_counter` | ESP32 → ROS2 | `std_msgs/Int32` | 每秒发布递增计数器 |
| `/esp32_led` | ROS2 → ESP32 | `std_msgs/Int32` | `0`=关 LED，`非0`=开 LED |

---

## 二、环境信息

| 组件 | 版本/型号 |
|------|-----------|
| 操作系统 | Ubuntu 22.04.5 LTS (x86_64) |
| ROS2 | Humble (`/opt/ros/humble/`) |
| MCU | ESP32-D0WDQ6 v1.1 (Xtensa 双核, Flash 4MB) |
| Arduino CLI | v1.5.0 (`~/.local/bin/arduino-cli`) |
| ESP32 Arduino Core | `esp32:esp32@3.3.8` |
| 烧录端口 | `/dev/ttyUSB0` |
| 板载 LED | GPIO 2 |

---

## 三、ESP32 端 — 安装 micro_ros_arduino

> ⚠️ **坑点预警**：版本必须和 ROS2 发行版匹配！

```bash
export PATH="$HOME/.local/bin:$PATH"

# 查看可用版本
arduino-cli lib search micro_ros_arduino
# 输出包含：2.0.5-humble, 2.0.7-humble, 3.0.0-iron 等

# 安装与 ROS2 Humble 匹配的版本
arduino-cli lib install "micro_ros_arduino@2.0.7-humble"
```

> 如果错误安装了 `3.0.0-iron`（对应 ROS2 Iron），会和 Humble 的 Agent 不兼容。先卸载再重装：
> ```bash
> arduino-cli lib uninstall micro_ros_arduino
> arduino-cli lib install "micro_ros_arduino@2.0.7-humble"
> ```

---

## 四、完整代码（双话题收发）

```cpp
#include <micro_ros_arduino.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>

// ==================== 配置 ====================
#define WIFI_SSID   "你的WiFi"
#define WIFI_PASS   "你的密码"
#define AGENT_IP    "192.168.3.104"  // PC 的 IP
#define AGENT_PORT  8888

#define PUB_TOPIC   "esp32_counter"
#define SUB_TOPIC   "esp32_led"
#define NODE_NAME   "esp32_node"
#define PUB_PERIOD  1000   // ms
#define LED_PIN     2
// =============================================

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

// 容错宏：关键步骤失败则进入 error_loop，非关键步骤失败忽略
#define RCCHECK(fn)  { rcl_ret_t rc = fn; if (rc != RCL_RET_OK) { error_loop(); }}
#define RCSOFT(fn)   { fn; }

void error_loop() {
  while (1) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(150);
  }
}

// ---------- 订阅回调：遥控 LED ----------
void sub_callback(const void *msgin) {
  const std_msgs__msg__Int32 *msg = (const std_msgs__msg__Int32 *)msgin;
  digitalWrite(LED_PIN, msg->data == 0 ? LOW : HIGH);
  Serial.printf("[SUB] LED %s\n", msg->data ? "ON" : "OFF");
}

// ---------- 定时发布回调：每秒发送递增计数器 ----------
void timer_callback(rcl_timer_t *timer, int64_t last_call_time) {
  (void)last_call_time;
  if (timer != NULL) {
    RCSOFT(rcl_publish(&publisher, &pub_msg, NULL));
    pub_msg.data++;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 1. WiFi + UDP 传输
  set_microros_wifi_transports((char*)WIFI_SSID, (char*)WIFI_PASS,
                               (char*)AGENT_IP, AGENT_PORT);
  delay(2000);
  digitalWrite(LED_PIN, HIGH);  // WiFi 已连，亮灯

  // 2. 初始化 micro-ROS 核心
  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, NODE_NAME, "", &support));

  // 3. 创建发布者（best_effort 适合 UDP）
  RCCHECK(rclc_publisher_init_best_effort(
    &publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    PUB_TOPIC));

  // 4. 创建订阅者
  RCCHECK(rclc_subscription_init_best_effort(
    &subscriber, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    SUB_TOPIC));

  // 5. 创建定时器
  RCCHECK(rclc_timer_init_default(
    &timer, &support,
    RCL_MS_TO_NS(PUB_PERIOD), timer_callback));

  // 6. 创建执行器（2 个 handle：定时器 + 订阅）
  RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  RCCHECK(rclc_executor_add_subscription(
    &executor, &subscriber, &sub_msg, &sub_callback, ON_NEW_DATA));

  pub_msg.data = 0;
  Serial.println("[micro-ROS] Ready!");
}

void loop() {
  rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
}
```

### 代码关键点解析

1. **`set_microros_wifi_transports()`** — 一键完成 WiFi 连接 + UDP 传输注册，是 `micro_ros_arduino` 封装的便利函数
2. **`RCCHECK` vs `RCSOFT`** — 初始化阶段用 `RCCHECK`（失败则卡死闪灯），运行期用 `RCSOFT`（容忍丢包）
3. **执行器 handle 数** — 定时器 + 订阅 = 2，`rclc_executor_init` 第三个参数必须 ≥ 实际 handle 数
4. **`best_effort` QoS** — UDP 传输不可靠，用尽力而为模式；如果走串口可用 `reliable`

---

## 五、PC 端 — 编译 micro-ROS Agent

> ⚠️ **这是本文最大的坑！详见第七章踩坑指南。**

### 5.1 为什么必须源码编译

micro-ROS Agent 可通过三种方式安装：

| 方式 | 结果 |
|------|------|
| `sudo snap install micro-ros-agent` | ❌ DDS 发现失败，话题不出现 |
| Docker | ❌ 本机未装 Docker |
| **源码编译** | ✅ 唯一可行方案 |

snap 版本虽然能收到 ESP32 的 UDP 包并建立 session，但其沙箱环境限制了 DDS 多播发现能力，导致 ROS2 网络始终看不到 micro-ROS 话题。

### 5.2 源码编译步骤

```bash
# 1. 创建工作空间
mkdir -p ~/microros_ws/src && cd ~/microros_ws

# 2. 克隆 Humble 分支
git clone -b humble https://github.com/micro-ROS/micro-ROS-Agent.git src/micro_ROS_Agent

# 3. 安装依赖
sudo apt install -y ros-humble-micro-ros-msgs

# 4. 编译
source /opt/ros/humble/setup.bash
colcon build --packages-select micro_ros_agent

# 5. 启动 Agent
source install/setup.bash
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
```

编译成功输出：`Summary: 1 package finished [2.85s]`

### 5.3 启动 Agent（运行命令）

```bash
source /opt/ros/humble/setup.bash
source ~/microros_ws/install/setup.bash
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
```

看到 `[info] running... | port: 8888` 即启动成功，等待 ESP32 连接。

---

## 六、编译烧录 & 验证

### 6.1 编译烧录 ESP32

```bash
cd /path/to/microros_publisher
export PATH="$HOME/.local/bin:$PATH"
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
```

编译结果：
```
Sketch uses 965011 bytes (73%) of program storage space.
Global variables use 66912 bytes (20%) of dynamic memory.
```

micro-ROS 库约占 Flash 的 73%，RAM 的 20%，剩余空间充裕。

### 6.2 验证话题

```bash
source /opt/ros/humble/setup.bash

# 查看话题
ros2 topic list
# 输出：
# /esp32_counter
# /esp32_led
# /parameter_events
# /rosout

# 看计数器（每秒递增）
ros2 topic echo /esp32_counter
# data: 42
# data: 43
# data: 44
# ...

# 遥控 LED 开关
ros2 topic pub -1 /esp32_led std_msgs/msg/Int32 "data: 1"   # LED 开
ros2 topic pub -1 /esp32_led std_msgs/msg/Int32 "data: 0"   # LED 关

# 查看订阅状态
ros2 topic info /esp32_led
# Type: std_msgs/msg/Int32
# Publisher count: 0
# Subscription count: 1    ← ESP32 正在订阅
```

---

## 七、踩坑指南（必读）

### 坑 ①：snap 版 Agent 不可用

**症状**：Agent 日志显示 `session established`、`publisher created`，但 `ros2 topic list` 永远看不到 micro-ROS 话题。

**原因**：snap 沙箱限制了 DDS 多播发现。

**解决**：源码编译 Agent（见第五章），不要用 snap。

### 坑 ②：micro_ros_arduino 版本不匹配

**症状**：编译报错、Agent 连接后反复重连、publisher 创建失败。

**排查**：
```bash
cat ~/Arduino/libraries/micro_ros_arduino/library.properties | grep version
# version=2.0.7-humble  ← 必须和系统 ROS2 发行版匹配
```

**解决**：卸载后安装匹配版本：
```bash
arduino-cli lib uninstall micro_ros_arduino
# ROS2 Humble → 2.0.7-humble
# ROS2 Iron   → 3.0.0-iron
arduino-cli lib install "micro_ros_arduino@2.0.7-humble"
```

### 坑 ③：串口无输出 / 忘记 Serial.begin()

**症状**：烧录后 ESP32 没有串口调试信息，无法判断状态。

**原因**：ESP32 Arduino 框架虽然默认初始化 `Serial`，但如果 micro-ROS 初始化失败可能卡在 `error_loop()` 里，看不到任何输出。

**解决**：在 `setup()` 最开头必须调用 `Serial.begin(115200)`，并在关键步骤加 `Serial.println()` 打点。

### 坑 ④：执行器 handle 数不足

**症状**：订阅不生效，但发布正常。

**原因**：`rclc_executor_init(&executor, &support.context, N, &allocator)` 的 N 必须 ≥ 实际 handle 总数。

**示例**：
```cpp
// 1 个定时器 + 1 个订阅 = 2 个 handle
RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
RCCHECK(rclc_executor_add_timer(&executor, &timer));
RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &sub_msg, &sub_callback, ON_NEW_DATA));
```

如果把 `2` 写成 `1`，最后一个 `add_subscription` 会静默失败。

### 坑 ⑤：Agent 启动时机

**症状**：先烧录 ESP32，后启动 Agent，ESP32 长时间连不上。

**原因**：`micro_ros_arduino` 库的 WiFi UDP 传输在初始化时不会无限重连。

**解决**：
- **方法一**：先启动 Agent，再烧录/重启 ESP32（推荐）
- **方法二**：如果 ESP32 已运行，重新烧录一次触发硬复位

### 坑 ⑥：ESP32 和 PC 不在同一网段

**症状**：Agent 日志完全看不到 `[==>> UDP <<==]`，ESP32 LED 不亮。

**排查**：
```bash
# PC 端查看 IP
hostname -I
# 假设输出 192.168.3.104 192.168.2.50

# 确认 ESP32 连接的 WiFi 在哪个网段
# 必须在同一网段！例如都是 192.168.3.x
```

**解决**：修改 `.ino` 中 `AGENT_IP` 为正确的网段 IP。

### 坑 ⑦：编译报错 `micro_ros_msgsConfig.cmake not found`

**症状**：`colcon build` 时报错找不到 `micro_ros_msgs`。

**解决**：
```bash
sudo apt install -y ros-humble-micro-ros-msgs
```

---

## 八、架构原理补充

### 8.1 micro-ROS 协议栈

```
ESP32 应用层 (Arduino sketch)
     │
     ▼
rclc (micro-ROS 客户端 C 库)
     │
     ▼
rmw_microxrcedds (micro-ROS 中间件)
     │
     ▼
Micro-XRCE-DDS Client (串行化 + 传输)
     │
     ▼ UDP ───────────────────────┐
                                  ▼
                          Micro-XRCE-DDS Agent (PC)
                                  │
                                  ▼
                          DDS (Fast-DDS / eProsima)
                                  │
                                  ▼
                          ROS2 Graph (话题发现/消息路由)
```

### 8.2 为什么用 best_effort 而不是 reliable

| QoS | 适用场景 | ESP32 适用性 |
|-----|---------|-------------|
| `reliable` | 不允许丢消息（如关键指令） | 内存有限，重传开销大 |
| `best_effort` | 允许丢消息（如传感器数据流） | ✅ 低开销，适合 WiFi UDP |

对于 LED 控制（偶尔丢一帧无所谓）和计数器（丢了下次还能看到），`best_effort` 完全够用。

---

## 九、扩展方向

1. **移植到墨水屏时钟**：用 ROS2 话题远程更新时间、天气数据、切换显示模式
2. **添加自定义消息**：定义 `.msg` 文件，传递传感器数据
3. **使用 Service**：改为请求-响应模式，比如 `GetSensorData.srv`
4. **多节点**：一个 ESP32 上创建多个 micro-ROS 节点
5. **串口传输**：适合对延迟敏感的场景，替代 UDP

---

## 十、参考链接

- [micro-ROS 官方文档](https://micro.ros.org/)
- [micro_ros_arduino GitHub](https://github.com/micro-ROS/micro_ros_arduino)
- [micro-ROS Agent GitHub](https://github.com/micro-ROS/micro-ROS-Agent)
- [ROS2 Humble 文档](https://docs.ros.org/en/humble/)

---

> **总结**：micro-ROS 让 ESP32 真正融入 ROS2 生态。最大的坑就是 Agent 安装方式，**请务必源码编译**。搞定这个，剩下的 API 其实和标准 `rclcpp` 高度相似，学习曲线平缓。希望本文帮你少走弯路 🚀
