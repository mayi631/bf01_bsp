# BF01 直播唤醒通信优先启动

## 改动目标

将模组通信与相机控制放在冷启动关键路径，收到直播命令后取消本次启动 AI 和尚未落盘的拍录，避免为了启动直播等待完整 AI 和后台服务。时间戳、AI 云端配置、关机保护及真实 SDIO 握手条件保持不变。

## 启动顺序

1. `S10auto_mount` 正常挂载分区。
2. DATA 有 `bf01-boot-priority` 且应用完整时，`S11bf01` 加载 SYS、IPCM、看门狗，提前启动 DATA 中的 `auto.sh`。
3. 应用启动脚本先启动 Wi-Fi 管理，注册 cfg80211/SDIO 并等待真实握手成功。通信服务建立本地 `control.sock` 后启动相机；此等待最多 8 秒，不依赖全量参数确认或 HTTP 凭证。自动校时在通信服务就绪后异步启动。
4. 有直播请求时取消 AI，丢弃 RAM 中的启动照片和视频，不启动本次 SD 拍录；无直播时保留正常 AI 门控，TPU 在 AI 请求时加载。
5. `rcP` 等相机控制循环就绪；直播中再等本次开机首帧真实完整发送成功，然后加载 IVE、温控，运行延后的普通网络/NTP/SSH/日志/USB 等服务。等待预算为 `rcP` 开始后 12 秒，超时也恢复诊断服务，不将超时视为业务成功。看门狗和关机检查不延后。

`rcS` 只对 BF01 新固件跳过 S20network、S23ntp、S24cvidaemon、S50dropbear，由 `rcP` 按原顺序补启。S99user 检查 `early-started`，避免二次启动。没有 DATA 标志时不启用早启分支，`loadsystemko.sh` 仍加载原有完整模块集合；有标志且 helper 可用时只加载 core。移除本板启动时无条件清空 page cache 的操作，减少应用与 USB 并发冷加载争用。

## 配套应用行为

- DATA 标志由应用 `build/install.mk` 安装，不需要客户命令行配置。
- `bf01-modules` 用锁防止并发重复加载，按 core / ai / late 分组，失败不伪装成功，关机中不再加载。
- live start 设置 `/run/ipcamera-wifi/boot-media-cancelled`，AI worker 取消并回收；stop 不清除此标志，避免同次启动补做 AI 或补写照片/录像。标志随重启清空，不修改持久化 `ai_enabled`。
- 已经在命令到达前完成落盘的文件不能追溯变成“从未写盘”；后续取消拦截、清理仍走原媒体所有权与存储保护流程。
- 相机使用控制套接字与 signalfd 的 poll 唤醒。待处理直播 100 毫秒重试、直播状态 500 毫秒轮询，保留实际首帧后才确认成功的语义。
- supervisor 已拥有相机或 IPCM 锁被占用时，不再 fork 一个竞争相机。

## 交付与兼容

本功能的 BSP 交付文件仅为 BF01 板级 overlay：`S11bf01`、`S99user`、`rcS`、`rcP`、`bf01-modules` 和 `loadsystemko.sh`。不修改公共 SDIO 驱动，不调整小核媒体时序，不改变内存布局。

必须配套支持直播优先流程的 IPCamera：由应用安装 `bf01-boot-priority` 标志，并维护 `camera-loop.ready`、`live-mode`、`live-first-frame`。这些标志是启动依赖，不是计时打印，不可删除。普通开机仍执行 AI 门控及原拍录流程；直播取消仅作用于当前启动，不改变云端 AI 开关。

没有 DATA 标志时保留旧启动路径；早启期间未完成相机就绪或首帧条件时，`rcP` 最多等待 12 秒后继续恢复后置服务，不将超时判为直播成功。Wi-Fi 握手失败不能通过跳过握手伪装为联网成功。

## 构建与验证

应用按原 BF01 方式构建并 install；SDK 已选择 BF01 板型的环境依次 `pack_rootfs`、`pack_data`、`pack_upgrade`。此次需要 ROOTFS + DATA 配套更新，不是只编译 osdrv，也无须为本次调度修改重编 U-Boot/内核/小核。

本次交付不包含专用耗时统计、测试程序或测试截图。原有服务日志和错误输出保留，不能将日志中未记录完整报文的情况认定为空报文。

静态检查和隔离脚本验证不代替整机验收。烧录后仍需分别验收：冷启动普通 AI 拍录上传、AI 关闭直接上传、APP 休眠唤醒直播、快速 start/stop 不补落盘、GPIO 低电平正常关机。对直播需检查真实首帧与后续播放，不能仅凭 ACK2、网络可达或 RTSP 200 判定通过。
