# BF01：40 MiB ION 与启动识别门控

日期：2026-11-05。设备：`0123456789`，CV1815JA BF01 SPI NAND。

## 内存判断纠正

本板 `CONFIG_ENABLE_ALIOS=y`，未启用 FreeRTOS/DOUBLESDK。AliOS 小核的
`CVI_SYS_IonAlloc` 和驱动 `sys_ion_alloc` 最终调用 `aos_ion_malloc`，再由
`krhino_mm_alloc_resv` 使用 `g_mm_region_resv`。源码和当前 `yoc.elf` 均核对过。

- Linux ION：`[0x85800000,0x88000000)`，40 MiB。
- 小核物理区：`[0x88000000,0x90000000)`，128 MiB，未扩大。
- `ALIOS_RESV`：`[0x88a00000,0x8fea0000)`，116.625 MiB，未扩大。
- `ALIOS_SYS_SIZE`、日志区、共享区、PQ 区不变。
- `FREERTOS_RESERVED_ION_SIZE=0` 表示小核不占用 Linux 的 ION 池，不是取消小核堆。

此前的 13.625 MiB 来自通用 fast-image 的 bitstream/ISP/logo/share 默认值，
不是当前 AliOS 多媒体实际分配。将其从本板 Linux ION 再次扣除并扩容至 56 MiB，
是错误的内存归属判断。现在板级内存图、Linux DT 和 RTOS IPC 报告统一为零跨区预留，
保留实际地址边界/容量校验和通用内核预留隔离，不绕过安全检查。

主要源码依据：

- `cvi_alios/components/aos/src/adapter/rhino/yoc_impl.c`：`g_mm_region_resv`。
- `cvi_alios/components/rhino/k_mm.c`：保留堆初始化。
- `cvi_alios/components/aos/src/adapter/rhino/aos_rhino.c`：`aos_ion_malloc`。
- `fsbl/plat/cv181x/bl2/bl2_main.c`：固定 ISP/bitstream 初始化受 DOUBLESDK 条件控制。

## 最终流程与本轮改动

小核上电拍三张照片并开始 H.265 录像，数据只缓存在 RAM；新增
`VPSS(1,1)` 640×640 RGB planar 供帧给 Linux TPU。大核应用先识别，确认通过
才进入 SD 保存流程；拒绝/算法异常先停录，不启动 SD 媒体落盘。
保持原有 JPEG、H.265 1920×1440@30、AAC 16 kHz 单声道格式。

| 文件（相对 SDK 根目录） | 本轮改动 |
| --- | --- |
| `bf01_bsp/build/boards/cv181x/cv1815ja_bf01_spinand/memmap.py` | Linux ION 40 MiB，实际跨区预留 0 |
| `bf01_bsp/cvi_alios/solutions/fastboot/customization/cv1815ja_bf01_spinand/src/bf01_boot_video.h` | 视频索引 512→1024 帧，增加 16 KiB 小核 RAM；协议版本 3 防止混用旧程序 |
| `ipcamera_bf01/solutions/ipcamera/src/app_ipcam_boot_capture.c` | 10 秒总照片交接期限覆盖分块传输，不只等待元信息 |
| `ipcamera_bf01/solutions/ipcamera/src/app_ipcam_capture.c` | 去除无期限外层 RPC 重试，交接失败/取消/非法配置及时停录 |
| `ipcamera_bf01/solutions/ipcamera/src/app_ipcam_boot_video.c` | 最早视频已覆盖则明确失败，不再静默保存后段冒充开机视频 |
| `ipcamera_bf01/solutions/ipcamera/src/app_ipcam_preview.c` | Wi-Fi off 时不启动 HTTP 预览监听，不重试绑定不存在的 wlan0 |

1024 帧索引在 30 fps 下约 34 秒；8 MiB 视频数据环未增大，实际可保留时长还受码率影响。
没有 SD 或启动异常拖延到缓存被覆盖时，不保证保留最早录像，必须报告失败。
不能把有限 RAM 当作无限离线录像缓存。

## IPC 使用限制

当前 SDK `/dev/ipcm_anon` 是进程间共享的单一接收队列，不按打开者或端口过滤。
`ipcamera` 和默认交互式 `alios_cli` 同时读取时，会竞争取走对方的消息。
50 MiB 故障现场两进程均打开该设备，照片应答反复超时，录像延后约 201 秒。
`alios_cli` 的父进程为串口 shell；标准 S99user 的启动项是注释，不是后台服务。

运行相机流程时不要启动默认交互式 `alios_cli`。本轮没有强制杀掉用户调试进程，
也没有用扩容或无限重试掩盖冲突。检查小核日志使用只读共享日志内存快照，不开匿名 IPC。
后续若必须让两个用户态消费者并发，需要独立设计驱动按端口分发，不能宣称当前 SDK 已支持。
另外已实测板上 `alios_cli -m 1 -c 0` 仍打开 `/dev/ipcm_anon` 并启动三个线程，
不能仅凭这些参数认为已进入安全的纯日志模式；该测试进程已结束，没有遗留后台 CLI。

## 验证记录

FIP/RTOS/BOOT 与应用已重编译。设备三个启动分区和应用已完整备份到
`/mnt/data/bf01_before_final40_20261105/`，本机副本在
`/tmp/bf01_final40_20261105/before/device/`。NAND FIP 保留原芯片参数、块头和双副本；
三个分区写后均完成 SHA-256 读回核验。没有刷写 ROOTFS、分区表和模型，也未降低原门限 0.25。

40 MiB 首次启动真实推理：`rejected/error=61`，`forward_us=406231`，
`storage_allowed=0`，未启动 SD 存储/拍录落盘业务。`61/ENODATA` 表示没有满足门限的
候选目标，不是 ION 或推理运行失败。Linux ION 实际池为 41,943,040 字节，
累计分配峰值 36,745,216 字节（35.04296875 MiB），推理退出后使用量 0。
峰值余量 5,197,824 字节（4.95703125 MiB）。

主机回归通过：40 MiB/零跨区预留、真实重叠拒绝、模型分数/类别/异常进程、
首帧及读取中视频覆盖拒绝、JPEG 分块 CRC、总交接期限、取消及临时文件清理。
测试和构建日志位于 `/tmp/bf01_final40_20261105/`。

两张原始鹦鹉图片另外在本轮 40 MiB 上完成真实 TPU 正样本测试，仍为门限 0.25：
第一张 1 目标，得分 0.9297，Forward 399.15 ms；第二张 2 目标，得分
0.8984/0.7109，Forward 402.50 ms。没有修改模型或用强制通过驱动相机落盘。
结果与镜头拒绝分支分别存于 SDK 根目录 `bf01-e2e-results/20261105-ion40/`。

最终应用已更新并重启，SHA-256 为
`5210e2e02bbe832f714d8f77c8c440fa7c38482cc896d80d49565e638f8724cd`，
第三轮镜头推理仍为正常未检出，Forward 406.595 ms，未放行 SD 落盘。
当前门限配置哈希 `83826b5f33c29b10a2ef1f0f80f277f959642abe273591ce05a81494db645f72`
与测试前完全相同。小核启动日志仍出现 ISP PCHIP/DRC 告警，本轮未改 ISP 调参，
不能将实际日志描述为“没有任何告警”。

## 目标就位后的真实镜头复测

本节均为上述 40 MiB、新应用与原始门限 0.25，不使用历史 56 MiB 结果。

### 只读取回输入与通过分支

目标就位后首次原始启动仍未检出，故增加一次只读诊断：独立小工具先获取 READY
元信息，将实际 640×640 RGB 帧复制到 `/run`，不发送 ACCEPT/REJECT、不改像素，
退出并关闭匿名 IPC 后，再运行原 `ipcamera`。该启动脚本在运行诊断前已自动原子恢复，
不是永久新增启动依赖；应用二进制、模型、门限始终未变。

本轮实际输入：640×640 RGB planar，上下各 80 行、补边精确为 `(114,114,114)`，
RTOS PTS 为 1,832,057 µs，与第三张 JPEG 的 PTS 相同。随后原应用真实 TPU 返回：

```text
state=passed
error=0
class_id=0
score=0.785156
forward_us=411388
storage_allowed=1
```

- 拍录任务 `completed/error=0`，Linux 任务耗时 3,128 ms；
  Linux 任务开始 uptime 9,330 ms、完成 uptime 12,458 ms。
- 新增且仅新增 `DSCF00028.jpg`、`DSCF00029.jpg`、`DSCF00030.jpg` 和 `DSCF00010.mp4`。
  三张照片均 1920×1440，完整像素解码及板端/电脑 SHA-256 比对通过。
- MP4 5,100,858 字节，H.265 1920×1440、30 fps、300 帧，视频轨 10.000 秒；
  AAC 16 kHz 单声道，音频轨 10.000 秒，容器 10.064 秒包含编码延迟。
  FFmpeg 完整音视频解码退出 0，错误输出为空。
- 录像第一编码帧的 slice 数据与 RTOS 环中 sequence 0 逐字节一致，
  RTOS 第一帧 PTS 2,065,386 µs、关键帧标记有效、oldest sequence 为 0。
  slice SHA-256：`03641fabb3834c20ad09dcc172586c929adae1db5dafe87506580734e4d03e55`。
  这证明保存的是开机最早片段，不是缓存覆盖后的替代片段。
- RTOS 音频及录像停止状态均为 `error=0`；Linux ION 峰值仍为
  36,745,216 字节，事后当前使用 0。没有出现之前的约 201 秒交接延迟或缓存覆盖错误。

Linux uptime 与 RTOS 硬件计时原点未对齐，不能把两者直接相减来声称完整上电耗时。
第一帧对比在录像已停止后只读提取原缓存，并验证协议头、世代、序号与实际 MP4 slice，
没有通过调试命令重新启动或修改编码器。

同一份实际输入另用原模型离线复跑，得分仍为 0.7852，Forward 402.73 ms，
与相机门控一致。但实际画面为桌面、线缆、织带，未见之前的鹦鹉图；模型框住左下角
织带。这只能证明真实模型返回值驱动了落盘，不能作为正确识别鹦鹉的精度验收。
实际输入、标注、三个 JPEG、MP4、完整解码及首帧核验见
`bf01-e2e-results/20261105-ion40/positive1/`。

### 恢复原始自动启动后的拒绝分支

再次以原始 `auto.sh` 重启，确认没有运行诊断工具或生成输入快照。
真实推理 `rejected/error=61`、Forward 407,437 µs，未启动 SD 拍录服务。
临时只读挂载核对后，SD 媒体文件列表与重启前完全相同，仍为 40 个文件，零新增。
只读挂载已卸载，没有遗留调试、CLI 或相机进程；拒绝后应用退出属于当前设计。

启动脚本 SHA-256 `7058a0050eb7a9af445beb6a584b532ede5150cd59ede705ffc4e06d5a14b4e1`，
应用和门限哈希与上文一致，且没有 `/mnt/cfg/ipcamera/boot_ai.ini` 覆盖。
证据见 `bf01-e2e-results/20261105-ion40/normal-reject/`。

因此通过后保存完整媒体、拒绝后零新增文件的链路均已有真实证据；但本轮带只读输入
快照的通过结果不能冒充“原始自动启动、真实鸟类正样本”的验收。需要将图片真正放入
摄像头视野，再完成这项正样本验证。不能通过放宽门限或强制通过代替。

模型类别含义、预处理约定、误检率、原厂合规确认、长稳和异常掉电仍需独立验收；
任何“不会出现任何错误”的绝对保证都不成立。

## 无屏图片联调与恢复正式流程

2026-11-05 按无屏测试需求，新增 Linux 应用编译宏 `BF01_BOOT_AI_TEST_INPUT`：
`1` 从本地预处理图片读入，`0` 直接使用原 `VPSS(1,1)` 帧，默认及最终发布值均为 `0`。
两模式仍执行真实 RTOS READY/ION 隔离检查、隔离子进程真实 TPU 推理和 RTOS 确认门控，
不改 BSP pipeline、编码、模型、门限或内存图。不存在模拟通过；测试照片/录像仍来自镜头。

最终用第一张鸟图检出 1 个目标（0.914062），第二张图检出 2 个目标
（0.906250/0.6484），两次真实上电均完成三张 JPEG 和十秒 H.265/AAC MP4，
任务 `completed/error=0`。全部媒体哈希、JPEG 完整解码、MP4 完整音视频解码验证通过。
纯黑图未检出、图片缺失均零新增 SD 媒体。五轮 Linux ION 峰值仍为
36,745,216 字节（35.043 MiB），本轮无需增大 40 MiB。

当前板端已恢复正式摄像头模式并重启，状态 `input_source=vpss_1_1`，
应用 SHA-256 为 `729c5cdf813ec1bf03a98b89cd0cbe76af7cc4bd3d53fea406edbd49f27414fa`。
原始启动脚本、门限配置及参数文件哈希不变；当前镜头未检出，按设计拒绝落盘。
因此静态图片模式两种正样本的完整业务已验收，但不能据此声称镜头已正确识别实际鸟类。
操作说明见 `../../ipcamera_bf01/BF01_BOOT_AI_GUIDE.md`，本轮完整证据和限制见
`../../bf01-e2e-results/20261105-file-input/README.md`。
