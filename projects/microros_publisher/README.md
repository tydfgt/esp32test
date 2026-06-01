# micro-ROS 双话题示例 for ESP32

> 一收一发：ESP32 ↔️ ROS2 双工通信

## 功能

| 话题 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `/esp32_counter` | ESP32 → ROS2 | `std_msgs/Int32` | 每秒发布递增计数器 |
| `/esp32_led` | ROS2 → ESP32 | `std_msgs/Int32` | `0`=关 LED, `非0`=开 LED |

## 架构

```
ESP32 (micro-ROS client) ──WiFi/UDP──▶ micro-ROS Agent (PC) ──DDS──▶ ROS2 网络
       PUB: /esp32_counter ◀──────────────────────────────────── ros2 topic echo
       SUB: /esp32_led      ◀──────────────────────────────────── ros2 topic pub
```

## 硬件

| 项目 | 值 |
|------|-----|
| MCU | ESP32-D0WDQ6 |
| 板载 LED | GPIO 2 |
| 连接方式 | WiFi → UDP → micro-ROS Agent |

## 依赖

| 依赖 | 版本 | 说明 |
|------|------|------|
| `micro_ros_arduino` | 2.0.7-humble | 预编译库，Arduino 库管理器安装 |
| ROS2 | Humble | PC 端 |
| micro-ROS Agent | humble | 源码编译（见下方） |

## 快速开始

### 1. 修改配置

编辑 `microros_publisher.ino` 顶部：

```cpp
#define WIFI_SSID   "你的WiFi名"
#define WIFI_PASS   "你的WiFi密码"
#define AGENT_IP    "192.168.x.x"   // PC 的 IP
```

### 2. 启动 micro-ROS Agent（PC 端）

```bash
source /opt/ros/humble/setup.bash
source ~/microros_ws/install/setup.bash   # 如果源码编译了
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
```

> ⚠️ snap 版 agent 的 DDS 发现可能不兼容系统 ROS2，推荐源码编译：
> ```bash
> mkdir -p ~/microros_ws/src && cd ~/microros_ws
> git clone -b humble https://github.com/micro-ROS/micro-ROS-Agent.git src/micro_ROS_Agent
> sudo apt install -y ros-humble-micro-ros-msgs
> source /opt/ros/humble/setup.bash
> colcon build --packages-select micro_ros_agent
> ```

### 3. 编译 & 烧录（ESP32）

```bash
cd /home/cedar/esp32/projects/microros_publisher
export PATH="$HOME/.local/bin:$PATH"
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
```

### 4. 验证

```bash
source /opt/ros/humble/setup.bash

# 查看话题列表
ros2 topic list

# 看计数器
ros2 topic echo /esp32_counter

# 遥控 LED
ros2 topic pub -1 /esp32_led std_msgs/msg/Int32 "data: 1"   # 开
ros2 topic pub -1 /esp32_led std_msgs/msg/Int32 "data: 0"   # 关
```

## 代码结构

```
microros_publisher/
├── microros_publisher.ino    # 主固件（一收一发双话题）
└── .vscode/
    └── arduino.json          # VS Code Arduino 配置
```

### 关键代码片段

```cpp
// 发布者（定时器触发，每秒发布）
RCCHECK(rclc_publisher_init_best_effort(&publisher, &node, ...));
RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(1000), timer_callback));

// 订阅者（收到消息时回调）
RCCHECK(rclc_subscription_init_best_effort(&subscriber, &node, ...));
RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &sub_msg, &sub_callback, ON_NEW_DATA));

// 执行器同时处理两个 handle
RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
```

## 编译结果

| 指标 | 值 |
|------|-----|
| Flash | ~965KB / 1310KB (73%) |
| RAM | ~67KB / 327KB (20%) |

## 故障排查

| 问题 | 解决 |
|------|------|
| `ros2 topic list` 无 `/esp32_counter` | 确认 Agent 源码编译（非 snap）；确认 WiFi 和 IP 正确 |
| Agent 显示 `session established` 但话题不出现 | 确认 `source /opt/ros/humble/setup.bash` 后再 `ros2 topic list` |
| 烧录失败 "Connecting..." | 按住 BOOT → EN → 松 BOOT；或降低波特率 |
| LED 不响应 `/esp32_led` | 确认 `ros2 topic info /esp32_led` 显示 `Subscription count: 1` |

## 下一步

- 添加自定义消息类型（如传感器数据）
- 使用 `reliable` QoS 替代 `best_effort`
- 添加 service 服务调用
- 移植到墨水屏时钟项目，用 ROS2 话题远程更新时间/天气
