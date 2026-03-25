from PIL import Image, ImageDraw, ImageFont
import sys
import os

def text_to_xbm(text, font_path, font_size, width, height, filename_prefix):
    # Create image (1-bit pixels, black background)
    img = Image.new('1', (width, height), b'\x00'[0])
    draw = ImageDraw.Draw(img)
    
    # Load font
    try:
        font = ImageFont.truetype(font_path, font_size)
    except Exception as e:
        # Fallback to a default font if we can't find the requested one
        print(f"// Error loading font: {e}", file=sys.stderr)
        font = ImageFont.load_default()

    # Calculate text size and center it
    bbox = draw.textbbox((0, 0), text, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    # Draw text in white (1)
    draw.text(((width - w) // 2, (height - h) // 2 - bbox[1]), text, font=font, fill=1)

    # Convert to XBM format
    data = []
    for y in range(height):
        for x_byte in range((width + 7) // 8):
            byte = 0
            for x_bit in range(8):
                pixel_x = x_byte * 8 + x_bit
                if pixel_x < width:
                    if img.getpixel((pixel_x, y)):
                        byte |= (1 << x_bit)
            data.append(byte)
    
    # Output C array to a list
    lines = []
    lines.append(f"// {text} ({width}x{height})")
    lines.append(f"const unsigned char {filename_prefix}[] PROGMEM = {{")
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        hex_line = ", ".join([f"0x{b:02x}" for b in chunk])
        lines.append(f"  {hex_line},")
    lines.append("};")
    lines.append(f"const int {filename_prefix}_width = {width};")
    lines.append(f"const int {filename_prefix}_height = {height};")
    return "\n".join(lines)

# Find a Chinese font on Windows
font_paths = [
    "C:/Windows/Fonts/msyhbd.ttc",
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/simsun.ttc"
]
active_font = None
for p in font_paths:
    if os.path.exists(p):
        active_font = p
        break

if not active_font:
    active_font = "arial.ttf" 

with open("src/user_interface/splash_image.h", "w", encoding="utf-8") as f:
    f.write("#ifndef SPLASH_IMAGE_H\n")
    f.write("#define SPLASH_IMAGE_H\n")
    f.write("#include <Arduino.h>\n\n")
    f.write(text_to_xbm("冯氏芦笋", active_font, 22, 128, 24, "splash_line1"))
    f.write("\n\n")
    f.write(text_to_xbm("组合称", active_font, 22, 128, 24, "splash_line2"))
    f.write("\n#endif\n")
