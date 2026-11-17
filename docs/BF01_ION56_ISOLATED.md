# BF01：ION 隔离修复及 56 MiB 实机验证

**历史方案，已撤回为最终配置的依据。** 当前使用 40 MiB，见 `BF01_ION40_FINAL.md`。
本记录中的 13.625 MiB 是当时设备树错误声明的预留，不是本板 AliOS 的实际 ION
占用。AliOS 的多媒体分配来自独立 `ALIOS_RESV` 堆；将其从 Linux ION 再扣一次并据此
扩容到 56 MiB 的判断有误。以下保留当时固件和测试结果，不应按此重新配置本板。

日期：2026-11-05。当前设备：`0123456789`，CV1815JA BF01 SPI NAND。
本方案已验证摄像头供帧、Linux TPU 门控及自动照片/录像落盘，但仍是待原厂确认的
临时集成配置，不代表合规认证或长时间稳定性验收。`BF01_ION48_TEMP.md` 保留历史记录。

## 1. 隔离检查失败的根因

内核 `drivers/staging/android/ion/ion_carveout_heap.c` 从设备树读取 RTOS 的
ION 预留，并缩小启动清零范围，却仍以原始 `heap_data->base/size` 注册
`gen_pool_add()`，`heap.total_size` 也仍是原始总量。启动日志显示“排除预留”
不代表分配位图真的排除了该段；应用检查到实际可分配范围覆盖 RTOS，所以拒绝推理。

修复让清零范围、池基址、池大小和容量统计统一，另检查非法预留及建池失败。
没有删除应用安全检查，也没有改变内核导出接口或模块结构体布局。

## 2. 改动清单

| 文件 | 改动 |
| --- | --- |
| `linux_5.10/drivers/staging/android/ion/ion_carveout_heap.c`（SDK 根目录下） | 真正排除 RTOS 预留，检查边界及建池失败 |
| `patches/0032-linux_5.10--fix-ion-exclude-rtos-reservation.patch` | 将同一内核修复纳入 BSP 补丁管理 |
| `build/boards/cv181x/cv1815ja_bf01_spinand/memmap.py` | 当前总 ION 从 40 改为 56 MiB；RTOS 区域及预留大小不变 |
| `patches/README.md`、本文件 | 说明修复和一致更新要求 |

SDK 的 `build/boards/cv181x/cv1815ja_bf01_spinand` 指向 BSP 配置，勿另改一份。
既有拍录及 pipeline 改动保留，本轮不再改变 H.264 预览、JPEG、H.265/AAC 录像格式。

## 3. 当时固件的内存图和实测

| 区域/统计 | 地址或容量 |
| --- | --- |
| DDR | `[0x80000000,0x90000000)`，256 MiB |
| RTOS 物理区域 | `[0x88000000,0x90000000)`，128 MiB |
| 总 ION | `[0x84800000,0x88000000)`，56 MiB |
| 当时设备树声明的 RTOS ION 预留（本板 AliOS 不实际使用） | `[0x84800000,0x855a0000)`，13.625 MiB |
| Linux ION 分配池 | `[0x855a0000,0x88000000)`，44,433,408 字节，42.375 MiB |
| 本轮 Linux ION 峰值 | 36,745,216 字节，35.04296875 MiB |
| 峰值容量余量 | 7,688,192 字节，7.33203125 MiB |
| 推理工作进程退出后 | 当前分配 0 字节 |
| Linux `MemTotal` | 63,744 KiB |

40 MiB 总池正确隔离后仅剩 26.375 MiB，48 MiB 仅剩 34.375 MiB，均小于
此次模型峰值。选择 56 MiB 是为了同时落实隔离并留出本轮实测容量余量，
不是声称这是厂家推荐值。它会减少 Linux 普通内存，需原厂评估总预算。
峰值仅指该轮 Linux ION 统计，不包含 RTOS 全部堆，也不是最坏工况保证。

设备实际检查入口：

```sh
cat /sys/kernel/debug/ion/cvi_carveout_heap_dump/summary
cat /run/ipcamera-ai/status.ini
cat /run/ipcamera-capture/status.ini
```

必须同时看到实际池大小 `44433408`、空闲范围起点 `855a0000`，不能只看启动打印。

## 4. 构建及模块一致性

在 SDK 根目录加载当前板型配置，不重新 defconfig 或 clean：

```sh
export SCENES=1 CHIP=cv1815ja BOARD=bf01_spinand
source build/envsetup_soc.sh
cvi_setup_env
export FLASH_PARTITION_XML="$PWD/build/boards/cv181x/cv1815ja_bf01_spinand/partition/partition_spinand.xml"
export NPROC=8
build_fsbl
build_kernel
build_osdrv
```

普通完整构建必须把匹配的内核及所有模块一同更新到 ROOTFS。工作树有修改时生成的
`-dirty` 内核不能搭配原板非 `-dirty` 模块；打包前逐个检查 `.ko` 的 `vermagic`。
本 SDK 按目录遍历复制模块，旧 `modules/lib/modules/` 中残留其他 release 的文件
可能覆盖新模块，不能将混合版本的暂存目录直接打包。
本轮已将旧 `-dirty` 模块缓存移出安装搜索路径，补齐当前 release 的内核及外部模块，
最终 ROOTFS 暂存目录的 19 个 `.ko` 均核对为同一非 `-dirty` release；未刷写到板端。

本次为避免在线刷只读 ROOTFS，在确认补丁不改导出接口和结构体布局的前提下，
仅为替换 BOOT 的这次兼容构建，执行：

```sh
export MAKEFLAGS='KERNELRELEASE=5.10.4-tag--g7b50980d408d'
build_kernel
unset MAKEFLAGS
build_osdrv
```

`KERNELRELEASE` 覆盖只用于内核这一步，不能带入顶层 `build_osdrv`：IVE 的外层
Makefile 根据该变量区分 Kbuild 分支，误传会导致 `No rule to make target 'all'`。
外部模块使用已经生成的内核头，仍须以最终 `vermagic` 为准。
该兼容内核实际 `UTS_RELEASE=5.10.4-tag--g7b50980d408d`，构建时间
2026-11-05 15:25:19 CST；保留了原板全部模块，实机 TPU/IPCM/USB 正常加载。
本次工作树仍有未提交补丁，不能据相同 release 将该内核误认为旧原版内核。

FIP、RTOS、BOOT 设备树必须来自同一内存图；本轮已验证生成的头文件和设备树一致。
不要只替换应用，不要混刷此前 40/48 MiB 的 FIP 或 RTOS。

## 5. 实际更新和备份

原设备完整备份：`/mnt/data/bf01_rollback_e2e_20261105_1458/`。
包含 MTD0、MTD1、MTD2、原程序、原配置、启动脚本及 `system-ko.tar.gz`。
电脑副本：`/tmp/bf01_e2e_20261105/rollback/`。备份读取的坏块、ECC 纠正及失败均为 0，
板端和电脑 SHA-256 一致。回退时须把三个旧内存图分区作为配套整体恢复。

实际刷写的是下列三份载荷，写后读回 SHA-256 全部一致：

| 分区 | 文件/格式 | 字节数 | SHA-256 |
| --- | --- | --- | --- |
| MTD0 | `fip_mtd0.bin`，带 NAND FIPH/FIPB 块头、双副本及原芯片参数的完整分区 | 2621440 | `8276bf1f990b795c9602204223873f90b9d56bf281d6d7fbce59fe094360f8aa` |
| MTD1 | `yoc.bin`，小写 cimg+LZ4 载荷 | 1389551 | `d470581ff03041d280ee2710afcc75fb4916a6d2d6b60400f5b31c065ffcb41c` |
| MTD2 | `boot.spinand`，原始 FIT | 2539296 | `f7d2f3c696991defdd51e6ed6e6c0b7b5853401b8acecd2f99e10453c62dd755` |

这些特定镜像和刷写日志在 `/tmp/bf01_e2e_20261105/staging/`、`logs/flash.log`。
已验证的配套载荷也持久保存在 SDK 根目录
`bf01-e2e-results/20261105-ion56/firmware/`，附 `SHA256SUMS`；
这不是整机升级包，本轮未重新打包现有 `upgrade.zip`，不要将旧升级包当成这次修复。
没有更新 ROOTFS、应用二进制、模型或 NAND 分区表。不能把普通 `fip_spl.bin`
或大写 `CIMG` 下载封装直接写 MTD；原始 FIP 缺少 NAND 块头和双副本布局。

## 6. 真实流程结果与边界

- 原门限 0.25：摄像头 VPSS(1,1) 640×640 RGB → Linux TPU，407.419 ms，
  `passed/error=0` → 自动保存三张 JPEG 和 10 秒 H.265/AAC MP4，`completed/error=0`。
- 受控拒绝：临时门限 0.99，实际 TPU 408.731 ms，`rejected/error=61`、
  `storage_allowed=0`；未自动挂载 SD，DCIM 前后文件列表完全一致，小核停录 `error=0`。
- 恢复原配置并核对 SHA-256，再次重启：TPU 406.406 ms、得分 0.366745，
  三张照片与 `/mnt/sd/DCIM/MOVIE/DSCF00008.mp4` 自动保存，10 秒、`completed/error=0`。
- 两轮 MP4 均哈希一致且完整解码，300 帧 H.265 1920×1440@30，16 kHz 单声道 AAC；
  六张 JPEG 可解码，但 FFmpeg 对原有 APP 元数据报警，未修改其编码内容。
- 结果工件位于 SDK 根目录 `bf01-e2e-results/20261105-ion56/`；
  内核建池函数的主机 ASan/UBSan 边界测试也通过。

当前恢复原门限 0.25，没有放宽门限或强制通过。但办公室画面无明显目标时仍得到
class 0、约 0.37～0.38 分，可能误检；流程成功不等于模型精度达标。
类别、预处理、正负样本、热启动/断电、多卡、故障注入及长期稳定性仍需专项验收，
内存布局合规性需原厂明确确认。
