"""Vector source for Potato's coral markup badge."""

import sys
from pathlib import Path

sys.dont_write_bytecode = True
from iconlib import Canvas, generate

ROOT = Path(__file__).resolve().parents[1]


def draw(canvas: Canvas) -> None:
    """The browser's existing coral circle and white HTML brackets."""
    white = (255, 255, 255)
    canvas.circle(50, 50, 48, (255, 99, 79))
    canvas.line(((34, 33), (20, 50), (34, 67)), 6, white)
    canvas.line(((66, 33), (80, 50), (66, 67)), 6, white)
    canvas.line(((56, 27), (44, 73)), 6, white)


if __name__ == "__main__":
    raise SystemExit(generate(draw, ROOT / "src/potato/res/potato.ico"))
