# 需要确认的临时变更

`core/include/openember/confirmed_change.hpp` 提供 `openember::ConfirmedChange`，供网络配置等模块复用。它只负责“等待确认、超时回退”的生命周期；模块提供确认和恢复动作。没有后台线程、权限、消息协议、持久化、配置备份或通用事务系统。

```cpp
openember::ConfirmedChange change;
change.Arm(std::chrono::steady_clock::now() + std::chrono::seconds(40),
           [&] { backend.Keep(); },
           [&] { backend.Restore(); });
// 在拥有者工作循环中调用：
change.Poll();
// 用户确认时：
bool kept = change.Confirm();
// 正常退出时，拥有者可显式调用：
change.Rollback();
```

使用单调时钟。在同一工作线程调用全部方法；不可复制。`Arm` 要求没有待确认变更且两个回调有效。到期后确认也会先回退，并返回 false。成功的确认和回退各只执行一次；完成后允许重新 Arm。

确认回调抛异常时，状态仍为 Pending，拥有者可以重试确认或回退。恢复回调抛异常时，状态成为 RecoveryFailed，向调用者传播异常；业务必须报告“恢复未确认”，不可宣称恢复成功。析构不执行 I/O 或回调，退出清理由拥有者显式处理。

它只在进程活着且拥有者继续 Poll 时提供超时处理。网络模块另外建立 NetworkManager 原生 checkpoint：即使 OpenEmber 被杀死，NetworkManager 仍能按自己的时限恢复。其他模块如果要求断电或崩溃恢复，也需自行提供持久化恢复机制。

独立测试覆盖提前确认、到期确认、重复调用、超时回退、确认失败后恢复、恢复失败。核心测试名 `confirmed_change`。
