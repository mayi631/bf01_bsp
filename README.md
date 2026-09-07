# bf01 BSP 拉码与编译

卓昊（zhuohao）bf01 项目 BSP。

- 芯片/板型：**CV1815JA_BF01**（CV1815JA，ARM，SPI NAND 启动）
- DDR：**外挂 256MB**（external DDR3）
- NAND：**256MB**
- SDK 基线：sophgo `cv18xx-v4.2.x` 分支，2026-08-24 快照（`weekly rls 2026.08.24`）

CV1815J 外挂 DDR 支持已包含在上述 SDK 基线中，无需额外补丁（`patches/` 为空）。

## 目录说明

- `manifest/sdk-github-cv181x_v4.2.0.xml`：BSP 仓库清单（github `sophgo`，`cv18xx-v4.2.x` 分支，含 host-tools）
- `manifest/git_version_github_cv181x_2026-08-24.txt`：版本快照（各仓 commit）
- `manifest/repo_config`：`repos` 脚本配置
- `patches/`：项目补丁（当前为空）
- `scripts/repos`：仓库管理脚本
- `scripts/sync.sh`：板卡目录同步脚本
- `build/boards/cv181x/cv1815ja_bf01_spinand/`：板卡配置

## 板卡目录说明（build/boards/cv181x/cv1815ja_bf01_spinand）

- `memmap.py`：`DRAM_SIZE = 256 * SIZE_1M`（外挂 DDR 256MB）
- `partition/partition_spinand.xml`：256MB NAND 分区表，合计 252MB
  （fip 2.5M / 2nd 3M / BOOT 8M / MISC 384K / ENV+BAK 256K /
  ROOTFS 70M / SYSTEM 40M / CFG 4M / DATA 124M），留坏块管理余量
- `config.json`：`C906B + SPINAND 256MB + External DDR3 256MB (CV1815JA_BF01)`
- `linux/`、`u-boot/`、`rootfs_script/` 为指向 `default/` 的相对软链接

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

## 同步板卡目录

```bash
./bf01_bsp/scripts/sync.sh          # 正向同步：bf01_bsp → SDK build/boards/cv181x/
./bf01_bsp/scripts/sync.sh -c       # 检查是否已同步（repos --check-env 最后一步会调用）
./bf01_bsp/scripts/sync.sh -r       # 反向同步：SDK 改动回写 bf01_bsp
```

## 打补丁

```bash
./bf01_bsp/scripts/repos --applypatch
```

当前 `patches/` 为空，正常执行无补丁可打。

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
