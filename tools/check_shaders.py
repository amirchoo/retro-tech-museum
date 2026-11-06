#!/usr/bin/env python3
"""Validate every shader against GLSL 410 core, the macOS ceiling.

  python3 tools/check_shaders.py            # needs glslangValidator on PATH

- .vert / .frag must start with '#version 410 core'
- '#include "x.glsl"' is expanded exactly like src/core/shader.cpp does
- the expanded source is compiled with glslangValidator
"""
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
SHADERS = ROOT / "shaders"
INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"')


def expand(path, seen, depth=0):
    if depth > 8:
        raise RuntimeError(f"{path}: #include nested too deep")
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        m = INCLUDE.match(line)
        if m:
            name = m.group(1)
            if name not in seen:
                seen.add(name)
                out.append(expand(SHADERS / name, seen, depth + 1))
        else:
            out.append(line)
    return "\n".join(out)


def main():
    failed = 0
    stages = sorted(list(SHADERS.glob("*.vert")) + list(SHADERS.glob("*.frag")))
    with tempfile.TemporaryDirectory() as tmp:
        for src in stages:
            first = next((l.strip() for l in src.read_text(encoding="utf-8").splitlines() if l.strip()), "")
            if first != "#version 410 core":
                print(f"::error file=shaders/{src.name}::first line must be '#version 410 core' (got: {first})")
                failed += 1
                continue
            expanded = pathlib.Path(tmp) / src.name
            expanded.write_text(expand(src, set()) + "\n", encoding="utf-8")
            r = subprocess.run(["glslangValidator", str(expanded)], capture_output=True, text=True)
            status = "ok" if r.returncode == 0 else "FAILED"
            print(f"{status:6} {src.name}")
            if r.returncode != 0:
                print(r.stdout.replace(str(expanded), f"shaders/{src.name} (expanded)"))
                failed += 1
    print(f"{len(stages)} shaders, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
