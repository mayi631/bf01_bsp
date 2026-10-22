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

## 更新与迁移

旧的 0008–0028 系列已用 revert 撤销，新的 0008–0020 替代它，
不是在已经应用的旧系列后面继续追加新系列。
已有 SDK 应先保存本地源码改动，再使用仓库管理流程恢复 manifest 基线并应用新系列；
不要在有未保存修改的 SDK 上直接执行强制 reset 或 `--applypatch --force`。

拉取 BSP 和应用补丁不会自动更新本机生成的 `build/.config`。
编译前应加载 BF01 fastboot 板级配置，确认 `CONFIG_ATBM6461_BF01=y`，
再编译 osdrv，检查 rootfs 中的驱动和 `mnt/system/wifi` 工具后打包。

补丁编号全局唯一且连续，`repos --applypatch` 按仓库过滤并按编号应用。
IPCamera、个人说明文档、真实网络凭据和编译产物不在本次同步范围。
