import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, "out")
FONT = "C:/Windows/Fonts/msyh.ttc"
FONT_BOLD = "C:/Windows/Fonts/msyhbd.ttc"

def font(size, bold=False):
    return ImageFont.truetype(FONT_BOLD if bold else FONT, size)

def rounded_bg(size, color, radius):
    img = Image.new("RGBA", size, color)
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size[0]-1, size[1]-1], radius=radius, fill=255)
    img.putalpha(mask)
    return img

big = Image.open(os.path.join(OUT, "logo-512.png")).convert("RGBA")
sheet = Image.new("RGBA", (900, 560), (255, 255, 255, 255))
d = ImageDraw.Draw(sheet)

card = rounded_bg((320, 320), (24, 27, 33, 255), 48)
sheet.alpha_composite(card, (32, 32))
sheet.alpha_composite(big.resize((240, 240), Image.LANCZOS), (72, 72))

x = 400
for size in [16, 24, 32, 48, 64, 128]:
    icon = big.resize((size, size), Image.LANCZOS)
    sheet.alpha_composite(icon, (x, 32 + (128 - size)//2))
    d.text((x, 172), f"{size}px", fill=(90, 96, 106, 255), font=font(14))
    x += max(size, 56) + 18

y = 230
bg = rounded_bg((440, 150), (250, 251, 253, 255), 20)
sheet.alpha_composite(bg, (400, y))
sheet.alpha_composite(big.resize((52, 52), Image.LANCZOS), (594, y + 14))
d.text((460, y + 72), "云会议", fill=(28, 32, 38, 255), font=font(22, True))
d.text((460, y + 106), "登录以开始你的云会议", fill=(141, 150, 162, 255), font=font(14))

d.text((32, 372), "CCMeeting / 云会议 · 应用图标", fill=(28, 32, 38, 255), font=font(18, True))
d.text((32, 404), "概念：字母 C 兼镜头光圈（开口朝右），中心一点为镜头", fill=(90, 96, 106, 255), font=font(15))
d.text((32, 430), "底板：22% 圆角，蓝渐变 #4C8DFF → #2F6BFF", fill=(90, 96, 106, 255), font=font(15))
d.text((32, 456), "小尺寸版（≤24px）单独加粗笔画，避免发糊", fill=(90, 96, 106, 255), font=font(15))
d.text((32, 482), "文件：design/logo/logo.svg · app.ico · out/logo-*.png", fill=(141, 150, 162, 255), font=font(14))

out = os.path.join(OUT, "logo-final-preview.png")
sheet.convert("RGB").save(out)
print("preview:", out)