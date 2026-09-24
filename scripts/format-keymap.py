#!/usr/bin/env python3
"""Align the layer bindings in the keymap into a grid matching the physical layout.

Usage: format-keymap.py [--check] [KEYMAP]
"""

import argparse
import re
import sys
from pathlib import Path

DEFAULT_KEYMAP = Path(__file__).resolve().parent.parent / "config" / "halcyon_kyria.keymap"

# Grid columns occupied by each physical row, in binding order. Columns 0-7 are
# the left half, 8-15 the right; the thumb cluster sits under the inner columns.
LAYOUT = [
    [*range(0, 6), *range(10, 16)],
    [*range(0, 6), *range(10, 16)],
    [*range(0, 16)],
    [*range(3, 13)],
    [*range(0, 5), *range(11, 16)],
]
BLANK_LINE_BEFORE_ROW = {4}
GRID_WIDTH = 16
HALF_SPLIT = 8
CELL_GAP = "  "
HALF_GAP = "    "

BINDINGS_BLOCK = re.compile(r"^(?P<indent>[ \t]*)bindings = <\n(?P<body>.*?)^(?P<close>[ \t]*>;)", re.M | re.S)


def split_bindings(body: str) -> list[str]:
    if "/*" in body or "//" in body:
        raise ValueError("comments inside bindings are not supported")
    bindings: list[list[str]] = []
    depth = 0
    for word in body.split():
        if depth == 0 and word.startswith("&"):
            bindings.append([])
        elif not bindings:
            raise ValueError(f"expected a binding starting with '&', got {word!r}")
        bindings[-1].append(word)
        depth += word.count("(") - word.count(")")
    return [" ".join(words) for words in bindings]


def layout_rows(bindings: list[str]) -> list[dict[int, str]]:
    expected = sum(len(row) for row in LAYOUT)
    if len(bindings) != expected:
        raise ValueError(f"expected {expected} bindings, got {len(bindings)}")
    rows = []
    remaining = iter(bindings)
    for columns in LAYOUT:
        rows.append({col: next(remaining) for col in columns})
    return rows


def render_row(row: dict[int, str], widths: list[int]) -> str:
    line = ""
    for col in range(GRID_WIDTH):
        if col > 0:
            line += HALF_GAP if col == HALF_SPLIT else CELL_GAP
        line += row.get(col, "").ljust(widths[col])
    return line.rstrip()


def format_body(body: str) -> str:
    rows = layout_rows(split_bindings(body))
    widths = [max((len(row.get(col, "")) for row in rows), default=0) for col in range(GRID_WIDTH)]
    lines = []
    for i, row in enumerate(rows):
        if i in BLANK_LINE_BEFORE_ROW:
            lines.append("")
        lines.append(render_row(row, widths))
    return "\n".join(lines) + "\n"


def format_keymap(text: str) -> str:
    def replace(match: re.Match[str]) -> str:
        line = text.count("\n", 0, match.start()) + 1
        try:
            body = format_body(match["body"])
        except ValueError as e:
            raise ValueError(f"line {line}: {e}") from None
        return f"{match['indent']}bindings = <\n{body}{match['close']}"

    return BINDINGS_BLOCK.sub(replace, text)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="exit 1 if the file would change, without writing")
    parser.add_argument("keymap", nargs="?", type=Path, default=DEFAULT_KEYMAP)
    args = parser.parse_args()

    text = args.keymap.read_text()
    try:
        formatted = format_keymap(text)
    except ValueError as e:
        print(f"{args.keymap}: {e}", file=sys.stderr)
        return 2

    if formatted == text:
        return 0
    if args.check:
        print(f"{args.keymap}: not formatted", file=sys.stderr)
        return 1
    args.keymap.write_text(formatted)
    return 0


if __name__ == "__main__":
    sys.exit(main())
