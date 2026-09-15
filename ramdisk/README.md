# bf01 ramdisk 定制说明

本目录是 bf01 的 ramdisk（rootfs）定制槽位。**当前无板级 rootfs 需求，overlay 目录未创建**，
按需启用（SDK 机制：overlay 目录不存在则自动跳过，wevb 板即此形态）。

## 启用方式（后续需要 rootfs 定制时）

创建 `ramdisk/rootfs/overlay/cv1815ja_bf01_spinand/`（**目录名 = 板名**，构建按
`CUST_FOLDER_NAME=$PROJECT_FULLNAME` 自动消费），按 rootfs 层级放文件：

```
ramdisk/rootfs/overlay/cv1815ja_bf01_spinand/
├── etc/init.d/S99user      # 自启脚本示例
└── bin/xxx                 # 工具示例
```

然后在 SDK 工作目录执行 `bash bf01_bsp/scripts/sync.sh` 同步（sync.sh 检测到目录存在
才同步该对，无需改脚本）。

## 机制

- `build/Makefile`（make rootfs）：`cp -r overlay/<板名>/* → $(OUTPUT_DIR)/rootfs`
- `build/common_functions.sh`（create_ramdisk_folder / pack_rootfs）导出
  `CUST_FOLDER_PATH=$RAMDISK_PATH/rootfs/overlay/$PROJECT_FULLNAME`
- 芯片层通用内容已由 SDK 基线覆盖（`ramdisk/configs/cv181x/` 等），板级只放增量

## ⚠️ 占位坑（为什么本目录不放占位文件）

- overlay 目录里**不能只放点文件**（如 `.gitkeep`）：构建的 `cp -r <overlay>/*` glob
  不匹配点文件 → cp 收到字面量 `*` → 报错断构建（实测 sh/bash 均退出码 1）
- **不能放 README.md 等说明文件占位**：会被原样拷进客户设备 rootfs 顶层

所以本说明放在 `ramdisk/README.md`（不参与 sync，永不进 SDK），overlay 目录整体按需创建。
