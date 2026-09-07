# bf01 补丁说明

- 0001-cviruntime--fix-tpu-sdk-skip-flatbuffers-host-tests.patch：
  TPU_REL=1 编译时 flatbuffers host tests 在 gcc11+ 报
  -Werror=class-memaccess，跳过 tests（install 目标不依赖 tests，
  flatc 正常产出）。已实测 TPU_REL=1 全链路 BUILD_ALL EXIT=0。
