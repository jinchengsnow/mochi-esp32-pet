# MOCHI 0.4.4 刷写与启动修复 — 2026-10-29

当前设备运行 **0.4.4**。已完成写入、启动、旧宠物存档恢复与后台保存验证；用户已确认显示、PWR 熄屏/亮屏、黑屏触摸屏蔽和电量显示正常，基础实机验收通过。

## 备份与初次更新

设备 `/dev/cu.usbmodem101` 当前原固件为 RoundNav Komoot 导航探针。释放原有只读串口监视器后，使用 esptool 4.12.0、2,000,000 波特率完整读取 32MB Flash，并核对设备身份、长度与 SHA256。

- 原始完整备份：项目 `backups/20261029T142123Z-2bj9pa6o/flash.bin`。
- 大小：33,554,432 字节。
- SHA256：`dae0f5fbb9e3efc7ef13896ddfa5e2cbc633ed9a26f7da4481682d486c7eeb4c`。
- 初次选择的 0.4.3 镜像固定在 `archives/flash-0.4.3-20261029T141843Z/`，其中还保留该发布包与 ELF。

0.4.3 的三个固件区域写入和哈希验证成功，但启动后未进入应用 ready；用户确认屏幕异常或卡住。

## 定位与修复

通过 ESP32-S3 内置 JTAG 读取 FreeRTOS 任务调用栈。主任务停在 `bsp_display_brightness_set()` → `panel_io_spi_tx_param()` → `spi_bus_lock_acquire_start()`，等待 SPI 信号量；显示总线状态已经空闲。

CO5300 的亮度设置本身是 SPI 命令。0.4.3 在启动 LVGL 后、取得显示锁前发送亮度命令，与显示刷新并发，引发等待。0.4.4 将这条调用移到 `bsp_display_lock()` 成功之后，与 UI 刷新串行执行。PWR 熄屏/亮屏本来就从 LVGL 线程执行，没有更改其寄存器或长按关机规则。

证据文件：

- `hardware-0.4.3-startup-failed-2026-10-29.log`：失败启动及诊断复位日志。
- `hardware-0.4.3-startup-stacks-2026-10-29.log`：所有任务调用栈。
- `hardware-0.4.3-startup-spi-2026-10-29.log`：SPI 等待与空闲总线状态。

JTAG 检查包含调试器发起的受控复位，不能将这些复位解释为应用自行崩溃。

## 修复后的更新

修复前重新读取整个 64KB `pet_nvs`，确认与本次完整备份相同；0.4.3 尚未进入 UI，宠物存档未改变。因此保留完整原始备份，直接更新修复版的三个固件区域，仍不写 `pet_nvs`。

0.4.4 固定镜像与 ELF 位于 `archives/flash-0.4.4-20261029T144446Z/`。修复写入记录在完整备份目录的 `flash-result-0.4.4-repair.json`；三个区域均报告 `Hash of data verified.`。

实际启动日志：

```text
app_init: App version:      0.4.4
pet_store: restored gen=150 satiety=68 xp=26 meals=1
PET state=sleeping satiety=68 xp=26
pet: MOCHI game 0.4.4 ready: satiety=68 xp=26 restored=1 storage=ready battery=polling
pet_store: saved gen=151 satiety=68 xp=26 meals=1
```

随后持续成功保存，累计开机计时也正常，饱食自然从 68 降至 67。旧成长 26 和喂食次数 1 已恢复；没有修改宠物成长或初始化新宠物。

## 验收边界

0.4.4 的编译、ASan/UBSan 回归与同代码 LVGL 预览通过。真实设备已进入 ready 并持续保存，未再出现 0.4.3 的启动等待。

用户在修复后的实物上操作并反馈：

- 屏幕显示正常；PWR 短按熄屏、黑屏时触摸无反应、再次短按亮屏：“全部正常”。
- 顶部电量图标：“正常百分比或充电中”。

据此确认这几项基础物理效果通过实机验收。未另外量化电量计精度，也未逐项实测手指持续按住时亮屏、成长页及游戏内按键等组合场景。

实际功耗、续航、长时间压力运行和写 Flash 瞬间物理断电尚未测试。
