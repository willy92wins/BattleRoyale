"""Copy the untracked binary assets (textures, sounds, models) from the main working tree into
another worktree, so a build from that worktree packs complete PBOs. Covers both mods
(BattleRoyale and BattleRoyale_VPP). Run the main checkout's copy of this script: it copies from
the tree it lives in.
Usage: python tools/sync_assets.py <target_worktree_root>
"""
import os, shutil, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXT = (".edds", ".paa", ".ogg", ".wss", ".p3d")
for mod in ("BattleRoyale", "BattleRoyale_VPP"):
    src = os.path.join(ROOT, mod, "addons")
    dst_root = os.path.join(sys.argv[1], mod, "addons")
    n = 0
    for base, _, files in os.walk(src):
        for f in files:
            if f.lower().endswith(EXT):
                rel = os.path.relpath(os.path.join(base, f), src)
                d = os.path.join(dst_root, rel)
                os.makedirs(os.path.dirname(d), exist_ok=True)
                if not os.path.exists(d) or os.path.getsize(d) != os.path.getsize(os.path.join(base, f)):
                    shutil.copyfile(os.path.join(base, f), d)
                    n += 1
    print("copied", n, "asset files into", dst_root)
