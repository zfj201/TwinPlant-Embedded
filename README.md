# TwinPlant-Embedded

TwinPlant 智能设备数字孪生平台的嵌入式仓库，统一管理 STM32F103、STM32F407、ESP32-S3 三套固件。

TwinPlant 是一个"嵌入式实时控制 + 3D 数字孪生"平台，面向学习、展示和小型场景落地。一期以**智能温室实验舱**为场景：真实传感器数据经 MCU 汇聚上云，浏览器用 Three.js 实时展示舱体和设备状态，并能远程控制执行器、看到真实反馈。后续可扩展到实验室、仓库、机房和小型工业设备。

## 相关仓库

| 仓库 | 内容 | 地址 |
|---|---|---|
| TwinPlant-Embedded（本仓库） | STM32F103、STM32F407、ESP32-S3 固件，协议文档 | https://github.com/zfj201/TwinPlant-Embedded |
| TwinPlant-Fronted | Web 前端（Vue 3 + Three.js + MQTT.js + Pinia） | https://github.com/zfj201/TwinPlant-Fronted |

两个仓库共用同一套应用层数据模型。修改 MQTT topic 或 payload 时，需要同步检查前端仓库。

## 系统架构

```
 传感器                     执行器（继电器 / RGB LED / 蜂鸣器）
   │                                   ▲
   ▼                                   │
┌──────────────┐  UART  ┌────────────────────┐  UART  ┌──────────────┐
│ STM32F103    │ ─────▶ │ STM32F407          │ ◀────▶ │ ESP32-S3     │
│ HAL + 裸机    │        │ HAL + FreeRTOS     │        │ Arduino      │
│ 采集与校准    │        │ 汇聚 / 规则 / 控制   │        │ Wi-Fi / MQTT │
└──────────────┘        └────────────────────┘        └──────┬───────┘
                                                             │ MQTT over TCP
                                                             ▼
                                                     ┌──────────────┐
                                                     │ EMQX Broker  │
                                                     └──────┬───────┘
                                                            │ MQTT over WebSocket
                                                            ▼
                                                     ┌──────────────────────┐
                                                     │ 浏览器                │
                                                     │ Vue 3 + Three.js     │
                                                     │ MQTT.js              │
                                                     └──────────────────────┘
```

上行：传感器 → F103 → F407 → ESP32-S3 → EMQX → 浏览器。
下行：浏览器发出控制请求 → EMQX → ESP32-S3 → F407 执行，并沿原路返回控制应答。

一期不开发业务后端，也不引入数据库。浏览器通过 MQTT over WebSocket 直接连接 EMQX。

### 各节点职责

| 节点 | 技术栈 | 职责 |
|---|---|---|
| STM32F103C8T6 最小系统板 | CubeMX + HAL，裸机 | 采集温湿度、光照、土壤湿度、水位，完成基础校准，按固定周期通过 UART 发给 F407 |
| STM32F407ZGT6 综合板 | CubeMX + HAL + FreeRTOS（CMSIS-RTOS v2） | 汇聚数据、执行规则、控制执行器、维护故障状态机和看门狗；继电器上电默认关闭 |
| ESP32-S3 N16R8 | Arduino + PlatformIO | Wi-Fi 入网、MQTT 连接、断线自动重连，在 F407 的 UART 协议与 MQTT 消息之间做转换 |
| EMQX | MQTT Broker | 设备与浏览器之间的消息中转，提供 TCP 和 WebSocket 两种接入 |
| 浏览器 | Vue 3 + Three.js + MQTT.js | 3D 场景、实时状态、设备详情、远程控制和调试面板 |

### 一期技术选型

| 环节 | 方案 |
|---|---|
| F103 | HAL + 裸机 |
| F407 | HAL + FreeRTOS |
| F103 ↔ F407 | UART |
| F407 ↔ ESP32-S3 | UART |
| ESP32-S3 | Arduino + PlatformIO |
| ESP32-S3 ↔ Broker | MQTT over TCP |
| Broker | EMQX |
| Web ↔ Broker | MQTT over WebSocket |
| Web MQTT 客户端 | MQTT.js |
| Web 3D | Vue 3 + Three.js |
| 业务后端 | 暂不开发 |
| 数据库 | 暂不引入 |

产品需求文档的技术要点里写的是 ESP32-S3 用 ESP-IDF、通信用 WebSocket 或 MQTT。一期按上表执行：ESP32-S3 用 Arduino，通信用 MQTT。后续如需切换，应用层数据模型保持不变。

## 数据协议原则

两段 UART 链路和 MQTT 消息使用同一套应用层数据模型：

- 每条消息都包含 `deviceId`、`timestamp`、`type`、`value`、`quality`、`sequence` 和协议版本。
- 消息类型包括：心跳、数据上报、控制请求、控制应答、错误码。
- 控制请求带 `requestId`，控制应答原样带回，用于把一次控制在设备日志和前端日志里串起来。
- UART 帧格式和 MQTT topic 设计以 `docs/` 中的协议文档为准（协议 V1 待编写）。C 端共用的帧定义和错误码放在 `shared/`。

## 分期目标

### 一期：真实硬件闭环 MVP（4～6 周）

打通"采集—通信—展示—控制—反馈"闭环，在真实硬件上连续运行，并在浏览器里实时看到。

- **采集**：温湿度、光照、土壤湿度、水位。MPU6050 可选，只用于姿态或振动演示。
- **执行器**：继电器控制风扇或灯；RGB LED 表示在线、告警和工作状态；蜂鸣器用于本地报警。
- **F407 FreeRTOS 任务**：传感器采集、串口通信、控制、报警、看门狗。用队列和事件组解耦采集、通信和控制。
- **ESP32-S3**：Wi-Fi 入网、MQTT 长连接、断线自动重连、上报连接状态。
- **Web 页面**：3D 总览页、设备详情抽屉、调试面板（显示原始消息、收发方向、序列号、延迟和错误码）。

验收标准：

1. 连续运行 24 小时，传感器每 2 秒上报一次，页面没有明显的内存增长。
2. Wi-Fi 断开再恢复后，30 秒内自动重连并继续上报。
3. 页面下发继电器指令后，1 秒内看到真实执行器的反馈。
4. 任一节点断开后，一个心跳周期内 3D 场景显示离线，并保留最后一次数据。
5. 任务异常时，看门狗能复位或让设备进入安全状态，继电器默认关闭。

### 二期：设备监控与规则（6～8 周）

把一期的演示系统升级为可管理、可追溯、可自动运行的单项目平台。嵌入式侧的重点：

- F407 故障状态机、任务运行时统计、栈水位监测、SD 卡日志。
- F407 本地环形缓存；ESP32-S3 断线重连后补传，页面区分实时数据和补传数据。
- OTA：下载、完整性校验、失败重试和回滚标记。
- 阈值规则：持续时间、动作、恢复条件、手动控制优先级。
- 可选增加一种传感器，优先振动，形成"异常振动—报警—3D 高亮—日志记录"闭环。
- 引入业务后端、时序数据和报警表，协议升级到 V2。

### 三期：可配置、可交付的平台（8～12 周）

3D 场景配置器、设备与协议模板、多项目与权限、仿真模式、批量 OTA（灰度、暂停、重试、回滚）、Docker Compose 部署。传感器只按落地行业选择一种：温室优先 CO₂，设备运维优先振动，能耗场景优先电流。

## 仓库结构

```
.
├── stm32/
│   ├── f103/                 # STM32F103C8T6，HAL + 裸机
│   │   ├── f103.ioc          # CubeMX 工程，外设和时钟以它为准
│   │   ├── Core/             # CubeMX 生成
│   │   ├── Drivers/          # CubeMX 复制的 HAL / CMSIS
│   │   ├── App/{Inc,Src}/    # 业务代码，CubeMX 不会修改
│   │   └── platformio.ini
│   └── f407/                 # STM32F407ZGT6，HAL + FreeRTOS
│       ├── f407.ioc
│       ├── Core/  Drivers/  Middlewares/   # CubeMX 生成
│       ├── App/{Inc,Src}/
│       ├── fpu_flags.py      # 硬件浮点参数（编译、汇编、链接）
│       └── platformio.ini
├── esp32s3/                  # ESP32-S3 N16R8，Arduino
│   ├── src/  include/  lib/
│   └── platformio.ini
├── shared/                   # 跨固件共用的协议定义（C 头文件）
├── docs/                     # 协议、接线、烧录说明、测试报告
├── TwinPlant-Embedded.code-workspace
├── README.md
└── AGENTS.md                 # 开发规范
```

## 开发环境

- VS Code 或 Cursor，安装 PlatformIO IDE 扩展
- STM32CubeMX 6.18（F1 固件包 V1.8.7，F4 固件包 V1.28.3）
- ST-Link（用于两块 STM32 的烧录和调试）
- USB 线（用于 ESP32-S3，接板子上的 UART/COM 口）

STM32 工程不使用 PlatformIO 的 `framework = stm32cube`。HAL、CMSIS、FreeRTOS、启动文件和链接脚本都来自 CubeMX 生成的文件，PlatformIO 只负责编译、烧录和调试。工具链固定为 GCC 12.3.1，因为 PlatformIO 默认的 GCC 7.2 不支持 CubeMX 链接脚本里的 `READONLY` 关键字。

在 Cursor 中使用时，`ms-vscode.cpptools` 要装 1.24.5 之前的版本，并关闭自动更新。更新的版本会拒绝在非微软编辑器中运行 IntelliSense，导致无法跳转到定义。

## 编译与烧录

用 VS Code 或 Cursor 打开 `TwinPlant-Embedded.code-workspace`，三个工程会作为三个 PlatformIO 项目出现。如果状态栏没有出现 PlatformIO 按钮，点一下侧边栏的 PlatformIO 图标。

也可以在对应工程目录下用命令行：

```bash
pio run
```

```bash
pio run -t upload
```

```bash
pio device monitor
```

| 工程 | 目录 | 烧录方式 | 当前时钟 |
|---|---|---|---|
| F103 | `stm32/f103` | ST-Link（SWD） | HSE 8MHz → 72MHz |
| F407 | `stm32/f407` | ST-Link（SWD） | HSE 8MHz → 168MHz |
| ESP32-S3 | `esp32s3` | USB 串口 | 240MHz，16MB Flash，8MB PSRAM |

## 当前进度

- [x] 三套工程初始化，PlatformIO 统一编译
- [x] F407 时钟按 8MHz 晶振配置，FreeRTOS 线程安全（newlib reentrant）已开启
- [ ] 三块板子上板验证（LED 闪烁、串口输出）
- [ ] 硬件接线图和引脚分配
- [ ] 协议 V1（UART 帧格式、MQTT topic、消息字段）
- [ ] F103 传感器采集
- [ ] F407 FreeRTOS 任务框架
- [ ] ESP32-S3 Wi-Fi、MQTT 和串口桥接
- [ ] 全链路联调和一期验收
