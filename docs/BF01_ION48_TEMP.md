# BF01：ION 48 MiB 临时验证方案

历史记录：当前使用 40 MiB，见 `BF01_ION40_FINAL.md`。此前将通用 fast-image
预留当成本板 AliOS 实际 ION 占用并扣除 13.625 MiB，判断有误；小核使用独立
`ALIOS_RESV` 堆。下文仅保留当时固件的配置与测试事实，不是当前推荐内存图。

日期：2026-11-05。用途：给 `yolov5s_cv181x_bf16.cvimodel` 的 Linux TPU 推理增加
连续设备内存。**这是待原厂确认的临时配置，不代表已验证稳定或满足原厂要求。**

## 1. 本次实际改动

只修改一处配置源文件：

`bf01_bsp/build/boards/cv181x/cv1815ja_bf01_spinand/memmap.py`

```diff
-    ION_SIZE = 16 * SIZE_1M
+    ION_SIZE = 48 * SIZE_1M
```

SDK 使用的 `build/boards/cv181x/cv1815ja_bf01_spinand/` 是指向上述 BSP 目录的软链接，
两条路径对应同一个文件，**不需要另改一份**。另外新增本说明文档，未改其他业务代码。

保持不变：DDR 总量 256 MiB，RTOS 区域 128 MiB，RTOS 系统区 10 MiB、
大块堆 116.625 MiB、共享区及参数区；RTOS 在 ION 中的预留仍为 13.625 MiB。
未修改摄像头缓冲区的大小、模型、算法、内核驱动、NAND 分区表或启动脚本。

## 2. 容量变化

| 项目 | 修改前 | 修改后 |
| --- | ---: | ---: |
| DDR 总量 | 256 MiB | 256 MiB |
| RTOS 物理区域 | 128 MiB | 128 MiB |
| ION 总预留 | 16 MiB | 48 MiB |
| ION 中 RTOS 预留 | 13.625 MiB | 13.625 MiB |
| Linux 可分配 ION 堆理论容量 | 2.375 MiB | 34.375 MiB |
| Linux 物理区域，不含 ION/RTOS | 112 MiB | 80 MiB |

表中 Linux ION 容量以正确排除 RTOS 预留为前提。2026-11-05 烧录后实测发现，
当前 Linux 分配池并未落实该排除，实际包含完整 48 MiB；这是隔离风险而非额外可用余量，
详见第 7 节。此前按启动日志将 2.375/34.375 MiB 视为实际堆容量的表述应予更正。

增加的 32 MiB 从 Linux 普通内存区域划出，**没有占用或缩小 RTOS 的 128 MiB 区域**。
Linux 原来的 `MemTotal` 约 102.25 MiB，假设其他开销相同，修改后约为 70.25 MiB；
实际数值应以上板后的 `/proc/meminfo` 为准，不能把物理区域大小直接当作 `MemAvailable`。

选取 48 MiB 的依据：模型的权重节约 13.600 MiB、命令节约 3.931 MiB、
共享工作区 12.500 MiB，合计约 30.031 MiB。34.375 MiB 的 Linux ION 堆作为初始验证容量，
并尽量保留 Linux 内存。**这些相加不是运行时精确用量或上限**；命令转换、对齐、
额外缓冲区和并行业务仍可能增加占用，必须实际验证，不能保证该容量一定足够。

## 3. 随配置自动变化的地址

以下不是手写修改，均由原有 `memmap.py` 表达式随 `ION_SIZE` 自动推导。
地址范围采用左闭右开区间。

| 项目 | 修改前 | 修改后 |
| --- | --- | --- |
| Linux 物理区域 | `[0x80000000, 0x87000000)` | `[0x80000000, 0x85000000)` |
| ION 区域 | `[0x87000000, 0x88000000)` | `[0x85000000, 0x88000000)` |
| RTOS 区域 | `[0x88000000, 0x90000000)` | 不变 |
| `ION_ADDR` | `0x87000000` | `0x85000000` |
| `H26X_BITSTREAM_ADDR` | `0x87000000` | `0x85000000` |
| `H26X_ENC_BUFF_ADDR` | `0x87200000` | `0x85200000` |
| `ISP_MEM_BASE_ADDR` | `0x87200000` | `0x85200000` |
| `BOOTLOGO_ADDR` | `0x87c00000` | `0x85c00000` |
| `CONFIG_SYS_TEXT_BASE` | `0x87d80000` | `0x85d80000` |

因此，“RTOS 区域不变”不等于“RTOS 固件不用更新”：其摄像头/显示等缓冲区地址随 ION
起始地址移动。FSBL、U-Boot、Linux DTB 和 RTOS 必须使用同一版内存布局，不能混刷。
`KERNEL_MEMORY_SIZE` 仍保留原配置的 256 MiB，通过设备树预留 ION/RTOS 区域排除，
没有把该字段手工改成 80 MiB。

## 4. 编译生成和部署要求

编译系统会自动生成 `build/output/cv1815ja_bf01_spinand/cvi_board_memmap.*`，
并将新布局供 FSBL、U-Boot、Linux DTB 和 RTOS 使用。不要手改这些生成文件。
RTOS 构建还会更新 `cvi_alios/components/aos/include/cvi_board_memmap.h` 和
`cvi_alios/solutions/fastboot/cvi_board_memmap.ld`；这些属于构建生成或复制的文件，
不是本次手工改动。

确认后可沿用当前板型配置，在 SDK 根目录执行完整增量构建：

```bash
export SCENES=1 CHIP=cv1815ja BOARD=bf01_spinand
source build/envsetup_soc.sh
cvi_setup_env
build_all
```

当前构建规则中，`build_all` 的 U-Boot/FSBL 依赖会构建 AliOS，相关目标也依赖
`memory-map`。需确认日志中的新布局被 FSBL/U-Boot、DTB 和 RTOS 使用，再按项目
既有流程部署同一次构建的匹配镜像。不要为此次调整执行 `clean_all`、重新 `defconfig`
或只刷一个旧/新混合的 `yoc.bin`。首次验证前保留可恢复的 16 MiB 版本镜像。

**首次修改阶段仅修改源配置并验证内存图生成，未由助手执行固件重新编译、刷机或重启。**
源文件修改不会自动更新板端；之后用户自行重新编译并烧录，烧录后的实测见第 7 节。

首次修改阶段静态验证结果：原有 `mmap_conv.py` 的 h/conf/ld 三种输出及 `memory_display.py`
的内存图生成均通过，输出隔离保存在 `/tmp/bf01_ion48_validation/`；已检查 ION/RTOS
边界、MiB 对齐、加载/解压区间与 ION 不重叠、RTOS 布局完全不变，以及 SDK/BSP 路径
确实指向同一源文件。`build/output/` 的旧布局未被替换，避免误认为现有镜像已更新。
这些检查不等于运行时验证，尤其不能证明摄像头、U-Boot 重定位和预编译组件均无问题。

部署后至少检查：

```sh
dmesg | grep -E 'Ion memory setup|rtos ion_size|size\(exclusion'
free
cat /proc/meminfo
cat /sys/kernel/debug/ion/cvi_carveout_heap_dump/total_mem
cat /sys/kernel/debug/ion/cvi_carveout_heap_dump/alloc_mem
```

预期 ION 启动地址 `0x85000000`，总量 48 MiB；排除 RTOS 后容量为
`0x2260000`，即 34.375 MiB。随后复测摄像头启动/抓图、预览、TPU 两张图片识别，
检查 ION 峰值、Linux `MemAvailable`、OOM、DMA 错误及长时间稳定性。

## 5. 提请原厂确认

1. CV1815JA ARM32 双系统下，ION 48 MiB、Linux 物理区 80 MiB、RTOS 128 MiB 的组合
   是否符合推荐配置、对齐要求和内存下限。
2. ION 基地址及摄像头缓冲区移动后，是否还有预编译库、DMA、启动代码或固件中的固定地址
   必须同步，当前 SDK 的自动生成流程是否覆盖完整。
3. `CONFIG_SYS_TEXT_BASE` 从 `0x87d80000` 移至 `0x85d80000` 是否允许，
   有无加载、解压、重定位或启动阶段重叠风险。
4. 该 BF16 YOLOv5 模型的实际 ION 峰值和厂商建议池大小，以及与摄像头并行时的容量余量。
5. Linux 可用内存减少 32 MiB 后，摄像头、网络、图片解码及升级流程所需的最低余量。

## 6. 回退

将同一份 `memmap.py` 中 `ION_SIZE = 48 * SIZE_1M` 改回 `16 * SIZE_1M`，
然后重新编译并部署使用一致布局的镜像。若尚未编译/部署，只需改回这一行即可恢复源配置。
不要通过占用 RTOS 物理内存或运行时直接改设备树/寄存器来绕过布局。

## 7. 2026-11-05 烧录后实测与新增风险

用户重新编译并烧录后，已确认 ION 基地址 `0x85000000`、总量 48 MiB，
Linux `MemTotal` 70.25 MiB。两张测试图均完成真实 TPU 推理，分别检出 1、2 个目标，
Forward 耗时 397.61、394.43 ms，退出码均为 0。结果图、日志和完整报告位于 SDK 根目录：
`bf01-yolo-results/20261105_103755/README.md`。

**发现当前 ION 分配池未排除 RTOS 预留区，不能据识别成功认定配置安全。**
启动日志虽然打印排除后 `0x2260000`（34.375 MiB），但 debugfs `summary`
分配位图实际包含整个 `[0x85000000, 0x88000000)`（48 MiB）。
SDK 源码 `linux_5.10/drivers/staging/android/ion/ion_carveout_heap.c` 的 169–174 行
仅更新局部 `page` / `size`；195–197 行 `gen_pool_add()` 仍使用原始基址与大小。
这使 RTOS 头部预留区可能被 Linux ION 分配，需原厂确认并给出隔离方案。

本次 Linux ION 分配峰值为 36,745,216 字节（35.04296875 MiB），退出后归零。
该峰值比正确隔离后的容量多约 0.668 MiB，因此即使后续落实隔离，也不能假定总池
48 MiB 足够当前程序使用；需结合厂商建议重新评估容量及与摄像头并行的余量。
未验证已经发生内存破坏，也未验证摄像头并行和长时间稳定性。

发现后停止追加推理，仅保存只读诊断证据；未修改驱动或内存源配置，未刷写和重启板卡。
建议向原厂同时提交 `ion_pool_evidence.log`、ION 峰值及上述驱动源码位置，
优先确认 RTOS ION 隔离，再安排进一步测试。
