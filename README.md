# bf01 BSP 拉码与编译

卓昊（zhuohao）bf01 项目 BSP。

- 芯片/板型：**CV1815JA_BF01**（CV1815JA，ARM，SPI NAND 启动）
- DDR：**外挂 256MB**（external DDR3）
- NAND：**256MB**
- SDK 基线：sophgo `cv18xx-v4.2.x` 分支，2026-08-24 快照（`weekly rls 2026.08.24`）

CV1815J 外挂 DDR 支持已包含在上述 SDK 基线中。

## 目录说明

- `manifest/sdk-github-cv181x_v4.2.0.xml`：BSP 仓库清单（github `sophgo`，`cv18xx-v4.2.x` 分支，含 host-tools）
- `manifest/git_version_github_cv181x_2026-08-24.txt`：版本快照（各仓 commit）
- `manifest/repo_config`：`repos` 脚本配置
- `patches/`：项目补丁（含 GC4683 驱动 / sensor_cfg / sensor_list）
- `scripts/repos`：仓库管理脚本
- `scripts/sync.sh`：板卡定制同步脚本（build 板卡目录 + cvi_alios 小核定制；ramdisk 板级 overlay 按需，未创建时自动跳过）
- `build/boards/cv181x/cv1815ja_bf01_spinand/`：板卡配置
- `cvi_alios/solutions/normboot/customization/cv1815ja_bf01_spinand/`：小核（RTSmart/alios）定制 pipeline，`defconfig` 已选中 `CONFIG_CV1815JA_BF01_SPINAND`

## 板卡目录说明（build/boards/cv181x/cv1815ja_bf01_spinand）

- `memmap.py`：`DRAM_SIZE = 256 * SIZE_1M`（外挂 DDR 256MB）
- `partition/partition_spinand.xml`：256MB NAND 分区表，合计 252MB
  （fip 2.5M / 2nd 3M / BOOT 8M / MISC 384K / ENV+BAK 256K /
  ROOTFS 70M / SYSTEM 40M / CFG 4M / DATA 124M），留坏块管理余量
- `config.json`：`C906B + SPINAND 256MB + External DDR3 256MB (CV1815JA_BF01)`
- `linux/`、`u-boot/`、`rootfs_script/` 为指向 `default/` 的相对软链接
- `cv1815ja_bf01_spinand_defconfig`：
  - 启用 `CONFIG_SENSOR_GCORE_GC4683`，关闭 `CONFIG_SENSOR_GCORE_GC4653`
  - `CONFIG_ALIOS_CUSTOMIZATION_PIPELINE="cv1815ja_bf01_spinand"`
  - 启用 `CONFIG_TARGET_PACKAGE_ADBD`
  - 关闭无 arm 包项：`PINMUX` / `LIBCRYPTO` / `OTASERVER`
  - bring-up 阶段暂时关闭 `CONFIG_RTOS_INIT_MEDIA`（防 IPC 卡死）

## AliOS customization 说明

`cvi_alios/.../customization/cv1815ja_bf01_spinand/` 为 BF01 专用小核配置：

- 单路 GC4683（IIC2 / addr `0x31` / CAM_MCLK0 / RST=XGPIOA[2] / PWR=XGPIOA[3]）
- `package.yaml.turnkey`：`CONFIG_SNS0_TYPE: 26`
- 由 `scripts/sync.sh` 同步到 SDK 同名路径

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
./bf01_bsp/scripts/sync.sh          # 正向同步：bf01_bsp → SDK（build 板卡目录 + cvi_alios 小核定制）
./bf01_bsp/scripts/sync.sh -c       # 检查是否已同步（repos --check-env 最后一步会调用）
./bf01_bsp/scripts/sync.sh -r       # 反向同步：SDK 改动回写 bf01_bsp
```

同步范围：

1. `build/boards/cv181x/cv1815ja_bf01_spinand`
2. `cvi_alios/solutions/normboot/customization/cv1815ja_bf01_spinand`
3. `ramdisk/rootfs/overlay/cv1815ja_bf01_spinand`（目录存在时才同步）

## 打补丁

```bash
./bf01_bsp/scripts/repos --applypatch
```

当前 `patches/` 含 GC4683 相关补丁（命名格式 `NNNN-仓库--说明.patch`）：

- `0001-cviruntime--fix-tpu-sdk-skip-flatbuffers-host-tests.patch`
- `0002-cvi_alios--feat-add-gcore-gc4683-sensor-support.patch`
- `0003-build--feat-add-GCORE_GC4683-to-sensor-list.patch`

> 注意：只跑 `sync.sh` 不够。必须先 `--applypatch`，否则缺 GC4683 枚举/驱动，编译 `custom_viparam.c` 会报 undeclared。

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
