# 进程依赖、就绪与关闭顺序

状态：Linux 原生 launch_manager 的第一版依赖编排。默认仍由 launch_manager 创建单个本机 Link router；普通服务作为 client 接入。它管理进程生命周期，产品状态和告警策略由独立产品模块负责。

## YAML 契约

```yaml
nodes:
  - name: motion
    command: ${openember.bin}/quadruped_motion_service
    args: [--connect, 'tcp/127.0.0.1:7547'] # 示例省略产品配置与凭据
    readiness: notify
    startup_timeout_ms: 30000
    shutdown_timeout_ms: 10000
    restart: on-failure
    restart_limit: 3
  - name: input
    command: ${openember.bin}/quadruped_motion_f710
    depends_on: [motion]
    readiness: notify
    startup_timeout_ms: 5000
```

runtime/v1 的 StartProcess 暂无依赖/就绪字段；重新启动已有配置节点时保留其原依赖、就绪与期限，不因旧版协议默认值丢失这些约束。改变拓扑需要更新 launch 配置。

`depends_on` 是进程名列表，顺序可与 YAML 声明顺序不同。重复进程名、重复依赖、缺失依赖与环在创建子进程前拒绝。依赖进程就绪后才启动消费者；依赖失效会停止其传递依赖的消费者，依赖恢复后重新创建消费者。消费者重新启动不恢复旧业务授权。

`readiness: process` 是兼容默认值，只表示 fork 创建成功，不保证 exec 或应用初始化成功。需要可靠启动门控的程序必须使用 `notify`，并在真正可提供接口时通知。`startup_timeout_ms` 在 notify 模式下是就绪期限，必须大于零；超时、通道提前关闭或非法通知触发停止，并按重启策略处理。心跳监督仍单独由 `heartbeat_timeout_ms` 开启，设为 0 不要求心跳。

应用使用 `<openember/launch.hpp>` 的 `openember::launch::NotifyReady()`。launch 在 exec 前通过环境变量 `OPENEMBER_READY_FD` 传入独立 socket；应用完成模型、配置和端点注册后调用一次，然后进入正常事件循环。独立启动时没有该变量，函数直接成功。通知是一次性初始化完成，不是持续业务可用性，也不代表硬件已使能。子进程不能自己修改监督器的进程表。

关闭时先向没有存活消费者的进程发送 SIGTERM，等待退出或超过其 shutdown_timeout_ms 后 SIGKILL，再停止其提供者。不同独立分支可以并行退出。程序故障后的依赖失效路径优先停止所有消费者，但已经崩溃的提供者无法再执行有序关闭；本地执行器保护必须继续独立工作。

失败重启使用 100、200、400 毫秒递增、最高 5 秒的退避；restart_limit 是进程整个监督生命周期内的重试次数，0 保留无限重试语义。依赖暂不可用导致的停启不消耗消费者的失败重试次数。显式 StopProcess 停止的目标不会自动重启；显式重新启动目标后，其仍被期望运行的消费者可以重新启动。整套退出期间不重启任何进程。

## 路径和变量

支持 `${openember.bin}`、`${launch.dir}` 和 `${env:NAME}`。前者为监督器可执行文件所在目录，launch.dir 为配置文件目录，环境变量必须已经设置且非空。展开用于 executable、args、working_directory 和子进程 env 的值，不经过 shell；`$HOME`、`~`、命令替换不会自动解释。相对资源路径优先配合显式 working_directory。

## 限制与验证

本版监督直接子进程，尚未建立 cgroup/进程组级隔离；被管理应用不应自行 daemonize。就绪通知不替代 health_monitor，也不保证持续的 RPC、设备和模型健康。只有显式配置为 notify 的节点具备应用级就绪门控，现有默认 launch 保持兼容。

`test/launch_dependencies.py` 使用独立回环端口、PID 文件和 Python 子进程验证乱序依赖、延迟就绪、提供者崩溃与下游重建、就绪超时、反向关闭及非法依赖图；不访问设备。已有 runtime_regression.py 的 supervision 用例继续验证旧配置兼容。
