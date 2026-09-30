# bf01 BSP 拉码与编译

卓昊（zhuohao）bf01 项目 BSP。

- 芯片/板型：**CV1815JA_BF01**（CV1815JA，ARM，SPI NAND 启动）
- DDR：**外挂 256MB**（external DDR3）
- NAND：**256MB**
- SDK 基线：sophgo `cv18xx-v4.2.x` 分支，2026-08-24 快照（`weekly rls 2026.08.24`）

CV1815J 外挂 DDR 支持已包含在上述 SDK 基线中；GC4683 sensor 与 SPL 启动相关改动见 `patches/`，需在编译前应用。

## 目录说明

- `manifest/sdk-github-cv181x_v4.2.0.xml`：BSP 仓库清单（github `sophgo`，`cv18xx-v4.2.x` 分支，含 host-tools）
- `manifest/git_version_github_cv181x_2026-08-24.txt`：版本快照（各仓 commit）
- `manifest/repo_config`：`repos` 脚本配置
- `patches/`：项目补丁（GC4683 sensor 驱动 + sensor_list + DTS 软链接跟随），详见 `patches/README.md`
- `scripts/repos`：仓库管理脚本
- `scripts/sync.sh`：板卡定制同步脚本（build 板卡目录 + cvi_alios 小核定制；ramdisk 板级 overlay 按需，未创建时自动跳过）
- `build/boards/cv181x/cv1815ja_bf01_spinand/`：板卡配置
- `cvi_alios/solutions/fastboot/customization/cv1815ja_bf01_spinand/`：小核（RTSmart/alios）定制 pipeline，`defconfig` 已选中 `CONFIG_CV1815JA_BF01_SPINAND`

## 板卡目录说明（build/boards/cv181x/cv1815ja_bf01_spinand）

- `memmap.py`：`DRAM_SIZE = 256 * SIZE_1M`（外挂 DDR 256MB）
- `partition/partition_spinand.xml`：256MB NAND 分区表，合计 252MB
  （fip 2.5M / 2nd 3M / BOOT 8M / MISC 384K / ENV+BAK 256K /
  ROOTFS 70M / SYSTEM 40M / CFG 4M / DATA 124M），留坏块管理余量
- `config.json`：`C906B + SPINAND 256MB + External DDR3 256MB (CV1815JA_BF01)`
- `linux/`、`u-boot/`、`rootfs_script/` 为指向 `default/` 的相对软链接
- `cv1815ja_bf01_spinand_fastboot_defconfig`：快速启动方案配置，`CONFIG_BOOT_TIME_OPTIMIZATION` / `CONFIG_NO_FB` / `CONFIG_NO_TP` / `CONFIG_UBOOT_SPL_CUSTOM` / `CONFIG_FASTBOOT=y`，不启用 sensor 与 `RTOS_INIT_MEDIA`

## ramdisk 板级 overlay

`ramdisk/rootfs/overlay/cv1815ja_bf01_spinand/`（**目录名 = 板名**）会被构建按
`CUST_FOLDER_NAME=$PROJECT_FULLNAME` 自动消费，内容 `cp -r` 进 `$(OUTPUT_DIR)/rootfs`。
当前含 `etc/init.d/S99user`（启动 app）、`etc/init.d/P10adbd`（adbd）、`system/ko/loadsystemko.sh`（ko 加载顺序）。

⚠️ overlay 目录里**不要放点文件**（如 `.gitkeep`）：构建的 `cp -r <overlay>/*` glob
不匹配点文件，cp 会收到字面量 `*` 报错断构建（实测 sh/bash 均退出码 1）。
**也不要放 README.md 等说明文件**：会被原样拷进设备 rootfs 顶层。

## 使用方式（在新建 SDK 目录执行）

先 clone 本仓库，再软链接到 SDK 工作目录：

```bash
git clone https://github.com/mayi631/bf01_bsp
mkdir -p <sdk_workdir> && cd <sdk_workdir>
ln -s <bf01_bsp 的绝对路径> bf01_bsp
```

## 拉取代码并复现到快照版本

```bash
cd <sdk_workdir>
./bf01_bsp/scripts/repos --bsp --gitclone --reproduce
```

脚本按 `sdk-github-cv181x_v4.2.0.xml` 从 github 拉取代码，
并把每个仓库 checkout 到 `git_version_github_cv181x_2026-08-24.txt` 记录的 commit。

## 同步板卡定制

```bash
./bf01_bsp/scripts/sync.sh          # 正向同步：bf01_bsp → SDK（build 板卡目录 + cvi_alios 小核定制 + ramdisk overlay）
./bf01_bsp/scripts/sync.sh -c       # 检查同步状态（repos --check-env 最后一步会调用）
```

同步方式是在 SDK 侧建立指向 `bf01_bsp` 的软链接，因此不存在"反向同步"：
在 `bf01_bsp` 里改动即对 SDK 生效。

## 打补丁

```bash
./bf01_bsp/scripts/repos --applypatch
```

当前 `patches/` 含 4 个补丁：

- `0001-cviruntime--fix-tpu-sdk-skip-flatbuffers-host-tests.patch`：`TPU_REL=1` 时跳过 flatbuffers host tests，规避 gcc11+ `-Werror=class-memaccess`
- `0002-build--feat-add-sensor-GCORE_GC4683-to-sensor-list.patch`：`sensor_list.json` 增加 `GCORE_GC4683`
- `0003-build--fix-u-boot-kernel-dts-find-follow-softlink.patch`：`find` 加 `-L`，跟随指向 `default/` 的软链接
- `0004-cvi_alios--feat-support-gcore-gc4683-sensor.patch`：gc4683 驱动与 `sensor_cfg` 注册

必须与 `sync.sh` 一起执行：只跑 `sync.sh` 不打补丁会缺 GC4683 枚举/驱动，编译 `custom_viparam.c` 报 undeclared。

可选：`./bf01_bsp/scripts/repos --genpatch` 从工作区 baseline 之上的新增提交导出补丁到 `patches/`。

## 环境检查

```bash
cd <sdk_workdir>
./bf01_bsp/scripts/repos --bsp --check-env
```

检查 clone/分支/commit/patch/submodule 是否完全同步（含 `sync.sh -c`）。

## 编译

```bash
source build/envsetup_soc.sh
defconfig cv1815ja_bf01_spinand    # 板卡目录需先经 sync.sh 同步进 SDK
clean_all
build_all
```

产物在 `install/soc_cv1815ja_bf01_spinand/`：`upgrade.zip` 烧录包及
`rawimages/*.spinand`、`fip.bin` 等。
