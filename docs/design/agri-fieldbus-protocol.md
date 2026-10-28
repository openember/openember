# Agri Fieldbus Protocol（OE-AFP）设计

- 状态：Draft（P0 契约 + P1 IO 卡 Mock/Master 已落地；无线 node/gateway 待 P2）
- 更新日期：2026-10-29
- 所有者：本页（现场总线与采集节点语义）；站端产品故事见 [agri-station-demo.md](./agri-station-demo.md)；HI 边界见 [hardware-interface.md](./hardware-interface.md)
- 实现落点：
  - 寄存器/缩放/Quality：`apps/agri_station_demo/agri_afp_regs.hpp`
  - 无线上联 PDU：`apps/agri_station_demo/agri_afp_uplink.hpp`
  - 实例拓扑（bus/unit/poll）：`configs/agri_fieldbus.yaml`
- 相关业界参照：Modbus RTU/TCP、温室柜内 RS485 变送器寄存器表、网关 Point Map、韩国 KS X 3267（温室 RS485/Modbus 接口思路，**不声称合规**）

## 1. 要解决什么

智慧农业站端常见两类现场设备：

| 角色 | 物理形态 | 站端看到什么 |
|------|----------|--------------|
| **IO 采集卡** | SPI / UART / RS485 / 以太网口挂在主控旁 | 一块「多通道从站」或「多从站总线」 |
| **无线采集节点 + 网关** | 节点近传感；网关 Wi‑Fi / 以太网回站 | 网关把多节点整理成与 IO 卡**同语义**的点位 |

OpenEmber Demo 要模拟的是这两类边界，而不是再发明一套与市面变送器无关的私有串口帧。  
因此 OE-AFP 约定：

1. **现场契约 = 寄存器图（Register Map）+ 点位语义**，优先落在 **Modbus**（RTU 有线、TCP 网口卡/网关）。  
2. **无线节点 → 网关** 可用更轻的 **上联 PDU**；网关负责译成同一寄存器语义或同一 OpenEmber Topic。  
3. **站内应用只订 Link Topic**；Modbus/PDU 止于 Hardware Interface（或等价采集进程）。

```text
传感探头
  ├─(柜内有线)──► IO 卡 / RS485 变送器 ──Modbus──► HI(Master) ──Topic──► App/UI
  └─(田边无线)──► 采集节点 ──上联 PDU──► 网关 ──Modbus 或 Topic──► HI/聚合 ──Topic──► App/UI
```

## 2. 非目标

- 不做完整 BACnet / OPC UA 栈。  
- 不做实驱水泵/风机的控制闭环（寄存器预留，Demo 只读监测）。  
- 不把 Modbus 帧直接暴露给 `agri_station_app`。  
- 不要求真无线芯片；Demo 用进程模拟「节点 / 网关 / IO 卡」即可。

## 3. 分层

| 层 | 名称 | 内容 |
|----|------|------|
| L0 | 承载 | RS485、UART TTL、SPI（模拟成串行字节流）、以太网、Wi‑Fi（经网关） |
| L1 | 现场会话 | **Modbus RTU**（有线主从）/ **Modbus TCP**（网口卡、网关南向或北向） |
| L1w | 无线上联 | **OE-AFP Uplink**（节点→网关；见 §7） |
| L2 | 点位语义 | 本页寄存器图与 Quality / 通道编号 |
| L3 | 站内契约 | `openember-msgs` + `/agri/...` Topic（见 §8） |

L1 与 L1w **可互换**：同一通道读数无论来自 IO 卡还是无线网关，L2/L3 字段一致。

## 4. 寻址模型

```text
station_id     站（一台 Linux 边缘机 / 一个 launch 实例）
  bus_id       现场总线或虚拟总线（如 rs485-0、eth-io-0、wlan-gw-0）
    unit_id    Modbus 从站地址 1–247（IO 卡整卡可占 1 个 unit；或一卡多 unit）
      channel  逻辑通道 0–N（如 10 组温湿）
```

Demo 默认：

| 符号 | 默认值 | 含义 |
|------|--------|------|
| `station_id` | `agri-station-demo` | 与现有 instance 对齐 |
| `bus_id` | `io0` / `gw0` | IO 卡总线 / 无线网关 |
| `unit_id` | `1`（IO 卡）、`10+`（无线节点经网关映射） | 从站号 |
| `channel` | `0..9` | 最多 10 组温湿示意 |

`sensor_id`（发到 Topic）建议稳定字符串：

```text
{bus_id}/u{unit_id}/ch{channel}/{quantity}
# 例：io0/u1/ch3/air_temperature
#     gw0/u12/ch0/soil_moisture
```

## 5. Modbus 会话约定（L1）

### 5.1 传输

| 模式 | 用途 | 默认参数 |
|------|------|----------|
| RTU | RS485 / UART TTL IO 卡或变送器 | 9600 8N1，CRC-16 |
| TCP | 以太网 IO 卡、网关北向 | 端口 **502**（Mock 可用 **1502** 避免 root） |

Master = OpenEmber 侧采集 Endpoint（HI Package 或 `agri_io_mock`）。  
Slave = IO 卡固件 / 变送器 / **网关暴露的虚拟从站**。

### 5.2 功能码（V1 必选）

| 码 | 用途 |
|----|------|
| `0x03` Read Holding Registers | 读传感与状态（业界传感变送器常用 Holding，本协议统一走 Holding，便于一块图讲完） |
| `0x06` Write Single Register | 写采样周期、使能位（可选；监测 MVP 可只实现读） |
| `0x10` Write Multiple Registers | 批量写配置（可选） |

不强制 Coil/Input Register；若对接真设备只有 Input Register，Package 内做地址映射，**对外 L2 仍按本表语义**。

### 5.3 轮询与超时

| 项 | 约定 |
|----|------|
| 默认轮询 | 先读信息区 16 字，再按 ≤120 字分块读通道区（Modbus FC03 上限 125） |
| 帧超时 | RTU 默认 200 ms；TCP 默认 500 ms |
| 连续失败 | ≥3 次 → 该 `unit_id` Quality=`COMM_LOST`，Topic 可停发或发带 quality 的末值策略由 HI 配置 |
| 总线负载 | 多从站时串行轮询；单从站多通道用一块连续读，避免每通道一帧 |

## 6. 规范寄存器图（L2）

地址为 **PDU 地址**（0-based）。一档 **unit** 表示「一个采集节点或一张 IO 卡上的一组逻辑设备」；多通道温湿用通道块重复。

### 6.1 设备信息区 `0x0000`–`0x000F`

| 地址 | 访问 | 类型 | 名称 | 说明 |
|------|------|------|------|------|
| 0x0000 | R | u16 | `proto_magic` | 固定 `0xAE01`（Agri Ember） |
| 0x0001 | R | u16 | `proto_version` | 高字节 major，低字节 minor；V1 = `0x0100` |
| 0x0002 | R | u16 | `device_class` | 1=IO_CARD，2=WIRELESS_NODE，3=GATEWAY_VIRTUAL_SLAVE |
| 0x0003 | R | u16 | `channel_count` | 本 unit 通道数（Demo IO 卡默认 10） |
| 0x0004 | R | u16 | `caps_bits` | bit0 温 bit1 湿 bit2 光照 bit3 土壤湿 bit4 土壤温 bit5 CO₂ |
| 0x0005 | R | u16 | `status_bits` | bit0 FAULT bit1 DEGRADED bit2 BUSY |
| 0x0006 | R | u16 | `uptime_s_lo` | 上电秒低 16 |
| 0x0007 | R | u16 | `uptime_s_hi` | 上电秒高 16 |
| 0x0008 | R/W | u16 | `heartbeat` | Master 递增；Slave 原样回读（存活） |
| 0x0009 | R/W | u16 | `sample_period_ms` | 建议采样周期；0=设备默认 |
| 0x000A | R | u16 | `last_error` | 见 §6.4 |
| 0x000B–0x000F | R | u16 | reserved | 读 0 |

### 6.2 通道块（每通道 16 寄存器）

基址：`channel_base(ch) = 0x0100 + ch * 0x0010`（ch = 0..31，V1 Demo 用 0..9）。

| 偏移 | 访问 | 类型 | 名称 | 缩放 / 语义 |
|------|------|------|------|-------------|
| +0 | R | i16 | `air_temp_c10` | 真实℃ = value / 10 |
| +1 | R | u16 | `air_humidity_pct10` | %RH = value / 10 |
| +2 | R | u16 | `illuminance_lux` | lux，饱和 65535 |
| +3 | R | u16 | `soil_moisture_pct10` | % = value / 10（相对墒情） |
| +4 | R | i16 | `soil_temp_c10` | ℃ = value / 10 |
| +5 | R | u16 | `co2_ppm` | ppm |
| +6 | R | u16 | `quality` | 见 §6.3 |
| +7 | R | u16 | `seq_lo` | 通道样本序号低 16 |
| +8 | R | u16 | `seq_hi` | 序号高 16 |
| +9 | R | u16 | `age_ms` | 距上次成功采样的毫秒（网关填；本地 IO 卡可为 0） |
| +A–+F | R | u16 | reserved / 扩展 | 读 0 |

未装备的量：寄存器可读但 `quality` 对应 bit 标 `NOT_INSTALLED`，数值为 0。

**整卡一读示例（10 通道）：** 读 Holding `0x0000` 长度 16，再读 `0x0100` 长度 `10*16`；或配置为只扫已启用通道。

### 6.3 Quality（u16 位域）

| Bit | 名 | 含义 |
|-----|-----|------|
| 0 | GOOD | 本周期数据可用 |
| 1 | STALE | 超过 2×sample_period 未更新 |
| 2 | OUT_OF_RANGE | 超设备量程 |
| 3 | SENSOR_FAULT | 探头故障 |
| 4 | NOT_INSTALLED | 本通道无此物理量 |
| 5 | COMM_LOST | 仅网关侧：节点失联（缓存末值时置位） |
| 6–15 | reserved | 0 |

应用层告警用 Topic；Quality 只表达**采集可信度**。

### 6.4 last_error 码

| 值 | 含义 |
|----|------|
| 0 | OK |
| 1 | CRC / 帧错误计数溢出提示 |
| 2 | 内部采样失败 |
| 3 | 配置非法 |
| 4 | 存储/校准缺失 |

## 7. 无线上联 PDU（L1w：节点 → 网关）

现场节点往往是 MCU + LoRa/Wi‑Fi，不宜每节点做完整 Modbus Slave。网关收上联后：

- 写入自己的 **虚拟 Modbus 从站**（供站端仍用 §5–§6），或  
- 直接由网关进程发 OpenEmber Topic（Demo 可二选一；**优先虚拟从站**，以便 IO 卡与无线路径共用一套 Master 代码）。

### 7.1 帧格式（小端）

```text
offset  size  field
0       2     magic = 0xAE10
2       1     version = 0x01
3       1     msg_type
4       2     node_id          # 映射到 unit_id（网关表）
6       1     channel
7       1     flags            # bit0=ack_req
8       2     seq
10      2     payload_len
12      N     payload
12+N    2     crc16_modbus     # 覆盖 magic..payload
```

### 7.2 msg_type

| type | 名 | payload |
|------|-----|---------|
| 0x01 | `SAMPLE` | 见下 |
| 0x02 | `HELLO` | `caps_bits` u16 + `channel_count` u16 |
| 0x03 | `GOODBYE` | 空 |
| 0x81 | `ACK` | 回显 seq（可选） |

### 7.3 SAMPLE payload

```text
u16 present_mask   # bit 同 caps：哪些量本包携带
i16 air_temp_c10
u16 air_humidity_pct10
u16 illuminance_lux
u16 soil_moisture_pct10
i16 soil_temp_c10
u16 co2_ppm
u16 quality
u32 sample_unix_s  # 节点钟；无 RTC 则填 0，网关用接收时刻
```

只带 `present_mask` 置位的字段时，仍按上表**定长**排（未带字段填 0），降低解析分支；后续若要省空中字节，可用 version=2 变长，V1 不做。

### 7.4 网关行为

| 事件 | 行为 |
|------|------|
| HELLO | 分配/确认 `unit_id`，刷新 caps |
| SAMPLE | 写入虚拟从站对应 channel 块；刷新 seq / age_ms=0 |
| 超时无 SAMPLE | age_ms 递增；超阈值 → quality\|=STALE\|COMM_LOST |
| 异频 | 允许；Master 轮询周期与节点上报周期解耦 |

## 8. 映射到 OpenEmber（L3）

HI / 采集进程读完 L2 后发布（与现有 agri demo 对齐，多通道时带 `sensor_id`）：

| 寄存器量 | Topic（建议） | 消息 |
|----------|---------------|------|
| `air_temp_c10` | `/agri/air/temperature` 或 `/agri/ch/{n}/air/temperature` | `TemperatureSample` |
| `air_humidity_pct10` | `/agri/air/humidity` 等 | `ScalarSample` quantity=`relative_humidity` |
| `illuminance_lux` | `/agri/light/illuminance` | `ScalarSample` |
| `soil_moisture_pct10` | `/agri/soil/moisture` | `ScalarSample` |
| `soil_temp_c10` | `/agri/soil/temperature` | `TemperatureSample` |
| `co2_ppm` | `/agri/air/co2` | `ScalarSample` |
| unit status / COMM_LOST | diagnostics + 可选 `/agri/station/status` | 由 App 聚合 |

**单通道 MVP** 可继续用如今的扁平 Topic；**≥2 通道** 推荐：

- Topic 仍扁平，靠 `sensor_id` 区分；或  
- `/agri/ch/{channel}/...` 与寄存器 channel 一致。

缩放在 Package 内完成（℃、%RH），Topic 上为物理单位浮点，与现有 Mock 一致。

## 9. Demo 进程划分（建议）

```text
launch_manager
  ├── agri_station_app          # 只订 Topic
  ├── agri_io_card_mock         # Modbus TCP Slave @1502，device_class=IO_CARD，10 ch
  ├── agri_field_master         # Modbus Master → 译 Topic（可先做独立进程，后并入 HI）
  ├── agri_radio_node_mock ×N   # 异频发 Uplink SAMPLE（UDP 即可）
  └── agri_radio_gateway_mock   # 收 Uplink → 虚拟 Slave 或直发 Topic
```

分期：

| 阶段 | 交付 |
|------|------|
| P0 | 本文契约 + `agri_afp_regs.hpp` / `agri_afp_uplink.hpp` + `configs/agri_fieldbus.yaml` | **已落地** |
| P1 | `openember_agri_io_card_mock` + `openember_agri_field_master` → `/agri/...`；Master 日志打印多通道；UI 订 `ui-channel`（默认 ch0） | **已落地** |
| P2 | 无线 node/gateway Mock；与 IO 卡共用 Master | |
| P3 | 真 RS485 变送器 Package（地址映射到本图或适配表） | |

## 10. 实例配置

拓扑示例见仓库根下 [`configs/agri_fieldbus.yaml`](../../configs/agri_fieldbus.yaml)。  
YAML **只**描述 bus / unit / 端口 / 轮询；寄存器偏移与缩放以头文件为准（`proto: oe-afp/1.0` 对应 `afp::kProtoId`）。

## 11. 成功标准

1. 同一套 Master 代码轮询「IO 卡 Mock」与「网关虚拟从站」无需改 App。  
2. 10 通道温湿可从寄存器连续读出并出现在 UI / 日志。  
3. 关掉一个 `radio_node_mock` → 对应 channel Quality 含 COMM_LOST/STALE，其它通道仍更新。  
4. 文档中的 magic / version / 缩放与实现头文件一致，可作为对外集成说明。

## 12. 修订

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | 2026-10-29 | 初稿：Modbus 寄存器图 + 无线上联 PDU + Topic 映射 |
| 0.2 | 2026-10-29 | P0：头文件常量表 + `agri_fieldbus.yaml` 拓扑 |
| 0.3 | 2026-10-29 | P1：Modbus TCP IO 卡 Mock + field master |
