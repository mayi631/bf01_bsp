# bf01 补丁说明

命名格式必须是：`NNNN-仓库名--说明.patch`
例如：`0002-cvi_alios--feat-add-gcore-gc4683-sensor-support.patch`

`repos --applypatch` 只识别这个格式，并且要求 mailbox/`git am` 可用的 patch。

## 现有补丁

- `0001-cviruntime--fix-tpu-sdk-skip-flatbuffers-host-tests.patch`
- `0002-cvi_alios--feat-add-gcore-gc4683-sensor-support.patch`
  - 新增 `gcore_gc4683` 驱动
  - `package.yaml` / `sensor_cfg.c` / `sensor_cfg.h`
  - `app_main` BF01 boot mark + media init fail-safe
- `0003-build--feat-add-GCORE_GC4683-to-sensor-list.patch`
  - `build/sensors/sensor_list.json` 增加 `GCORE_GC4683`

## 正确使用顺序

```bash
./bf01_bsp/scripts/repos --applypatch
./bf01_bsp/scripts/sync.sh
source build/envsetup_soc.sh
defconfig cv1815ja_bf01_spinand
build_alios   # 或 build_all
```

注意：只跑 `sync.sh` 不同时 `--applypatch`，会缺 GC4683 枚举/驱动，编译 `custom_viparam.c` 会报 undeclared。
