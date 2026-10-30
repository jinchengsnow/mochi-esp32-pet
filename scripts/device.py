#!/usr/bin/env python3
"""Explicit, backup-first esptool operations for the 32 MB ESP32-S3 board.

Only ``list`` runs without esptool. Importing this module performs no I/O.
Inspection and backup use ROM/stub reads; they may reset the device.
The ``flash`` subcommand is the only operation that writes device flash.
"""

from __future__ import annotations

import argparse
import datetime as dt
import glob
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile


FLASH_BYTES = 32 * 1024 * 1024
PORT_PREFIX = "/dev/cu.usb"


class DeviceError(RuntimeError):
    pass


def ensure_port(port: str) -> None:
    if not port.startswith(PORT_PREFIX) or "/" in port[len("/dev/"):]:
        raise DeviceError("Port must be an existing /dev/cu.usb* device.")
    try:
        mode = os.stat(port).st_mode
    except OSError as exc:
        raise DeviceError(f"Port does not exist or is inaccessible: {port}") from exc
    if not stat.S_ISCHR(mode):
        raise DeviceError(f"Port is not a character device: {port}")


def list_ports() -> list[str]:
    ports = []
    for port in sorted(glob.glob(PORT_PREFIX + "*")):
        try:
            ensure_port(port)
            ports.append(port)
        except DeviceError:
            pass
    return ports


def python_path(explicit: str | None) -> str:
    candidate = explicit
    if not candidate and os.environ.get("IDF_PYTHON_ENV_PATH"):
        candidate = str(Path(os.environ["IDF_PYTHON_ENV_PATH"]) / "bin/python")
    # Preserve venv/bin/python symlinks: resolving them loses pyvenv.cfg and
    # makes `python -m esptool` run outside the selected virtual environment.
    path = Path(candidate or sys.executable).expanduser().absolute()
    if not path.is_file() or not os.access(path, os.X_OK):
        raise DeviceError(f"Python executable not found: {path}")
    return str(path)


class Esptool:
    def __init__(self, port: str, python: str, baud: int = 460800):
        self.port, self.python, self.baud = port, python, baud

    def run(self, args: list[str], *, chip: str = "auto") -> str:
        ensure_port(self.port)  # Never let esptool open an absent or unrelated port.
        command = [self.python, "-m", "esptool", "--chip", chip,
                   "--port", self.port, "--baud", str(self.baud),
                   "--after", "hard_reset", *args]
        try:
            result = subprocess.run(command, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True,
                                    check=False, timeout=3600)
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise DeviceError(f"esptool could not complete: {exc}") from exc
        if result.returncode:
            raise DeviceError(f"esptool failed ({result.returncode}):\n{result.stdout}")
        return result.stdout


def flash_capacity(text: str) -> int:
    match = re.search(r"Detected flash size:\s*(\d+)\s*(KB|MB|GB)\b", text, re.I)
    if not match:
        raise DeviceError("Could not determine detected flash capacity; refusing to guess.")
    return int(match[1]) * {"KB": 1024, "MB": 1024**2, "GB": 1024**3}[match[2].upper()]


def inspect_device(tool: Esptool) -> dict:
    chip_output = tool.run(["chip_id"])
    # Accept esptool v4 ('Chip is') and v5 ('Chip type:') output.
    chip = re.search(r"(?:Chip is|Chip type:|Connected to)\s+(ESP32(?:-[A-Z0-9]+)?)\b",
                     chip_output, re.I)
    if not chip:
        raise DeviceError("Could not identify the chip from esptool output.")
    flash_output = tool.run(["flash_id"])
    mac = re.search(r"(?:Base\s+)?MAC:\s*((?:[0-9a-f]{2}:){5}[0-9a-f]{2})\b",
                    chip_output + "\n" + flash_output, re.I)
    return {"port": tool.port, "chip": chip[1].upper(),
            "flash_bytes": flash_capacity(flash_output),
            "mac": mac[1].lower() if mac else None}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def unique_json(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise DeviceError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> dict:
    try:
        with path.open(encoding="utf-8") as stream:
            value = json.load(stream, object_pairs_hook=unique_json)
    except (OSError, ValueError) as exc:
        raise DeviceError(f"Cannot read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise DeviceError(f"Expected a JSON object: {path}")
    return value


def firmware_plan(build_dir: Path) -> dict:
    build = build_dir.expanduser().resolve()
    data = load_json(build / "flasher_args.json")
    settings = data.get("flash_settings", {})
    extra = data.get("extra_esptool_args", {})
    if not isinstance(settings, dict) or not isinstance(extra, dict):
        raise DeviceError("Malformed flash_settings/extra_esptool_args.")
    if extra.get("chip") != "esp32s3" or settings.get("flash_size") != "32MB":
        raise DeviceError("Build must explicitly target esp32s3 with flash_size 32MB.")
    mode, frequency = settings.get("flash_mode"), settings.get("flash_freq")
    if mode not in {"dio", "dout", "qio", "qout"} or frequency not in {"20m", "26m", "40m", "80m", "120m"}:
        raise DeviceError("Unsupported or missing build flash mode/frequency.")
    # Do not forward arbitrary options from JSON (especially erase-all/force).
    raw_args = data.get("write_flash_args")
    expected = {"--flash_mode": mode, "--flash_freq": frequency, "--flash_size": "32MB"}
    if not isinstance(raw_args, list) or len(raw_args) != 6 or not all(isinstance(x, str) for x in raw_args):
        raise DeviceError("Expected exactly mode, frequency and size write_flash_args.")
    actual = {}
    for index in range(0, len(raw_args), 2):
        key = raw_args[index]
        if key.startswith("--"):
            key = "--" + key[2:].replace("-", "_")
        if key in actual:
            raise DeviceError("Duplicate write_flash option.")
        actual[key] = raw_args[index + 1]
    if actual != expected:
        raise DeviceError("write_flash_args contradict flash_settings or contain unsafe options.")
    mapping = data.get("flash_files")
    if not isinstance(mapping, dict) or not mapping:
        raise DeviceError("Build contains no flash_files mapping.")
    files = []
    for address, filename in mapping.items():
        try:
            offset = int(address, 0)
        except (ValueError, TypeError) as exc:
            raise DeviceError(f"Invalid flash offset: {address}") from exc
        if not isinstance(filename, str) or Path(filename).is_absolute():
            raise DeviceError("Firmware paths must be relative to the build directory.")
        path = (build / filename).resolve()
        if not path.is_relative_to(build) or not path.is_file():
            raise DeviceError(f"Missing firmware file or path escapes build directory: {filename}")
        size = path.stat().st_size
        if offset < 0 or offset % 4096 or size <= 0 or offset + size > FLASH_BYTES:
            raise DeviceError(f"Unaligned, empty or out-of-bounds flash region: {address} {filename}")
        files.append({"offset": offset, "path": str(path), "bytes": size, "sha256": sha256(path)})
    files.sort(key=lambda item: item["offset"])
    for previous, current in zip(files, files[1:]):
        if previous["offset"] + previous["bytes"] > current["offset"]:
            raise DeviceError("Firmware flash regions overlap.")
    return {"build_dir": str(build), "mode": mode, "frequency": frequency,
            "flash_bytes": FLASH_BYTES, "files": files}


def require_target(info: dict) -> None:
    if info["chip"] != "ESP32-S3" or info["flash_bytes"] != FLASH_BYTES:
        raise DeviceError("Flashing requires detected ESP32-S3 and exactly 32MB flash.")
    if not info.get("mac"):
        raise DeviceError("Cannot bind the backup to this device without a detected MAC address.")


def backup_device(tool: Esptool, output_dir: Path, info: dict | None = None,
                  plan: dict | None = None) -> Path:
    info = info or inspect_device(tool)
    output = output_dir.expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    # mkdtemp creates a new directory atomically; an earlier backup is never reused.
    directory = Path(tempfile.mkdtemp(prefix=f"{stamp}-", dir=output))
    binary = directory / "flash.bin"
    print(f"Reading all {info['flash_bytes']} flash bytes to {binary}", file=sys.stderr, flush=True)
    tool.run(["read_flash", "0x0", str(info["flash_bytes"]), str(binary)])
    if not binary.is_file() or binary.stat().st_size != info["flash_bytes"]:
        raise DeviceError(f"Incomplete backup; device will not be flashed. Retained: {directory}")
    metadata = {"format_version": 1, "status": "complete", "created_utc": stamp,
                "device": info, "file": "flash.bin", "bytes": binary.stat().st_size,
                "sha256": sha256(binary)}
    if plan is not None:
        metadata["planned_firmware"] = plan
    with (directory / "metadata.json").open("x", encoding="utf-8") as stream:
        json.dump(metadata, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    verify_backup(directory, info)
    return directory


def verify_backup(directory: Path, info: dict) -> None:
    metadata = load_json(directory / "metadata.json")
    binary = directory / "flash.bin"
    if (metadata.get("status") != "complete" or metadata.get("device") != info
            or metadata.get("file") != "flash.bin" or metadata.get("bytes") != info["flash_bytes"]
            or not binary.is_file() or binary.stat().st_size != info["flash_bytes"]
            or sha256(binary) != metadata.get("sha256")):
        raise DeviceError("Backup metadata, size or SHA256 verification failed; refusing to flash.")


def flash_device(tool: Esptool, build_dir: Path, backup_dir: Path) -> Path:
    plan = firmware_plan(build_dir)  # Reject unsafe builds before touching a device.
    info = inspect_device(tool)
    require_target(info)
    directory = backup_device(tool, backup_dir, info, plan)
    # Recheck identity and files after the potentially long backup operation.
    if inspect_device(tool) != info:
        raise DeviceError("Device identity/capacity changed after backup; refusing to flash.")
    if firmware_plan(build_dir) != plan:
        raise DeviceError("Build files changed during backup; refusing to flash.")
    verify_backup(directory, info)
    args = ["write_flash", "--flash_mode", plan["mode"],
            "--flash_freq", plan["frequency"], "--flash_size", "32MB"]
    for item in plan["files"]:
        args.extend([hex(item["offset"]), item["path"]])
    print(f"Verified complete backup: {directory}\nWriting only the build's mapped regions.",
          file=sys.stderr, flush=True)
    output = tool.run(args, chip="esp32s3")
    with (directory / "flash-result.json").open("x", encoding="utf-8") as stream:
        json.dump({"status": "esptool-write-succeeded", "esptool_output": output}, stream, indent=2)
        stream.write("\n")
    return directory


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    commands.add_parser("list", help="List existing /dev/cu.usb* character devices only")
    descriptions = {
        "inspect": "Read chip identity and detected flash capacity (may reset device)",
        "backup": "Read all detected flash into a unique backup directory; never write flash",
        "flash": "Validate 32MB ESP32-S3 build, make/verify a full backup, then write mapped regions",
    }
    for action, description in descriptions.items():
        command = commands.add_parser(action, help=description, description=description)
        command.add_argument("--port", required=True)
        command.add_argument("--esptool-python", help="Python with esptool installed; defaults to IDF env or current Python")
        command.add_argument("--baud", type=int, default=460800)
        if action == "backup":
            command.add_argument("--output-dir", type=Path, default=Path("backups"))
        if action == "flash":
            command.add_argument("--build-dir", type=Path, default=Path("build"))
            command.add_argument("--backup-dir", type=Path, default=Path("backups"))
    args = parser.parse_args(argv)
    try:
        if args.action == "list":
            ports = list_ports()
            print("\n".join(ports) if ports else "No /dev/cu.usb* ports found.")
            return 0
        if not 9600 <= args.baud <= 2000000:
            raise DeviceError("Baud must be between 9600 and 2000000.")
        ensure_port(args.port)
        tool = Esptool(args.port, python_path(args.esptool_python), args.baud)
        if args.action == "inspect":
            print(json.dumps(inspect_device(tool), indent=2))
        elif args.action == "backup":
            print(backup_device(tool, args.output_dir))
        else:
            print(flash_device(tool, args.build_dir, args.backup_dir))
        return 0
    except (DeviceError, OSError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
