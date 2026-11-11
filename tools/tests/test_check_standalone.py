"""Tests for tools/check_standalone.py.

Run from the repository root:
    python -I -m unittest discover -s tools/tests -t . -p test_check_standalone.py
or  python -I tools/tests/test_check_standalone.py

The symbol checks read the local reference sources (Expansion, CF, Dabs,
vanilla) at the checker's default paths; no network is used. Input-error
tests use a tiny fake reference tree and inject PermissionError on os.scandir.

Coverage limits: the checker is a heuristic scanner, not a compiler; these
tests pin its negative controls, and a compile without Expansion (V3)
complements it.
"""

import contextlib
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_ROOT = os.path.dirname(TOOLS_DIR)
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

import check_standalone as cs  # noqa: E402

CLEAN_CONFIG = """class CfgPatches
{
	class BR_Scripts
	{
		units[] = {};
		requiredAddons[] =
		{
			"DZ_Data",
			"JM_CF_Scripts"
		};
	};
};
"""

CLEAN_SCRIPT = """class BRWidgetHelper
{
	protected ref array<string> m_Names;

	void BRWidgetHelper()
	{
		m_Names = new array<string>;
	}

	int Count()
	{
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (player && player.GetIdentity())
			Print(player.GetIdentity().GetName());
		return Math.Max(m_Names.Count(), 0);
	}
};
"""

CLEAN_LAYOUT = """FrameWidgetClass BRRoot {
 size 1 1
 {
  ImageWidgetClass Icon {
   image0 "set:dayz_gui image:icon_x"
  }
 }
}
"""


def run_cli(argv):
    out = io.StringIO()
    err = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = cs.main(argv)
    return code, out.getvalue(), err.getvalue()


def run_json(root, *extra):
    code, out, err = run_cli([root, "--json"] + list(extra))
    return code, (json.loads(out) if out.strip() else None), err


def matches(report, category):
    return {f["match"] for f in report["errors"] if f["category"] == category}


def reference_model():
    return cs.build_reference_model(
        [os.path.join(cs.DEFAULT_REF_ROOT, d) for d in cs.EXPANSION_REFS],
        [cs.DEFAULT_VANILLA] + [os.path.join(cs.DEFAULT_REF_ROOT, d) for d in cs.REQUIRED_OTHER_REFS],
        [os.path.join(cs.DEFAULT_REF_ROOT, d) for d in cs.OTHER_REFS
         if d not in cs.REQUIRED_OTHER_REFS and os.path.isdir(os.path.join(cs.DEFAULT_REF_ROOT, d))])


@contextlib.contextmanager
def scandir_denied(target):
    """Make os.scandir raise PermissionError for one directory."""
    real = os.scandir
    target = os.path.normcase(os.path.abspath(target))

    def fake(path="."):
        if os.path.normcase(os.path.abspath(os.fspath(path))) == target:
            raise PermissionError("simulated unreadable directory: %s" % path)
        return real(path)

    with mock.patch.object(os, "scandir", side_effect=fake):
        yield


def make_pbo(entries):
    """Build an uncompressed PBO: Vers header, entry headers, data, checksum."""
    head = b"\0" + struct.pack("<5I", cs.PBO_VERS, 0, 0, 0, 0) + b"prefix\0BattleRoyale\\x\0\0"
    data = b""
    for name, blob in entries:
        head += name.encode("latin-1") + b"\0" + struct.pack("<5I", 0, len(blob), 0, 0, len(blob))
        data += blob
    head += b"\0" + struct.pack("<5I", 0, 0, 0, 0, 0)
    return head + data + b"\0" + b"\x11" * 20


class MiniModTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="br_check_")
        self.root = os.path.join(self.tmp, "repo")
        scripts = os.path.join(self.root, "BattleRoyale", "addons", "scripts")
        gui = os.path.join(self.root, "BattleRoyale", "addons", "gui")
        os.makedirs(os.path.join(scripts, "5_Mission"))
        os.makedirs(gui)
        self.write("BattleRoyale/addons/scripts/config.cpp", CLEAN_CONFIG)
        self.write("BattleRoyale/addons/scripts/5_Mission/BRWidgetHelper.c", CLEAN_SCRIPT)
        self.write("BattleRoyale/addons/gui/root.layout", CLEAN_LAYOUT)
        self.write("BattleRoyale/CHANGELOG.md", "Removed DayZ Expansion dependency.\n")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def write(self, rel, text):
        path = os.path.join(self.root, *rel.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        return path

    def add_script(self, body):
        self.write("BattleRoyale/addons/scripts/5_Mission/Negative.c", body)

    def assert_finding(self, category, name):
        code, report, err = run_json(self.root)
        self.assertEqual(code, 1, err)
        self.assertIn(name, matches(report, category), report["errors"])
        return report

    # -- clean baseline ----------------------------------------------------

    def test_clean_mod_exits_0(self):
        code, report, err = run_json(self.root)
        self.assertEqual(code, 0, report and report["errors"])
        self.assertEqual(report["errors"], [])
        code, out, _ = run_cli([self.root])
        self.assertEqual(code, 0)
        self.assertIn("CLEAN", out)

    def test_vanilla_homonym_not_counted(self):
        # GetWorldSize: Expansion adds it to DayZGame, vanilla declares it on World.
        any_decl = cs.Resolver([reference_model()]).member_any("GetWorldSize")
        self.assertIn("exp", any_decl, "precondition: Expansion declares GetWorldSize")
        self.assertIn("other", any_decl, "precondition: vanilla declares GetWorldSize")
        self.add_script("""class BRWorldProbe
{
	float Size()
	{
		return GetGame().GetWorld().GetWorldSize();
	}
};
""")
        code, report, _ = run_json(self.root)
        self.assertEqual(code, 0, report["errors"])
        self.assertEqual(report["warnings"], [])

    def test_homonym_on_expansion_receiver(self):
        # Same name, but the receiver is DayZGame, where only Expansion declares it.
        self.add_script("""class BRWorldProbe
{
	float Size()
	{
		return GetDayZGame().GetWorldSize();
	}
};
""")
        report = self.assert_finding("symbol", "GetWorldSize")
        self.assertEqual(matches(report, "text"), set())

    def test_homonym_via_auto_local(self):
        self.add_script("""class BRWorldProbe
{
	float Size()
	{
		auto game = GetDayZGame();
		return game.GetWorldSize();
	}
};
""")
        report = self.assert_finding("symbol", "GetWorldSize")
        self.assertEqual([w for w in report["warnings"] if w["category"] == "ambiguous_homonym"], [])

    def test_homonym_via_parenthesized_receiver(self):
        self.add_script("""class BRWorldProbe
{
	float Size()
	{
		return (DayZGame.Cast(GetGame())).GetWorldSize();
	}
};
""")
        report = self.assert_finding("symbol", "GetWorldSize")
        self.assertEqual([w for w in report["warnings"] if w["category"] == "ambiguous_homonym"], [])

    def test_auto_and_parentheses_on_vanilla_receiver_stay_clean(self):
        self.add_script("""class BRWorldProbe
{
	float Size()
	{
		auto world = GetGame().GetWorld();
		float a = world.GetWorldSize();
		return a + (GetGame().GetWorld()).GetWorldSize();
	}
};
""")
        code, report, _ = run_json(self.root, "--strict", "--reviewed", self.reviewed(""))
        self.assertEqual(code, 0, report["errors"])
        self.assertEqual(report["warnings"], [])

    def test_primitive_receiver_resolves(self):
        # ToString is declared by Expansion (EXType) and on vanilla int/float.
        self.add_script("""class BRNumbers
{
	string Text(int n, float f)
	{
		return n.ToString() + f.ToString();
	}
};
""")
        code, report, _ = run_json(self.root, "--strict", "--reviewed", self.reviewed(""))
        self.assertEqual(code, 0, report)
        self.assertEqual(report["warnings"], [])

    UNRESOLVED = """class BRWorldProbe
{
	float Size(BRUnknownType thing)
	{
		return thing.GetWorldSize();
	}
};
"""

    def reviewed(self, body):
        path = os.path.join(self.tmp, "reviewed_%d.txt" % len(os.listdir(self.tmp)))
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("# header\n" + body)
        return path

    def test_homonym_unresolved_receiver_warns(self):
        self.add_script(self.UNRESOLVED)
        code, report, _ = run_json(self.root)
        self.assertEqual(code, 0, report["errors"])
        self.assertIn("GetWorldSize", {w["match"] for w in report["warnings"]
                                       if w["category"] == "ambiguous_homonym"})

    def test_strict_fails_on_unreviewed_homonym(self):
        self.add_script(self.UNRESOLVED)
        code, out, _ = run_cli([self.root, "--strict", "--reviewed", self.reviewed("")])
        self.assertEqual(code, 1)
        self.assertIn("BattleRoyale/addons/scripts/5_Mission/Negative.c:5:GetWorldSize  # <reason>", out)
        self.assertIn("UNREVIEWED HOMONYMS", out)
        code, report, _ = run_json(self.root, "--strict", "--reviewed", self.reviewed(""))
        self.assertEqual(code, 1)
        self.assertEqual(report["errors"], [])
        self.assertEqual(report["unreviewed"], [{"file": "BattleRoyale/addons/scripts/5_Mission/Negative.c",
                                                 "line": 5, "name": "GetWorldSize"}])

    def test_strict_passes_with_reviewed_entry(self):
        self.add_script(self.UNRESOLVED)
        path = self.reviewed("BattleRoyale/addons/scripts/5_Mission/Negative.c:5:GetWorldSize"
                             "  # BRUnknownType is a test stub, not an Expansion class\n")
        code, report, _ = run_json(self.root, "--strict", "--reviewed", path)
        self.assertEqual(code, 0, report)
        self.assertEqual(report["unreviewed"], [])
        # A reviewed entry for another line does not cover this use.
        path = self.reviewed("BattleRoyale/addons/scripts/5_Mission/Negative.c:6:GetWorldSize  # other line\n")
        code, report, _ = run_json(self.root, "--strict", "--reviewed", path)
        self.assertEqual(code, 1)
        self.assertEqual(report["stale_reviewed"], ["BattleRoyale/addons/scripts/5_Mission/Negative.c:6:GetWorldSize"])

    def test_strict_reviewed_file_errors_exit_2(self):
        self.add_script(self.UNRESOLVED)
        no_reason = self.reviewed("BattleRoyale/addons/scripts/5_Mission/Negative.c:5:GetWorldSize\n")
        self.assertEqual(run_cli([self.root, "--strict", "--reviewed", no_reason])[0], 2)
        bad = self.reviewed("Negative.c:GetWorldSize  # no line\n")
        self.assertEqual(run_cli([self.root, "--strict", "--reviewed", bad])[0], 2)
        missing = os.path.join(self.tmp, "missing_reviewed.txt")
        self.assertEqual(run_cli([self.root, "--strict", "--reviewed", missing])[0], 2)
        # Without --strict the reviewed file is not read.
        self.assertEqual(run_cli([self.root, "--reviewed", missing])[0], 0)

    def test_default_reviewed_file_loads(self):
        self.assertTrue(os.path.isfile(cs.DEFAULT_REVIEWED))
        cs.load_reviewed(cs.DEFAULT_REVIEWED)
        code, report, _ = run_json(self.root, "--strict")
        self.assertEqual(code, 0, report)

    def test_local_elsewhere_does_not_shadow(self):
        # A local named like the Expansion method, in another class and file,
        # must not hide the dependent call.
        self.add_script("class BRProbe { string NameOf(PlayerBase p) { return p.GetIdentityName(); } };\n")
        self.write("BattleRoyale/addons/scripts/5_Mission/Helper.c",
                   "class BRHelper { void F() { int GetIdentityName = 0; } };\n")
        self.assert_finding("symbol", "GetIdentityName")

    def test_local_in_other_function_same_file_does_not_shadow(self):
        # Unqualified uses: the local only exists inside F(); N() and the later
        # use in G() (before G's own local) still depend on Expansion.
        self.add_script("""modded class PlayerBase
{
	void F() { int GetIdentityName = 0; }
	string N() { return GetIdentityName(); }
	string G() { string n = GetIdentityName(); int GetIdentityName = 1; return n; }
};
""")
        report = self.assert_finding("symbol", "GetIdentityName")
        uses = [u["line"] for f in report["errors"] if f["match"] == "GetIdentityName" for u in f["uses"]]
        self.assertEqual(uses, [4, 5])

    def test_own_method_elsewhere_does_not_shadow(self):
        self.add_script("""class BRNames
{
	string GetIdentityName() { return "x"; }
	string Self() { return GetIdentityName(); }
};
class BRProbe
{
	string NameOf(PlayerBase p) { return p.GetIdentityName(); }
};
""")
        report = self.assert_finding("symbol", "GetIdentityName")
        uses = [u for f in report["errors"] if f["match"] == "GetIdentityName" for u in f["uses"]]
        self.assertEqual([u["line"] for u in uses], [8])  # only BRProbe.NameOf

    def test_local_in_scope_shadows(self):
        self.add_script("""class BRHelper
{
	int F()
	{
		int ExpansionlessCount = 0;
		int COLOR_EXPANSION_NOTIFICATION_INFO = 1;
		return COLOR_EXPANSION_NOTIFICATION_INFO;
	}
};
""")
        code, report, _ = run_json(self.root)
        self.assertEqual(matches(report, "symbol"), set())
        self.assertEqual(code, 1)  # the names still contain "expansion" as text

    def test_define_with_expansion_resource_is_error(self):
        self.write("BattleRoyale/addons/scripts/config.cpp",
                   '#define BR_ICON "DayZExpansion\\Core\\GUI\\icons\\hud\\persona_64x64.edds"\n' + CLEAN_CONFIG)
        report = self.assert_finding("resource", "DayZExpansion\\")
        self.assertEqual(report["warnings"], [])

    def test_define_with_expansion_text_is_error(self):
        self.add_script('#define BR_EXPANSION_COMPAT\nclass BRX {};\n')
        code, report, _ = run_json(self.root)
        self.assertEqual(code, 1)
        self.assertTrue(matches(report, "text"))
        self.assertEqual(report["warnings"], [])

    def test_allow_list_is_configurable(self):
        # CHANGELOG.md mentions Expansion and the clean baseline still exits 0.
        self.write("BattleRoyale/addons/gui/history.json", '{"note": "was Expansion"}\n')
        code, _, _ = run_json(self.root, "--allow", "*.md", "--allow", "*/history.json")
        self.assertEqual(code, 0)
        code, report, _ = run_json(self.root)
        self.assertEqual(code, 1)
        self.assertIn("BattleRoyale/addons/gui/history.json", {f["file"] for f in report["errors"]})

    # -- negative controls, one per class ----------------------------------

    def test_type_use(self):
        self.add_script("""class BRLoader
{
	ref ExpansionArray<string> m_Paths;
};
""")
        self.assert_finding("symbol", "ExpansionArray")

    def test_method_use(self):
        # No "expansion" text at all: only the symbol check can see it.
        self.add_script("""class BRNames
{
	string NameOf(PlayerBase player)
	{
		return player.GetIdentityName();
	}
};
""")
        report = self.assert_finding("symbol", "GetIdentityName")
        self.assertEqual(matches(report, "text"), set())

    def test_modded_expansion_class(self):
        self.add_script("""modded class ExpansionMapMenu
{
	override void OnShow()
	{
		super.OnShow();
	}
};
""")
        self.assert_finding("symbol", "ExpansionMapMenu")

    def test_constant_use(self):
        self.add_script("""class BRColors
{
	int Info()
	{
		return COLOR_EXPANSION_NOTIFICATION_INFO;
	}
};
""")
        self.assert_finding("symbol", "COLOR_EXPANSION_NOTIFICATION_INFO")

    def test_layout_resource(self):
        self.write("BattleRoyale/addons/gui/bad.layout",
                   CLEAN_LAYOUT.replace("set:dayz_gui", "set:expansion_iconset"))
        self.assert_finding("resource", "set:expansion_iconset")

    def test_required_addon(self):
        self.write("BattleRoyale/addons/scripts/config.cpp",
                   CLEAN_CONFIG.replace('"JM_CF_Scripts"', '"JM_CF_Scripts",\n\t\t\t"DayZExpansion_Core_Scripts"'))
        self.assert_finding("required_addon", "DayZExpansion_Core_Scripts")

    def test_commented_required_addon_ignored_by_addon_rule(self):
        self.write("BattleRoyale/addons/scripts/config.cpp",
                   CLEAN_CONFIG.replace('"JM_CF_Scripts"', '"JM_CF_Scripts"//, "DayZExpansion_Core_Scripts"'))
        code, report, _ = run_json(self.root)
        self.assertEqual(code, 1)  # still reported as text
        self.assertEqual(matches(report, "required_addon"), set())
        self.assertTrue(matches(report, "text"))

    def test_ifdef_is_warning_only(self):
        self.add_script("""#ifdef EXPANSIONMODNAVIGATION
class BRNavOnly
{
	int Zero() { return 0; }
};
#endif
""")
        code, report, _ = run_json(self.root)
        self.assertEqual(code, 0, report["errors"])
        self.assertEqual([w["match"] for w in report["warnings"]], ["EXPANSIONMODNAVIGATION"])

    def test_own_declaration_shadows_expansion_name(self):
        self.add_script("""class BRNames
{
	string GetIdentityName()
	{
		return "x";
	}
};
""")
        code, report, _ = run_json(self.root)
        self.assertEqual(code, 0, report["errors"])

    # -- PBO scan ----------------------------------------------------------

    def test_pbo_scan(self):
        pbo_dir = os.path.join(self.tmp, "pbos")
        os.makedirs(pbo_dir)
        clean = make_pbo([("config.cpp", CLEAN_CONFIG.encode()), ("data\\a.paa", b"\x00binary")])
        with open(os.path.join(pbo_dir, "clean.pbo"), "wb") as fh:
            fh.write(clean)
        code, report, err = run_json(self.root, "--pbo-dir", pbo_dir)
        self.assertEqual(code, 0, (err, report and report["errors"]))
        bad = make_pbo([("config.cpp", CLEAN_CONFIG.replace('"DZ_Data"', '"DayZExpansion_Core_Scripts"').encode()),
                        ("gui\\x.layout", b'image0 "set:expansion_iconset image:a"\n')])
        with open(os.path.join(pbo_dir, "bad.pbo"), "wb") as fh:
            fh.write(bad)
        code, report, _ = run_json(self.root, "--pbo-dir", pbo_dir)
        self.assertEqual(code, 1)
        self.assertIn("DayZExpansion_Core_Scripts", matches(report, "pbo:required_addon"))
        self.assertIn("set:expansion_iconset", matches(report, "pbo:resource"))
        with open(os.path.join(pbo_dir, "broken.pbo"), "wb") as fh:
            fh.write(b"abc")
        code, _, err = run_json(self.root, "--pbo-dir", pbo_dir)
        self.assertEqual(code, 2, err)

    # -- usage / input errors ---------------------------------------------

    def test_input_errors_exit_2(self):
        self.assertEqual(run_cli([os.path.join(self.tmp, "missing")])[0], 2)
        self.assertEqual(run_cli([self.tmp])[0], 2)  # no mod folders
        self.assertEqual(run_cli([self.root, "--ref-root", os.path.join(self.tmp, "nope")])[0], 2)
        self.assertEqual(run_cli([self.root, "--bogus"])[0], 2)
        self.assertEqual(run_cli([])[0], 2)

    def make_fake_refs(self, empty=None):
        """Tiny reference tree; the corpus named `empty` gets no script files."""
        ref = os.path.join(self.tmp, "refs_%s" % (empty or "full"))
        van = os.path.join(ref, "vanilla")
        corpora = {d: os.path.join(ref, d) for d in cs.EXPANSION_REFS + cs.REQUIRED_OTHER_REFS}
        corpora["vanilla"] = van
        for name, d in corpora.items():
            os.makedirs(os.path.join(d, "sub"), exist_ok=True)
            if name == empty:
                with open(os.path.join(d, "sub", "readme.txt"), "w") as fh:
                    fh.write("no scripts here\n")
                continue
            with open(os.path.join(d, "sub", "%s.c" % name), "w") as fh:
                if name == "exp_core_scripts":
                    fh.write("modded class PlayerBase { string GetIdentityName() { return \"\"; } };\n")
                elif name == "vanilla":
                    fh.write("class PlayerBase { };\n")
                else:
                    fh.write("class Ref_%s { };\n" % name)
        return ref, van

    def test_fake_refs_baseline_detects(self):
        self.add_script("class BRProbe { string NameOf(PlayerBase p) { return p.GetIdentityName(); } };\n")
        ref, van = self.make_fake_refs()
        code, report, err = run_json(self.root, "--ref-root", ref, "--vanilla", van)
        self.assertEqual(code, 1, err)
        self.assertIn("GetIdentityName", matches(report, "symbol"))

    def test_empty_reference_corpus_exits_2(self):
        self.add_script("class BRProbe { string NameOf(PlayerBase p) { return p.GetIdentityName(); } };\n")
        for corpus in cs.EXPANSION_REFS + cs.REQUIRED_OTHER_REFS + ("vanilla",):
            ref, van = self.make_fake_refs(empty=corpus)
            code, out, err = run_cli([self.root, "--ref-root", ref, "--vanilla", van])
            self.assertEqual(code, 2, (corpus, out, err))
            self.assertIn("indexes no script files", err)

    def test_unreadable_mod_dir_exits_2(self):
        self.add_script("class BRProbe { string NameOf(PlayerBase p) { return p.GetIdentityName(); } };\n")
        ref, van = self.make_fake_refs()
        with scandir_denied(os.path.join(self.root, "BattleRoyale")):
            code, _, err = run_cli([self.root, "--ref-root", ref, "--vanilla", van])
        self.assertEqual(code, 2, err)
        self.assertIn("simulated unreadable", err)
        with scandir_denied(os.path.join(self.root, "BattleRoyale", "addons", "scripts", "5_Mission")):
            code, _, err = run_cli([self.root, "--ref-root", ref, "--vanilla", van])
        self.assertEqual(code, 2, err)

    def test_unreadable_reference_dir_exits_2(self):
        ref, van = self.make_fake_refs()
        cs._INDEX_CACHE.clear()
        with scandir_denied(os.path.join(ref, "cf_scripts", "sub")):
            code, _, err = run_cli([self.root, "--ref-root", ref, "--vanilla", van])
        self.assertEqual(code, 2, err)
        self.assertIn("simulated unreadable", err)

    def test_unreadable_pbo_dir_exits_2(self):
        pbo_dir = os.path.join(self.tmp, "pbos")
        os.makedirs(os.path.join(pbo_dir, "sub"))
        with open(os.path.join(pbo_dir, "clean.pbo"), "wb") as fh:
            fh.write(make_pbo([("config.cpp", CLEAN_CONFIG.encode())]))
        ref, van = self.make_fake_refs()
        with scandir_denied(os.path.join(pbo_dir, "sub")):
            code, _, err = run_cli([self.root, "--ref-root", ref, "--vanilla", van, "--pbo-dir", pbo_dir])
        self.assertEqual(code, 2, err)


class RealRepoTest(unittest.TestCase):
    EXPECTED = ("ExpansionMapMenu", "ExpansionMapWidgetBase", "ExpansionNotification",
                "GetIdentityName", "ExpansionArray", "ExpansionNotificationView")

    def test_real_worktree(self):
        if not os.path.isdir(os.path.join(REPO_ROOT, "BattleRoyale")):
            self.skipTest("BattleRoyale/ not found next to tools/")
        script = os.path.join(TOOLS_DIR, "check_standalone.py")
        t0 = time.monotonic()
        proc = subprocess.run([sys.executable, "-I", script, REPO_ROOT, "--json"],
                              capture_output=True, text=True, timeout=300)
        elapsed = time.monotonic() - t0
        self.assertEqual(proc.returncode, 1, proc.stderr)
        report = json.loads(proc.stdout)
        found = matches(report, "symbol")
        for name in self.EXPECTED:
            self.assertIn(name, found)
        self.assertLess(elapsed, 60.0)


if __name__ == "__main__":
    unittest.main()
