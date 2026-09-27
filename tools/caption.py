#!/usr/bin/env python3
"""Burn captions onto a recorded take.

A take is recorded by hand — F9 starts and stops it — and the words that explain it are written
afterwards, in a markdown file of the same shape as a demo script: a heading per caption, the
time it appears, and the words underneath.

    # Snap to rows

    ## 0:04.2
    The wheel moves a row at a time.

    ## 0:11.7
    Command with the arrows changes how many rows are shown, one to five.

    ## 0:14 top
    A cable is dragged from the row below to the row above.

    ## 0:18 clear

A caption appears at its own time and stays until the next heading, or to the end of the film.
A heading that says `clear` after the time takes the caption away and leaves the picture alone
until the next one, and one that says `top` puts that caption at the top of the picture instead
of the bottom — for a moment when what is being described is happening where the caption would
otherwise sit.

WHY NOT DRAW THEM WHILE RECORDING. The captions would then have to be written before the take
and performed to, which is the thing that makes scripted demos hard to get right. Written
afterwards, against a film that already exists, each one can be placed where it belongs and
reworded as often as it takes without anything being recorded again.

WHY A PICTURE PER CAPTION rather than a subtitle file or ffmpeg's own text drawing. The ffmpeg
on this machine was built without freetype and without libass, so it has neither drawtext nor
subtitles nor ass — 489 filters and not one of them can write a word. It does have `overlay`,
and Python can draw text, so each caption is drawn to a transparent picture here and laid over
the film for the seconds it belongs to. Nothing has to be installed, and the type and the box
behind it are ours to set rather than a subtitle renderer's.

    tools/caption.py <take.mp4> <captions.md> [out.mp4]
"""

import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont


# A caption is this tall a share of the picture, sits this far up from the bottom, and is kept
# within this much of the width so that a long one breaks into two lines rather than running
# from edge to edge. Chosen against a 1080-high film: about 44 points of type.
FONT_SHARE = 1.0 / 24.0
MARGIN_SHARE = 1.0 / 20.0
WIDTH_SHARE = 0.8
# The box behind the words: how far it stands out from them, and how dark it is.
PAD_SHARE = 0.5
BOX_ALPHA = 205

FONTS = [
    "/System/Library/Fonts/HelveticaNeue.ttc",
    "/System/Library/Fonts/Helvetica.ttc",
    "/System/Library/Fonts/SFNS.ttf",
    "/Library/Fonts/Arial Unicode.ttf",
]


def font_at(size):
    for path in FONTS:
        try:
            return ImageFont.truetype(path, size)
        except Exception:
            continue
    return ImageFont.load_default()


def probe(path, entries):
    out = subprocess.check_output([
        "ffprobe", "-v", "error", "-select_streams", "v:0",
        "-show_entries", entries, "-of", "csv=p=0:s=x", path,
    ], text=True).strip()
    return out.splitlines()[0]


def parse_time(text):
    """`12.5`, `1:02`, `1:02.5` or `0:01:02.5`, in seconds."""
    parts = text.split(":")
    try:
        seconds = float(parts[-1])
    except ValueError:
        return None
    if len(parts) > 1:
        seconds += 60.0 * int(parts[-2])
    if len(parts) > 2:
        seconds += 3600.0 * int(parts[-3])
    return seconds


def read_captions(path):
    """The markdown, as a list of (seconds, words, at_top). `clear` comes back as no words."""
    captions = []
    when = None
    clear = False
    top = False
    words = []

    def flush():
        if when is not None:
            captions.append((when, "" if clear else " ".join(words).strip(), top))

    with open(path) as f:
        for line in f:
            line = line.strip()
            if line.startswith("## "):
                flush()
                bits = line[3:].strip().split()
                when = parse_time(bits[0]) if bits else None
                if when is None:
                    raise SystemExit("caption: no time in \"%s\"" % line)
                rest = [b.lower() for b in bits[1:]]
                clear = "clear" in rest
                top = "top" in rest
                words = []
            elif line.startswith("#"):
                continue
            elif line and when is not None:
                words.append(line)
    flush()

    captions.sort(key=lambda c: c[0])
    for i in range(1, len(captions)):
        if captions[i][0] <= captions[i - 1][0]:
            raise SystemExit("caption: two captions at %gs" % captions[i][0])
    return captions


def wrap(draw, words, font, width):
    """Break the words to fit, at spaces, and as evenly as two lines can be made."""
    def measure(text):
        return draw.textbbox((0, 0), text, font=font)[2]

    if measure(words) <= width:
        return [words]
    parts = words.split()
    lines = []
    line = ""
    for word in parts:
        trial = (line + " " + word).strip()
        if line and measure(trial) > width:
            lines.append(line)
            line = word
        else:
            line = trial
    if line:
        lines.append(line)
    return lines


def draw_caption(words, width, height, at_top, path):
    """One caption, as a picture the size of the film with everything else transparent."""
    size = int(round(height * FONT_SHARE))
    font = font_at(size)
    pad = int(round(size * PAD_SHARE))
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    lines = wrap(draw, words, font, int(width * WIDTH_SHARE) - 2 * pad)
    spacing = int(round(size * 0.28))
    heights = [draw.textbbox((0, 0), line, font=font)[3] for line in lines]
    widths = [draw.textbbox((0, 0), line, font=font)[2] for line in lines]
    text_w = max(widths)
    text_h = sum(heights) + spacing * (len(lines) - 1)

    box_w = text_w + 2 * pad
    box_h = text_h + 2 * pad
    box_x = (width - box_w) // 2
    margin = int(round(height * MARGIN_SHARE))
    box_y = margin if at_top else height - margin - box_h

    draw.rounded_rectangle([box_x, box_y, box_x + box_w, box_y + box_h],
                           radius=pad // 2, fill=(0, 0, 0, BOX_ALPHA))
    y = box_y + pad
    for line, h in zip(lines, heights):
        w = draw.textbbox((0, 0), line, font=font)[2]
        draw.text(((width - w) // 2, y), line, font=font, fill=(255, 255, 255, 255))
        y += h + spacing
    image.save(path)


def duration(path):
    out = subprocess.check_output([
        "ffprobe", "-v", "error", "-show_entries", "format=duration",
        "-of", "csv=p=0", path,
    ], text=True).strip()
    return float(out)


def main(argv):
    if len(argv) < 3:
        raise SystemExit("caption.py <take.mp4> <captions.md> [out.mp4]")
    video, markdown = argv[1], argv[2]
    out = argv[3] if len(argv) > 3 else None
    if out is None:
        base, ext = os.path.splitext(video)
        out = base + " captioned" + (ext or ".mp4")

    captions = read_captions(markdown)
    shown = [c for c in captions if c[1]]
    if not shown:
        raise SystemExit("caption: no captions in " + markdown)

    size = probe(video, "stream=width,height")
    width, height = (int(n) for n in size.split("x")[:2])
    end = duration(video)

    here = os.path.dirname(os.path.abspath(out)) or "."
    stem = os.path.splitext(os.path.basename(out))[0]

    # One picture per caption, and one overlay per picture, each switched on for its own seconds.
    inputs = ["-i", video]
    chain = []
    last = "0:v"
    made = []
    for i, (when, words, at_top) in enumerate(captions):
        if not words:
            continue
        nxt = [c[0] for c in captions if c[0] > when]
        until = nxt[0] if nxt else end
        if until <= when:
            continue
        png = os.path.join(here, "%s caption %d.png" % (stem, i + 1))
        draw_caption(words, width, height, at_top, png)
        made.append(png)
        inputs += ["-i", png]
        label = "v%d" % len(made)
        chain.append("[%s][%d:v]overlay=0:0:enable='between(t,%.3f,%.3f)'[%s]"
                     % (last, len(made), when, until, label))
        last = label

    cmd = ["ffmpeg", "-y", "-v", "error", "-stats"] + inputs + [
        "-filter_complex", ";".join(chain), "-map", "[%s]" % last,
    ]
    # The take may have no sound in it at all; ask for it only when there is some.
    try:
        subprocess.check_output(["ffprobe", "-v", "error", "-select_streams", "a:0",
                                 "-show_entries", "stream=index", "-of", "csv=p=0", video],
                                text=True).strip() and cmd.extend(["-map", "0:a", "-c:a", "copy"])
    except subprocess.CalledProcessError:
        pass
    cmd += ["-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p", out]

    subprocess.check_call(cmd)
    for png in made:
        os.remove(png)
    print("%d captions burned in\n%s" % (len(made), out))


if __name__ == "__main__":
    main(sys.argv)
