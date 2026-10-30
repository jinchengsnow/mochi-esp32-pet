#!/usr/bin/env python3
"""Package source and the three mapped firmware regions without filling NVS gaps."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile
from device import firmware_plan

project = Path(__file__).resolve().parents[1]
plan = firmware_plan(project / "build")
dist = project / "dist"
dist.mkdir(exist_ok=True)
name = "amoled-pet-game-0.8.1"
release = dist / name
if release.exists():
    shutil.rmtree(release)
release.mkdir()
for folder in ("main", "scripts", "tests", "preview", "docs", "assets"):
    shutil.copytree(project / folder, release / folder,
                    ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
for filename in ("CMakeLists.txt", "sdkconfig.defaults", "partitions.csv",
                 "dependencies.lock", "README.md", "NOTICE.md", "LICENSE", ".gitignore"):
    shutil.copy2(project / filename, release / filename)
firmware = release / "firmware"
firmware.mkdir()
for item in plan["files"]:
    relative = Path(item["path"]).relative_to(project / "build")
    target = firmware / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(item["path"], target)
for filename in ("flasher_args.json", "flash_args", "project_description.json"):
    # project_description embeds local paths and is not needed to flash.
    if filename != "project_description.json":
        shutil.copy2(project / "build" / filename, firmware / filename)
notices = release / "third_party"
notices.mkdir()
for dep in sorted((project / "managed_components").iterdir()):
    for file in sorted(dep.iterdir()):
        if file.is_file() and file.name.upper().startswith(("LICENSE", "COPYING", "NOTICE")):
            target = notices / dep.name / file.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target)
font = project / "managed_components/lvgl__lvgl/src/font/lv_font_source_han_sans_sc_16_cjk.c"
if font.exists():
    (notices / "source_han_font_notice.txt").write_text(
        "\n".join(font.read_text().splitlines()[:35]) + "\n", encoding="utf-8")
# Confirm the packaged layout is actually usable by the flash helper.
firmware_plan(firmware)
lines = []
for file in sorted(release.rglob("*")):
    if file.is_file():
        lines.append(f"{hashlib.sha256(file.read_bytes()).hexdigest()}  {file.relative_to(release)}")
(release / "SHA256SUMS").write_text("\n".join(lines) + "\n")
archive = dist / f"{name}.zip"
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
    for file in sorted(release.rglob("*")):
        if file.is_file():
            z.write(file, f"{name}/{file.relative_to(release)}")
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
(dist / f"{name}.sha256").write_text(f"{digest}  {archive.name}\n")
print(json.dumps({"archive": str(archive), "bytes": archive.stat().st_size,
                  "sha256": digest, "firmware_bytes": sum(f["bytes"] for f in plan["files"])}, indent=2))

# A smaller package for users who only need to flash the verified image.
minimal_name = "amoled-pet-firmware-0.8.1"
minimal = dist / minimal_name
if minimal.exists():
    shutil.rmtree(minimal)
minimal.mkdir()
shutil.copytree(firmware, minimal / "firmware")
(minimal / "scripts").mkdir()
for filename in ("device.py", "device.sh"):
    shutil.copy2(project / "scripts" / filename, minimal / "scripts" / filename)
for filename in ("LICENSE", "NOTICE.md"):
    shutil.copy2(project / filename, minimal / filename)
shutil.copytree(notices, minimal / "third_party")
shutil.copy2(project / "docs/flashing.md", minimal / "README.md")
checks = [f"{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.relative_to(minimal)}"
          for f in sorted(minimal.rglob("*")) if f.is_file()]
(minimal / "SHA256SUMS").write_text("\n".join(checks) + "\n")
minimal_zip = dist / f"{minimal_name}.zip"
with zipfile.ZipFile(minimal_zip, "w", zipfile.ZIP_DEFLATED) as z:
    for f in sorted(minimal.rglob("*")):
        if f.is_file():
            z.write(f, f"{minimal_name}/{f.relative_to(minimal)}")
minimal_hash = hashlib.sha256(minimal_zip.read_bytes()).hexdigest()
(dist / f"{minimal_name}.sha256").write_text(f"{minimal_hash}  {minimal_zip.name}\n")
print(json.dumps({"archive": str(minimal_zip), "bytes": minimal_zip.stat().st_size,
                  "sha256": minimal_hash}, indent=2))
