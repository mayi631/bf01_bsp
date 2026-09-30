#!/bin/bash
# set -x # 显示执行的命令
set -e # 遇到错误立即退出

# bf01 板卡目录同步脚本
# 把 bf01_bsp 中的板卡定制同步到 SDK 工作目录：
#   1. build/boards/cv181x/cv1815ja_bf01_spinand（板卡目录，defconfig 菜单由 boards_scan.py 扫描生成）
#   2. cvi_alios/solutions/normboot/customization/cv1815ja_bf01_spinand（alios 定制 pipeline，
#      envsetup 的 gen_pipeline_config.py 扫描生成 Kconfig 选项）
#   3. ramdisk/rootfs/overlay/cv1815ja_bf01_spinand（ramdisk 板级 overlay，构建按板名自动叠加；
#      当前无板级需求、目录未创建，检测到目录存在才同步该对）
# 在 SDK 工作目录执行（与 bf01_bsp 软链接同级）。

# 定义颜色代码
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 帮助信息
show_help() {
    echo -e "${BLUE}用法: $0 [选项]${NC}"
    echo -e "${BLUE}选项:${NC}"
    echo -e "${BLUE}  -c, --check      检查是否已同步，不执行实际同步操作${NC}"
    echo -e "${BLUE}  -h, --help       显示此帮助信息${NC}"
    echo -e ""
    echo -e "${BLUE}默认行为: 执行正向同步（从源到目标）${NC}"
}

# 检查目录是否已同步
check_sync_status() {
    local source_path="$1"
    local target_path="$2"
    local description="$3"
    local source_abs

    echo -e "${BLUE}🔍 正在检查 $description 是否已同步...${NC}"

    # 检查源目录是否存在
    if [ ! -d "$source_path" ]; then
        echo -e "${RED}错误: 源目录 '$source_path' 不存在${NC}"
        return 1
    fi

    source_abs="$(realpath "$source_path")"

    # 检查目标是否为软连接且指向源目录
    if [ ! -L "$target_path" ]; then
        echo -e "${YELLOW}❌ 目标路径 '$target_path' 不是软连接，未同步${NC}"
        return 1
    fi

    if [ "$(realpath "$target_path")" = "$source_abs" ]; then
        echo -e "${GREEN}✅ $description 软连接已正确指向源目录${NC}"
        return 0
    fi

    echo -e "${YELLOW}❌ $description 软连接指向不正确${NC}"
    echo -e "${YELLOW}当前指向: $(realpath "$target_path")${NC}"
    echo -e "${YELLOW}期望指向: $source_abs${NC}"
    return 1
}

# 同步目录函数
sync_directory() {
    local source_path="$1"
    local target_path="$2"
    local description="$3"
    echo -e "${BLUE}🔄 正在同步 $description 到 $target_path...${NC}"

    # 检查源目录是否存在
    if [ ! -d "$source_path" ]; then
        echo -e "${RED}错误: 源目录 '$source_path' 不存在${NC}"
        return 1
    fi

    # 确保目标目录的父目录存在
    local target_parent="$(dirname "$target_path")"
    mkdir -p "$target_parent"

    # 目标路径已存在时，先清理再创建软连接
    rm -rf "$target_path"
    ln -snf "$source_path" "$target_path"
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}$description 同步成功${NC}"
        return 0
    else
        echo -e "${RED}错误: $description 同步失败${NC}"
        return 1
    fi
}

# 解析命令行参数
CHECK_MODE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--check)
            CHECK_MODE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}未知选项: $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

CURRENT_DIR="$(pwd)"

# 定义需要同步的目录对
# 注意：这里使用了相对路径，在 SDK 工作目录（与 bf01_bsp 软链接同级）执行
SOURCE_BUILD="$CURRENT_DIR/bf01_bsp/build/boards/cv181x/cv1815ja_bf01_spinand"
TARGET_BUILD="$CURRENT_DIR/build/boards/cv181x/cv1815ja_bf01_spinand"

SOURCE_ALIOS="$CURRENT_DIR/bf01_bsp/cvi_alios/solutions/fastboot/customization/cv1815ja_bf01_spinand"
TARGET_ALIOS="$CURRENT_DIR/cvi_alios/solutions/fastboot/customization/cv1815ja_bf01_spinand"

SOURCE_RAMDISK="$CURRENT_DIR/bf01_bsp/ramdisk/rootfs/overlay/cv1815ja_bf01_spinand"
TARGET_RAMDISK="$CURRENT_DIR/ramdisk/rootfs/overlay/cv1815ja_bf01_spinand"

# 根据模式执行操作
if [ "$CHECK_MODE" = "true" ]; then
    echo -e "${BLUE}执行检查模式...${NC}"
    check_sync_status "$SOURCE_BUILD" "$TARGET_BUILD" "build目录"
    check_sync_status "$SOURCE_RAMDISK" "$TARGET_RAMDISK" "ramdisk目录"
    check_sync_status "$SOURCE_ALIOS" "$TARGET_ALIOS" "alios目录"
else
    echo -e "${BLUE}执行同步模式...${NC}"
    sync_directory "$SOURCE_BUILD" "$TARGET_BUILD" "build目录"
    sync_directory "$SOURCE_RAMDISK" "$TARGET_RAMDISK" "ramdisk目录"
    sync_directory "$SOURCE_ALIOS" "$TARGET_ALIOS" "alios目录"
fi
