"""Pack and sign the br_loadtest addon (local tests only) into <out>/addons.

Usage: python -I BattleRoyale_dev/tests/solo_match/build_solotest.py <output @folder> [--key <.biprivatekey>]
"""
import argparse
import os
import shutil
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
TOOLS = r"C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin"
ADDON_BUILDER = os.path.join(TOOLS, "AddonBuilder", "AddonBuilder.exe")
DSSIGN = os.path.join(TOOLS, "DsUtils", "DSSignFile.exe")
DEFAULT_KEY = r"C:\Users\guill\Repos\br-keys\BattleRoyaleCommunity.biprivatekey"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--key", default=DEFAULT_KEY)
    a = ap.parse_args()
    # DSSignFile opens an error dialog and waits on a path with forward slashes
    out = os.path.abspath(a.out)
    src = os.path.join(HERE, "br_loadtest")
    prefix = open(os.path.join(src, "$PBOPREFIX$"), encoding="ascii").read().strip()
    dest = os.path.join(out, "addons")
    os.makedirs(dest, exist_ok=True)
    os.makedirs(os.path.join(out, "keys"), exist_ok=True)
    temp = os.path.join(ROOT, "build", "abtemp")
    r = subprocess.run([ADDON_BUILDER, src, dest, "-packonly", "-clear", f"-prefix={prefix}", f"-temp={temp}"],
                       capture_output=True, text=True, timeout=120)
    pbo = os.path.join(dest, "br_loadtest.pbo")
    if r.returncode != 0 or not os.path.isfile(pbo):
        print(r.stdout[-2000:], r.stderr[-2000:])
        raise SystemExit(f"FAIL pack rc={r.returncode}")
    s = subprocess.run([DSSIGN, a.key, pbo], capture_output=True, text=True, timeout=120)
    if s.returncode != 0:
        raise SystemExit(f"FAIL sign rc={s.returncode} {s.stdout[-500:]} {s.stderr[-500:]}")
    bikey = os.path.splitext(a.key)[0] + ".bikey"
    shutil.copyfile(bikey, os.path.join(out, "keys", os.path.basename(bikey)))
    print(f"ok br_loadtest: {os.path.getsize(pbo)} bytes, prefix {prefix}, in {dest}")


if __name__ == "__main__":
    main()
