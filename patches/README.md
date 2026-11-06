# bf01 补丁说明

补丁名称沿用 DC100/DC309 的 `<序号>-<repo>--<类型>-<功能或问题>.patch`。
先按 SDK 仓库归类，再按完整功能或单个 bug 划分；不按文件数或修改行数拆提交。
同一功能所需的源码、构建和安装规则必须一起交付。

## 既有补丁 0001–0007

- `0001-cviruntime--fix-tpu-sdk-skip-flatbuffers-host-tests.patch`：
  TPU_REL=1 时跳过 flatbuffers host tests，避免新版主机 GCC 的
  `-Werror=class-memaccess` 阻塞；正常生成 flatc。
- `0002-build--feat-add-sensor-GCORE_GC4683-to-sensor-list.patch`：
  在传感器列表加入 GC4683，供配置生成器选择对应驱动。
- `0003-build--fix-u-boot-kernel-dts-find-follow-softlink.patch`：
  DTS 查找跟随板级软链接，避免遗漏 U-Boot 或 Linux DTS 文件。
- `0004-cvi_alios--feat-support-gcore-gc4683-sensor.patch`：
  AliOS GC4683 驱动、四种模式、对象及编译配置。
- `0005-cvi_mpi--fix-support-gcore-gc4683-sensor.patch`：
  MPI sample 的 GC4683 VI/ISP 模式处理；缺失声明由独立修复 0008 补齐。
- `0006-cvi_alios--fix-h264-vui-frame-rate.patch`：
  修正 H.264 VUI 帧率；应用后需要重新编译 AliOS 并更新 yoc 分区。
- `0007-cvi_alios--fix-sys-init-failed.patch`：
  ARM 大核下将 IPCM 发送目标设为 CPU 0，修复系统初始化失败。

上述七个补丁内容保持不变，不属于本次提交组织调整范围。

## cvi_mpi：GC4683 sample 编译修复

- `0008-cvi_mpi--fix-gc4683-sample-mode-declarations.patch`：
  补齐 sample 的四种传感器模式及条件对象声明。
  仅依赖既有 0005，与 Wi-Fi 功能无关。

## osdrv：ATBM6461 驱动基础功能

- `0009-osdrv--feat-wifi-add-cv181x-atbm6461-sdio-driver.patch`：
  一个可独立编译的完整驱动适配功能：原厂源码、CV181x Kbuild、
  RHEA/SDIO/24 MHz 配置、CVITEK 平台回调、Linux ioctl/可选宏兼容、
  原厂用户态工具及 SDK osdrv 构建入口一起交付。
  不要求先叠加 bug 修复、STA 服务或诊断才能编译。
  原厂版本为 `b1d1eeae8e95ab9a1670679f8599e968df6a495d`；
  两个含非 UTF-8 注释的源码用 Git binary hunk 保留，并非预编译程序。

## build：BF01 Wi-Fi 硬件使能

- `0010-build--feat-wifi-add-bf01-atbm6461-selection.patch`：
  添加 BF01 专用驱动选择项及精简固件的外置无线驱动选择。
- `0011-build--fix-dts-preserve-cv181x-wifi-sdio-host.patch`：
  公共 BGA DTS 保留 `wifisd`，允许板级 DTS 启用。

0010、0011 与以下板级文件在同一笔 BSP 硬件使能提交中交付：

- `build/boards/cv181x/cv1815ja_bf01_spinand/cv1815ja_bf01_spinand_fastboot_defconfig`
- `build/boards/cv181x/cv1815ja_bf01_spinand/dts_arm/cv1815ja_bf01_spinand.dts`
- `build/boards/cv181x/cv1815ja_bf01_spinand/linux/cvitek_cv1815ja_bf01_spinand_fastboot_defconfig`
- `build/boards/cv181x/cv1815ja_bf01_spinand/u-boot/cvi_board_init.c`

这四项共同完成驱动选择、SDIO 使能/25 MHz 上限、GPIOA20 复位保持、
网络栈和 pinmux，不再把一个硬件使能功能拆成多笔半成品提交。
前提是 0009 驱动基础功能；不配置 GPIOA14 或模组 GPIO29。

## osdrv：独立 bug 修复

以下每个补丁只解决一个问题，各自只依赖 0009，不互相依赖：

- `0012-osdrv--fix-wifi-rcu-unload-deadlock.patch`：
  修复 CVITEK 卸载路径等待未初始化 completion 的 RCU 死锁风险。
- `0013-osdrv--fix-wifi-module-init-error-unwind.patch`：
  修复模块初始化失败时资源未正确释放、注销未注册驱动的问题。
- `0014-osdrv--fix-wifi-stop-probe-on-sdio-enable-error.patch`：
  修复 SDIO function 使能失败后仍继续执行核心 probe 的问题。
- `0015-osdrv--fix-wifi-abort-invalid-register-initialization.patch`：
  修复寄存器访问失败后继续初始化、清理未订阅 IRQ 的问题。
- `0016-osdrv--fix-wifi-stop-unsupported-rhea-reset-retries.patch`：
  修复启动超时后反复执行不支持的 RHEA CPU 复位流程的问题。
- `0017-osdrv--fix-wifi-keep-bf01-supplicant-driver-loaded.patch`：
  修复 BF01 常供电场景下原厂 supplicant 自动卸载驱动的问题。
- `0020-osdrv--fix-wifi-demote-repetitive-tx-buffer-log.patch`：
  修复高频 TX buffer 日志刷屏，只调整日志等级。

## osdrv + ramdisk：完整 STA 联网服务

- `0018-osdrv--feat-wifi-add-complete-persistent-sta-service.patch`：
  独立控制器、扫描/连接/状态、模组 DHCP 同步 Linux IP/路由/DNS、
  私有配置持久化、生命周期脚本、工具构建及 `system/wifi` 安装规则
  在一个功能补丁内，不再拆成“有服务但未安装”的多阶段依赖。

同一笔 BSP 服务提交还包含：

- `ramdisk/rootfs/overlay/cv1815ja_bf01_spinand/usr/bin/wifi`
- `ramdisk/rootfs/overlay/cv1815ja_bf01_spinand/etc/init.d/S99user`

前提是 0009 驱动基础功能和 BF01 硬件使能；不依赖任何可选修复或诊断补丁
即可编译、安装。撤销此 BSP 服务提交会一起去掉服务补丁、命令入口和启动钩子。

## osdrv：可选启动诊断

- `0019-osdrv--feat-wifi-add-sdio-startup-diagnostics.patch`：
  将首次 SDIO 错误/CCCR 状态和 WSM 轮询、IRQ/RX 计数合为一个诊断功能。
  只依赖 0009，不是 STA 服务或 bug 修复的前提。

## 依赖与回退边界

```text
cvi_mpi 0005 -> 0008（GC4683 编译修复，与 Wi-Fi 无关）

osdrv 基线 -> 0009（可独立编译的驱动基础功能）
              |-> build 0010 + 0011 + BF01 板级配置（硬件使能）
              |-> 0012 / 0013 / 0014 / 0015 / 0016 / 0017 / 0020（独立修复）
              |-> 0018 + ramdisk 命令/启动钩子（完整 STA 服务）
              `-> 0019（可选诊断）
```

基础驱动是所有驱动扩展的必要依赖，不承诺删除基础驱动后其扩展仍能存在。
独立修复的含义是：撤掉一个修复，不迫使撤掉其他功能或修复；
被修复的原始问题可能重新出现，编译验证不等于运行时行为仍然正确。

本次在隔离工作区验证：

- 0001–0020 按仓库从 manifest 基线执行 `git am --ignore-whitespace` 成功。
- 新旧补丁系列最终五个 SDK 仓库的完整源码树完全相同。
- 单独 0009 可编译 ARM 驱动和原厂工具；0009 + 0018 可编译并安装 STA 服务。
- 0012–0020 逐个省略时，其余补丁可重放，相关驱动或工具可编译。
- 完整系列逐个 `git revert` 0012–0020 无冲突，结果与省略该补丁的源码树一致。
- GC4683 sample 严格语法编译、服务脚本语法及运行时安装目录检查通过。

## cvi_alios：小核启动抓拍入口

- `0021-cvi_alios--feat-add-early-boot-capture-hook.patch`：
  RTOS 媒体初始化后调用可选的板级抓拍入口，同时传递初始化结果；
  没有实现入口的其他板型保持原流程。依赖既有 fastboot 主流程，
  BF01 的三连拍、内存缓存及 IPC 协议实现位于板级 customization/src。
  必须重新编译 AliOS 并更新 yoc 分区，不能只推送 Linux 应用。

当前 BF01 抓拍模式的板级 defconfig 已关闭 ATBM6461 和外置无线驱动编译，
S99user 不再启动 Wi-Fi 服务；驱动补丁保留供后续无线功能负责人使用。
下文 Wi-Fi 验证方法仅适用于重新启用无线配置后的版本。

## 内部 SDK 同步补丁 0022–0026

- `0022-fsbl--fix-bl31-rebuild-cv181x-bl31-with-opteed-and-remove-.patch`：
  重新构建 cv181x BL31（带 opteed）并移除启动日志；只改 fsbl 预编译产物。
- `0023-cvi_alios--feat-sensor-add-gc4683-1920x1440-60fps-linear-and-30.patch`：
  GC4683 新增 1920x1440 60fps linear 与 30fps WDR 模式；依赖既有 0004。
- `patches_public/0024-cvi_alios--fix-venc-support-get-stream-from-alios.patch`：
  更新预编译 `cvi_mw_venc/obj/cvi_venc.fastboot.cv181x.o`，支持小核直接获取
  VENC 码流；源码改动不随本仓库发布，只交付对象文件。应用后需重新编译
  AliOS 并更新 yoc 分区。
- `patches_public/0025-cvi_mpi--fix-cvi_mpi-sample-declare-GC4683-1920x1440-modes-us.patch`：
  `sensor_cfg.h` 声明 GC4683 1920x1440 60fps linear 与 30fps WDR 两个 SNS_TYPE，
  与 cvi_alios 0023 的 sensor_cfg.h 保持一致；依赖既有 0008。
- `0026-cvi_mpi--feat-sample-add-gc4683-1920x1440-60fps-linear-and-30.patch`：
  sample common VI/ISP 表加入上述两个模式（Bayer GR、stSnsGc4683_Obj、
  disEnableSbm），新增 `PIC_1920x1440` 尺寸；依赖 0025 与 cvi_alios 0023。

- `0032-build--feat-fip-add-FSBL_LOG_LEVEL-Kconfig-to-control-fsbl-.patch`：
  `Kconfig` FIP setting 新增 `FSBL_LOG_LEVEL`（0~4，默认随 `BUILD_FOR_DEBUG`
  取 4/1），`scripts/fip_v2.mk` 优先以它作为 fsbl 的 `LOG_LEVEL`。配合板级
  `cv1815ja_bf01_spinand_fastboot_defconfig` 的 `CONFIG_FSBL_LOG_LEVEL=0`，
  FSBL 串口只保留 `E:` 行，BF01 实测 `release_blcp_2nd` 527ms→453ms。
  需 `defconfig cv1815ja_bf01_spinand` 重新应用后 `build_uboot`。
- `0033-fsbl--fix-clk-change-clk_spi_nand-to-166.67MHz-and-SCK-to-.patch`：
  `plat/cv181x/platform.c` 的 `sys_pll_od/nd` 把 `clk_spi_nand` 改为
  FPLL/9=166.67MHz，并写 `REG_SPI_NAND_TRX_CTRL1=0x900`（SCK=83.3MHz）、
  `REG_SPI_NAND_BOOT_CTRL=0x40280100`，提升 FSBL 读 yoc.bin / loader_2nd 的
  SPI NAND 速度。延时线参数沿用 Y15，需在 BF01（GD5F2GM7UEYIG）上验证。

- `0029-build--feat-alios-pack-yoc.bin-with-128-byte-cimg-header-wh.patch`：
  `alios.mk` 在 `CONFIG_MIRROR_2ND_PACK_HEAD=y` 时先用 `raw2cimg_alios.py`
  （PackYocTool）给 yoc.bin 加 128B 头（magic/file_sz/chunk_crc），`fip_v2.mk`
  把该宏导出给 fsbl；`raw2cimg_alios.py` 魔数改为小写 `cimg`，避免与 `CIMG`
  下载头冲突。须与 0030 同时应用。
- `0030-fsbl--fix-bl2-load-packed-2nd-image-by-header-file_sz-on-S.patch`：
  `bl2_opt.c` 的 `MIRROR_2ND_PACK_HEAD` 路径重写：先整块读入 DRAM 再 memcpy
  出头，从分区起点读 `file_sz+128` 后跳头做 CRC16/解压（原实现整页读会踩
  .bss、SPI NAND 页对齐导致解压源错位）；blcp_2nd 解压类型 `COMP_LZMA`→
  `COMP_LZ4`；`MAGIC_2ND` 改为 `cimg`；`print_head_2nd` 降为 NOTICE。
  须与 0029、0031 同时应用。
- `0031-cvi_alios--feat-build-compress-yoc.bin-with-lz4-and-keep-uncomp.patch`：
  `boards/cv181xc_evb/script/aft_build.sh` 的 yoc.bin 压缩由 `lzma -zf` 改为
  `lz4 -9`（fsbl 解压约 138ms，LZMA 约 270ms）；`solutions/fastboot/Makefile`
  不再用压缩后的 yoc.bin 覆盖 `generated/data/prim`（主机 `product image`
  对 LZ4 格式段错误）。配合板级 `CONFIG_COMPRESS_RTOS_BIN=y`、
  `CONFIG_MIRROR_2ND_PACK_HEAD=y`（已写入 `cv1815ja_bf01_spinand_fastboot_defconfig`），
  BF01 `release_blcp_2nd` 397ms→355ms。应用后需 `defconfig cv1815ja_bf01_spinand`
  重新加载配置，再 `build_alios`、`build_uboot`，主机需安装 `lz4`。

- `0034-build--feat-fip-export-ENABLE_BOOTLOGO-to-the-fsbl-build.patch`：
  `scripts/fip_v2.mk` 把 `ENABLE_BOOTLOGO`（`envsetup_soc.sh`，BF01 为 0）导出给
  fsbl make，原来该变量只控制 U-Boot 的 `CONFIG_BOOTLOGO`。须与 0035 同时应用。
- `0035-fsbl--fix-bl2-load-the-boot-logo-partition-only-when-ENABL.patch`：
  `Makefile` 在 `ENABLE_BOOTLOGO=1` 时定义 `FSBL_LOAD_BOOTLOGO`；`mmap.h` 的
  `BL_LOGO_*` 增加该宏条件，bootlogo 关闭时 fsbl 不再读 384KB MISC 分区
  （192 页 SPI NAND）。开 bootlogo 的板子行为不变。BF01 实测 `release_blcp_2nd`
  363ms→331ms。

内部 `fix(fastboot): select fastboot MPI objects`（release 构建前 clean 并以
`BOOT_SOLUTION=fastboot` 重建 cvi_mpi 归档）的功能已包含在 0021 对
`solutions/fastboot/Makefile` 的改动中，外部不再单独提供该补丁。

## BF01：小核早期录像与采音

- `0027-cvi_alios--feat-dma-shared-channel-and-i2s-async-rx-for-bf01-audio.patch`：
  为 BF01 小核早期采音提供共享 DMA 通道和 I2S 异步接收支持；录像联调期间的
  DMA 初始化、通道释放和中断处理修复合并在此功能补丁中。依赖 0021 和内部
  `patches_public/0024` 的 AliOS VENC GetStream 支持。
- `0028-linux_5.10--feat-dma-reserve-rtos-channel-for-bf01-audio.patch`：
  Linux 侧避开保留给小核 MIC 的 SysDMA 通道，修正共享 DMA 的中断初始化和完成
  处理，并保持 NAND 等其他通道可用。依赖 0027。

BF01 板级实现随后分为两个独立提交：

```text
0021 启动抓拍入口
  -> 0027 AliOS DMA/I2S 采音支持
  -> 0028 Linux SysDMA 通道保护
  -> BF01 设备树 DMA 映射
  -> BF01 小核 H.265/PCM 缓存与大核交接
```

板级 H.265 视频、PCM 音频缓存、三张照片后的录像启动、大小核交接和喇叭关闭
代码位于 `cvi_alios/solutions/fastboot/customization/cv1815ja_bf01_spinand/src`。
`ipcamera_bf01` 应用目录不属于本次 BSP 提交。

## A/B 分区（0036–0037）

- `0036-build--feat-ab-add-misc-controlled-A-B-boot-partitions-and-.patch`：
  `ROOTFS options` 新增 `Enable ab system`（CONFIG_AB_SYSTEM）与 `Enable b system`（CONFIG_B_SYSTEM）；
  分区表含 `rootfs_b`/`boot_b` 时同时打包 `rootfs_b.spinand`/`boot_b.spinand`；新增 `make misc`/`pack_misc`
  生成 misc.spinand（AvbABData，默认槽按 B_SYSTEM）；`mkcvipart.py` 输出 `BOOT_DEV(_B)`、`ROOTFS_DEV(_B)`、
  `SPL_BOOT(_B)_PART_OFFSET`、`AB_MISC_OFFSET/SIZE`；新增 `tools/common/update_misc`（Linux 静态工具：
  `boot_ok [N]` / `dump` / `get_active` / `set_active a|b [N]` / `set_tries a|b N` / `reinit`），AB_SYSTEM=y 时装入
  rootfs `/bin`；CFG 分区可选（无 CFG 时不打 cfg 镜像、不拷 PQ 参数）。
- `0037-u-boot-2021.10--feat-ab-select-the-boot-slot-from-the-misc-partition.patch`：
  新增 `cmd/cvi_boot_mode.c`（CONFIG_CMD_BOOT_MODE，SPL 与 u-boot 共用）：从 `misc` 裸 NAND 块读 A/B 元数据选槽、
  tries 递减、坏块跳过；SPL 快启按槽加载 `boot_a`/`boot_b`，加载失败标记该槽不可启动并同次回退另一槽；
  bootargs 带 `ubi.mtd=rootfs_x slot_suffix=_x`；`nandboot` 先 `ab_select`；命令 `get_slot`/`set_slot`/
  `dump_abdata`/`reinit_abdata`；cvsnfc 增加编程/擦除失败检测；`cvi_update.c` 在 `ENV_IS_NOWHERE` 下不再调 `saveenv`。

0036/0037 与以下板级文件在同一笔 BSP 提交中交付：

- `build/boards/cv181x/cv1815ja_bf01_spinand/partition/partition_spinand.xml`：
  `fip | 2nd | boot_a | boot_b | misc | rootfs_a | rootfs_b | SYSTEM | DATA`（删除 MISC/ENV/ENV_BAK/CFG，并入 DATA）。
- `build/boards/cv181x/cv1815ja_bf01_spinand/u-boot/cvitek_cv1815ja_bf01_spinand_defconfig`：
  `CONFIG_CMD_BOOT_MODE=y`、`CONFIG_ENV_IS_NOWHERE=y`（无 ENV 分区）、关闭 `CONFIG_SPL_RAW_IMAGE_SUPPORT`
  （否则擦空的 boot 槽会被当裸镜像执行，无法回退）。
- `ramdisk/rootfs/overlay/cv1815ja_bf01_spinand/etc/init.d/S98ab_misc`：开机 `update_misc spinand boot_ok` 补满当前槽 tries。

应用后 `menuconfig` 打开 `Enable ab system`，执行 `build_uboot`、`build_all`（含 `pack_misc`）；分区偏移变化，
所有镜像需重新打包并全量烧录。Linux 下 `update_misc spinand set_active b` 重启切到 B 槽。

## 更新与迁移

旧的 0008–0028 Wi-Fi/SDK 系列已用 revert 撤销，新的 0008–0020 替代它；
本次 0027/0028 是基于远端已有 0022–0026 的 BF01 早期录像功能补丁，
不是恢复或重复旧系列。
已有 SDK 应先保存本地源码改动，再使用仓库管理流程恢复 manifest 基线并应用新系列；
不要在有未保存修改的 SDK 上直接执行强制 reset 或 `--applypatch --force`。

拉取 BSP 和应用补丁不会自动更新本机生成的 `build/.config`。
编译前应加载 BF01 fastboot 板级配置，确认 `CONFIG_ATBM6461_BF01=y`，
再编译 osdrv，检查 rootfs 中的驱动和 `mnt/system/wifi` 工具后打包。

补丁编号全局唯一且连续，`repos --applypatch` 按仓库过滤并按编号应用。
IPCamera、个人说明文档、真实网络凭据和编译产物不在本次同步范围。

## 0038：ION 的 RTOS 预留隔离

`0038-linux_5.10--fix-ion-exclude-rtos-reservation.patch` 修复 carveout 驱动只调整
清零范围、但分配池和统计仍包含完整 ION 的问题。分配池改用扣除 RTOS 预留后的
起始地址、长度及 `total_size`，并检查非法预留及建池失败；不是放宽应用的隔离检查。
该补丁不改变内核导出接口或模块结构体布局。修改 ION 总容量时，FIP、RTOS、Linux
设备树必须使用同一内存图，不能只替换应用或混刷不同内存布局的镜像。

BF01 当前 AliOS 的实际多媒体分配来自独立 `ALIOS_RESV` 堆，故板级
`FREERTOS_RESERVED_ION_SIZE=0`，Linux `ION_SIZE=40 MiB`，不再扣除通用 fast-image
默认的 13.625 MiB。此补丁仍对确实存在预留的配置执行隔离；零预留不缩小分配池。
详见 `../docs/BF01_ION40_FINAL.md`，旧 48/56 MiB 说明为历史记录。
