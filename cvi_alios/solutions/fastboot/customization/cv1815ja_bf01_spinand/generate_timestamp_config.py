import argparse
import configparser
from pathlib import Path


def generate(solution):
    source = solution.parents[2] / "ipcamera_bf01/resource/parameter/fbm/cv1815ja_bf01_gc4683.ini"
    output = solution / "out/bf01_boot_timestamp_config.h"
    parser = configparser.ConfigParser(
        interpolation=None, inline_comment_prefixes=(";", "#"), strict=False
    )
    with source.open(encoding="utf-8") as configuration:
        parser.read_file(configuration)
    enabled = parser.getint("boot_timestamp", "enabled", fallback=0)
    date_format = parser.getint("boot_timestamp", "format", fallback=0)
    utc_offset = parser.getint("boot_timestamp", "utc_offset_minutes", fallback=480)
    if enabled not in (0, 1) or date_format not in (0, 1, 2) or not -720 <= utc_offset <= 840:
        raise ValueError("invalid boot_timestamp enabled/format/utc_offset_minutes")
    content = (
        "#ifndef BF01_BOOT_TIMESTAMP_CONFIG_H\n"
        "#define BF01_BOOT_TIMESTAMP_CONFIG_H\n"
        f"#define BF01_TIMESTAMP_ENABLED {enabled}\n"
        f"#define BF01_TIMESTAMP_FORMAT {date_format}\n"
        f"#define BF01_TIMESTAMP_UTC_OFFSET_MINUTES {utc_offset}\n"
        "#endif\n"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != content:
        temporary = output.with_suffix(".h.tmp")
        temporary.write_text(content, encoding="utf-8")
        temporary.replace(output)
    print(f"[boot-timestamp] embedded enabled={enabled} format={date_format} utc_offset_minutes={utc_offset}")


if __name__ == "__main__":
    arguments = argparse.ArgumentParser()
    arguments.add_argument("-s", dest="solution", type=Path, required=True)
    options = arguments.parse_args()
    generate(options.solution.resolve())
