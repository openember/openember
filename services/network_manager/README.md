# network_manager

Linux 公共网络服务。默认只做观测：接口列表、IPv4/IPv6、MAC、MTU、管理启用状态、内核链路状态、载波、64 位累计收发/错误/丢包计数。使用有界 RTM_GETLINK / RTM_GETADDR 查询，不启动 shell、nmcli 或外部进程，不需要 root，不配置网卡。Wi-Fi 通过 sysfs 标识；不推断 SSID、热点、DHCP、NetworkManager 连接或互联网状态。

启用 `OPENEMBER_SERVICE_NETWORK_MANAGER`，要求 Linux、Link、messages network contract >= 2。产品需要显式编排，未启用时不构建此服务。

```bash
network_manager --robot DEMO_A --connect tcp/127.0.0.1:7547
```

`--host-id` 默认 hostname；`--period-ms` 默认 1000，范围 100–10000。发布 `/network/state`、`/nodes/network_manager/info` 和 `/nodes/network_manager/heartbeat`。初始化并发布首个快照后发送 launch readiness；采样失败不代表服务死亡，快照及 heartbeat 明确标记数据不可用，后续继续采样。默认过期为采样周期的三倍加 500 ms，退出发布 offline。

每次最多 64 个接口、每接口 16 个地址；单次 dump 最多 1 MiB，500 ms 期限，快照不超过 60 KiB。内核 dump 被打断、丢失或超过限制时不把部分列表标为完整；地址查询失败时仍保留接口事实，地址显示未知。下一周期重新查询。接口可能在两次查询间热插拔，快照不是跨内核对象的原子事务。

接口索引可能复用，计数会随接口重建或系统重启归零，不是速率。`admin_up`、`operational_state`、`carrier`、IP 地址分别表达不同事实；`carrier_valid=false` 不是断线。IPv6 暂定、重复地址和弃用标志保留。只读后端与 NetworkManager 共存；可选配置后端通过 NetworkManager D-Bus 执行，不能通过原始写接口与其竞争。

启用核心 `TESTS_ENABLED`（或仅开启服务测试 `OPENEMBER_ENABLE_TESTS`）并构建 `network_sampler_test` 后运行 `ctest -R '^network_sampler$'`。测试覆盖异常/截断记录、重复接口、IPv4 本地地址与点对点对端区别、IPv6 状态、64 位计数，以及本机内核只读采集。产品 Link 到 HTTP 的编排验证见 `openember-quadruped/docs/network-monitor.md`。

Linux 数据来源参考 [内核接口统计文档](https://www.kernel.org/doc/html/latest/networking/statistics.html) 与 [链路运行状态](https://www.kernel.org/doc/html/latest/networking/operstates.html)。多项统计使用 rtnetlink 一次查询，避免逐个 sysfs 统计文件重复获取。

## NetworkManager 配置后端

增加 `--manage` 后，通过 GIO 调用系统 NetworkManager D-Bus，提供本地 Link `/network/rpc`。构建需要 `pkg-config` 和 `libglib2.0-dev`，不依赖 libnm、不执行 nmcli 或 shell。未指定 `--manage` 时保持纯内核只读观测。依赖的 network contract 为 2。

管理接口从 NetworkManager 自动发现，仅接受其管理的 Ethernet/Wi-Fi。支持 IPv4 DHCP/静态配置、保存连接的激活/断开、Wi-Fi 扫描与 Personal 认证连接、每个连接的自动连接开关、受保护热点创建与关闭。不更改全局联网/无线开关，不创建固定 wlan0/wlan1 角色。连接断开可能由 NetworkManager 的自动连接策略重新连接，不等于禁用网卡。企业认证/OWE/WEP 暂不提供连接操作。

操作在单独线程串行执行，RPC 立即返回 queued；使用 request_id 查询结果。请求绑定主机/服务实例与同机单调时钟期限；有界队列最多四项、缓存最多 64 项，保留五分钟。状态采集持续独立运行，NetworkManager 元数据过期与内核数据过期分别标记。不自动重试结果不明的写操作。扫描、配置和激活均有期限；元数据一次读取总期限三秒，最多每两秒刷新一次。

本地调用者使用自动生成的 0600 进程凭据文件，默认在运行用户的 `~/.config/openember/network/control.token`；`--token-file` 可覆盖。Web 与服务应使用同一系统账户、同一主机及本机 Link 端点。凭据在文件完整写入后原子发布，不回传浏览器，也不记录请求、Wi-Fi 密码或 GetSecrets 的内容。不需要手动签发凭据、控制权租约或设备维护模式。

权限由现有 Web 登录和操作系统决定。SSH 用户的 NetworkManager 权限若为 auth，后台无认证代理时会拒绝操作；不通过 sudo 启动整个 Web。部署时管理员可按 `config/openember-network.rules.example` 为运行服务的系统账户配置所需的少量 NetworkManager 权限，规则不自动安装；这是系统权限配置，不新增 OpenEmber 用户角色。Ubuntu 22.04 的旧 polkit LocalAuthority 使用相同动作的 `config/openember-network.pkla.example`，安装到 `/etc/polkit-1/localauthority/50-local.d/49-openember-network.pkla`；不要同时安装两种格式。

HTTP 当前本地地址属于被操作接口时，写入前创建 90 秒 NetworkManager checkpoint，配置成功后使用公共 [ConfirmedChange](../../docs/design/confirmed-change.md) 等待 40 秒确认。确认后才按 save 选项持久化；超时恢复原设置和连接。正常退出主动恢复；进程异常退出由 NetworkManager 超时恢复。checkpoint 创建失败则拒绝该次变更。失败恢复必须验证结果，不把未知当成功。临时更改仍可能在原服务之外被 NetworkManager 或管理员重载。

`network_backend` 测试通过私有 D-Bus 假 NetworkManager 验证扫描聚合、任意接口名、自动连接、原配置/IPv6 保留、checkpoint 确认/恢复和热点，不碰系统网络。需要 dbus-daemon、python3-dbus、python3-gi。产品端到端测试包含登录、同源、异步 Link 和真实 40 秒超时恢复。
