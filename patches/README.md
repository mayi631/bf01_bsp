# bf01 补丁说明

- 0001-cviruntime--fix-tpu-sdk-skip-flatbuffers-host-tests.patch：
  TPU_REL=1 编译时 flatbuffers host tests 在 gcc11+ 报
  -Werror=class-memaccess，跳过 tests（install 目标不依赖 tests，
  flatc 正常产出）。已实测 TPU_REL=1 全链路 BUILD_ALL EXIT=0。

- 0002-build--feat-add-sensor-GCORE_GC4683-to-sensor-list.patch：
  `sensors/sensor_list.json` 增加 `GCORE_GC4683` 条目，使
  `gen_sensor_config.py` 生成 `CONFIG_SENSOR_GCORE_GC4683=y`，与
  SensorSupportList / cvi_mpi 的 gc4683 驱动对应。

- 0003-build--fix-u-boot-kernel-dts-find-follow-softlink.patch：
  `u-boot-dts` / `kernel-dts` 的 `find` 加 `-L`，跟随
  `boards/.../linux`、`u-boot`、`rootfs_script` 等指向 `default/` 的软链接，
  否则 DTS 拷不全。对应 build 仓 baseline `0fdd807e` 之后的提交
  `20873b5d`、`52337cf1`。

- 0004-cvi_alios--feat-support-gcore-gc4683-sensor.patch：
  新增 `components/cvi_sensor/gcore_gc4683/` 驱动（`gc4683_cmos.c` /
  `gc4683_cmos_ex.h` / `gc4683_cmos_param.h` / `gc4683_sensor_ctl.c`），
  并在 `sensor_cfg.h` 注册 4M@60/2M@60/1M@120 与 4M WDR2TO1 四种模式、
  `sensor_cfg.c` 挂接 `stSnsGc4683_Obj`、`package.yaml` 加入编译条件。
  与 0002 的 `GCORE_GC4683` 配置项配套，两者需同时生效。

本目录编号全局唯一且连续，从 0001 起按顺序递增，不区分仓库；
补丁文件名仍保留 `<序号>-<repo>--<描述>.patch` 格式供 `repos --applypatch`
按仓库过滤。
