# bf01 BSP 拉码与同步检查

卓昊（zhuohao）bf01 项目 SDK。

- 芯片/板型：**CV1815JA_BF01_SPIANND**（CV1815JA，SPI NAND 启动）
- DDR：**外挂 256MB**（CV1815J external DDR3）
- NAND：**256MB**
- SDK 版本：**v4.2.0-20260824**（内部 `v4.2.0` 分支 / github `cv18xx-v4.2.x` 分支，2026-08-24 状态）

## 关于 CV1815J 外挂 DDR 支持（重要）

CV1815J 外挂 DDR 的支持**已包含在本 SDK 基线中**，无需额外打补丁：

- 内部 `v4.2.0` 分支上对应 6 笗 commit（fsbl 仓）：
  `5130411`（DDR vendor/pinmux 扩展）、`c918138`（1815J 外挂 DDR3 bringup）、
  `4bfe579`（外挂 DDR3 RX ODT 120ohm）、`c1bca14`（ddr3_1600_x16 配置）、
  `1db9c60`（编译期宏选择外挂 DDR2/DDR3）、`48dca85`（GP_REG3 写 0x1815）。
- github `cv18xx-v4.2.x` 分支的 `weekly rls 2026.08.24` 已将上述全部代码以
  squash 方式合入（已逐文件 md5 核实一致，仅 `.gitignore`/`release.sh` 存在 rls 常规差异）。
- 因此 `patches/` 目录**留空**：内部与外部基线均已包含该功能，
  再打补丁会在 `repos --applypatch` 时报 already-applied 错误。

## 板卡目录（build/boards/cv181x/cv1815ja_bf01_spinand）

客户板卡配置存放在本目录 `build/boards/cv181x/cv1815ja_bf01_spinand/`，基于
`cv1815ja_wevb_0020a_spinand`（internal v4.2.0）派生，差异：

- 板名：`cv1815ja_bf01_spinand`（目录名必须小写：`boards_scan.py` 的
  `board_dir_to_name()` 不做大小写归一，大写 chip 段会报 unknown chip）
- DDR：外挂 256MB → `memmap.py` 中 `DRAM_SIZE = 256 * SIZE_1M`
- NAND：256MB → `partition/partition_spinand.xml` **实体文件**（不沿用 default 软链接），
  分区合计 252MB（fip 2.5M / 2nd 3M / BOOT 8M / MISC 384K / ENV+BAK 256K /
  ROOTFS 70M / SYSTEM 40M / CFG 4M / DATA 124M），留坏块管理余量
- `config.json` 的 `board_information`：`C906B + SPINAND 256MB + External DDR3 256MB (CV1815JA_BF01)`
- `linux/`、`u-boot/`、`rootfs_script/` 保留指向 `default/` 的相对软链接

拉码后通过 `sync.sh` 把板卡目录同步进 SDK（见下文「同步板卡目录」）。

## 先拉取 project-patches 仓库

`zhuohao/bf01` 目录属于 `project-patches` 仓库的 `cv181x_cv180x` 分支，建议先拉取该分支：

```bash
git clone -b cv181x_cv180x ssh://$(whoami)@gerrit-ai.sophgo.vip:29418/cvitek/project-patches.git /data/$(whoami)/project_cv181x
```

## 目录说明

- `manifest/sdk-cv181x_v4.2.0.xml`：内部 BSP 仓库清单（仅内部使用，release 不发布）
- `manifest/sdk-github-cv181x_v4.2.0.xml`：外部 BSP 仓库清单（github `sophgo`，`cv18xx-v4.2.x` 分支，含 host-tools）
- `manifest/git_version_cv181x_2026-08-24.txt`：内部版本快照
- `manifest/git_version_github_cv181x_2026-08-24.txt`：外部（github）版本快照
- `manifest/repo_config`：`repos` 脚本配置（内部基线，`--reproduce` 落位到内部快照）
- `manifest/repo_rls_config`：`repos` 脚本配置（外部基线）
- `patches/`：客户项目补丁（当前为空，见上文说明）
- `scripts/repos`：仓库管理脚本入口（软链接，指向分支根 `scripts/repos`）

## 使用方式（在新建 SDK 目录执行）

不要在当前项目目录直接执行。应先新建一个 SDK 工作目录，再把 `zhuohao/bf01/v4.2.0-20260824` 软链接进去：

```bash
mkdir -p /data/$(whoami)/13-zhuohao/bf01/bf01_sdk
cd /data/$(whoami)/13-zhuohao/bf01/bf01_sdk
ln -s /data/$(whoami)/project_cv181x/zhuohao/bf01/v4.2.0-20260824 bf01_bsp
```

## 拉取代码并复现到快照版本（内部基线）

```bash
cd /data/$(whoami)/13-zhuohao/bf01/bf01_sdk
./bf01_bsp/scripts/repos --bsp --gitclone --reproduce
```

脚本会读取 `bf01_bsp/manifest/repo_config`，按 `sdk-cv181x_v4.2.0.xml` 拉取代码，
并把每个仓库 checkout 到 `git_version_cv181x_2026-08-24.txt` 记录的 commit。

## 同步板卡目录

```bash
./bf01_bsp/scripts/sync.sh          # 正向同步：project-patches → SDK build/boards/cv181x/
./bf01_bsp/scripts/sync.sh -c       # 检查是否已同步（repos --check-env 最后一步会调用）
./bf01_bsp/scripts/sync.sh -r       # 反向同步：SDK 改动回写 project-patches
```

## 打补丁

```bash
./bf01_bsp/scripts/repos --applypatch
```

当前 `patches/` 为空，正常执行无补丁可打。

## 检查与远端同步状态（st）

```bash
cd /data/$(whoami)/13-zhuohao/bf01/bf01_sdk
./bf01_bsp/scripts/repos --run st
```

## 编译参考

```bash
source build/envsetup_soc.sh
defconfig cv1815ja_bf01_spinand    # 板卡目录需先经 sync.sh 同步进 SDK
clean_all
build_all
```
