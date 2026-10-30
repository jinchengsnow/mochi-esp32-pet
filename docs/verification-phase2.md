# 第二阶段验证记录 — 2026-10-29

## 已完成

- ESP-IDF 5.5.5 / ESP32-S3 完整构建通过。BSP 3.0.0、LVGL 9.5.0，版本锁定。最终构建日志未发现编译 warning/error。
- 应用镜像 719,856 字节，分区容量 6MB。esptool 镜像格式、checksum 和 validation hash 的校验结果见 `image-info.txt`。
- 独立状态机通过 AddressSanitizer / UndefinedBehaviorSanitizer，覆盖状态持续时长的精确边界、连续点击不重置吃饭动画、吃饭转开心再转待机、手动睡觉、自动睡觉、各类唤醒、延迟调度和 32 位毫秒计数回绕。四种状态的动画参数检查覆盖两分钟内的采样范围。
- LVGL 离屏集成验证使用固件中的同一份 `pet_ui.c`、`pet_model.c` 和 `pet_font.c`。通过指针输入回调模拟按下和释放，验证角色区域与三个按钮、连续喂食、睡觉切换、自动睡觉、从睡觉进入喂食及完整吃饭结束流程。
- MOCHI UI 18 字库的 138 个字符均通过 LVGL glyph lookup，未返回占位符。修复了 LVGL 内置 CJK 小字库缺少当前界面用字的问题。已查看四状态 PNG 拼图，中文、按钮和角色均正常显示。
- 生成 160 帧 RGB565 同代码渲染预览，25Hz，6.4 秒 GIF，以及四张单状态 PNG 和拼图。预览的圆形遮罩用于模拟可视区域。
- Bash 脚本语法与 Python 脚本编译检查通过。刷写计划校验使用实际构建的 `flasher_args.json`，也会再次检查交付包内的镜像布局。

## Flash 布局

| 地址 | 文件 |
|---|---|
| 0x0 | bootloader/bootloader.bin |
| 0x8000 | partition_table/partition-table.bin |
| 0x10000 | amoled_pet.bin |

目标明确为 ESP32-S3 / 32MB / DIO / 80MHz。三个区域没有覆盖 NVS 的 0x9000–0xefff 间隔。交付的是分离镜像；工具写入前先备份并核验完整 Flash。

## 实机验收 — 2026-10-29

软件开发完成时未连接开发板；随后用户接板，本次已完成备份、刷写、启动与基本互动验收。

- 串口 `/dev/cu.usbmodem101` 识别为 ESP32-S3、32MB Flash；启动日志识别到 8MB PSRAM，PSRAM 测试通过。
- 完成当前 Flash 的 33,554,432 字节完整备份，并再次核对文件长度与 SHA256；原应用和数据备份保存在项目 `backups/20261029T120341Z-7nh_6b1f/`，未放入交付 ZIP。
- 使用 esptool 4.12.0 / 2,000,000 波特率写入启动器、分区表和应用；三个区域均报告 `Hash of data verified.`。默认速度的首次读取曾主动中止以提高读取速度，期间未写入 Flash；最终使用的是全新目录中的完整备份。
- 日志确认运行 `amoled_pet` 0.2.0，CO5300 屏幕和 CST9217 触摸初始化完成，进入 `MOCHI phase 2 ready`。本次监视期间只出现一次启动，未见 panic、assert、看门狗或棕断重启标记。
- 实际互动日志出现 `idle`、`happy`、`eating`、`sleeping` 四种状态及多次唤醒切换；用户明确确认“显示正常，三个按钮都有效”。
- 第二阶段基本上板验收通过。详细过程和日志见 [实机记录](hardware-test-2026-10-29.md)。

实机帧率、电池续航、60 秒自动睡觉的精确时长及长期稳定性尚未测量。当前睡觉只改变角色状态，不进入芯片低功耗休眠。第三阶段的饥饿值、成长与存档尚未实现。

交付包的 `SHA256SUMS` 包含各文件校验值；ZIP 旁的 `.sha256` 校验整个交付包。
