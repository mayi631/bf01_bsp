# BF01 / ATBM6461 Wi-Fi 操作与固件集成

## 1. 实现范围

本方案实现 STA 联网：开启、扫描、连接、保存一个网络、断开、关闭、忘记网络，
并自动同步 Linux 的 IP、子网掩码、默认路由和 DNS。

参考了 DC100、DC309 的开机初始化，以及 DC309 的开启、扫描、连接、保存网络流程。
但联网后端不能照搬：8189FS 使用 Linux `wpa_supplicant`，本板 ATBM6461 的已验证方案
由模组固件完成关联和 DHCP，主控通过原厂 ioctl 控制并读取结果。

没有修改 ATBM 原厂驱动、原厂 supplicant/CLI 源码、GPIO 或模组固件。
新增独立用户态工具 `atbm_wifi_ctl` 和管理脚本 `wifi.sh`。
原厂工具仍随镜像安装，供专项调试使用，但不能与本服务同时运行。

当前 BF01 SDK 不包含 DC309 的 sophcam Wi-Fi 页面，因此本次提供的是命令行和应用调用后端，
不是已经移植好的图形界面，也不是 DC309 `wifi-daemon` 的 socket 协议兼容层。

## 2. 板端操作

以下命令在板端 root shell 执行。

### 首次配网

```sh
wifi on
wifi scan
wifi connect 'HOWNICE' '填写实际密码' WPA2
wifi status
```

不要原样使用“填写实际密码”。密码不写入 SDK 源码或本文档。

连接成功后，脚本将配置原子保存到 `/mnt/cfg/wifi/network.conf`，文件权限为 `0600`。
它只保存一个网络；连接新网络成功后替换旧网络，失败则保留旧配置。

`connect` 会等待关联、模组取得地址以及 Linux 地址/路由同步完成，最长约 45 秒。
返回 `0` 表示这些步骤成功；不代表目标网络必然能访问互联网，仍需进行下面的联网测试。

SSID 和密码支持空格、引号和 shell 特殊字符，必须正确引用；不支持 CR/LF 字符。
SSID 为 1～32 字节。支持 `NONE`、`WPA`、`WPA2`、`WPA3`，默认有密码时为 `WPA2`。
WPA/WPA2 支持 8～63 字节密码或 64 个十六进制字符；WPA3 密码为 8～63 字节。
WPA3 是否实际可用取决于模组固件/AP，本次主要验证 WPA2。

开放网络使用：

```sh
wifi connect '开放网络名称'
```

### 日常使用

| 命令 | 行为 |
| --- | --- |
| `wifi on` | 开启并连接已保存的网络；无配置时等待配网 |
| `wifi off` | 关闭主控 Wi-Fi 网络，保存关闭状态，下次开机保持关闭 |
| `wifi scan` | 必要时启动服务，等待扫描完成并直接打印结果 |
| `wifi status` | 打印开关状态、模组连接状态、Linux 地址、路由、DNS |
| `wifi disconnect` | 断开并关闭模组自动重连；保留保存的网络 |
| `wifi forget` | 断开、清除模组保存的网络和主控保存的网络 |
| `wifi boot` | 开机入口：按保存的开关状态启动，连接已保存网络 |
| `wifi shutdown` | 系统关机入口：停止服务，不改变保存的开关状态 |

`wifi scan` 不清除持久化的关闭标记；使用 `wifi on` 或成功的 `wifi connect` 才保存开启状态。
`wifi off` 不拉低 GPIO、不卸载共享的 cfg80211，也不保证模组射频物理断电。
如果需要硬件断电/低功耗，应另行根据模块规范实现，不能将本命令当作电流测试接口。

### 判断是否联网

```sh
wifi status
ping -I wlan0 -c 3 192.168.10.1
ping -I wlan0 -c 3 223.5.5.5
nslookup example.com
timeout 15 wget -O /tmp/wifi-check.html http://example.com
```

网关地址请换成 `wifi status` 中实际显示的 gateway。
不同网络可能禁止 ICMP 或外网访问，不能仅凭某个公网地址不回应 ping 就判断 SDIO 失败。
扫描输出的 `WPA2+` 表示 RSN 网络，不是对 WPA3 的准确识别。

## 3. 烧录、开机和配置保留

开机链路：

```text
inittab → rcS → S10auto_mount → S20network → S99user
                                         ↓
                             wifi.sh boot（后台运行）
                                         ↓
              cfg80211 + ATBM 驱动 → 网络监控 → 保存网络重连
```

Wi-Fi 启动入口放在 `S99user` 的应用/auto.sh 判断之前，避免 `/mnt/data/auto.sh` 提前退出而跳过它。
`S99user stop` 调用 `wifi shutdown`，不会把正常关机误保存为用户关闭 Wi-Fi。

开机没有保存网络时，服务准备好，但不会凭空知道 SSID/密码。首次需要执行一次 `wifi connect`。
已配网且保持开启时，以后开机或 `wifi on` 会连接保存网络。

配置存在 CFG 分区，不在只读 rootfs。
更新固件时应保留 CFG 分区，否则密码和开关状态会丢失。
若烧录流程同时擦除/覆盖 `cfg.spinand`，烧录后必须重新配网。
生产首次烧录如需预置网络，应通过单独的安全配网/生产步骤写入配置，勿把真实密码提交到仓库。

主控固件镜像不会自动给 ATBM 模组烧录配套固件；新模组仍需厂商提供的、与当前 SDIO 驱动匹配的固件。

## 4. 修改文件和打包

源文件：

- `osdrv/extdrv/wireless/realtek/atbm6461/tools/wifi_ctl.c`：原厂 ioctl 控制、事件排空、地址同步。
- `osdrv/extdrv/wireless/realtek/atbm6461/tools/wifi.sh`：服务和配置管理。
- `osdrv/extdrv/wireless/realtek/atbm6461/tools/Makefile`：编译新工具。
- `osdrv/extdrv/wireless/realtek/atbm6461/Makefile`：将工具和脚本安装进 `/mnt/system/wifi/`。
- `bf01_bsp/ramdisk/rootfs/overlay/cv1815ja_bf01_spinand/etc/init.d/S99user`：开机/关机入口。
- `bf01_bsp/ramdisk/rootfs/overlay/cv1815ja_bf01_spinand/usr/bin/wifi`：板端命令包装。

`install/.../rootfs/` 是构建产物。后续应修改上述源文件，再打包；不要只改安装目录。

完整构建使用本 SDK 现有流程：

```sh
cd /home/mayi/SDK_CV181X_BF01
export SCENES=1
source build/envsetup_soc.sh
defconfig cv1815ja_bf01_spinand
build_all
```

已有匹配内核/模块，仅更新用户态 Wi-Fi 集成时：

```sh
cd /home/mayi/SDK_CV181X_BF01
export SCENES=1 CHIP=cv1815ja BOARD=bf01_spinand
source build/envsetup_soc.sh
cvi_setup_env
make -C osdrv/extdrv/wireless/realtek/atbm6461 \
    CROSS_COMPILE="$CROSS_COMPILE" INSTALL_DIR="$SYSTEM_OUT_DIR/ko" tools runtime_install
pack_rootfs
pack_upgrade
```

增量流程沿用已有配置，不执行 `defconfig`，避免覆盖本地内核/板级配置。
它要求已有工具链、匹配的内核模块和其它分区镜像；首次构建请使用完整构建流程。

`CONFIG_ATBM6461_BF01=y` 必须保持开启。若内核配置/版本改动，应执行完整构建，不能混用旧模块。

构建产物位于 `install/soc_cv1815ja_bf01_spinand/`：
`rootfs.spinand` 是打包后的 ROOTFS 镜像，`rawimages/rootfs.spinand` 是原始 UBI 镜像，
`upgrade.zip` 是 SDK 升级包。不要混用原始镜像与带头镜像。
默认升级包还包含 CFG/DATA 等分区；直接执行完整升级可能覆盖配置和用户数据，
升级前请检查包内 `utils/update-script.sh_`，按项目原有烧录流程选择要更新的分区。
本次只重新打包用户态集成和升级包，没有重新编译其它分区，也没有执行烧录。

打包前检查：

```sh
ROOTFS=install/soc_cv1815ja_bf01_spinand/rootfs
ls -l "$ROOTFS/usr/bin/wifi"
ls -l "$ROOTFS/mnt/system/wifi/wifi.sh" "$ROOTFS/mnt/system/wifi/atbm_wifi_ctl"
ls -l "$ROOTFS/mnt/system/ko/cfg80211.ko" "$ROOTFS/mnt/system/ko/3rd/atbm6461_wifi_sdio.ko"
grep -n 'wifi.sh' "$ROOTFS/etc/init.d/S99user"
```

## 5. 网络同步与日志

监控进程每秒查询实际状态；连接事件丢失时，也能根据模组当前状态恢复地址配置。
它读取模组 DHCP 返回值，不写死测试时的 `192.168.10.130`。
默认路由使用 metric 50，不删除其他网卡的默认路由；存在更优先的 eth0 默认路由时，普通流量可能走 eth0。

模组提供 DNS 时使用实际 DNS；未提供 DNS 时尝试使用网关作为 DNS。
无 DNS 且网关也无法提供解析时，需要网络管理员提供可用 DNS。
脚本将 `/run/resolv.conf` bind mount 到只读 rootfs 的 `/etc/resolv.conf`，无需重新挂载根目录为可写。
关闭/断开时，监控进程清理 Wi-Fi 地址/路由并恢复启动服务前备份的 DNS。

```sh
cat /run/atbm_wifi-boot.log
tail -80 /run/atbm_wifi/wifi.log
cat /run/atbm_wifi/watch.pid
dmesg | grep -Ei 'mmc1|sdio|atbm|timeout|fail' | tail -80
```

日志在 tmpfs，重启后清空。监控仅处理 STA 网络与通用事件；没有实现原厂工具中的 BLE、低功耗、AP 等专项业务。

## 6. 与应用界面衔接

BF01 后续界面可以参考 DC309 的行为，但需调用本后端：

- 开关开启：`/usr/bin/wifi on`。
- 开关关闭：`/usr/bin/wifi off`。
- 扫描列表：`/usr/bin/wifi scan`，读取标准输出。
- 选网连接：`/usr/bin/wifi connect SSID PASSWORD SECURITY`。
- 连接状态：`/mnt/system/wifi/atbm_wifi_ctl status`，读取 key=value 输出。
- 忘记网络：`/usr/bin/wifi forget`。

扫描/连接可能等待数秒，应在工作线程或子进程执行，不能阻塞 UI 主线程。
应用传递 SSID/密码应使用 `exec`/`posix_spawn` 参数数组，不应拼接未转义的 shell 命令。
不要同时启动 `wpa_supplicant`、原厂 `atbm_iot_supplicant` 或另一个 Wi-Fi 管理进程。

## 7. 常见问题

- 提示原厂 supplicant 正在运行：先停止它，再运行本服务；不要并行管理同一模组。
- 提示 CFG 未挂载：检查 `S10auto_mount` 和分区状态；脚本不会自动格式化 CFG。
- 模块已加载但没有 wlan0：检查驱动/模组固件是否匹配及 WSM startup 日志。
- 已连接但无法上网：检查 Linux IP/掩码、默认路由、DNS和路由器外网条件。
- 连接失败：检查 SSID、加密方式、密码、信号、DHCP；查看日志的连接失败 reason。
- 冷启动验收：断电重启，确认保存网络自动连接；关闭后重启应保持关闭，再 `wifi on` 恢复连接。

完整镜像烧录和物理断电冷启动需要在实际烧录后验收；不能将运行时 ADB 测试当作冷启动结果。

## 8. 本次验证记录（2026-10-10）

通过 ADB 在实际 BF01 板上暂存新脚本和工具，使用已工作的 ATBM 驱动/配套模组固件测试：

- 扫描能找到 `HOWNICE`，WPA2 连接成功，配置文件权限为 `0600`。
- Linux 地址、掩码、网关和 DNS 从模组结果同步，网关和公网 IP 的 ping 均为 3/3。
- 使用实际路由器 DNS 成功解析 `example.com`，HTTP 下载成功。
- `off` 后执行 `boot` 保持关闭；`on` 重新连接保存的网络。
- `shutdown` 不保存关闭状态，再执行 `boot` 能重新连接并访问外网。
- 无效 SSID 返回失败，原保存配置未变；`forget` 清除配置后重新配网成功。
- 新工具通过 `-Wall -Wextra -Werror` 编译，脚本通过 shell 语法检查。

上述 `boot` 测试是调用开机入口，不是物理断电重启。ROOTFS 和升级包已经重新打包，
但未刷写到当前板子。新镜像烧录后仍需执行第 7 节的冷启动验收。
当前旧镜像上的临时测试入口为 `/mnt/data/atbm_wifi_test/wifi.sh`，
新镜像的正式入口才是 `/usr/bin/wifi`。
