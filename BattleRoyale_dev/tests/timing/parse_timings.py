"""Turn DayZ script logs into per-stage timings for server boot and player join.

Reads a server script log (required), optionally a client script log and the log
written by BattleRoyale_server/tools/restart_loop.ps1, and prints one table per
source with the time of every mark, the duration from the previous mark and the
time from the start of the log. Stages that do not appear are listed as missing,
never estimated. Events that repeat are measured once per occurrence; server
joins are grouped per connection.

Usage:
    python -I parse_timings.py --server <script.log> [--client <script.log>]
                               [--loop <restart_loop.log>] [--json]
    python -I parse_timings.py --compare a.json b.json

Standard library only.
"""

import argparse
import datetime as dt
import json
import re
import sys

SCHEMA = "br-timings/1"

# "HH:MM:SS.mmm" at the start of every script log line.
LINE_TS_RE = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\.(\d{3})\s")
# Header: "Log <path> started at 2026-11-11 17:25:55 (2026-11-11 16:25:55 UTC)".
HEADER_RE = re.compile(
    r"started at (\d{4}-\d{2}-\d{2}) (\d{2}):(\d{2}):(\d{2})"
    r"(?: \((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) UTC\))?"
)
MODULE_RE = re.compile(r"SCRIPT\s+: Module: (\w+); loaded ")
# Mod trace: "17:26:19 [DayZ-BattleRoyale][4] Vigrid-BattleRoyale OnInit()".
MOD_RE = re.compile(r"\[DayZ-BattleRoyale\]\[\d\] (.*?)\s*$")
# Community Framework debug lines that carry the PlayerIdentity pointer.
CF_PREPARE_RE = re.compile(r"\[CF\] MissionServer::OnClientPrepareEvent PlayerIdentity<(0x[0-9A-Fa-f]+)>")
CF_NEW_RE = re.compile(r"\[CF\] MissionServer::OnClientNewEvent PlayerIdentity<(0x[0-9A-Fa-f]+)>")
CF_CONNECT_RE = re.compile(r"\[CF\] MissionServer::InvokeOnConnect .*?PlayerIdentity<(0x[0-9A-Fa-f]+)>")
CONNECTED_RE = re.compile(r"^Player (.+) connected!$")
ADD_PLAYER_RE = re.compile(r"^BattleRoyaleState::AddPlayer: (.+)$")
# A state change moves every player to the next state (BattleRoyaleServer.c:199-208):
# Leaving State, Deactivate and RemoveAllPlayers come before the AddPlayer calls it makes,
# so after any of them an AddPlayer is a transfer, not the join of BattleRoyaleServer.c:284.
TRANSFER_PREFIXES = (
    "[State Machine] Leaving State",
    "BattleRoyaleState::Deactivate:",
    "BattleRoyaleState::RemoveAllPlayers:",
)
# End of the mission's ClientNew handler (diag EXTRACE build), e.g.
# "[EXTRACE] -CustomMission<0x...>::OnEvent ClientNewEventTypeID 184.983 ms". The join
# AddPlayer runs inside that handler (MissionServer.c InvokeOnConnect -> OnPlayerConnected),
# so once it has returned no later AddPlayer belongs to the join.
CLIENT_NEW_END_RE = re.compile(r"\[EXTRACE\] -\S+::OnEvent ClientNewEventTypeID\b")
# restart_loop.ps1 Write-Log: "yyyy-MM-dd HH:mm:ss <message>" (local time, whole seconds).
LOOP_RE = re.compile(r"^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) (.*?)\s*$")

# A time of day that goes back by more than this is taken as a day change;
# smaller steps back are treated as out-of-order lines of the same day.
ROLLOVER_THRESHOLD = dt.timedelta(hours=1)

SERVER_MODULES = ["GameLib", "Game", "World", "Mission"]
CONNECTION_STAGES = ["ClientPrepare", "ClientNew", "InvokeOnConnect", "Connected", "AddPlayer"]

SERVER_EXPECTED = ["Module: " + m for m in SERVER_MODULES] + ["OnInit", "Debug Zone State", "Restarting"]
CLIENT_EXPECTED = ["Module: " + m for m in SERVER_MODULES] + [
    "MPSessionStart", "LoginTime", "Preload", "MPSessionPlayerReady", "HUD Initialized",
]
LOOP_EXPECTED = ["Server exited", "Start", "Storage moved", "Storage deleted"]

# Exact mod trace messages (BattleRoyaleUtils.Trace/Debug) mapped to mark keys.
SERVER_MOD_MARKS = {
    "Vigrid-BattleRoyale OnInit()": "OnInit",  # scripts_server/5_Mission/Mission/MissionServer.c:9
    "BattleRoyaleState::Activate: Debug Zone State": "Debug Zone State",  # States/0_BattleRoyaleState.c:71
    "[Restart State] Restarting!": "Restarting",  # States/9_BattleRoyaleRestart.c:11
}
CLIENT_MOD_MARKS = {
    "DayZGame::OnEvent MPSessionStartEventTypeID": "MPSessionStart",  # scripts_client/3_Game/DayZGame.c:19
    "DayZGame::OnEvent LoginTimeEventTypeID": "LoginTime",  # DayZGame.c:65
    "DayZGame::OnEvent PreloadEventTypeID": "Preload",  # DayZGame.c:75
    "DayZGame::OnEvent MPSessionPlayerReadyEventTypeID": "MPSessionPlayerReady",  # DayZGame.c:34
    "MissionGameplay::OnInit": "MissionGameplay OnInit",  # scripts_client/5_Mission/Mission/MissionGameplay.c:57
    "HUD Initialized": "HUD Initialized",  # MissionGameplay.c:107
}


def read_lines(path):
    """Return the text lines of a log, honouring a UTF-16 or UTF-8 BOM."""
    with open(path, "rb") as handle:
        return decode_bytes(handle.read())


def decode_bytes(raw):
    """Decode raw log bytes (UTF-16 with BOM, else UTF-8) into lines."""
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        text = raw.decode("utf-16", errors="replace")
    else:
        text = raw.decode("utf-8-sig", errors="replace")
    return text.splitlines()


class Clock:
    """Turns "HH:MM:SS.mmm" times of day into datetimes, following day changes."""

    def __init__(self, start_date):
        self.date = start_date
        self.last = None

    def stamp(self, hours, minutes, seconds, millis):
        current = dt.datetime.combine(self.date, dt.time(hours, minutes, seconds, millis * 1000))
        if self.last is not None and current < self.last and self.last - current > ROLLOVER_THRESHOLD:
            self.date = self.date + dt.timedelta(days=1)
            current = current + dt.timedelta(days=1)
        if self.last is None or current > self.last:
            self.last = current
        return current


def iter_script_lines(lines):
    """Yield (line_number, datetime_or_None, text) and the header start time.

    Returns a generator plus a dict filled with 'start', 'start_utc' and
    'start_source' once the header (or the first timestamped line) is seen.
    """
    info = {"start": None, "start_utc": None, "start_source": None}
    header_date = None
    for line in lines[:10]:
        match = HEADER_RE.search(line)
        if match:
            header_date = dt.date.fromisoformat(match.group(1))
            info["start"] = dt.datetime.combine(
                header_date, dt.time(int(match.group(2)), int(match.group(3)), int(match.group(4)))
            )
            info["start_utc"] = match.group(5)
            info["start_source"] = "header"
            break

    def generate():
        # Without a header the date is unknown; 1900-01-01 marks it as such.
        clock = Clock(header_date or dt.date(1900, 1, 1))
        if info["start"] is not None:
            clock.last = info["start"]
        for number, line in enumerate(lines, 1):
            match = LINE_TS_RE.match(line)
            if not match:
                yield number, None, line
                continue
            stamp = clock.stamp(*(int(g) for g in match.groups()))
            if info["start"] is None:
                info["start"] = stamp
                info["start_source"] = "first line"
            yield number, stamp, line

    return generate(), info


def new_mark(key, stamp, line_number, occurrence, detail=None):
    mark = {"key": key, "occurrence": occurrence, "t": stamp, "line": line_number}
    if detail:
        mark["detail"] = detail
    return mark


class MarkList:
    def __init__(self):
        self.marks = []
        self.counts = {}

    def add(self, key, stamp, line_number, detail=None):
        self.counts[key] = self.counts.get(key, 0) + 1
        self.marks.append(new_mark(key, stamp, line_number, self.counts[key], detail))


def parse_server(lines):
    """Parse a server script log into boot marks and per-connection stages."""
    stream, info = iter_script_lines(lines)
    marks = MarkList()
    connections = []
    awaiting_prepare = []  # connections whose ClientPrepare trace still lacks the CF identity line
    pending_new = None  # (stamp, line) of a ClientNew trace not yet bound to a connection
    current = None  # connection being joined (between ClientNew and Connected)
    join_open = None  # connection after "connected!" whose join AddPlayer may still come
    unmatched = []
    conflicts = []
    outside_join = []  # AddPlayer lines that are not a join (state transfers, or no open join)

    def open_connection(stamp, line_number):
        conn = {"index": len(connections) + 1, "identity": None, "name": None, "stages": {}, "pairing": None}
        conn["stages"]["ClientPrepare"] = {"t": stamp, "line": line_number}
        connections.append(conn)
        return conn

    def bind_new(identity):
        """Attach the pending ClientNew to a prepared connection; return it.

        A known identity is only paired with a ClientPrepare of the same identity,
        or by order with one whose identity is unknown; never with a different one.
        """
        nonlocal pending_new
        stamp, line_number = pending_new
        pending_new = None
        waiting = [c for c in connections if "ClientNew" not in c["stages"] and "ClientPrepare" in c["stages"]]
        target = None
        pairing = None
        if identity is not None:
            for conn in waiting:
                if conn["identity"] == identity:
                    target, pairing = conn, "identity"
                    break
            if target is None:
                for conn in waiting:
                    if conn["identity"] is None:
                        target, pairing = conn, "order"
                        break
            if target is None and waiting:
                conflicts.append({
                    "type": "identity_conflict",
                    "line": line_number,
                    "client_new_identity": identity,
                    "pending_prepare_identities": [c["identity"] for c in waiting],
                })
        elif waiting:
            target, pairing = waiting[0], "order"
        if target is None:
            # ClientNew without a matching ClientPrepare: keep it, with ClientPrepare missing.
            target = {"index": len(connections) + 1, "identity": identity, "name": None, "stages": {}, "pairing": None}
            connections.append(target)
            pairing = "none"
        if target["identity"] is None:
            target["identity"] = identity
        target["pairing"] = pairing
        target["stages"]["ClientNew"] = {"t": stamp, "line": line_number}
        return target

    for number, stamp, line in stream:
        if stamp is None:
            continue
        module = MODULE_RE.search(line)
        if module:
            marks.add("Module: " + module.group(1), stamp, number)
            continue
        cf_prepare = CF_PREPARE_RE.search(line)
        if cf_prepare:
            if awaiting_prepare:
                awaiting_prepare.pop(0)["identity"] = cf_prepare.group(1)
            continue
        cf_new = CF_NEW_RE.search(line)
        if cf_new:
            if pending_new is not None:
                current = bind_new(cf_new.group(1))
            continue
        if CLIENT_NEW_END_RE.search(line):
            # The ClientNew handler returned: the join AddPlayer, if any, has been logged.
            join_open = None
            continue
        mod = MOD_RE.search(line)
        if not mod:
            continue
        message = mod.group(1)
        if message in SERVER_MOD_MARKS:
            marks.add(SERVER_MOD_MARKS[message], stamp, number)
            continue
        if message.startswith(TRANSFER_PREFIXES):
            join_open = None
            continue
        if message == "ClientPrepareEventTypeID":
            join_open = None
            if pending_new is not None:
                current = bind_new(None)
            awaiting_prepare.append(open_connection(stamp, number))
            continue
        if message == "ClientNewEventTypeID":
            join_open = None
            if pending_new is not None:
                current = bind_new(None)
            awaiting_prepare = []
            pending_new = (stamp, number)
            continue
        if message == "InvokeOnConnect()":
            join_open = None
            if pending_new is not None:
                current = bind_new(None)
            if current is not None and "InvokeOnConnect" not in current["stages"]:
                current["stages"]["InvokeOnConnect"] = {"t": stamp, "line": number}
            else:
                unmatched.append({"event": "InvokeOnConnect", "line": number})
            continue
        connected = CONNECTED_RE.match(message)
        if connected:
            join_open = None
            if pending_new is not None:
                current = bind_new(None)
            if current is not None and "Connected" not in current["stages"]:
                current["stages"]["Connected"] = {"t": stamp, "line": number}
                current["name"] = connected.group(1)
                join_open = current
            else:
                unmatched.append({"event": "Connected", "line": number})
            continue
        added = ADD_PLAYER_RE.match(message)
        if added:
            # The join AddPlayer (BattleRoyaleServer.c:284) follows "connected!" before any
            # other connection or state-change trace; anything else is not a join.
            name = added.group(1)
            if join_open is not None and join_open["name"] == name:
                join_open["stages"]["AddPlayer"] = {"t": stamp, "line": number}
                join_open = None
            else:
                outside_join.append({"name": name, "line": number})
            continue
    if pending_new is not None:
        bind_new(None)

    start = info["start"]
    result = build_timeline(marks.marks, start, SERVER_EXPECTED)
    result["start_utc"] = info["start_utc"]
    result["start_source"] = info["start_source"]
    result["connections"] = []
    name_counts = {}
    for conn in connections:
        if conn["name"]:
            name_counts[conn["name"]] = name_counts.get(conn["name"], 0) + 1
            conn_id = "conn:%s#%d" % (conn["name"], name_counts[conn["name"]])
        else:
            conn_id = "conn%d" % conn["index"]
        stages = []
        previous = None
        previous_stage = None
        for stage in CONNECTION_STAGES:
            entry = conn["stages"].get(stage)
            if entry is None:
                stages.append({"stage": stage, "missing": True})
                continue
            delta = seconds(entry["t"] - previous) if previous else None
            if delta is not None and delta < 0:
                conflicts.append({"type": "negative_interval", "connection": conn_id, "stage": stage,
                                  "from": previous_stage, "delta_s": delta, "line": entry["line"]})
            stages.append({
                "stage": stage,
                "t": entry["t"].isoformat(timespec="milliseconds"),
                "line": entry["line"],
                "from": previous_stage,
                "delta_s": delta,
                "from_start_s": seconds(entry["t"] - start) if start else None,
            })
            previous = entry["t"]
            previous_stage = stage
        result["connections"].append({
            "id": conn_id,
            "index": conn["index"],
            "name": conn["name"],
            "identity": conn["identity"],
            "pairing": conn["pairing"],
            "stages": stages,
        })
    result["unmatched"] = unmatched
    result["conflicts"] = conflicts
    result["add_player_outside_join"] = outside_join
    return result


def parse_client(lines):
    stream, info = iter_script_lines(lines)
    marks = MarkList()
    for number, stamp, line in stream:
        if stamp is None:
            continue
        module = MODULE_RE.search(line)
        if module:
            marks.add("Module: " + module.group(1), stamp, number)
            continue
        mod = MOD_RE.search(line)
        if mod and mod.group(1) in CLIENT_MOD_MARKS:
            marks.add(CLIENT_MOD_MARKS[mod.group(1)], stamp, number)
    result = build_timeline(marks.marks, info["start"], CLIENT_EXPECTED)
    result["start_utc"] = info["start_utc"]
    result["start_source"] = info["start_source"]
    return result


def parse_loop(lines):
    marks = MarkList()
    start = None
    for number, line in enumerate(lines, 1):
        match = LOOP_RE.match(line)
        if not match:
            continue
        stamp = dt.datetime.strptime(match.group(1), "%Y-%m-%d %H:%M:%S")
        if start is None:
            start = stamp
        message = match.group(2)
        if message.startswith("Server exited with code "):
            marks.add("Server exited", stamp, number, message[len("Server exited with code "):])
        elif message.startswith("Start #"):
            marks.add("Start", stamp, number, message[len("Start #"):].split(":", 1)[0])
        elif message.startswith("Moved world storage to "):
            marks.add("Storage moved", stamp, number)
        elif message.startswith("No world storage at "):
            marks.add("No storage to move", stamp, number)
        elif message.startswith("Deleted old world storage "):
            marks.add("Storage deleted", stamp, number)
    result = build_timeline(marks.marks, start, LOOP_EXPECTED)
    result["start_source"] = "first line"
    # Downtime per restart: from each "Server exited" to the next "Start #".
    cycles = []
    exited = None
    for mark in marks.marks:
        if mark["key"] == "Server exited":
            exited = mark
        elif mark["key"] == "Start" and exited is not None:
            cycles.append({
                "exit_line": exited["line"],
                "start_run": mark.get("detail"),
                "downtime_s": seconds(mark["t"] - exited["t"]),
            })
            exited = None
    result["cycles"] = cycles
    return result


def seconds(delta):
    return round(delta.total_seconds(), 3)


def build_timeline(marks, start, expected):
    ordered = sorted(marks, key=lambda m: (m["t"], m["line"]))
    rows = []
    previous = start
    previous_id = "start" if start else None
    for mark in ordered:
        rows.append({
            "key": mark["key"],
            "occurrence": mark["occurrence"],
            "detail": mark.get("detail"),
            "t": mark["t"].isoformat(timespec="milliseconds"),
            "line": mark["line"],
            "from": previous_id,
            "delta_s": seconds(mark["t"] - previous) if previous else None,
            "from_start_s": seconds(mark["t"] - start) if start else None,
        })
        previous = mark["t"]
        previous_id = "%s#%d" % (mark["key"], mark["occurrence"])
    present = {m["key"] for m in marks}
    return {
        "start": start.isoformat(timespec="milliseconds") if start else None,
        "marks": rows,
        "missing": [key for key in expected if key not in present],
    }


def format_seconds(value):
    return "-" if value is None else "%.3f" % value


def label_for(row, counts):
    label = row["key"]
    if counts[row["key"]] > 1:
        label += " #%d" % row["occurrence"]
    if row.get("detail"):
        label += " (%s)" % row["detail"]
    return label


def render_timeline(title, section):
    out = ["== %s ==" % title, "start: %s (%s)" % (section["start"], section.get("start_source"))]
    if section.get("start_utc"):
        out[-1] += ", UTC %s" % section["start_utc"]
    counts = {}
    for row in section["marks"]:
        counts[row["key"]] = counts.get(row["key"], 0) + 1
    out.append("%-34s %-23s %10s %11s" % ("mark", "time", "delta_s", "from_start"))
    for row in section["marks"]:
        out.append("%-34s %-23s %10s %11s" % (
            label_for(row, counts), row["t"], format_seconds(row["delta_s"]), format_seconds(row["from_start_s"])))
    out.append("missing: %s" % (", ".join(section["missing"]) if section["missing"] else "none"))
    return out


def render(result):
    out = []
    if "server" in result:
        server = result["server"]
        out += render_timeline("server " + result["sources"]["server"], server)
        out.append("")
        if not server["connections"]:
            out.append("connections: none (no ClientPrepareEventTypeID / ClientNewEventTypeID traces)")
        for conn in server["connections"]:
            out.append("-- connection %d [%s]: %s (identity %s, paired by %s) --" % (
                conn["index"], conn.get("id"), conn["name"] or "name missing", conn["identity"] or "missing",
                conn["pairing"]))
            for stage in conn["stages"]:
                if stage.get("missing"):
                    out.append("   %-18s missing" % stage["stage"])
                else:
                    out.append("   %-18s %-23s %10s %11s" % (
                        stage["stage"], stage["t"], format_seconds(stage["delta_s"]),
                        format_seconds(stage["from_start_s"])))
        if server["unmatched"]:
            out.append("unmatched events: %s" % ", ".join(
                "%s@%d" % (u["event"], u["line"]) for u in server["unmatched"]))
        for conflict in server.get("conflicts", []):
            if conflict["type"] == "identity_conflict":
                out.append("CONFLICT line %d: ClientNew identity %s matches no pending ClientPrepare (%s); not paired" % (
                    conflict["line"], conflict["client_new_identity"],
                    ", ".join(str(i) for i in conflict["pending_prepare_identities"])))
            else:
                out.append("CONFLICT line %d: %s %s is %s s after %s (negative)" % (
                    conflict["line"], conflict["connection"], conflict["stage"],
                    format_seconds(conflict["delta_s"]), conflict["from"]))
        outside = server.get("add_player_outside_join", [])
        if outside:
            out.append("AddPlayer outside a join (state transfers): %d" % len(outside))
    if "client" in result:
        out.append("")
        out += render_timeline("client " + result["sources"]["client"], result["client"])
    if "loop" in result:
        out.append("")
        out += render_timeline("restart loop " + result["sources"]["loop"], result["loop"])
        for cycle in result["loop"]["cycles"]:
            out.append("   downtime before start #%s: %s s" % (cycle["start_run"], format_seconds(cycle["downtime_s"])))
    return "\n".join(out)


def flatten(result):
    """Map each stage id to {delta, from, from_start}.

    'from' names the mark the delta is measured from, so that two deltas are only
    compared when they measure the same interval. JSON written before 'from'
    existed gives from=None, which is never comparable.
    """
    flat = {}
    for source in ("server", "client", "loop"):
        section = result.get(source)
        if not section:
            continue
        for row in section["marks"]:
            origin = row.get("from")
            flat["%s/%s#%d" % (source, row["key"], row["occurrence"])] = {
                "delta": row["delta_s"],
                "from": None if origin is None else "%s/%s" % (source, origin),
                "from_start": row["from_start_s"],
            }
        for conn in section.get("connections", []):
            conn_id = conn.get("id") or "conn%d" % conn["index"]
            for stage in conn["stages"]:
                if stage.get("missing"):
                    continue
                origin = stage.get("from")
                flat["%s/%s/%s" % (source, conn_id, stage["stage"])] = {
                    "delta": stage["delta_s"],
                    "from": None if origin is None else "%s/%s/%s" % (source, conn_id, origin),
                    "from_start": stage["from_start_s"],
                }
    return flat


def compare_results(a, b, label_a="A", label_b="B"):
    """Per-stage B-A differences; a delta is only subtracted when both measure the same interval."""
    fa, fb = flatten(a), flatten(b)
    keys = list(fa) + [k for k in fb if k not in fa]
    out = ["A: %s" % label_a, "B: %s" % label_b,
           "%-44s %10s %10s %10s %12s %12s %12s" % (
               "stage", "A delta", "B delta", "diff", "A from_start", "B from_start", "diff")]
    empty = {"delta": None, "from": None, "from_start": None}

    def diff(x, y):
        return None if x is None or y is None else round(y - x, 3)

    for key in keys:
        ea, eb = fa.get(key, empty), fb.get(key, empty)
        notes = []
        if key not in fb:
            notes.append("only in A")
        elif key not in fa:
            notes.append("only in B")
        delta_diff = "-"
        if ea["delta"] is not None and eb["delta"] is not None:
            if ea["from"] is not None and ea["from"] == eb["from"]:
                delta_diff = format_seconds(diff(ea["delta"], eb["delta"]))
            else:
                delta_diff = "n/c"
                notes.append("delta not comparable: A from %s, B from %s" % (ea["from"], eb["from"]))
        out.append("%-44s %10s %10s %10s %12s %12s %12s%s" % (
            key, format_seconds(ea["delta"]), format_seconds(eb["delta"]), delta_diff,
            format_seconds(ea["from_start"]), format_seconds(eb["from_start"]),
            format_seconds(diff(ea["from_start"], eb["from_start"])),
            ("  " + "; ".join(notes)) if notes else ""))
    return "\n".join(out)


def compare(path_a, path_b):
    with open(path_a, encoding="utf-8") as handle:
        a = json.load(handle)
    with open(path_b, encoding="utf-8") as handle:
        b = json.load(handle)
    return compare_results(a, b, path_a, path_b)


def analyse(server=None, client=None, loop=None):
    result = {"schema": SCHEMA, "sources": {}}
    if server:
        result["sources"]["server"] = server
        result["server"] = parse_server(read_lines(server))
    if client:
        result["sources"]["client"] = client
        result["client"] = parse_client(read_lines(client))
    if loop:
        result["sources"]["loop"] = loop
        result["loop"] = parse_loop(read_lines(loop))
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description="Per-stage timings from DayZ BattleRoyale logs.")
    parser.add_argument("--server", help="server script_*.log")
    parser.add_argument("--client", help="client script_*.log")
    parser.add_argument("--loop", help="log written by restart_loop.ps1")
    parser.add_argument("--json", action="store_true", help="print JSON instead of tables")
    parser.add_argument("--compare", nargs=2, metavar=("A_JSON", "B_JSON"), help="diff two --json outputs")
    args = parser.parse_args(argv)
    if args.compare:
        print(compare(*args.compare))
        return 0
    if not args.server:
        parser.error("--server is required unless --compare is given")
    result = analyse(args.server, args.client, args.loop)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(render(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
