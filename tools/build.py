"""Pack and sign every addon of BattleRoyale/addons into build/@BattleRoyale, and the VPP
compatibility addon of BattleRoyale_VPP/addons into build/@BattleRoyale-VPP.

Each addon is packed with AddonBuilder in -packonly mode (scripts, layouts, csv and already
binarized assets go in as they are) using the prefix from its $PBOPREFIX$ file, then signed
with DSSignFile. After packing, every PBO is checked entry by entry against its source tree:
a missing source file or an unexpected entry fails the build. Both mods use the same key.

Usage: python tools/build.py [--key <path to .biprivatekey>] [--only <addon> ...]
--only takes addon folder names of either mod (vpp_compat is the compatibility addon).
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ADDONS = os.path.join(ROOT, "BattleRoyale", "addons")
OUT = os.path.join(ROOT, "build", "@BattleRoyale")
VPP_ADDONS = os.path.join(ROOT, "BattleRoyale_VPP", "addons")
VPP_OUT = os.path.join(ROOT, "build", "@BattleRoyale-VPP")
TOOLS = r"C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin"
ADDON_BUILDER = os.path.join(TOOLS, "AddonBuilder", "AddonBuilder.exe")
DSSIGN = os.path.join(TOOLS, "DsUtils", "DSSignFile.exe")
DEFAULT_KEY = r"C:\Users\guill\Repos\br-keys\BattleRoyaleCommunity.biprivatekey"
GENERATED = {"$pboprefix$", "$pboprefix$.txt", "texheaders.bin"}
SKIP_SOURCE = {"$pboprefix$", "removed.md"}


def pbo_entries(path):
    names = []
    with open(path, "rb") as f:
        def rz():
            b = b""
            while True:
                c = f.read(1)
                if c in (b"\0", b""):
                    return b.decode("utf-8", "replace")
                b += c
        while True:
            name = rz()
            mime, _orig, _res, _ts, _size = struct.unpack("<5I", f.read(20))
            if name == "" and mime == 0x56657273:
                while rz():
                    pass
                continue
            if name == "":
                break
            names.append(name.replace("\\", "/").lower())
    return names


def source_files(src):
    files = []
    for base, _, names in os.walk(src):
        for n in names:
            if n.lower() in SKIP_SOURCE:
                continue
            files.append(os.path.relpath(os.path.join(base, n), src).replace("\\", "/").lower())
    return files


def build_one(name, key, temp, addons=ADDONS, out=OUT):
    src = os.path.join(addons, name)
    prefix = open(os.path.join(src, "$PBOPREFIX$"), encoding="ascii").read().strip()
    dest = os.path.join(out, "addons")
    cmd = [ADDON_BUILDER, src, dest, "-packonly", "-clear", f"-prefix={prefix}", f"-temp={temp}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    pbo = os.path.join(dest, name + ".pbo")
    if r.returncode != 0 or not os.path.isfile(pbo):
        print(r.stdout[-2000:], r.stderr[-2000:])
        raise SystemExit(f"FAIL pack {name} rc={r.returncode}")
    entries = set(pbo_entries(pbo))
    expected = set(source_files(src))
    missing = sorted(expected - entries)
    extra = sorted(e for e in entries - expected if e.split("/")[-1] not in GENERATED)
    if missing or extra:
        raise SystemExit(f"FAIL content {name}: missing={missing[:10]} extra={extra[:10]}")
    s = subprocess.run([DSSIGN, key, pbo], capture_output=True, text=True)
    if s.returncode != 0 or not any(f.startswith(name + ".pbo.") and f.endswith(".bisign") for f in os.listdir(dest)):
        raise SystemExit(f"FAIL sign {name} rc={s.returncode} {s.stdout[-500:]} {s.stderr[-500:]}")
    print(f"ok {name}: {len(expected)} files, {os.path.getsize(pbo)} bytes, prefix {prefix}")


def addon_names(addons):
    return sorted(d for d in os.listdir(addons) if os.path.isfile(os.path.join(addons, d, "config.cpp")))


def build_mod(addons, out, names, full, key, temp, mod_cpp):
    if full and os.path.isdir(os.path.join(out, "addons")):
        # A full build starts from an empty output so removed addons do not linger.
        shutil.rmtree(os.path.join(out, "addons"))
    os.makedirs(os.path.join(out, "addons"), exist_ok=True)
    os.makedirs(os.path.join(out, "keys"), exist_ok=True)
    for n in names:
        build_one(n, key, temp, addons, out)
    shutil.copyfile(mod_cpp, os.path.join(out, "mod.cpp"))
    bikey = os.path.splitext(key)[0] + ".bikey"
    shutil.copyfile(bikey, os.path.join(out, "keys", os.path.basename(bikey)))
    for f in ("LICENSE", "NOTICE.md"):
        shutil.copyfile(os.path.join(ROOT, f), os.path.join(out, f))
    print("built", len(names), "addons into", out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--key", default=DEFAULT_KEY)
    ap.add_argument("--only", nargs="*")
    a = ap.parse_args()
    names = addon_names(ADDONS)
    vpp_names = addon_names(VPP_ADDONS)
    if a.only:
        unknown = sorted(set(a.only) - set(names) - set(vpp_names))
        if unknown:
            raise SystemExit(f"FAIL unknown addon {unknown}")
        names = [n for n in names if n in a.only]
        vpp_names = [n for n in vpp_names if n in a.only]
    temp = os.path.join(ROOT, "build", "abtemp")
    if names or not a.only:
        build_mod(ADDONS, OUT, names, not a.only, a.key, temp, os.path.join(ROOT, "BattleRoyale", "mod.cpp"))
    if vpp_names or not a.only:
        build_mod(VPP_ADDONS, VPP_OUT, vpp_names, not a.only, a.key, temp, os.path.join(ROOT, "BattleRoyale_VPP", "mod.cpp"))


if __name__ == "__main__":
    main()
