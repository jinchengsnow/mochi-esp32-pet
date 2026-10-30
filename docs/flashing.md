# MOCHI 0.8.1 刷机指南

仅适用于 Waveshare **ESP32-S3-Touch-AMOLED-1.75C，32MB Flash / 8MB PSRAM**。普通 ESP32-S3 板不能直接使用。

## 选择文件包

- `amoled-pet-firmware-0.8.1.zip`：刷机所需的三个镜像、刷机工具、说明和 SHA256 清单。
- `amoled-pet-game-0.8.1.zip`：完整源码、固件、玩法文档、预览图和第三方许可。

下载后解压，先核对旁附 `.sha256` 文件；包内 `SHA256SUMS` 用于校验各文件。完整包也包含本指南。

## macOS 推荐流程

安装 Python 3.9 或以上版本，并在独立虚拟环境安装刷机依赖：

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install 'esptool>=4.9,<5'
```

使用能传输数据的 USB 线连接开发板，打开电源，在解压后的包目录运行：

```bash
python scripts/device.py list
python scripts/device.py inspect --port /dev/cu.usbmodem实际编号
python scripts/device.py flash --port /dev/cu.usbmodem实际编号 --baud 2000000 --build-dir firmware
```

请用 `list` 输出的真实串口替换示例。辅助工具目前面向 macOS，会先读取并校验整块 32MB Flash 备份，再写入三个镜像。备份可能需要数分钟，保存在包目录的 `backups/`，请保留，不要上传其中的设备数据。

## 镜像地址

| 地址 | 文件 | 用途 |
|---|---|---|
| `0x0` | `firmware/bootloader/bootloader.bin` | 启动器 |
| `0x8000` | `firmware/partition_table/partition-table.bin` | 分区表 |
| `0x10000` | `firmware/amoled_pet.bin` | 游戏程序 |

Flash 参数使用包内 `firmware/flasher_args.json` 的设置。固件按三个区域分别刷写，**不要全片擦除，不要生成并写入填充整块 Flash 的镜像**。宠物存档位于 `0x610000`、长度64KB，普通升级不写入该区域。

其他系统可以使用 esptool 对照该地址表及配置刷写，但包内自动备份工具尚未验证 Windows/Linux；操作前应自行备份设备数据。

## 启动与使用

刷写完成后重启。已有设备从宠物存档恢复；新设备建立初始宠物。首次点击顶部“校时”，设置北京时间。PWR短按亮灭屏，BOOT短按开关声音。详细操作见完整源码包的 `docs/gameplay-logic.md`。

若未发现串口，先检查数据线、USB接口和电源，必要时拔插。若无法进入下载模式，可按住BOOT，再复位或重新上电，进入下载后松开BOOT。下载模式下不能操作游戏；刷写结束后正常重启。

本包固件来自已完成启动、存档恢复与声音基本开关验收的0.8.1构建。本次打包不代表再次完成续航、长期稳定性或所有低功耗边界验收。
