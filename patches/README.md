# bf01 补丁说明

- 0001-cviruntime--fix-tpu-sdk-skip-flatbuffers-host-tests.patch：
  TPU_REL=1 编译时 flatbuffers host tests 在 gcc11+ 报
  -Werror=class-memaccess，跳过 tests（install 目标不依赖 tests，
  flatc 正常产出）。已实测 TPU_REL=1 全链路 BUILD_ALL EXIT=0。

- 0002-build--feat-add-sensor-GCORE_GC4683-to-sensor-list.patch：
  `sensors/sensor_list.json` 增加 `GCORE_GC4683` 条目，使
  `gen_sensor_config.py` 生成 `CONFIG_SENSOR_GCORE_GC4683=y`，与
  SensorSupportList / cvi_mpi 的 gc4683 驱动对应。

- 0003-build--fix-u-boot-kernel-dts-find-follow-softlink.patch：
  `u-boot-dts` / `kernel-dts` 的 `find` 加 `-L`，跟随
  `boards/.../linux`、`u-boot`、`rootfs_script` 等指向 `default/` 的软链接，
  否则 DTS 拷不全。对应 build 仓 baseline `0fdd807e` 之后的提交
  `20873b5d`、`52337cf1`。

- 0004-cvi_alios--feat-support-gcore-gc4683-sensor.patch：
  新增 `components/cvi_sensor/gcore_gc4683/` 驱动（`gc4683_cmos.c` /
  `gc4683_cmos_ex.h` / `gc4683_cmos_param.h` / `gc4683_sensor_ctl.c`），
  并在 `sensor_cfg.h` 注册 4M@60/2M@60/1M@120 与 4M WDR2TO1 四种模式、
  `sensor_cfg.c` 挂接 `stSnsGc4683_Obj`、`package.yaml` 加入编译条件。
  与 0002 的 `GCORE_GC4683` 配置项配套，两者需同时生效。

- 0005-cvi_mpi--fix-support-gcore-gc4683-sensor.patch：
  为 `cvi_mpi` 增加 GC4683 驱动及 sample 中的模式处理。
  sample 共用头文件缺失的枚举与对象声明由独立补丁 0010 补齐。

- 0006-cvi_alios--fix-h264-vui-frame-rate.patch：
  修正 `components/cvi_platform/media/src/media_video.c` 中 H.264 VUI 的
  `time_scale`，按 `fps = time_scale / (2 * num_units_in_tick)` 设置，
  避免编码 15 fps 时播放器将码流识别为 7.5 fps。
  应用后需重新执行 `build_alios` 并更新板上的 `yoc` 分区，
  使启动时缓存的 SPS 包含正确时序。

- 0007-cvi_alios--fix-sys-init-failed.patch：
  在 `CONFIG_CHIP_BIG_ARM` 配置下，`solutions/fastboot/application/app_main.c`
  启动时调用 `ipcm_set_snd_cpu(0)`，将 IPCM 发送目标设为 ARM CPU 0。
  修正预编译 IPCM 库默认使用 RISC-V 大核导致的系统初始化失败。

## build

- 0008-build--feat-add-bf01-atbm6461-option.patch：
  增加 BF01 专用 ATBM6461 配置项，允许精简固件保留外置无线驱动。
- 0009-build--fix-retain-cv181x-wifi-sdio-node.patch：
  公共 BGA DTS 不再删除 `wifisd`，由板级 DTS 决定启用与时钟配置。

板级驱动选择、SDIO 节点启用、25 MHz 时钟上限、GPIOA20 复位电平、
内核网络配置以及 U-Boot pinmux 均直接保存在
`build/boards/cv181x/cv1815ja_bf01_spinand/`，按功能分别提交，
不重复生成补丁。

## cvi_mpi

- 0010-cvi_mpi--fix-declare-gc4683-sample-sensors.patch：
  补齐 0005 引用的四种 GC4683 模式及传感器对象声明，修复 sample 编译错误。

## osdrv

- 0011-osdrv--feat-import-atbm-iot-sdio-b1d1eeae.patch：
  单独导入原厂 `b1d1eeae8e95ab9a1670679f8599e968df6a495d` 的 183 个源码、
  配置文件，统一换行；不包含预编译程序、模块产物、备份及 Word 文档。
  两个含非 UTF-8 注释的源文件通过 Git binary hunk 保留原始注释字节，
  并非编译生成的二进制程序。
- 0012-osdrv--feat-build-atbm6461-for-cv181x.patch：
  增加 CV181x 独立构建入口、RHEA/SDIO/24 MHz 配置及原厂用户态工具交叉编译。
- 0013-osdrv--feat-add-cvitek-sdio-platform-callbacks.patch：
  增加 CVITEK 平台回调，使用 `cvi_sdio_rescan()`；不切换模组电源或占用唤醒 GPIO。
- 0014-osdrv--feat-wire-atbm6461-into-sdk-build.patch：
  将可选 ATBM6461 驱动接入 `osdrv` 和外置无线模块构建依赖。
- 0015-osdrv--fix-default-disabled-secure-boot-option.patch：
  为未配置的可选安全启动宏提供关闭默认值，修复未定义标识符编译错误。
- 0016-osdrv--fix-linux-ioctl-callback-signature.patch：
  修正 Linux `unlocked_ioctl` 回调命令参数类型及对应打印格式。
- 0017-osdrv--fix-cvitek-rcu-unload-synchronization.patch：
  使用标准 RCU 同步接口，避免 CVITEK 失败清理路径等待未初始化的 completion。
- 0018-osdrv--fix-unwind-module-initialization-failures.patch：
  传播模块初始化错误，并按实际初始化进度释放已注册的资源。
- 0019-osdrv--fix-propagate-sdio-enable-failures.patch：
  检查原有 `sdio_enable_func()` 的返回值，失败时不再继续核心 probe。
- 0020-osdrv--feat-log-first-sdio-io-failure.patch：
  首次 SDIO 传输失败时记录地址、长度、主机配置及 CCCR 状态，避免重复刷屏。
- 0021-osdrv--fix-abort-firmware-init-on-register-errors.patch：
  寄存器读写失败时停止固件接口初始化，避免清理尚未订阅的中断。
- 0022-osdrv--feat-diagnose-wsm-startup-polling.patch：
  增加 WSM 启动轮询、IRQ/RX 计数诊断，读取失败时不处理无效控制值。
- 0023-osdrv--fix-stop-unsupported-rhea-reset-on-timeout.patch：
  RHEA 启动超时时记录状态并返回错误，不再循环执行不支持的 CPU 复位流程。
- 0024-osdrv--fix-disable-auto-rmmod-on-always-on-bf01.patch：
  BF01 常供电模式关闭原厂 supplicant 自动卸载驱动，其他平台默认行为不变。
- 0025-osdrv--feat-add-standalone-sta-ioctl-controller.patch：
  增加独立 STA 控制器，实现扫描、连接、状态及模组 DHCP 事件同步到 Linux 网络。
- 0026-osdrv--feat-add-persistent-wifi-service.patch：
  增加 Wi-Fi 服务生命周期和 `/mnt/cfg/wifi` 私有配置持久化，不内置实际网络凭据。
- 0027-osdrv--feat-install-wifi-runtime-into-system.patch：
  将重新编译的工具及服务安装至 `system/wifi`，链接 `system/ko/3rd` 中的模组驱动。
- 0028-osdrv--fix-demote-sdio-tx-command-log.patch：
  高频 `cmd free/used` 信息改用已有 debug 日志级别，不改变传输和错误日志行为。

## 依赖与范围

0008、0009 配合板级配置启用 Wi-Fi；0010 依赖 0005；
0012–0028 是对 0011 原厂源码的逐项适配和修复，按编号应用。
启动钩子及 `wifi` 命令包装直接保存在 BF01 ramdisk overlay，分别提交。
IPCamera 应用、个人使用说明、网络凭据及编译生成的文件不在本次同步范围。

本目录编号全局唯一且连续，从 0001 起按顺序递增，不区分仓库；
补丁文件名仍保留 `<序号>-<repo>--<描述>.patch` 格式供 `repos --applypatch`
按仓库过滤。
