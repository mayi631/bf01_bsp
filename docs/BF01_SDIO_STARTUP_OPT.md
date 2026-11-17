# BF01 SDIO 驱动启动优化（2026-11-16）

## 已有证据与范围

最近一轮真实启动记录中，Linux 时间轴为：ATBM 初始化 2092ms，真实后端就绪
2244ms；WSM 接受日志报告等待 50ms，第一次握手即成功，没有走 1 秒失败重试。
完整直播指令 2963ms，首帧完整发送成功 3659ms。硬件启动计数器与 Linux 的偏移
估算约 1359ms，因此约 5018ms 是估计上电至主控完整发送首帧，不是 APP 渲染时间。

大部分时间在驱动执行之前及指令到达之前，不能把约 5 秒全部归因于握手。
本次不改 DATA 挂载、FSBL、相机、应用调度或模组固件，也不删除硬件保护等待。
115200 波特率下，常见 8N1 每 100 个字符的线上发送时间约 8.7ms；驱动正常打印
原来多为 `KERN_ERR`，在 `loglevel=4` 下仍同步送串口，是可避免的开销。
具体收益取决于实际 console、调度及并发情况，尚无新版板端测量，不保证节省秒级时间。

## 实现

SDK 源码位于 `osdrv/extdrv/wireless/realtek/atbm6461/hal_apollo/`：

- `debug.h`：Linux RHEA SDIO 的 INIT 类信息改为 `KERN_DEBUG`；增加正常启动横幅
  入口。错误级 `atbm_dbg` 仍走 ERR，而不是被 INIT 的低级别隐藏。
- `apollo_sdio.c`：正常 poll、空闲超时、版本/芯片信息退出默认串口关键路径。
  绑定、真实后端 ready 改为 `KERN_INFO`，绑定包含 `startup=event-v1` 标识。
- `main.c`：使用原有 `wsm_cmd_wq` 的 `wait_event_timeout`，真实 startup 到达时
  立即唤醒；无通知时仍每 20ms 检查取消/传输错误，并受同一个 1000ms 截止点约束。
- `wsm.c`：完整解析、状态/类型/版本校验成功后才以 release 发布 firmwareReady，
  随即唤醒队列；等待者用 acquire 读取，不把截断或失败通知当作就绪。

保留缺失 IRQ 的轮询恢复、位宽重同步、function enable、block size 校验、失败
清理及 1000ms 重试。保留 `bf01_startup_state`、stop 取消、传输静止、卸载和关机
流程。非 Linux RHEA SDIO 不启用新的事件等待；错误级 atbm_dbg 的路由修正为共享
日志行为。没有修改全局 printk 等级，不隐藏其他驱动错误。

## 交付与构建

补丁是 `patches/0045-osdrv--perf-wifi-quiet-startup-and-wake-on-handshake.patch`，
依赖新版 0042。当前 SDK 源码已修改，不要再对当前工作树重复应用 0045。
没有提交、推送、替换板端、重载驱动或烧录。

仅本功能不需要编译 U-Boot、小核或重新配置内核。已有匹配内核及板型环境时，可
在 SDK 根目录按现有增量构建流程执行：

```sh
build_osdrv wireless
make -C ipcamera_bf01 ipcamera install
pack_system
pack_data
pack_upgrade
```

前提是已经加载 `build/envsetup_soc.sh` 并设置当前 BF01 编译环境；不要为了这次
优化盲目执行 `defconfig` 覆盖本地配置。应用 install 会独立编译 SDK 中的新驱动，
安装到应用 `install/wifi/` 与 SDK `OUTPUT_DIR/data/wifi/`；必须把这一份更新纳入
DATA 镜像，否则主控可能仍优先加载旧的 `/mnt/data/wifi/atbm6461_wifi_sdio.ko`。
osdrv 的 SYSTEM 副本同时更新，避免回退路径版本不一致。若内核配置或版本变更，
应使用完整匹配构建，不能混用旧内核和新 ko。

## 本次验证与明日验收

- 匹配当前 5.10.4 ARMv7 内核的隔离 ko 编译通过。
- 183 项基于实际握手代码抽取的隔离控制流用例通过，包括提前就绪、事件唤醒、
  伪唤醒、1 秒超时/边界、传输失败、取消、时钟回绕、错误版本/状态/类型及
  所有截断长度；另验证 BF01 INIT 串口降噪、ERR/WARN 可见及其他平台日志路径。
- 这些用例使用模拟传输，不能代替真实供电、SDIO 通信、直播、AI 或关机验收。
- 当前 ADB 远端服务连接超时，未采集新版本板端启动，也没有重启或部署。

明日首先检查新 ko 校验值、唯一通信消费者、实际网络与版本；确认 `dmesg` 有
`startup=event-v1`。三处 INFO 记录在默认串口关闭但仍留在内核环形日志，INIT
也保留在 `dmesg`，不要因为串口安静误判驱动没加载。不要把 console 改成 debug
级别做性能对比，否则会重新引入串口开销。

分别做正常上电、休眠直播唤醒、模组延迟就绪重试及 GPIO29 低电平关机，按
`WIFI模块通讯/主控联调测试例子.md` 保存完整双向报文、ID、通道、返回状态、
阶段耗时及带电脑时间的串口证据；AI/时间戳沿用原配置，不为测试绕过。
比较同一时钟口径下的驱动开始→真实ready、收令→首帧及上电→首帧，区分主控发送
成功与 APP 显示。没有实际板测的项目标为未完成，不把隔离用例当作业务通过。

回退时先保存当前改动，反向应用仅 0045 并重新构建安装相同镜像；不要回退 0042
或清除 CFG/绑定。不要在线强制卸载、热重载或写 stop 后尝试继续使用当前驱动。
