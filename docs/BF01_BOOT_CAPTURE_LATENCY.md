# BF01 冷启动首拍耗时优化

## 目标和统计口径

目标是上电到首张 1920×1440 JPEG 及其同 PTS 的 640×640 AI 输入均在内存中准备完成，
尽量接近 650ms。不是把 JPEG 编码开始、VI 出帧或者 JPEG 单独完成冒充首拍完整完成。
本轮不修改 AI 门限、识别结果门控、三张抓拍、500ms 后续抓拍间隔、录像和上传策略。
预热帧仍不计入三张照片，不修改帧率、分辨率、DDR、ION 或供电配置。

2026-11-13 优化前用户提供的冷启动日志：

| 阶段 | 上电时刻 | 阶段耗时 |
| --- | ---: | ---: |
| 小核 release | 359ms | — |
| 媒体就绪 | 504ms | release 后 145ms |
| 预热 VI 帧取得 | 720ms | 媒体就绪后 216ms |
| 第一张正式 VI 帧取得 | 752ms | 预热后约 32ms |
| 首张 JPEG + AI 输入准备完成 | 844ms | 取得正式帧后 92ms |

`load_loader_2nd_end=379ms` 是 loader 完成，不是小核 release 时间。
`boot_to_frame_ms` 是 VI GetChnFrame 返回时刻，不是传感器曝光起点。

## 根因与修改

板级 `pipeline/custom_viparam.c` 虽设置 `s8ByPassNum=0`，通用
`components/cvi_platform/media/src/media_video.c` 却仅在非零时调用
`CVI_ISP_SetBypassFrm`。0 没有覆盖 ISP 库默认值。

优化前实际链接的 `yoc.elf` 中，`g_bypassFrm` 的前两个字节都是 5，
`isp_freeze_init` 从该数组取得旁路帧数并向 VI 设置；setter 只更新该数组，
可以在媒体初始化前调用。30fps 下丢弃 5 帧对应约 167ms，是该次启动
216ms 首次 VI 等待中的重要来源，但各阶段实际缩短量仍需冷启动测量。

### 当前预编译 ISP 库的 getter ABI

2026-11-13 首个优化候选在板端暴露出头文件与实际库不一致：
头文件声明 `CVI_ISP_GetBypassFrm(VI_PIPE, CVI_U8 *)`，但链接镜像的实现
只把 `isp_freeze_get_bypass_frame_num` 返回的实际帧数直接作为返回值，
完全不写第二个参数。底层 getter 直接读取 `g_bypassFrm[pipe]` 返回。

因此 `frames=255 ret=0` 中，255 是调用方预置的哨兵，不是寄存器读值或真实帧数；
返回值 0 才是实际回读的 bypass 帧数。首次候选错误地检查了输出指针，返回 EIO=5，
导致 MEDIA_VIDEO_Init 根本未执行，后续 VI 的 `Call SetDevAttr first` 是级联错误。

BF01 调用现在按已核实的当前库 ABI 接收返回帧数，负值按错误处理，
非零实际帧数仍阻止启动，不再把未写的输出指针作为回读结果。
保留第二个占位参数以匹配 SDK 头文件，不修改公共头文件或预编译库。
升级 ISP 库时必须重新验证此 ABI，不能假设头文件签名等于实际实现。
主机模拟 getter 也必须模拟真实的“返回帧数、不写指针”，不能再用理想接口替身验收。

- 扩展已有 0021 补丁：增加可选 weak `APP_BootCapturePrepare`，在媒体初始化前调用；
  BF01 专属实现主动设置并回读 ISP bypass=0，设置或校验失败则停止媒体启动并传递错误。
  其他没有此 hook 的板卡保持原流程，不改通用媒体模块对 0 的语义。
- CRC32 使用只读 16 项表替代逐 bit 循环，保留多项式 0xedb88320、初末异或和增量语义，
  不修改 IPC 协议版本、结构或大核验收规则。
- 默认 `BF01_BOOT_CAPTURE_TRACE=0`，省去首拍热路径的逐步成功打印，保留错误、
  ISP 回读、预热结果、首拍计时和每张照片汇总；诊断构建可设为 1 恢复详细成功打印。

无需客户敲命令，全部随固件自动运行。

## 新日志

- `ISP bypass: pipe=0 frames=0 ret=0`：初始化前设置和回读通过。
- `first-photo timing`：`boot_to_photo_ms` 仍在 JPEG 校验、CRC、释放 VI/VENC、
  同 PTS AI 输入复制和发布、照片状态发布之后采样；`end=jpeg_and_ai_in_ram`。
- `boot_to_jpeg_ms`：JPEG 完整拷贝和 CRC 校验完成、释放 VI/VENC 后的独立时刻。
- `first-photo stages_us`：VI 等待、AI 旧帧清理/水印更新/VPSS 提交、编码等待、
  JPEG 分配/复制/CRC、释放、AI 输入准备和状态发布分段。
- 计时在汇总输出前采样，`photo=1 completed_ms` 不再额外包含本轮统计打印的耗时。

## 编译与验收

在已经正确配置 BF01 的 SDK 环境下执行 `build_alios`，重新生成小核镜像。
只编译应用或 `build_uboot` 不会更新这次小核抓拍逻辑。
重新打包固件使用 `pack_upgrade`；本轮候选完整包为
`install/soc_cv1815ja_bf01_spinand/upgrade.zip`，其中应包含本次生成的 `yoc.bin`。

静态/主机验证不等于板端验收。烧录完整候选包并彻底断电后，至少重复 3 次冷启动，
记录所有次的 ISP 回读、首帧、JPEG 和 JPEG+AI 完成时刻；不能只取最快一次。
检查首张图亮度、清晰度、完整性和 AI 输入 PTS 一致性，并复验三张抓拍和识别/上传流程。
减少 ISP 丢帧后，更早的图像可能尚未完成 AE/AWB 收敛，画质必须同时验收，不能仅看时间。
650ms 是本轮目标，不是尚未实测即可认定的结果。
