"""Convert frames rendered by firmware UI code into inspectable PNGs and GIF."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont

project = Path(__file__).resolve().parents[1]
frames_path = Path(sys.argv[1])
output = project / "docs" / "preview"
output.mkdir(parents=True, exist_ok=True)
frames = [Image.open(p).convert("RGB") for p in sorted(frames_path.glob("*.ppm"))]
assert len(frames) == 418
names = ["idle", "happy", "eating", "sleeping"]
sheet = Image.new("RGB", (1012, 1120), "#0c0c13")
draw = ImageDraw.Draw(sheet)
font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial.ttf", 24)
small = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial.ttf", 15)
for i, name in enumerate(names):
    image = frames[i * 40 + 20]
    image.save(output / f"{name}.png")
    x, y = 20 + (i % 2) * 506, 28 + (i // 2) * 552
    sheet.paste(image, (x, y + 35))
    draw.text((x + 233, y), name.upper(), font=font, fill="#cbb4f6", anchor="mt")
    draw.text((x + 233, y + 511), "466 x 466 / LVGL / same firmware UI", font=small,
              fill="#9d97ae", anchor="mt")
sheet.save(output / "states.png")
for i, name in enumerate(("hungry", "young", "adult")):
    frames[160 + i].save(output / f"{name}.png")
growth = Image.new("RGB", (1458, 510), "#0c0c13")
for i in range(3):
    growth.paste(frames[160+i], (10+i*486, 34))
    ImageDraw.Draw(growth).text((243+i*486, 5), ("HUNGRY", "YOUNG", "ADULT")[i], font=small,
                               fill="#cbb4f6", anchor="mt")
growth.save(output / "growth.png")
frames[0].save(output / "mochi.gif", save_all=True, append_images=frames[1:160],
               duration=40, loop=0, optimize=False, disposal=2)
print(f"Wrote {output}: 4 states, contact sheet, 6.4-second animation")

for i, name in enumerate(("wishes", "game", "result", "growth-shop", "bow", "crown")):
    frames[163+i].save(output / f"{name}.png")
game_sheet = Image.new("RGB", (1458, 1008), "#0c0c13")
for i, name in enumerate(("WISHES", "CATCH THE STARS", "REWARDS", "GROWTH & SHOP", "BOW", "CROWN")):
    x, y = 10 + (i % 3)*486, 30 + (i // 3)*504
    game_sheet.paste(frames[163+i], (x, y))
    ImageDraw.Draw(game_sheet).text((x+233, y-25), name, font=small, fill="#cbb4f6", anchor="mt")
game_sheet.save(output / "game-overview.png")
print("Wrote 6 game/quest/shop/cosmetic scenes")

# These are real RGB565 firmware renders, cropped for an at-a-glance evolution lineup.
cn_font = ImageFont.truetype(str(project / "assets/fonts/NotoSansSC-subset.ttf"), 20)
forms = ["奶团幼猫", "蜜桃小猫", "杏糖花猫", "蜜金橘猫", "奶霜长毛", "月光银猫", "星纹灵猫", "星云长毛", "极光灵猫", "星耀守护猫"]
evolution = Image.new("RGB", (1480, 650), "#12121e")
evo_draw = ImageDraw.Draw(evolution)
for i, form in enumerate(forms):
    source = frames[169+i*4]
    source.save(output / f"level-{i+1:02}.png")
    for j, state in enumerate(("idle", "happy", "eating", "sleeping")):
        frames[169+i*4+j].save(output / f"level-{i+1:02}-{state}.png")
    col, row = i % 5, i // 5
    x, y = col*296, row*325
    evo_draw.text((x+148, y+16), f"Lv.{i+1:02}  /  {i*60} XP", font=small, fill="#cbb4f6", anchor="mt")
    crop = source.crop((80, 145, 386, 347)).resize((291, 192), Image.Resampling.LANCZOS)
    evolution.paste(crop, (x+2, y+49))
    evo_draw.text((x+148, y+256), form, font=cn_font, fill="#ffe8ca", anchor="mt")
    features = ["圆脸 / 短尾", "蜜桃毛色 / 尾纹", "杏色斑纹 / 颊毛", "金色皮毛 / 长耳", "奶霜颈毛 / 蓬尾",
                "银紫毛色 / 月纹", "星形额纹 / 星光", "星云长毛 / 光环", "薄荷极光 / 月纹", "金色星纹 / 守护光环"]
    evo_draw.text((x+148, y+289), features[i], font=cn_font, fill="#9d97ae", anchor="mt")
evolution.save(output / "evolution.png")
# Check state animation and accessory fit separately from the clean lineup.
state_sheet = Image.new("RGB", (1458, 1008), "#0c0c13")
for i, index in enumerate((169+9*4, 169+9*4+1, 169+9*4+2, 169+9*4+3, 209+8*2, 209+8*2+1)):
    x, y = 10+(i%3)*486, 30+(i//3)*504
    state_sheet.paste(frames[index], (x, y))
    ImageDraw.Draw(state_sheet).text((x+233, y-25), ("LV10 IDLE", "HAPPY", "EATING", "SLEEPING", "BOW", "CROWN")[i],
                                   font=small, fill="#cbb4f6", anchor="mt")
state_sheet.save(output / "evolution-states.png")
print("Wrote 10 evolution forms, four states per level, and accessory-fit sheet")

frames[231].save(output / "evolution-unlocked.png")

crown_sheet = Image.new("RGB", (1944, 510), "#0c0c13")
for i, index in enumerate((226, 228, 229, 230)):
    crown_sheet.paste(frames[index], (i*486, 34))
    ImageDraw.Draw(crown_sheet).text((i*486+233, 5), ("CROWN / IDLE", "MAX HAPPY HOP", "EATING", "SLEEPING")[i],
                                   font=small, fill="#cbb4f6", anchor="mt")
crown_sheet.save(output / "evolution-crown-states.png")

for i, name in enumerate(("battery-normal", "battery-low", "battery-10", "battery-charging", "battery-unavailable", "battery-absent", "screen-off", "screen-awake")):
    frames[232+i].save(output / f"{name}.png")
power_sheet = Image.new("RGB", (1458, 1008), "#0c0c13")
for i, index in enumerate((232, 233, 235, 234, 238, 239)):
    x, y = 10+(i%3)*486, 30+(i//3)*504
    power_sheet.paste(frames[index], (x, y))
    ImageDraw.Draw(power_sheet).text((x+233, y-25), ("BATTERY", "LOW < 10%", "CHARGING", "10% BOUNDARY", "SCREEN OFF", "PWR TO WAKE")[i],
                                   font=small, fill="#cbb4f6", anchor="mt")
power_sheet.save(output / "power-overview.png")
print("Wrote battery status and screen-off/wake scenes")

time_sheet = Image.new("RGB", (1458, 510), "#0c0c13")
for i, name in enumerate(("BASELINE", "DISPLAY ON / 1 HOUR", "LOW POWER / 1 HOUR")):
    time_sheet.paste(frames[240+i], (i*486, 34))
    ImageDraw.Draw(time_sheet).text((i*486+233, 5), name, font=small, fill="#cbb4f6", anchor="mt")
    frames[240+i].save(output / ("time-baseline.png", "time-bright-hour.png", "time-rest-hour.png")[i])
time_sheet.save(output / "time-rates.png")

for i, name in enumerate(("touch-head", "touch-play", "touch-tickle", "touch-hug", "touch-left", "touch-right", "touch-high-five")):
    frames[243+i].save(output / f"{name}.png")
touch_sheet = Image.new("RGB", (1458, 1008), "#0c0c13")
for i, index in enumerate((243, 245, 246, 247, 248, 249)):
    x, y = 10+(i%3)*486, 30+(i//3)*504
    touch_sheet.paste(frames[index], (x, y))
    ImageDraw.Draw(touch_sheet).text((x+233, y-25), ("单击 · 摸头", "三击 · 挠肚皮", "长按 · 抱抱", "左滑 · 逗猫", "右滑 · 逗猫", "上滑 · 击掌")[i],
                                   font=cn_font, fill="#cbb4f6", anchor="mt")
touch_sheet.save(output / "touch-overview.png")
print("Wrote seven gesture scenes from real LVGL pointer events")

routine_names = ("night", "morning", "nap", "afternoon")
for i, name in enumerate(routine_names):
    frames[250+i].save(output / f"routine-{name}.png")
for i, name in enumerate(("stretch", "groom", "ball", "watch")):
    start = 254 + i*40
    frames[start+20].save(output / f"ambient-{name}.png")
    frames[start].save(output / f"ambient-{name}.gif", save_all=True,
                       append_images=frames[start+1:start+40], duration=100,
                       loop=0, optimize=False, disposal=2)
frames[414].save(output / "clock-setting.png")
routine_sheet = Image.new("RGB", (1458, 1512), "#0c0c13")
for i, index in enumerate((250, 251, 252, 253, 274, 314, 354, 394, 414)):
    x, y = 10+(i%3)*486, 30+(i//3)*504
    routine_sheet.paste(frames[index], (x, y))
    ImageDraw.Draw(routine_sheet).text((x+233, y-25),
        ("NIGHT / 22:40", "MORNING / VARIABLE WAKE", "NAP / 13:20", "AFTER NAP", "STRETCH", "GROOM", "YARN BALL", "WATCH BUTTERFLY", "SET LOCAL TIME")[i],
        font=small, fill="#cbb4f6", anchor="mt")
routine_sheet.save(output / "routine-overview.png")
ambient_frames = []
for tick in range(40):
    combined = Image.new("RGB", (732, 500), "#12121e")
    for activity, caption in enumerate(("伸个懒腰", "洗洗脸", "自己玩毛线球", "看看四周")):
        x, y = 10 + (activity % 2)*366, 10 + (activity // 2)*246
        ImageDraw.Draw(combined).text((x+178, y), caption, font=cn_font, fill="#cbb4f6", anchor="mt")
        combined.paste(frames[254+activity*40+tick].crop((55, 135, 411, 349)), (x, y+28))
    ambient_frames.append(combined)
ambient_frames[20].save(output / "ambient-overview.png")
ambient_frames[0].save(output / "ambient-overview.gif", save_all=True,
    append_images=ambient_frames[1:], duration=100, loop=0, optimize=False, disposal=2)
print("Wrote schedule, clock editor and four 4-second autonomous animations")
sound_sheet = Image.new("RGB", (1458, 510), "#0c0c13")
for i, name in enumerate(("sound-on", "sound-muted", "sound-unavailable")):
    frames[415+i].save(output / f"{name}.png")
    sound_sheet.paste(frames[415+i], (10+i*486, 34))
    ImageDraw.Draw(sound_sheet).text((243+i*486, 5), ("SOUND ON", "BOOT / MUTED", "AUDIO UNAVAILABLE")[i],
                                    font=small, fill="#cbb4f6", anchor="mt")
sound_sheet.save(output / "sound-overview.png")
print("Wrote audio on/muted/fault indicators from real UI code")
