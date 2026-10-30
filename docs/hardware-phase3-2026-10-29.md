# MOCHI 第三阶段实机记录 — 2026-10-29

结果：**0.3.0 已刷入；喂食、成长、存档写入和硬件重启恢复通过。**

## 备份与刷写

本机串口 `/dev/cu.usbmodem101`，识别为 ESP32-S3 / 32MB Flash。先在后台完成第二阶段当前 Flash 的完整 33,554,432 字节备份，同时在本机完成编译与界面验证。

备份保存在项目 `backups/20261029T123036Z-svlhc96q/`，其 `metadata.json` 记录原始镜像 SHA256 与设备身份。刷写前重新核对设备身份、完整备份长度与 SHA256、最终三个固件文件的计划与校验；并确认新分区 `pet_nvs` 的 0x610000–0x61ffff 区域原为空白。

使用 esptool 4.12.0、2,000,000 波特率，只写入启动器、分区表和应用。三个区域分别报告 `Hash of data verified.`。未全片擦除，也未写默认 NVS 间隔。此次写入结果及最终镜像计划保存在备份目录 `flash-result.json`。备份未打入发行包。

## 互动及持久化

完整串口日志：[hardware-phase3-2026-10-29.log](hardware-phase3-2026-10-29.log)。

1. 启动识别应用 `amoled_pet` 0.3.0、独立 `pet_nvs`、32MB Flash、8MB PSRAM；屏幕和 CST9217 触摸正常初始化，进入 `storage=ready`。
2. 初始宠物饱食 70、成长 0。自动保存工作正常。
3. 用户实际喂食后，串口记录 `satiety=95 xp=4 meals=1`；用户明确反馈“已喂食，数值增加且显示已保存”。
4. 随后实际互动记录成长增加到 5，喂食次数仍为 1。保存代数 12 记录 `satiety=95 xp=5 meals=1`。吃饱后再次喂食的拒绝加分行为另有主机集成测试覆盖。
5. 等待上述记录成功提交后，通过 ESP-IDF Monitor 的硬件复位指令主动重启。
6. 再次启动实际读取：

```text
pet_store: restored gen=12 satiety=95 xp=5 meals=1
PET state=idle satiety=95 xp=5
pet: MOCHI phase 3 ready: satiety=95 xp=5 restored=1 storage=ready
pet_store: saved gen=13 satiety=95 xp=5 meals=1
```

饱食、成长和喂食次数在 Flash 保存与硬件重启后保持一致，且启动后仍可继续保存。两次监视中的启动均为受控操作；未见存档失败、panic、assert、看门狗或棕断异常。

## 验证范围

实机验证了基本互动、真实 NVS 写入与复位后的读取。幼崽到少年/成年的边界、饥饿长时间变化、计时器回绕、损坏存档识别和奖励冷却在软件测试中验证；三阶段外观通过同代码 LVGL 预览检查。没有用测试后门修改实物成长数值。

此次没有在写 Flash 瞬间物理断电做故障注入，也未测量长期运行、实际帧率或电池续航。需要断电时，应等顶栏“已保存”；掉电未提交的数据可能回到上次完整记录。关机期间的饥饿/冷却计时暂停，睡觉仍为角色动画。
