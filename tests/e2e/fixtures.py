# Writes raw PPM/PGM fixtures with known pixel content (run.sh converts them with the vips CLI)
# plus the byte-level fixtures vips cannot make (EXIF orientation patch, junk files).
# usage: python3 fixtures.py OUTDIR
import os, random, struct, sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
R, G, B, W, K = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255), (0, 0, 0)


def ppm(name, w, h, fn):
    with open(os.path.join(out, name), "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (w, h))
        f.write(bytes(c for y in range(h) for x in range(w) for c in fn(x, y)))


def pgm(name, w, h, fn):
    with open(os.path.join(out, name), "wb") as f:
        f.write(b"P5\n%d %d\n255\n" % (w, h))
        f.write(bytes(fn(x, y) for y in range(h) for x in range(w)))


ppm("tb.ppm", 200, 200, lambda x, y: R if y < 100 else B)                 # top red / bottom blue
ppm("tall.ppm", 200, 400, lambda x, y: R if y < 200 else B)              # portrait, no EXIF
ppm("lr.ppm", 200, 200, lambda x, y: R if x < 100 else B)                 # left red / right blue
ppm("quad.ppm", 400, 200, lambda x, y: (R if x < 200 else G) if y < 100 else (B if x < 200 else W))
ppm("gray.ppm", 100, 100, lambda x, y: (100, 100, 100))
ppm("gray128.ppm", 100, 100, lambda x, y: (128, 128, 128))
ppm("white.ppm", 200, 200, lambda x, y: W)
ppm("orient_src.ppm", 400, 200, lambda x, y: R if x < 200 else B)        # stored landscape, EXIF 6 makes it portrait
ppm("rgba_rgb.ppm", 100, 100, lambda x, y: (200, 50, 50))
pgm("rgba_a.pgm", 100, 100, lambda x, y: 128)
ppm("transp_rgb.ppm", 100, 100, lambda x, y: K if x < 50 else G)
pgm("transp_a.pgm", 100, 100, lambda x, y: 0 if x < 50 else 255)        # left half fully transparent
ppm("wm_rgb.ppm", 50, 50, lambda x, y: R)
pgm("wm_a.pgm", 50, 50, lambda x, y: 255)
pgm("wmhalf_a.pgm", 50, 50, lambda x, y: 0 if x < 25 else 255)          # left half of watermark transparent
ppm("prefer_green.ppm", 40, 40, lambda x, y: G)
ppm("prefer_red.ppm", 40, 40, lambda x, y: R)

# Animated strips (frames stacked vertically; run.sh saves with --page-height)
FR = [R, G, B]
ppm("anim.ppm", 100, 300, lambda x, y: FR[y // 100] if x < 50 else W)     # frame k: left colour k, right white
# every frame: top red / bottom blue; x>=90 strip is the frame colour so cgif keeps the frames distinct
ppm("anim_tb.ppm", 100, 300, lambda x, y: FR[y // 100] if x >= 90 else (R if y % 100 < 50 else B))
ppm("anim_wide.ppm", 100, 150, lambda x, y: FR[y // 50] if x < 50 else W)  # 100x50 frames

# Busy picture so JPEG/WebP quality changes file size noticeably
rnd = random.Random(1)
noise = [rnd.randrange(256) for _ in range(800 * 600)]
ppm("photo.ppm", 800, 600, lambda x, y: (noise[y * 800 + x], (x * 255) // 800, (y * 255) // 600))


def exif_orient(src, dst, orientation):
    """Insert a minimal APP1/EXIF segment carrying only the Orientation tag right after SOI."""
    data = open(src, "rb").read()
    assert data[:2] == b"\xff\xd8"
    tiff = b"II*\x00" + struct.pack("<I", 8) + struct.pack("<H", 1) + \
        struct.pack("<HHIHH", 0x0112, 3, 1, orientation, 0) + struct.pack("<I", 0)
    payload = b"Exif\x00\x00" + tiff
    app1 = b"\xff\xe1" + struct.pack(">H", len(payload) + 2) + payload
    open(dst, "wb").write(data[:2] + app1 + data[2:])


if len(sys.argv) > 2 and sys.argv[2] == "exif":
    exif_orient(os.path.join(out, "orient_plain.jpg"), os.path.join(out, "orient.jpg"), 6)
    sys.exit(0)

open(os.path.join(out, "notimage.png"), "wb").write(b"hello, this is plain text and not an image at all\n")
open(os.path.join(out, "tiny.png"), "wb").write(b"\x89PNG\r\n\x1a\n\x00")   # 9 bytes, valid PNG magic
open(os.path.join(out, "empty.png"), "wb").close()
open(os.path.join(out, "corrupt.jpg"), "wb").write(b"\xff\xd8\xff\xe0" + bytes(rnd.randrange(256) for _ in range(4000)))
with open(os.path.join(out, "huge.jpg"), "wb") as f:                       # 50 MiB + 1 byte, JPEG magic
    f.write(b"\xff\xd8\xff\xe0")
    f.truncate(50 * 1024 * 1024 + 1)
