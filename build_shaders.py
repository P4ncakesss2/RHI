#!/usr/bin/env python3
"""
Compiles .slang shaders under shaders/ into per-stage DXIL blobs (build/shaders/).
Each entry point is compiled as its own single-stage target (vs_6_0/ps_6_0/...),
since bundling multiple stages produces a DXIL "library" target that this dxc
version's validator doesn't support, and PSOs want separate VS/PS blobs anyway.

Requires slangc on PATH.
Usage: python build_shaders.py
"""

import subprocess
import sys
from pathlib import Path

SLANGC = "slangc"
SHADER_DIR = Path("shaders")
OUTPUT_DIR = Path("build/shaders")

SHADERS = [
    {
        "source": "triangle.slang",
        "entries": [
            ("vertexMain", "vertex", "vs_6_0"),
            ("fragmentMain", "fragment", "ps_6_0"),
        ],
    },
]

def compile_entry(src: Path, entry_name: str, stage: str, profile: str) -> bool:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    out_dxil = OUTPUT_DIR / f"{src.stem}_{entry_name}.dxil"

    cmd = [
        SLANGC,
        str(src),
        "-entry", entry_name,
        "-stage", stage,
        "-profile", profile,
        "-target", "dxil",
        "-o", str(out_dxil),
    ]

    print(f"Compiling {src}::{entry_name} ({stage}) -> {out_dxil}")
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"FAILED: {src}::{entry_name}", file=sys.stderr)
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        return False

    return True

def main():
    if not SHADER_DIR.exists():
        print(f"No shader directory found at {SHADER_DIR}", file=sys.stderr)
        sys.exit(1)

    failures = 0
    for shader in SHADERS:
        src = SHADER_DIR / shader["source"]
        if not src.exists():
            print(f"Missing shader file: {src}", file=sys.stderr)
            failures += 1
            continue

        for entry_name, stage, profile in shader["entries"]:
            if not compile_entry(src, entry_name, stage, profile):
                failures += 1

    if failures:
        print(f"{failures} shader(s) failed to compile", file=sys.stderr)
        sys.exit(1)

    print("All shaders compiled successfully.")

if __name__ == "__main__":
    main()