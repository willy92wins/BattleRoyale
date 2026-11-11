"""Tests for parse_timings.py: synthetic logs plus the real fixture copies.

Everything runs in memory except one CLI --compare test, which needs a temporary
folder and is skipped (with the reason) when the system has none.
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import parse_timings as pt  # noqa: E402

FIXTURES = os.environ.get("BR_TIMING_FIXTURES", r"C:\Users\guill\Repos\br-runs\v13-f0\fixtures_logs")
SCRIPT = os.path.join(HERE, "parse_timings.py")

HEADER = (
    "---------------------------------------------\n"
    "Log P:\\profiles\\script_x.log started at 2026-11-11 23:59:50 (2026-11-11 22:59:50 UTC)\n"
    "\n"
)


def mod(ts, message, level=4):
    return "%s SCRIPT       : %s [DayZ-BattleRoyale][%d] %s\n" % (ts, ts[:8], level, message)


def module(ts, name):
    return "%s SCRIPT       : Module: %s; loaded 1x files; 1x classes; defines: \"X\"\n" % (ts, name)


def cf(ts, text):
    return "%s SCRIPT       : [DEBUG]\t[CF] %s\n" % (ts, text)


def lines(text):
    return text.splitlines()


def marks_by_key(section):
    result = {}
    for row in section["marks"]:
        result.setdefault(row["key"], []).append(row)
    return result


def stage(conn, name):
    for entry in conn["stages"]:
        if entry["stage"] == name:
            return entry
    raise KeyError(name)


def server_result(text):
    return {"schema": pt.SCHEMA, "sources": {"server": "memory"}, "server": pt.parse_server(lines(text))}


def compare_row(text, key):
    for row in text.splitlines():
        if row.startswith(key + " "):
            return row
    raise KeyError(key)


SERVER_FULL = (
    HEADER
    + module("23:59:51.000", "GameLib")
    + module("23:59:52.000", "Game")
    + module("23:59:55.500", "World")
    + module("00:00:20.000", "Mission")
    + mod("00:00:20.400", "Vigrid-BattleRoyale OnInit()")
    + mod("00:00:20.450", "BattleRoyaleState::Activate: Debug Zone State", 3)
    + mod("00:01:00.000", "ClientPrepareEventTypeID")
    + cf("00:01:00.000", "MissionServer::OnClientPrepareEvent PlayerIdentity<0x00A> - ID 'a'")
    + mod("00:01:00.001", "ClientPrepareEventTypeID")
    + cf("00:01:00.001", "MissionServer::OnClientPrepareEvent PlayerIdentity<0x00B> - ID 'b'")
    # B finishes loading first: identity pairing must not fall back to arrival order.
    + mod("00:01:30.000", "ClientNewEventTypeID")
    + cf("00:01:30.000", "MissionServer::OnClientNewEvent PlayerIdentity<0x00B> - ID 'b'")
    + mod("00:01:30.010", "InvokeOnConnect()")
    + mod("00:01:30.200", "Player Bob connected!")
    + mod("00:01:30.201", "BattleRoyaleState::AddPlayer: Bob", 2)
    + mod("00:01:35.000", "ClientNewEventTypeID")
    + cf("00:01:35.000", "MissionServer::OnClientNewEvent PlayerIdentity<0x00A> - ID 'a'")
    + mod("00:01:35.020", "InvokeOnConnect()")
    + mod("00:01:35.300", "Player Ann connected!")
    + mod("00:01:35.300", "BattleRoyaleState::AddPlayer: Ann", 2)
    + mod("00:01:36.000", "BattleRoyaleState::Deactivate: Debug Zone State", 3)
    + mod("00:01:36.000", "[State Machine] Leaving State `Debug Zone State`")
    + mod("00:01:36.000", "BattleRoyaleState::RemoveAllPlayers: removed 2 players", 3)
    + mod("00:01:36.000", "BattleRoyaleState::AddPlayer: Bob", 2)
    + mod("00:01:36.000", "BattleRoyaleState::AddPlayer: Ann", 2)
    + mod("00:10:00.000", "[Restart State] Restarting!")
)

# Review scenario (REVIEW.md, defect 1): ClientPrepare of 0xA, then a ClientNew of 0xB.
IDENTITY_MISMATCH = (
    "Log x started at 2026-11-11 10:00:00\n"
    + mod("10:00:01.000", "ClientPrepareEventTypeID")
    + cf("10:00:01.000", "MissionServer::OnClientPrepareEvent PlayerIdentity<0xA>")
    + mod("10:00:11.000", "ClientNewEventTypeID")
    + cf("10:00:11.000", "MissionServer::OnClientNewEvent PlayerIdentity<0xB>")
    + mod("10:00:11.100", "InvokeOnConnect()")
    + mod("10:00:11.200", "Player Bob connected!")
    + mod("10:00:11.300", "BattleRoyaleState::AddPlayer: Bob", 2)
)

# Review scenario (REVIEW.md, defect 3): same times, B only loses InvokeOnConnect.
SHORT_JOIN = (
    "Log x started at 2026-11-11 10:00:00\n"
    + mod("10:00:01.000", "ClientPrepareEventTypeID")
    + mod("10:00:11.000", "ClientNewEventTypeID")
    + mod("10:00:11.100", "InvokeOnConnect()")
    + mod("10:00:11.200", "Player Bob connected!")
    + mod("10:00:11.300", "BattleRoyaleState::AddPlayer: Bob", 2)
)


class ServerSyntheticTest(unittest.TestCase):
    def setUp(self):
        self.result = pt.parse_server(lines(SERVER_FULL))

    def test_boot_marks_and_day_change(self):
        by_key = marks_by_key(self.result)
        self.assertEqual(self.result["start"], "2026-11-11T23:59:50.000")
        self.assertEqual(by_key["Module: GameLib"][0]["from_start_s"], 1.0)
        self.assertEqual(by_key["Module: GameLib"][0]["from"], "start")
        # Mission loads after midnight: the date moves on and durations stay positive.
        mission = by_key["Module: Mission"][0]
        self.assertEqual(mission["t"], "2026-11-12T00:00:20.000")
        self.assertEqual(mission["delta_s"], 24.5)
        self.assertEqual(mission["from"], "Module: World#1")
        self.assertEqual(mission["from_start_s"], 30.0)
        self.assertEqual(by_key["OnInit"][0]["delta_s"], 0.4)
        self.assertEqual(by_key["Debug Zone State"][0]["delta_s"], 0.05)
        self.assertEqual(by_key["Restarting"][0]["from_start_s"], 610.0)
        self.assertEqual(self.result["missing"], [])

    def test_repeated_connections_paired_by_identity(self):
        conns = self.result["connections"]
        self.assertEqual(len(conns), 2)
        ann, bob = conns
        self.assertEqual((ann["name"], ann["identity"], ann["pairing"]), ("Ann", "0x00A", "identity"))
        self.assertEqual((bob["name"], bob["identity"], bob["pairing"]), ("Bob", "0x00B", "identity"))
        self.assertEqual((ann["id"], bob["id"]), ("conn:Ann#1", "conn:Bob#1"))
        self.assertEqual(stage(ann, "ClientNew")["delta_s"], 35.0)
        self.assertEqual(stage(bob, "ClientNew")["delta_s"], 29.999)
        self.assertEqual(stage(bob, "InvokeOnConnect")["delta_s"], 0.01)
        self.assertEqual(stage(bob, "Connected")["delta_s"], 0.19)
        self.assertEqual(stage(bob, "AddPlayer")["t"], "2026-11-12T00:01:30.201")
        self.assertEqual(stage(ann, "AddPlayer")["delta_s"], 0.0)
        self.assertEqual(self.result["conflicts"], [])
        # The two AddPlayer of the state transfer are counted apart, not as joins.
        self.assertEqual([a["name"] for a in self.result["add_player_outside_join"]], ["Bob", "Ann"])

    def test_render_and_flatten(self):
        result = {"schema": pt.SCHEMA, "sources": {"server": "s.log"}, "server": self.result}
        text = pt.render(result)
        self.assertIn("connection 1 [conn:Ann#1]: Ann", text)
        self.assertIn("missing: none", text)
        self.assertIn("AddPlayer outside a join (state transfers): 2", text)
        flat = pt.flatten(result)
        self.assertEqual(flat["server/conn:Bob#1/ClientNew"],
                         {"delta": 29.999, "from": "server/conn:Bob#1/ClientPrepare", "from_start": 100.0})
        self.assertEqual(flat["server/OnInit#1"],
                         {"delta": 0.4, "from": "server/Module: Mission#1", "from_start": 30.4})


class IdentityConflictTest(unittest.TestCase):
    def test_contradicting_identity_is_not_paired(self):
        result = pt.parse_server(lines(IDENTITY_MISMATCH))
        self.assertEqual(len(result["connections"]), 2)
        prepared, joined = result["connections"]
        # The 0xA preparation stays incomplete.
        self.assertEqual(prepared["identity"], "0xA")
        self.assertFalse(stage(prepared, "ClientPrepare").get("missing"))
        for name in ("ClientNew", "InvokeOnConnect", "Connected", "AddPlayer"):
            self.assertTrue(stage(prepared, name).get("missing"), name)
        # The 0xB join keeps its own stages but no ClientPrepare and no 10 s interval.
        self.assertEqual((joined["name"], joined["identity"], joined["pairing"]), ("Bob", "0xB", "none"))
        self.assertTrue(stage(joined, "ClientPrepare").get("missing"))
        self.assertIsNone(stage(joined, "ClientNew")["delta_s"])
        self.assertEqual(stage(joined, "AddPlayer")["delta_s"], 0.1)
        self.assertEqual(result["conflicts"], [{
            "type": "identity_conflict", "line": 4,
            "client_new_identity": "0xB", "pending_prepare_identities": ["0xA"],
        }])
        text = pt.render({"sources": {"server": "s"}, "server": result})
        self.assertIn("CONFLICT line 4: ClientNew identity 0xB matches no pending ClientPrepare (0xA)", text)

    def test_unknown_prepare_identity_still_pairs_by_order(self):
        text = IDENTITY_MISMATCH.replace(cf("10:00:01.000", "MissionServer::OnClientPrepareEvent PlayerIdentity<0xA>"), "")
        result = pt.parse_server(lines(text))
        self.assertEqual(len(result["connections"]), 1)
        conn = result["connections"][0]
        self.assertEqual((conn["identity"], conn["pairing"]), ("0xB", "order"))
        self.assertEqual(stage(conn, "ClientNew")["delta_s"], 10.0)
        self.assertEqual(result["conflicts"], [])

    def test_negative_interval_is_reported(self):
        text = SHORT_JOIN.replace("10:00:11.100", "10:00:10.900")
        result = pt.parse_server(lines(text))
        self.assertEqual([(c["type"], c["stage"], c["delta_s"]) for c in result["conflicts"]],
                         [("negative_interval", "InvokeOnConnect", -0.1)])


class AddPlayerOriginTest(unittest.TestCase):
    def test_missing_join_addplayer_is_not_replaced_by_transfer(self):
        text = (
            "Log x started at 2026-11-11 10:00:00\n"
            + mod("10:00:01.000", "ClientPrepareEventTypeID")
            + mod("10:00:11.000", "ClientNewEventTypeID")
            + mod("10:00:11.100", "InvokeOnConnect()")
            + mod("10:00:11.200", "Player Bob connected!")
            + mod("10:05:00.000", "BattleRoyaleState::Deactivate: Debug Zone State", 3)
            + mod("10:05:00.000", "[State Machine] Leaving State `Debug Zone State`")
            + mod("10:05:00.000", "BattleRoyaleState::RemoveAllPlayers: removed 1 players", 3)
            + mod("10:05:00.000", "BattleRoyaleState::AddPlayer: Bob", 2)
        )
        result = pt.parse_server(lines(text))
        conn = result["connections"][0]
        self.assertTrue(stage(conn, "AddPlayer").get("missing"))
        self.assertEqual(result["add_player_outside_join"], [{"name": "Bob", "line": 9}])

    def test_next_connection_closes_the_join(self):
        text = (
            SHORT_JOIN.replace(mod("10:00:11.300", "BattleRoyaleState::AddPlayer: Bob", 2), "")
            + mod("10:00:20.000", "ClientPrepareEventTypeID")
            + mod("10:00:21.000", "BattleRoyaleState::AddPlayer: Bob", 2)
        )
        result = pt.parse_server(lines(text))
        self.assertTrue(stage(result["connections"][0], "AddPlayer").get("missing"))
        self.assertEqual(len(result["add_player_outside_join"]), 1)

    def test_end_of_clientnew_handler_closes_the_join(self):
        # Delta review: join AddPlayer gone, handler end logged, a later AddPlayer of the
        # same player with none of the transfer markers.
        text = (
            SHORT_JOIN.replace(mod("10:00:11.300", "BattleRoyaleState::AddPlayer: Bob", 2), "")
            + "10:00:11.301 SCRIPT       : [EXTRACE] -CustomMission<0x00000213C0F8C960>::OnEvent "
              "ClientNewEventTypeID 301.000 ms\n"
            + mod("10:05:00.000", "BattleRoyaleState::AddPlayer: Bob", 2)
        )
        result = pt.parse_server(lines(text))
        conn = result["connections"][0]
        self.assertTrue(stage(conn, "AddPlayer").get("missing"))
        self.assertEqual(result["add_player_outside_join"], [{"name": "Bob", "line": 7}])

    def test_handler_start_does_not_close_the_join(self):
        text = SHORT_JOIN.replace(
            mod("10:00:11.300", "BattleRoyaleState::AddPlayer: Bob", 2),
            "10:00:11.250 SCRIPT       : [EXTRACE] +CustomMission<0x1>::OnEvent ClientNewEventTypeID\n"
            + mod("10:00:11.300", "BattleRoyaleState::AddPlayer: Bob", 2))
        result = pt.parse_server(lines(text))
        self.assertEqual(stage(result["connections"][0], "AddPlayer")["delta_s"], 0.1)

    def test_addplayer_with_another_name_is_not_the_join(self):
        text = SHORT_JOIN.replace("AddPlayer: Bob", "AddPlayer: Eve")
        result = pt.parse_server(lines(text))
        self.assertTrue(stage(result["connections"][0], "AddPlayer").get("missing"))

    def test_lone_addplayer_creates_no_connection(self):
        result = pt.parse_server(lines("Log x started at 2026-11-11 10:00:00\n"
                                       + mod("10:00:01.000", "BattleRoyaleState::AddPlayer: Bob", 2)))
        self.assertEqual(result["connections"], [])
        self.assertEqual(result["add_player_outside_join"], [{"name": "Bob", "line": 2}])


class ServerMissingTest(unittest.TestCase):
    def test_missing_marks_and_order_fallback(self):
        text = (
            HEADER
            + module("23:59:51.000", "GameLib")
            + mod("23:59:59.000", "ClientPrepareEventTypeID")
            + mod("23:59:59.500", "ClientPrepareEventTypeID")
            + mod("00:00:05.000", "ClientNewEventTypeID")
            + mod("00:00:05.100", "Player Cid connected!")
        )
        result = pt.parse_server(lines(text))
        self.assertEqual(result["missing"],
                         ["Module: Game", "Module: World", "Module: Mission", "OnInit", "Debug Zone State", "Restarting"])
        first, second = result["connections"]
        self.assertEqual(first["pairing"], "order")
        self.assertEqual(stage(first, "ClientNew")["delta_s"], 6.0)
        self.assertTrue(stage(first, "InvokeOnConnect")["missing"])
        self.assertEqual(stage(first, "Connected")["delta_s"], 0.1)
        self.assertEqual(stage(first, "Connected")["from"], "ClientNew")
        self.assertTrue(stage(first, "AddPlayer")["missing"])
        self.assertIsNone(second["pairing"])
        for name in ("ClientNew", "InvokeOnConnect", "Connected", "AddPlayer"):
            self.assertTrue(stage(second, name)["missing"])
        self.assertIn("ClientNew", pt.render({"sources": {"server": "s"}, "server": result}))

    def test_empty_log(self):
        result = pt.parse_server([])
        self.assertIsNone(result["start"])
        self.assertEqual(result["marks"], [])
        self.assertEqual(result["connections"], [])
        self.assertEqual(len(result["missing"]), len(pt.SERVER_EXPECTED))

    def test_no_header_uses_first_line(self):
        result = pt.parse_server(lines(module("10:00:00.250", "GameLib") + module("10:00:01.250", "Game")))
        self.assertEqual(result["start_source"], "first line")
        self.assertEqual(result["marks"][1]["from_start_s"], 1.0)

    def test_small_step_back_is_not_a_day_change(self):
        text = HEADER + module("23:59:51.500", "GameLib") + module("23:59:51.499", "Game")
        result = pt.parse_server(lines(text))
        self.assertTrue(all(row["t"].startswith("2026-11-11") for row in result["marks"]))


class ClientSyntheticTest(unittest.TestCase):
    def test_all_marks_and_repeats(self):
        text = (
            HEADER
            + module("23:59:51.000", "GameLib")
            + module("23:59:52.000", "Game")
            + module("23:59:56.000", "World")
            + mod("00:00:20.000", "DayZGame::OnEvent MPSessionStartEventTypeID")
            + mod("00:00:23.000", "DayZGame::OnEvent LoginTimeEventTypeID")
            + module("00:00:40.000", "Mission")
            + mod("00:00:40.300", "MissionGameplay::OnInit")
            + mod("00:00:40.350", "HUD Initialized")
            + mod("00:00:40.400", "DayZGame::OnEvent PreloadEventTypeID")
            + mod("00:00:45.000", "DayZGame::OnEvent MPSessionPlayerReadyEventTypeID")
            + mod("00:02:00.000", "DayZGame::OnEvent MPSessionPlayerReadyEventTypeID")
        )
        result = pt.parse_client(lines(text))
        by_key = marks_by_key(result)
        self.assertEqual(result["missing"], [])
        self.assertEqual(by_key["MPSessionStart"][0]["t"], "2026-11-12T00:00:20.000")
        self.assertEqual(by_key["LoginTime"][0]["delta_s"], 3.0)
        ready = by_key["MPSessionPlayerReady"]
        self.assertEqual([r["occurrence"] for r in ready], [1, 2])
        self.assertEqual(ready[1]["delta_s"], 75.0)
        self.assertIn("MPSessionPlayerReady #2", pt.render({"sources": {"client": "c"}, "client": result}))

    def test_missing_client_marks(self):
        result = pt.parse_client(lines(HEADER + module("23:59:51.000", "GameLib")))
        self.assertIn("MPSessionStart", result["missing"])
        self.assertIn("HUD Initialized", result["missing"])
        self.assertNotIn("Module: GameLib", result["missing"])


LOOP_LOG = (
    "2026-11-11 23:59:00 Server: P:\\DayZServer_x64.exe\n"
    "2026-11-11 23:59:00 Moved world storage to P:\\mpmissions\\m\\storage_1.prev-20261111-235900\n"
    "2026-11-11 23:59:00 Start #1: P:\\DayZServer_x64.exe -config=serverDZ.cfg\n"
    "2026-11-12 00:20:00 Server exited with code 0\n"
    "2026-11-12 00:20:00 Next start in 5 s, press Ctrl+C to stop the loop\n"
    "2026-11-12 00:20:05 Moved world storage to P:\\mpmissions\\m\\storage_1.prev-20261112-002005\n"
    "2026-11-12 00:20:06 Deleted old world storage P:\\mpmissions\\m\\storage_1.prev-20261101-000000\n"
    "2026-11-12 00:20:06 Start #2: P:\\DayZServer_x64.exe -config=serverDZ.cfg\n"
)


class LoopTest(unittest.TestCase):
    def test_loop_marks_and_downtime(self):
        result = pt.parse_loop(lines(LOOP_LOG))
        by_key = marks_by_key(result)
        self.assertEqual([m["detail"] for m in by_key["Start"]], ["1", "2"])
        self.assertEqual(by_key["Server exited"][0]["detail"], "0")
        self.assertEqual(len(by_key["Storage moved"]), 2)
        self.assertEqual(by_key["Storage deleted"][0]["delta_s"], 1.0)
        self.assertEqual(result["cycles"], [{"exit_line": 4, "start_run": "2", "downtime_s": 6.0}])
        self.assertEqual(result["missing"], [])

    def test_utf16_bytes(self):
        raw = LOOP_LOG.encode("utf-16")  # BOM + UTF-16LE, as PowerShell 5.1 '>' writes it
        self.assertTrue(raw.startswith(b"\xff\xfe"))
        result = pt.parse_loop(pt.decode_bytes(raw))
        self.assertEqual(len(result["marks"]), 6)

    def test_utf8_bom_bytes(self):
        result = pt.parse_loop(pt.decode_bytes(b"\xef\xbb\xbf" + LOOP_LOG.encode("utf-8")))
        self.assertEqual(result["marks"][0]["key"], "Storage moved")


class CompareTest(unittest.TestCase):
    def test_same_interval_is_subtracted(self):
        a = server_result(SERVER_FULL)
        b = server_result(SERVER_FULL.replace("00:00:20.000 SCRIPT       : Module: Mission",
                                              "00:00:10.000 SCRIPT       : Module: Mission"))
        text = pt.compare_results(a, b)
        self.assertEqual(compare_row(text, "server/Module: Mission#1").split()[2:5], ["24.500", "14.500", "-10.000"])
        self.assertEqual(compare_row(text, "server/OnInit#1").split()[1:4], ["0.400", "10.400", "10.000"])
        self.assertNotIn("n/c", text)

    def test_different_interval_is_not_comparable(self):
        a = server_result(SHORT_JOIN)
        b = server_result(SHORT_JOIN.replace(mod("10:00:11.100", "InvokeOnConnect()"), ""))
        text = pt.compare_results(a, b)
        connected = compare_row(text, "server/conn:Bob#1/Connected")
        # A measures InvokeOnConnect->Connected (0.1), B ClientNew->Connected (0.2): no subtraction.
        self.assertEqual(connected.split()[1:4], ["0.100", "0.200", "n/c"])
        self.assertIn("delta not comparable: A from server/conn:Bob#1/InvokeOnConnect, "
                      "B from server/conn:Bob#1/ClientNew", connected)
        self.assertNotIn("0.100      0.200      0.100", text)
        # from_start still compares (same anchor on both sides).
        self.assertEqual(connected.split()[4:7], ["11.200", "11.200", "0.000"])
        self.assertIn("only in A", compare_row(text, "server/conn:Bob#1/InvokeOnConnect"))
        self.assertEqual(compare_row(text, "server/conn:Bob#1/AddPlayer").split()[1:4], ["0.100", "0.100", "0.000"])

    def test_old_json_without_from_is_not_comparable(self):
        a = server_result(SHORT_JOIN)
        b = json.loads(json.dumps(a))
        for conn in b["server"]["connections"]:
            for entry in conn["stages"]:
                entry.pop("from", None)
        text = pt.compare_results(a, b)
        self.assertIn("n/c", compare_row(text, "server/conn:Bob#1/ClientNew"))

    def test_only_in_one_side(self):
        full = server_result(SERVER_FULL)
        short = server_result(HEADER + module("23:59:51.000", "GameLib"))
        text = pt.compare_results(full, short)
        self.assertIn("only in A", text)
        self.assertNotIn("only", compare_row(text, "server/Module: GameLib#1"))


@unittest.skipUnless(os.path.isdir(FIXTURES), "fixture logs not found: " + FIXTURES)
class RealFixturesTest(unittest.TestCase):
    REMOVED = "17:27:25.342  SCRIPT       : 17:27:25 [DayZ-BattleRoyale][2] BattleRoyaleState::AddPlayer: Dev"

    def server_logs(self):
        folder = os.path.join(FIXTURES, "server")
        return sorted(os.path.join(folder, n) for n in os.listdir(folder) if n.startswith("script_"))

    def test_server_fixtures(self):
        logs = self.server_logs()
        self.assertEqual(len(logs), 2)
        for path in logs:
            with self.subTest(path=path):
                result = pt.parse_server(pt.read_lines(path))
                keys = {row["key"] for row in result["marks"]}
                for name in pt.SERVER_MODULES:
                    self.assertIn("Module: " + name, keys)
                self.assertIn("OnInit", keys)
                self.assertIn("Debug Zone State", keys)
                self.assertEqual(result["conflicts"], [])
                self.assertEqual(sorted(c["name"] for c in result["connections"]), ["Dev", "Laptop"])
                for conn in result["connections"]:
                    self.assertEqual(conn["pairing"], "identity")
                    for name in pt.CONNECTION_STAGES:
                        self.assertFalse(stage(conn, name).get("missing"), name)
                    self.assertGreater(stage(conn, "ClientNew")["delta_s"], 0)
                    # The join AddPlayer follows "connected!" within the same event.
                    self.assertLess(stage(conn, "AddPlayer")["delta_s"], 0.01)

    def test_real_fixture_without_join_addplayer(self):
        path = os.path.join(FIXTURES, "server", "script_2026-11-11_17-25-50.log")
        original = pt.read_lines(path)
        self.assertEqual(original.count(self.REMOVED), 1)
        mutated = [row for row in original if row != self.REMOVED]
        result = pt.parse_server(mutated)
        dev = next(c for c in result["connections"] if c["name"] == "Dev")
        laptop = next(c for c in result["connections"] if c["name"] == "Laptop")
        self.assertTrue(stage(dev, "AddPlayer").get("missing"))
        self.assertNotIn("delta_s", stage(dev, "AddPlayer"))
        self.assertEqual(stage(laptop, "AddPlayer")["t"], "2026-11-11T17:32:00.058")
        for conn in result["connections"]:
            for entry in conn["stages"]:
                self.assertNotEqual(entry.get("delta_s"), 274.721)
        text = pt.render({"sources": {"server": path}, "server": result})
        dev_block = text.split("[conn:Dev#1]")[1].split("-- connection")[0]
        self.assertIn("AddPlayer          missing", dev_block)

    def test_real_fixture_late_addplayer_after_handler_end(self):
        path = os.path.join(FIXTURES, "server", "script_2026-11-11_17-25-50.log")
        original = pt.read_lines(path)
        end_index = original.index(self.REMOVED) + 1
        self.assertIn("[EXTRACE] -CustomMission<", original[end_index])
        self.assertIn("::OnEvent ClientNewEventTypeID", original[end_index])
        late = "17:27:25.400  SCRIPT       : 17:27:25 [DayZ-BattleRoyale][2] BattleRoyaleState::AddPlayer: Dev"
        variants = {
            # A later AddPlayer right after the handler end, no other trace in between.
            "late line": original[:end_index - 1] + [original[end_index], late] + original[end_index + 1:],
            # The join AddPlayer and every transfer marker removed from the whole log.
            "no transfer markers": [
                row for row in original
                if row != self.REMOVED and not any(p in row for p in pt.TRANSFER_PREFIXES)
            ],
        }
        for name, mutated in variants.items():
            with self.subTest(variant=name):
                result = pt.parse_server(mutated)
                dev = next(c for c in result["connections"] if c["name"] == "Dev")
                self.assertTrue(stage(dev, "AddPlayer").get("missing"))
                self.assertEqual(result["conflicts"], [])
                laptop = next(c for c in result["connections"] if c["name"] == "Laptop")
                self.assertEqual(stage(laptop, "AddPlayer")["t"], "2026-11-11T17:32:00.058")

    def test_client_fixture(self):
        folder = os.path.join(FIXTURES, "client")
        path = next(os.path.join(folder, n) for n in os.listdir(folder) if n.startswith("script_"))
        result = pt.parse_client(pt.read_lines(path))
        keys = {row["key"] for row in result["marks"]}
        for name in pt.SERVER_MODULES:
            self.assertIn("Module: " + name, keys)
        self.assertIn("MPSessionStart", keys)
        self.assertIn("MPSessionPlayerReady", keys)

    def test_cli_json_on_fixture(self):
        path = self.server_logs()[0]
        out = subprocess.run([sys.executable, "-I", SCRIPT, "--server", path, "--json"],
                             capture_output=True, text=True, check=True).stdout
        data = json.loads(out)
        self.assertEqual(data["schema"], pt.SCHEMA)
        self.assertEqual(len(data["server"]["connections"]), 2)


def usable_tempdir():
    try:
        return tempfile.TemporaryDirectory()
    except OSError as error:
        raise unittest.SkipTest("no usable temporary folder: %s" % error)


class CliCompareTest(unittest.TestCase):
    def test_cli_compare(self):
        with usable_tempdir() as tmp:
            paths = []
            for name, text in (("a.json", SHORT_JOIN), ("b.json", SHORT_JOIN.replace("10:00:11.000", "10:00:12.000"))):
                path = os.path.join(tmp, name)
                with open(path, "w", encoding="utf-8") as handle:
                    json.dump(server_result(text), handle)
                paths.append(path)
            out = subprocess.run([sys.executable, "-I", SCRIPT, "--compare"] + paths,
                                 capture_output=True, text=True, check=True).stdout
        self.assertEqual(compare_row(out, "server/conn:Bob#1/ClientNew").split()[1:4], ["10.000", "11.000", "1.000"])
        self.assertEqual(compare_row(out, "server/conn:Bob#1/InvokeOnConnect").split()[1:4], ["0.100", "-0.900", "-1.000"])


if __name__ == "__main__":
    unittest.main()
