# MOCHI 0.7.0 刷写与启动验收 — 2026-10-30

已按用户指令将新版刷入此前同一块ESP32-S3-Touch-AMOLED-1.75C。串口 `/dev/cu.usbmodem21201`，MAC `b8:1f:3f:ca:e6:10`，Flash检测为32MB。此前版本在启动提示中使用固定版本号，本次改为从应用描述读取，保证启动提示与实际镜像一致。

## 备份与写入

- 完整备份：`backups/20261030T033248Z-3df3_9bz/flash.bin`，33,554,432字节。
- 备份SHA256：`f096a569ec1aa594f7f2eea361386d6a3e805683f7d121d1a65fbbfdc0a4519f`；完整长度与元数据校验通过。
- 固定镜像与ELF：`archives/flash-0.7.0-20261030T033157Z/`。
- 应用809,904字节，esptool镜像checksum与validation hash有效，应用版本0.7.0。
- 使用esptool4.12.0、2,000,000波特率，只写0x0启动器、0x8000分区表和0x10000应用。三个区域均报告 `Hash of data verified.`。
- 未全片擦除，未写位于0x610000、64KB的 `pet_nvs`。完整刷写输出在备份目录的 `flash-result.json`。

## 启动与存档

实际启动记录：

```text
app_init: App version:      0.7.0
pet_store: restored gen=1410 satiety=62 xp=144 meals=7
PET state=sleeping satiety=62 xp=144
pet: MOCHI game 0.7.0 ready: satiety=62 xp=144 restored=1 storage=ready battery=polling
pet_store: saved gen=1411 satiety=62 xp=144 meals=7
pet_store: saved gen=1412 satiety=62 xp=144 meals=7
```

启动观察约40秒，旧宠物成长144、饱食62、喂食7次成功恢复，启动约1秒后及约30秒后均保存成功。观察期间没有panic、断言失败、看门狗或再次启动。BSP的显示和触摸初始化完成，应用进入ready；读日志不等同于目视确认屏幕和真实触控。

完整串口日志：[hardware-0.7.0-boot-2026-10-30.log](hardware-0.7.0-boot-2026-10-30.log)。串口采集已结束并释放，设备继续运行。

## 后续实物操作

请在顶部“校时”保存当前北京时间。校准后启用夜间、午休和白天待机；完全切断供电后需要重新校时。原存档睡眠姿态在首次启动时被保留，可以摸醒或校时后按作息恢复。

本次确认了写入、启动、存档恢复和后台保存。作息跨界、RTC漂移、软件复位保时、完全断电重校、四种待机动画观感、真实触控、PWR低功耗恢复、电流和续航尚需实物验收，不能视为已全部通过。
