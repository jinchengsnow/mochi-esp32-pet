# MOCHI 第二阶段实机记录 — 2026-10-29

结果：**已刷入 0.2.0；启动与基本互动验收通过。**

## 设备与写入

- 本机串口：`/dev/cu.usbmodem101`。
- 识别：ESP32-S3，硅片 v0.2，32MB Flash；启动日志确认 8MB PSRAM、80MHz，PSRAM 内存测试通过。
- 使用 `scripts/device.py flash` 执行备份后刷写流程。默认读取速度较慢，首次只读备份主动中止；以 2,000,000 波特率重新执行，创建了独立的完整备份。64KB 读取测试确认较高波特率可用。
- 最终备份：项目根目录下 `backups/20261029T120341Z-7nh_6b1f/flash.bin`，33,554,432 字节。
- 备份 SHA256：`98bae78b1614caa0d6657a6a491506d32b3db17b32ea0dcdac451290b474ff3c`。
- 备份目录中的 `metadata.json` 记录设备身份、完整性和计划写入区域；`flash-result.json` 保存 esptool 写入与校验结果。备份没有打入交付 ZIP。
- 0x0、0x8000、0x10000 三个区域均成功写入，并分别报告 `Hash of data verified.`。未执行全片擦除，NVS 间隔未被本次写入覆盖。

## 启动与互动

使用 ESP-IDF Monitor 重启设备并观察串口，日志保存在 `hardware-boot-2026-10-29.log`。

关键输出：

```text
app_init: Project name:     amoled_pet
app_init: App version:      0.2.0
CST9217: Resolution X: 466, Y: 466
esp_lvgl:touch: Touch input device registered successfully (IRQ mode: enabled)
PET state=idle
pet: MOCHI phase 2 ready: stroke / feed / sleep; 466x466, 25 Hz target
```

本次监视共记录 19 次状态输出，涵盖 `idle`、`happy`、`eating` 和 `sleeping`。启动标记只出现一次；未见 Guru Meditation、assert、abort、看门狗或棕断异常标记。

用户在实物上操作后明确反馈：**“显示正常，三个按钮都有效”。** 结合实际状态日志，确认本次显示及“摸摸、喂食、睡觉/叫醒”的基本互动可用。

驱动启动时输出 CO5300 3Ah 初始化覆盖提示、I2C 上拉检查提示，以及手势识别未启用提示；本次未出现 I2C 事务错误，触摸芯片成功识别，用户确认按钮有效。当前应用只使用单点点击。

## 验收范围

此次确认固件能在目标板运行，并完成四状态的基本互动。尚未测量实机帧率、耗电、长期运行稳定性或精确计时自动休眠；睡觉仍是角色动画，不是低功耗模式。饥饿、成长和掉电存档留待后续阶段。
