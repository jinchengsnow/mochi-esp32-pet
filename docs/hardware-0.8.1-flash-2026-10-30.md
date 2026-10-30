# MOCHI 0.8.1 声音更新与实机记录 — 2026-10-30

最终刷入0.8.1，设备MAC `b8:1f:3f:ca:e6:10`，32MB ESP32-S3，串口 `/dev/cu.usbmodem21201`。本次在既有养成游戏中加入声音、BOOT开关和静音持久化。此前串口未枚举，用户重新插拔USB后恢复识别。

## 备份与镜像

- 完整32MB备份：`backups/20261030T042659Z-7_ulge1u/flash.bin`，33,554,432字节；SHA256 `be4619f2c3e529cd80e55a637191ac1c656773b13224e491afa32370d0bc6097`。
- 声音初版0.8.0写入后，补充处理BOOT唤醒恰好与Light-sleep入口重叠时的正常sleep-reject，避免恢复显示。
- 实机0.8.0日志确认 `cue=head`、`cue=tickle`、BOOT声音off/on与设置保存，未见Playback failed；用户反馈无声音或过小。
- 0.8.1把ES8311输出从45档调整为80档，mono音色复制到左右I2S声道，PCM峰值仍为6500。应用849,488字节。
- 最终镜像和ELF：`archives/flash-0.8.1-20261030T044131Z/`。原完整备份继续保留，每次后续代码更新前额外读取64KB当前 `pet_nvs`，不重用旧宠物快照覆盖当前数据。
- 最终NVS补充备份：`pet_nvs-before-final-0.8.1.bin`，SHA256 `cd9615b269428c59d9686703ba89749a91cab4cb5147cdf640ca3173b99dd23d`。
- 最终刷写三个固定区域：0x0、0x8000、0x10000，三处均 `Hash of data verified.`。0x610000宠物分区没有被写入或擦除。
- 写入结果：备份目录 `flash-result-0.8.1-final.json`，对应计划 `final-0.8.1-plan.json`。

## 实际启动

```text
app_init: App version:      0.8.1
pet_store: restored gen=1528 satiety=69 xp=199 meals=9
pet_audio: ES8311 TX-only ready=1 sound=on volume=80
pet: MOCHI game 0.8.1 ready: satiety=69 xp=199 restored=1 storage=ready battery=polling audio=ready
pet_store: saved gen=1529 satiety=69 xp=199 meals=9
pet_store: saved gen=1530 satiety=69 xp=199 meals=9
```

最终版本约40秒观察内没有panic、断言失败或看门狗，后台连续保存两次。原宠物成长199、饱食69、喂食9次成功恢复。变化来自用户此前互动和正常时间养成，没有初始化新宠物。

证据：

- [0.8.0初次启动](hardware-0.8.0-boot-2026-10-30.log)
- [BOOT边界修正后启动](hardware-0.8.0-boot-final-2026-10-30.log)
- [实际互动和BOOT设置记录](hardware-0.8.0-interactions-2026-10-30.log)
- [最终0.8.1启动](hardware-0.8.1-boot-final-2026-10-30.log)

串口采集可能触发USB_UART_CHIP_RESET；这是受控串口连接复位，不能当作应用自行崩溃。本次听感与开关验收采用下述用户实测反馈。

## 验收状态

0.8.0实际按键与音效事件已确认，初版听感未通过。调整并刷入0.8.1后，用户于2026-10-30反馈“有声了”，并明确确认“声音正常，BOOT也能开关”。据此确认板载扬声器实际出声、当前音量听感和BOOT基本声音开关通过实机验收。

重启静音保持、熄屏BOOT不亮屏、极短按与长按边界、功耗和续航仍需专门实物验收；本次基本功能确认不扩展为这些项目已全部通过。
