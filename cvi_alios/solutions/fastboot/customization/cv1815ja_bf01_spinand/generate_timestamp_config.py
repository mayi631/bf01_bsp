import argparse
import configparser
import json
import os
import struct
import zlib
from pathlib import Path


def generate(solution, app_settings=None):
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
    rtc_offset = parser.getint("boot_timestamp", "external_rtc_utc_offset_minutes", fallback=480)
    if enabled not in (0, 1) or date_format not in (0, 1, 2) or not -720 <= utc_offset <= 840:
        raise ValueError("invalid boot_timestamp enabled/format/utc_offset_minutes")
    if not -720 <= rtc_offset <= 840:
        raise ValueError("invalid boot_timestamp external_rtc_utc_offset_minutes")
    content = (
        "#ifndef BF01_BOOT_TIMESTAMP_CONFIG_H\n"
        "#define BF01_BOOT_TIMESTAMP_CONFIG_H\n"
        f"#define BF01_TIMESTAMP_ENABLED {enabled}\n"
        f"#define BF01_TIMESTAMP_FORMAT {date_format}\n"
        f"#define BF01_TIMESTAMP_UTC_OFFSET_MINUTES {utc_offset}\n"
        f"#define BF01_EXTERNAL_RTC_UTC_OFFSET_MINUTES {rtc_offset}\n"
        "#endif\n"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != content:
        temporary = output.with_suffix(".h.tmp")
        temporary.write_text(content, encoding="utf-8")
        temporary.replace(output)
    policy = 0x42460000
    settings_path = app_settings or os.environ.get("BF01_BOOT_SETTINGS_JSON")
    if settings_path:
        settings = json.loads(Path(settings_path).read_text(encoding="utf-8"))
        if not isinstance(settings, dict):
            raise ValueError("APP settings must be a JSON object")
        if "datetime_wm_switch" in settings:
            if type(settings["datetime_wm_switch"]) is not bool:
                raise ValueError("invalid datetime_wm_switch")
            policy |= 0x100 | int(settings["datetime_wm_switch"])
        if "datetime_wm_format" in settings:
            formats = ("yyyy-MM-dd HH:mm:ss", "MM/dd/yyyy HH:mm:ss", "dd-MM-yyyy HH:mm:ss")
            policy |= 0x200 | (formats.index(settings["datetime_wm_format"]) << 1)
    prefix = struct.pack("<6I", 0x31535442, 1, 32, 1, policy, policy ^ 0xFFFFFFFF)
    record = prefix + struct.pack("<2I", zlib.crc32(prefix) & 0xFFFFFFFF, 0)
    image = b"\xff" * 2048 + record + b"\xff" * (2048 - len(record))
    for filename in ("timestamp.bin", "timestamp_bak.bin"):
        destination = output.parent / filename
        if not destination.exists() or destination.read_bytes() != image:
            temporary = destination.with_suffix(".bin.tmp")
            temporary.write_bytes(image)
            temporary.replace(destination)
    print(f"[boot-timestamp] factory cache policy=0x{policy:08x} source={settings_path or 'embedded defaults'}")
    print(f"[boot-timestamp] embedded enabled={enabled} format={date_format} utc_offset_minutes={utc_offset}")


if __name__ == "__main__":
    arguments = argparse.ArgumentParser()
    arguments.add_argument("-s", dest="solution", type=Path, required=True)
    arguments.add_argument("--app-settings", type=Path)
    options = arguments.parse_args()
    generate(options.solution.resolve(), options.app_settings)
