"""Copy the untracked binary assets (textures, sounds, models) from the main working tree into
another worktree, so a build from that worktree packs complete PBOs. Covers both mods
(BattleRoyale and BattleRoyale_VPP). It copies from the tree it lives in, or from <source_root> when given, and
replaces any asset whose content differs (two versions of a texture can have the same size).
Usage: python tools/sync_assets.py <target_worktree_root> [<source_root>]
"""
import filecmp, os, shutil, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if len(sys.argv) > 2:
    ROOT = os.path.abspath(sys.argv[2])
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
                if not os.path.exists(d) or not filecmp.cmp(d, os.path.join(base, f), shallow=False):
                    shutil.copyfile(os.path.join(base, f), d)
                    n += 1
    print("copied", n, "asset files into", dst_root)
