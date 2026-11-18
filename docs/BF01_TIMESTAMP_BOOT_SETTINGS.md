# BF01 时间戳首拍启动设置

## 问题与边界

2026-11-17实际媒体诊断：APP保存月日年，而三张照片和录像开头仍是年月日，录像后半段才变为月日年。原因是小核在Linux读取统一JSON、注册IPC之前已经编码首拍；运行时IPC确认不能修改历史JPEG/码流。

本修改仅提前传递`datetime_wm_switch`、`datetime_wm_format`。不改AI门控、抓拍/录像调度、上传条件、音频、RTC设置、直播通道或水印挂接通道；直播不增加水印。不等待Linux、不重新编码照片或视频、不借用RTC寄存器、A/B元数据或SDK媒体PARAM。

## 唯一配置源与缓存

用户设置仍唯一保存在`/mnt/data/ipcamera-config/app-settings.json`，真实CFG挂载存在时沿用应用的CFG路径。启动缓存仅是这两个键的派生数据，不对外提供第二套参数接口，也不反向覆盖JSON。缺失键保持缺失位，由小核内置INI默认补足。

记录为小端32字节：magic、version、size、generation、policy、policy反码、前24字节CRC32、保留0。generation非零且不回绕；同代不同policy拒绝选择。共享头`bf01_timestamp_settings.h`供ARM与小核共用。每槽256KiB、两个128KiB擦除块，仅使用第一个好块；记录放在第二个2KiB页，首个页保持全FF，避免覆盖本项目Linux NAND驱动在首个数据页头两个字节维护的坏块标记。仅支持当前板型2KiB页/128KiB块，几何不符拒绝写入。

Linux保存流程在统一配置独占锁下执行：

1. 合并增量键并原子保存、同步、读回JSON。
2. 仅当此次包含时间戳键时，检查两个缓存分区的名称、物理地址、大小、页/块几何和坏块。
3. 先更新无效/较旧槽，保留最新有效槽；再更新另一槽。同policy不擦写，CRC无效副本会修复。
4. 擦除、同步写页、关闭可写MTD描述符触发内核`mtd_sync`，重新打开设备读回并验证记录。该内核MTD字符设备没有`fsync`接口，不把`fsync`的EINVAL忽略后伪称持久化。
5. 两份均成功才允许设置ACK0；失败返回ACK2。其他设置不依赖这两份缓存。

JSON和原始NAND不是跨设备原子事务：JSON已写但缓存未完成时掉电或报错，这次不能确认成功，重启可能读到旧或新policy。下次Linux启动尽力用JSON修复；同值重发也修复缓存。启动修复失败只告警，不中断拍录；不把ACK2、仅收到命令或本机模拟当作实机业务通过。

FSBL在普通与镜像启动路径释放小核之前预加载两个槽各4KiB至现有64KiB共享参数缓冲，失败清空该副本，不阻止启动。读取严格限制在该槽两个块内，与Linux使用同样的数据区坏块标记。小核在`APP_BootCapturePrepare`中、读取RTC和媒体初始化之前选择有效最新记录；所有记录无效或同代冲突则保持原INI默认。运行时IPC继续保留，接口不变。没有改变内存地址或缓冲大小。

预加载存在少量NAND读取和启动开销，尚未做新版实机首拍耗时测量；不增加等待Linux或SD卡的同步点，不等于保证启动时间完全不变。

## 分区与迁移安全

| 分区 | 新大小 | 物理起点 | 变化 |
| --- | --- | --- | --- |
| SYSTEM | 40448KiB | 0x06600000 | 仅尾部缩小512KiB |
| BF01_TS0 | 256KiB | 0x08d80000 | 新原始槽，镜像timestamp.bin |
| BF01_TS1 | 256KiB | 0x08dc0000 | 新原始槽，镜像timestamp_bak.bin |
| DATA | 112768KiB | 0x08e00000 | 地址及大小不变，mtd8改为mtd10 |

fip、2nd、boot_a/b、misc、rootfs_a/b的地址和大小均不变。`misc`及其A/B数据不写入。槽不用`PARAM`名字，避免SDK `pack_param`覆盖或通用预加载按分区全长读爆共享区。`mkcvipart.py`为新槽生成FSBL常量，布局不一致直接构建失败。挂载脚本从新XML生成，只挂DATA，不挂载或格式化缓存槽。

**这是固件布局升级，不是可直接在线覆盖应用的修复。** 必须备份配置和用户数据，确认SYSTEM实际用途、原UBI卷大小及升级/恢复方案后，重新生成缩小后的SYSTEM镜像、新布局的引导参数及两套A/B根文件系统挂载脚本；不能把原40MiB SYSTEM UBI卷直接截尾。DATA虽未移动，新旧MTD编号不同，旧脚本在新布局可能把mtd8当DATA并尝试格式化缓存槽，严禁混用。旧布局上应用会明确ENODEV/EXDEV，不擦写别的分区；不能通过改节点名或伪造偏移绕过检查。

本次只修改与编译主机源码，**未在板端改分区、格式化、烧录、重启或中断直播**。实际迁移/刷机前需用户确认。

## 构建与首次参数种子

在干净SDK中按顺序应用BSP已有补丁及新增：

- `0046-fsbl--feat-bf01-load-timestamp-boot-settings.patch`：CV181X FSBL有界只读预加载，在板级marker存在时启用。
- `0047-build--feat-bf01-timestamp-boot-cache-images.patch`：生成分区常量、复制并打包两个出厂缓存镜像，仅BF01流水线新增镜像。

当前工作SDK已经正向修改，不要重复叠加；其他项目没有marker时不启用预加载。BSP板型目录与fastboot定制目录按原方式同步，应用仓库也需更新。重新`build_fsbl`（含U-Boot、小核及FIP）并按项目原流程完整重建布局相关镜像及应用安装内容；仅`build_alios`、仅编译应用或只替换yoc.bin都不完整。不要混用输出目录内的旧boot/rootfs/SYSTEM镜像。

出厂没有实存JSON时，默认生成带缺失键标志的缓存，使用INI内置默认，不擅自写入APP未下发值。首次升级若希望**第一次首拍**就匹配已有APP设置，先只读备份设备上的完整实存JSON，保留原JSON，并在SDK构建环境中指定备份的主机绝对路径：

```sh
export BF01_BOOT_SETTINGS_JSON=/absolute/path/app-settings.json
build_fsbl
```

Makefile自动调用`generate_timestamp_config.py`，此环境变量会种入两个新缓存镜像，不更改INI或云端凭证。也可用该脚本的`--app-settings`做独立生成校验，但独立生成不等于已打包生产镜像。种子仅派生两个时间戳键；类型或格式非法构建失败。不要用本次日志抄写的值替代设备实存JSON，也不要把种子回写为另一个配置源。完成专用升级镜像后取消该环境变量，避免将一台设备的设置固化给其他设备。

如果首次新布局启动没有正确种子，只能使用出厂默认完成那次首拍，Linux稍后修复缓存供下一次启动；不能宣称首拍已匹配。新布局应用可执行`ipcamera --settings-sync-boot`从统一JSON重试，不创建第二个相机/Wi-Fi/IPC消费者；该命令不能迁移分区、修改已编码照片或替代新版FSBL/小核。

`ipcamera --settings-show`的`boot_timestamp.errno=0`、`matches_saved=true`、`redundant=true`仅表示缓存与JSON一致；还必须核对新版FSBL日志`BF01 TS0/1 preload=0`及小核`boot-cache slot=... generation=... before first capture`，不能只看Linux IPC的`confirmed=1`。

## 验证状态

本机构建：ARM应用、小核、FSBL/BL2及FIP、两个缓存镜像通过。隔离回归覆盖三种格式×开关、全量/增量、同值不擦写、JSON/缓存失败ACK2、擦写/短写/关闭同步/读回故障、两槽掉电窗口、坏块边界、CRC每字节破坏、代数冲突与溢出、首次种子以及现有音频/迁移/并发行为。隔离测试使用内存MTD与硬件桩，不是实际NAND掉电试验。

首拍前导入与FSBL读取函数使用实际源码提取编译测试；分区和内存地址与原版本逐项比较，DATA挂载脚本确认使用mtd10。没有给现有仓库另建测试框架。

**未完成：** 新布局实际升级、真实APP全量/单独下发及其完整双向串口/SDIO证据、真实NAND断电、重启后的三张照片和录像首帧/中段/末帧、开关关闭及直播无水印的媒体验收。COM9最近一次为被占用状态，本次未抢占；历史媒体只用来定位问题，不作为新版通过结果。
