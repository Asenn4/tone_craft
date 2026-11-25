"""
Pack all .nam and .wav/.aif/.aiff files inside assets/modelpacks/ (recursively)
into a single compressed ModelPacks.zip for zero-bloat Windows PE RCDATA bundling.
Only rewrites ModelPacks.zip when the source file list/mtimes/sizes actually change,
keeping incremental C++ builds instant!
"""

import hashlib
import os
import re
import sys
import zipfile

NUM_PREFIX_RE = re.compile(r"^0*(\d{1,3})(?:[^0-9].*)?$")
ALLOWED_EXTS = {".nam", ".wav", ".aif", ".aiff"}


def collect_entries(src_dir: str):
    entries = []
    if not os.path.isdir(src_dir):
        return entries

    seen = set()
    for root, dirs, files in os.walk(src_dir):
        dirs.sort()
        files.sort()
        for fname in files:
            ext = os.path.splitext(fname)[1].lower()
            if ext not in ALLOWED_EXTS:
                continue
            stem = os.path.splitext(fname)[0]
            m = NUM_PREFIX_RE.match(stem)
            if not m:
                continue
            excel_no = int(m.group(1))
            if excel_no < 1 or excel_no > 265:
                continue
            full_path = os.path.join(root, fname)
            try:
                st = os.stat(full_path)
            except OSError:
                continue
            if st.st_size <= 0:
                continue
            is_ir = ext != ".nam"
            key = (excel_no, is_ir)
            if key in seen:
                continue
            seen.add(key)
            rel_name = os.path.relpath(full_path, src_dir).replace("\\", "/")
            entries.append((excel_no, is_ir, rel_name, full_path, st.st_size, st.st_mtime_ns))

    entries.sort(key=lambda e: (e[0], e[1], e[2]))
    return entries


def main() -> int:
    if len(sys.argv) < 3:
        print("Usage: pack_modelpacks.py <src_modelpacks_dir> <out_zip_path>")
        return 1

    src_dir = os.path.abspath(sys.argv[1])
    out_zip = os.path.abspath(sys.argv[2])
    manifest_path = out_zip + ".manifest"
    os.makedirs(os.path.dirname(out_zip), exist_ok=True)

    entries = collect_entries(src_dir)
    h = hashlib.sha256()
    for excel_no, is_ir, rel_name, _, sz, mtime_ns in entries:
        h.update(f"{excel_no}|{int(is_ir)}|{rel_name}|{sz}|{mtime_ns}\n".encode("utf-8"))
    digest = h.hexdigest()

    if os.path.isfile(out_zip) and os.path.isfile(manifest_path):
        try:
            with open(manifest_path, "r", encoding="utf-8") as mf:
                if mf.read().strip() == digest:
                    return 0
        except OSError:
            pass

    tmp_zip = out_zip + ".tmp"
    with zipfile.ZipFile(tmp_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for _, _, rel_name, full_path, _, _ in entries:
            zf.write(full_path, arcname=rel_name)
    os.replace(tmp_zip, out_zip)

    with open(manifest_path, "w", encoding="utf-8") as mf:
        mf.write(digest + "\n")

    nam_cnt = sum(1 for e in entries if not e[1])
    ir_cnt = sum(1 for e in entries if e[1])
    zip_kb = os.path.getsize(out_zip) / 1024.0
    print(f"[ModelPacks] Bundled {len(entries)} capture(s) ({nam_cnt} NAM, {ir_cnt} IR) -> {os.path.basename(out_zip)} ({zip_kb:.1f} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
