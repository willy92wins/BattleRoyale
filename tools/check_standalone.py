#!/usr/bin/env python3
"""Report remaining dependencies of the BattleRoyale mod on DayZ Expansion.

Checks (errors unless noted):
  text               the word "expansion" (any case) in mod text files
  required_addon     requiredAddons entries naming DayZExpansion* in config.cpp
  resource           Expansion imagesets or paths (set:expansion_*, DayZExpansion\\),
                     also inside #define values
  symbol             identifiers used in the mod's .c files that resolve to a
                     declaration made only by the Expansion sources, plus
                     Expansion-named identifiers declared nowhere (modules missing
                     from the references)
  ambiguous_homonym  (warning) a member name declared both by Expansion and
                     elsewhere, used on a receiver whose type was not resolved
  ifdef              (warning) #if/#ifdef/#ifndef/#elif conditions on Expansion defines
  pbo:*              the same text checks inside built, uncompressed .pbo files

Symbol resolution is scope-aware: a local or parameter only shadows uses inside
its own function, a member only inside its class hierarchy. Member accesses
(`a.b`) resolve the receiver's type from locals, members, globals, known return
types (GetGame() -> DayZGame, GetDayZGame() -> DayZGame, X.Cast() -> X), `this`
and `super`, and then look the member up on that class and its bases.

Coverage limits: this is a heuristic token scanner, not an Enforce compiler. It
does not evaluate #ifdef branches, overloads, template substitution beyond
array/set/map element types, implicit conversions or engine-only (non-script)
declarations. Unresolved receivers on homonyms are reported as warnings, never
silenced. A compile of the mod without Expansion (gate V3) complements it.

Resolution also covers `auto x = expr` (type of expr), `foreach (auto x : c)`,
`new X()` and grouping parentheses `( expr ).m()`.

--strict is the V1 gate mode: every ambiguous_homonym use must be listed in the
reviewed file (tools/check_standalone_reviewed.txt by default, one
`path:line:name  # reason` per line) or the exit code is 1; the unreviewed uses
are printed in that exact format. Without --strict they stay warnings.

Exit codes: 0 clean, 1 findings (or unreviewed homonyms with --strict), 2 usage
or input error (including an empty or unreadable reference corpus, mod folder or
PBO folder, and a missing or malformed reviewed file with --strict).
Standard library only; runs under ``python -I``.
"""

import argparse
import bisect
import fnmatch
import json
import os
import re
import struct
import sys
import time

MOD_DIRS = ("BattleRoyale", "BattleRoyale_VPP", "BattleRoyale_server")
TEXT_EXTS = (".c", ".cpp", ".h", ".hpp", ".layout", ".imageset", ".xml", ".json", ".csv")
SCRIPT_EXTS = (".c", ".h")
DEFAULT_ALLOW = ("CHANGELOG.md", "*.md")

DEFAULT_REF_ROOT = r"C:\Users\guill\Repos\br-ref"
DEFAULT_VANILLA = r"C:\Users\guill\OneDrive\Documentos\DayZ Projects\scripts"
EXPANSION_REFS = ("exp_core_scripts", "expansion_scripts", "navigation_scripts")
OTHER_REFS = ("cf_scripts", "dabs_scripts", "cot", "vpp")
REQUIRED_OTHER_REFS = ("cf_scripts", "dabs_scripts")
DEFAULT_REVIEWED = os.path.join(os.path.dirname(os.path.abspath(__file__)), "check_standalone_reviewed.txt")

RE_TEXT = re.compile(r"expansion", re.I)
RE_RESOURCE = re.compile(r"set:expansion_\w*|DayZExpansion(?:\\\\|\\|/)", re.I)
RE_PREPROC_COND = re.compile(r"^\s*#\s*(ifdef|ifndef|if|elif)\b(.*)$")
RE_EXP_DEFINE = re.compile(r"\b\w*EXPANSION\w*\b", re.I)
RE_REQUIRED = re.compile(r"requiredAddons\s*\[\s*\]\s*=\s*\{(.*?)\}", re.S)
RE_QUOTED = re.compile(r'"([^"]*)"')

# Comments, strings and preprocessor lines are blanked before tokenizing.
RE_STRIP = re.compile(
    r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|^[ \t]*#[^\n]*',
    re.S | re.M,
)
RE_TOKEN = re.compile(r"[A-Za-z_]\w*|\S")

# Words that can never be the type part of a declaration.
NOT_TYPE = frozenset("""
return new delete case else if while for foreach switch class enum typedef modded
extends static const private protected public override proto native volatile event
owned ref reference autoptr out inout notnull external sealed local thread default
break continue this super null NULL true false goto
""".split())
# Reserved words and primitive types; never reported as symbols.
RESERVED = NOT_TYPE | frozenset("""
void int float bool string vector typename auto func Class Managed array map set
""".split())
TYPE_MODIFIERS = frozenset(("ref", "autoptr", "notnull", "const", "out", "inout", "owned", "reference"))

PBO_VERS = 0x56657273
PBO_COMPRESSED = 0x43707273

ORIGINS = ("exp", "other", "mod")


class InputError(Exception):
    pass


# --------------------------------------------------------------------------
# Files
# --------------------------------------------------------------------------

def _raise(exc):
    raise exc


def iter_files(root, exts):
    """Yield matching files under root; any directory read error raises."""
    for dirpath, dirnames, filenames in os.walk(root, onerror=_raise):
        dirnames.sort()
        for fn in sorted(filenames):
            if fn.lower().endswith(exts):
                yield os.path.join(dirpath, fn)


def list_files(root, exts, label):
    try:
        return list(iter_files(root, exts))
    except OSError as exc:
        raise InputError("cannot read %s %s: %s" % (label, root, exc))


def read_text(path):
    with open(path, "rb") as fh:
        data = fh.read()
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    return data.decode("utf-8", errors="replace")


# --------------------------------------------------------------------------
# Enforce Script scanner
# --------------------------------------------------------------------------

def _blank(match):
    return re.sub(r"[^\n]", " ", match.group(0))


def tokenize(src):
    """Return (tokens, lines) with comments, strings and preprocessor removed."""
    clean = RE_STRIP.sub(_blank, src)
    newlines = [m.start() for m in re.finditer("\n", clean)]
    tokens = []
    lines = []
    for m in RE_TOKEN.finditer(clean):
        tokens.append(m.group(0))
        lines.append(bisect.bisect_left(newlines, m.start()) + 1)
    return tokens, lines


def _is_ident(tok):
    c = tok[0]
    return c.isalpha() or c == "_"


def _type_before(tokens, i):
    """Return the type tokens ending at tokens[i-1] (Foo, Foo<...>, Foo[]), or None."""
    if i == 0:
        return None
    p = tokens[i - 1]
    if _is_ident(p):
        return None if p in NOT_TYPE else (p,)
    if p == "]":
        if i >= 2 and tokens[i - 2] == "[":
            inner = _type_before(tokens, i - 2)
            return inner + ("[", "]") if inner else None
        return None
    if p == ">":
        depth = 0
        j = i - 1
        stop = max(0, i - 40)
        while j >= stop:
            t = tokens[j]
            if t == ">":
                depth += 1
            elif t == "<":
                depth -= 1
                if depth == 0:
                    if j > 0 and _is_ident(tokens[j - 1]) and tokens[j - 1] not in NOT_TYPE:
                        return tuple(tokens[j - 1:i])
                    return None
            elif t in (";", "{", "}", "(", ")", "=", "&", "|", "!"):
                return None
            j -= 1
    return None


def _statement_has(tokens, i, word):
    """True if `word` appears between the previous statement boundary and i."""
    j = i - 1
    while j >= 0:
        t = tokens[j]
        if t in (";", "{", "}"):
            return False
        if t == word:
            return True
        j -= 1
    return False


def _skip_generic(tokens, j):
    """If tokens[j] == '<', return the index after the matching '>'."""
    if j < len(tokens) and tokens[j] == "<":
        depth = 0
        while j < len(tokens):
            if tokens[j] == "<":
                depth += 1
            elif tokens[j] == ">":
                depth -= 1
                if depth == 0:
                    return j + 1
            elif tokens[j] in (";", "{", "}"):
                return j
            j += 1
    return j


class FileScan(object):
    """Declarations of one file and, with track=True, per-token context."""

    __slots__ = ("decls", "classes", "typedefs", "decl_idx", "ctx_class", "ctx_func", "funcs")

    def __init__(self):
        # (scope, owner, name, kind, type, line); scope: global|member|local
        self.decls = []
        self.classes = []      # (name, base, modded, params)
        self.typedefs = []     # (name, target tokens)
        self.decl_idx = set()  # token indexes that are declared names
        self.ctx_class = None  # per token: (class, modded, base) or None
        self.ctx_func = None   # per token: function id or None
        self.funcs = []        # per function: [(name, type, idx)]


def scan(tokens, lines, track=False):
    fs = FileScan()
    n = len(tokens)
    if track:
        fs.ctx_class = [None] * n
        fs.ctx_func = [None] * n
    stack = [("top", None)]   # (kind, class ctx)
    pending = None            # ("class", ctx) | ("enum", name) | ("func",)
    header = None             # parameter list of the function being declared
    paren = 0
    cur_func = None
    func_stack = []
    i = 0
    while i < n:
        t = tokens[i]
        kind, cctx = stack[-1]
        if track:
            fs.ctx_class[i] = cctx
            fs.ctx_func[i] = cur_func
        if t == "{":
            if pending and pending[0] == "class":
                stack.append(("class", pending[1]))
            elif pending and pending[0] == "enum":
                stack.append(("enum", (pending[1], False, None)))
            elif pending and pending[0] == "func":
                func_stack.append(cur_func)
                fs.funcs.append(list(header or []))
                cur_func = len(fs.funcs) - 1
                stack.append(("func", cctx))
            else:
                stack.append(("block", cctx))
            pending = None
            header = None
            paren = 0
        elif t == "}":
            if len(stack) > 1:
                popped = stack.pop()
                if popped[0] == "func":
                    cur_func = func_stack.pop() if func_stack else None
            pending = None
            header = None
            paren = 0
        elif t == ";":
            pending = None
            header = None
            paren = 0
        elif t == "(":
            paren += 1
        elif t == ")":
            if paren:
                paren -= 1
        elif _is_ident(t):
            nxt = tokens[i + 1] if i + 1 < n else ""
            at_scope = kind in ("top", "class") and paren == 0
            owner = cctx[0] if (cctx and kind == "class") else None
            if t in ("class", "enum") and kind in ("top", "class"):
                if i + 1 < n and _is_ident(nxt):
                    name = nxt
                    modded = i > 0 and tokens[i - 1] == "modded"
                    j = i + 2
                    params = []
                    if j < n and tokens[j] == "<":
                        end = _skip_generic(tokens, j)
                        params = [tokens[k] for k in range(j + 1, end - 1)
                                  if _is_ident(tokens[k]) and tokens[k + 1] in (",", ">")]
                        j = end
                    base = None
                    if j + 1 < n and tokens[j] in (":", "extends") and _is_ident(tokens[j + 1]):
                        base = tokens[j + 1]
                    if not modded:
                        fs.decls.append(("global", None, name, t, None, lines[i + 1]))
                        fs.decl_idx.add(i + 1)
                    if t == "class":
                        fs.classes.append((name, base, modded, params))
                        pending = ("class", (name, modded, base))
                    else:
                        fs.classes.append((name, None, modded, []))
                        pending = ("enum", name)
                    if track:
                        fs.ctx_class[i + 1] = cctx
                        fs.ctx_func[i + 1] = cur_func
                    i += 2
                    continue
            elif t == "typedef" and kind in ("top", "class"):
                j = i + 1
                last = None
                while j < n and tokens[j] not in (";", "{", "}"):
                    if _is_ident(tokens[j]):
                        last = j
                    j += 1
                if last is not None:
                    fs.decls.append(("global", None, tokens[last], "typedef", None, lines[last]))
                    fs.typedefs.append((tokens[last], tuple(tokens[i + 1:last])))
                    fs.decl_idx.add(last)
                if track:
                    for k in range(i, min(j, n)):
                        fs.ctx_class[k] = cctx
                        fs.ctx_func[k] = cur_func
                i = j
                continue
            elif kind == "enum":
                prev = tokens[i - 1] if i else ""
                if prev in ("{", ",") and nxt in (",", "=", "}"):
                    fs.decls.append(("member", cctx[0], t, "enum_member", None, lines[i]))
                    fs.decl_idx.add(i)
            elif t not in RESERVED or t in ("Class", "Managed"):
                if nxt == "(" and paren == 0 and kind in ("top", "class"):
                    typ = _type_before(tokens, i)
                    if typ is not None:
                        if not _statement_has(tokens, i, "override"):
                            scope = "member" if owner else "global"
                            fs.decls.append((scope, owner, t, "function", typ, lines[i]))
                            fs.decl_idx.add(i)
                        pending = ("func",)
                        header = []
                elif nxt in (";", "=", ",", ")", ":"):
                    typ = _type_before(tokens, i)
                    if typ is not None:
                        if at_scope:
                            k = "const" if _statement_has(tokens, i, "const") else "variable"
                            scope = "member" if owner else "global"
                            fs.decls.append((scope, owner, t, k, typ, lines[i]))
                            fs.decl_idx.add(i)
                        elif header is not None and paren > 0 and kind in ("top", "class"):
                            header.append((t, typ, i))
                            fs.decl_idx.add(i)
                        elif cur_func is not None and kind in ("func", "block"):
                            fs.funcs[cur_func].append((t, typ, i))
                            fs.decl_idx.add(i)
        i += 1
    return fs


# --------------------------------------------------------------------------
# Declaration model
# --------------------------------------------------------------------------

class Model(object):
    """Classes, members, globals and typedefs with the origin of each declaration."""

    def __init__(self):
        self.classes = {}      # name -> {"bases": [], "params": [], "members": {name: {origin: info}}}
        self.globals = {}      # name -> {origin: info}
        self.typedefs = {}     # name -> target tokens
        self.member_any = {}   # name -> {origin: (class, info)}
        self.files = 0

    def _cls(self, name):
        c = self.classes.get(name)
        if c is None:
            c = self.classes[name] = {"bases": [], "params": [], "members": {}}
        return c

    def add(self, fs, origin, path):
        self.files += 1
        for name, base, modded, params in fs.classes:
            c = self._cls(name)
            if base and not modded and base not in c["bases"]:
                c["bases"].append(base)
            if params and not c["params"]:
                c["params"] = params
        for name, target in fs.typedefs:
            self.typedefs.setdefault(name, target)
        for scope, owner, name, kind, typ, line in fs.decls:
            info = (kind, typ, path, line)
            if scope == "global":
                slot = self.globals.setdefault(name, {})
                # A later declaration (higher script module) wins for globals.
                if kind == "function" or origin not in slot:
                    slot[origin] = info
            else:
                members = self._cls(owner)["members"]
                members.setdefault(name, {}).setdefault(origin, info)
                self.member_any.setdefault(name, {}).setdefault(origin, (owner, info))


class Resolver(object):
    """Layered lookups over the reference model and the mod model."""

    def __init__(self, models):
        self.models = models

    def is_class(self, name):
        return any(name in m.classes or name in m.typedefs for m in self.models)

    def bases(self, name):
        out = []
        for m in self.models:
            c = m.classes.get(name)
            if c:
                for b in c["bases"]:
                    if b not in out:
                        out.append(b)
        return out

    def params(self, name):
        for m in self.models:
            c = m.classes.get(name)
            if c and c["params"]:
                return c["params"]
        return []

    def typedef(self, name):
        for m in self.models:
            if name in m.typedefs and name not in m.classes:
                return m.typedefs[name]
        return None

    def normalize(self, typ):
        """Strip modifiers and resolve typedefs; return type tokens or None."""
        for _ in range(6):
            if not typ:
                return None
            typ = tuple(t for t in typ if t not in TYPE_MODIFIERS)
            if not typ:
                return None
            if typ[-2:] == ("[", "]"):
                return ("array", "<") + typ[:-2] + (">",)
            target = self.typedef(typ[0])
            if target is None or len(typ) > 1:
                return typ
            typ = target
        return typ

    def class_of(self, typ):
        # Primitive types are script classes in vanilla (enconvert.c, enstring.c).
        typ = self.normalize(typ)
        if not typ or typ[0] == "void":
            return None
        return typ[0]

    @staticmethod
    def type_args(typ):
        if not typ or len(typ) < 3 or typ[1] != "<":
            return []
        args, cur, depth = [], [], 0
        for t in typ[2:]:
            if t == "<":
                depth += 1
            elif t == ">":
                if depth == 0:
                    break
                depth -= 1
            elif t == "," and depth == 0:
                args.append(tuple(cur))
                cur = []
                continue
            cur.append(t)
        if cur:
            args.append(tuple(cur))
        return args

    def chain(self, name):
        seen = []
        todo = [name]
        while todo and len(seen) < 40:
            c = todo.pop(0)
            if c in seen:
                continue
            seen.append(c)
            todo.extend(self.bases(c))
        return seen

    def member(self, cls, name):
        """Merged {origin: info} for `name` on cls and its bases."""
        out = {}
        for c in self.chain(cls):
            for m in self.models:
                cinfo = m.classes.get(c)
                if cinfo and name in cinfo["members"]:
                    for o, info in cinfo["members"][name].items():
                        out.setdefault(o, (c,) + info)
        return out

    def member_type(self, typ, name):
        typ = self.normalize(typ)
        cls = self.class_of(typ)
        if cls is None:
            return None
        info = self.member(cls, name)
        for o in ("mod", "other", "exp"):
            if o in info:
                rt = info[o][2]
                if not rt or rt == ("void",):
                    return None
                params = self.params(cls)
                args = self.type_args(typ)
                rt = tuple(t for t in rt if t not in TYPE_MODIFIERS)
                if len(rt) == 1 and rt[0] in params and params.index(rt[0]) < len(args):
                    return args[params.index(rt[0])]
                return rt
        return None

    def element_type(self, typ):
        typ = self.normalize(typ)
        if not typ:
            return None
        args = self.type_args(typ)
        if typ[0] in ("array", "set") and args:
            return args[0]
        if typ[0] == "map" and len(args) > 1:
            return args[1]
        return None

    def global_info(self, name):
        out = {}
        for m in self.models:
            for o, info in m.globals.get(name, {}).items():
                out.setdefault(o, info)
        return out

    def global_type(self, name):
        info = self.global_info(name)
        for o in ("mod", "other", "exp"):
            if o in info and info[o][1]:
                return info[o][1]
        return None

    def member_any(self, name):
        out = {}
        for m in self.models:
            for o, v in m.member_any.get(name, {}).items():
                out.setdefault(o, v)
        return out


def build_model(dirs, origin, model, label):
    for d in dirs:
        files = list_files(d, SCRIPT_EXTS, label)
        if not files:
            raise InputError("%s indexes no script files: %s" % (label, d))
        for path in files:
            toks, lns = tokenize(read_text(path))
            model.add(scan(toks, lns), origin, path)


_INDEX_CACHE = {}


def build_reference_model(exp_dirs, required_dirs, optional_dirs):
    key = (tuple(exp_dirs), tuple(required_dirs), tuple(optional_dirs))
    if key in _INDEX_CACHE:
        return _INDEX_CACHE[key]
    model = Model()
    build_model(exp_dirs, "exp", model, "Expansion reference")
    build_model(required_dirs, "other", model, "reference")
    for d in optional_dirs:
        if list_files(d, SCRIPT_EXTS, "reference"):
            build_model([d], "other", model, "reference")
    _INDEX_CACHE[key] = model
    return model


# --------------------------------------------------------------------------
# Use analysis
# --------------------------------------------------------------------------

def _match_back(tokens, j, open_t, close_t):
    depth = 0
    while j >= 0:
        if tokens[j] == close_t:
            depth += 1
        elif tokens[j] == open_t:
            depth -= 1
            if depth == 0:
                return j
        j -= 1
    return None


class UseAnalyzer(object):
    def __init__(self, resolver, rel, tokens, fs):
        self.r = resolver
        self.rel = rel
        self.tokens = tokens
        self.fs = fs

    def local_decl(self, fid, name, j):
        """(type, index) of the latest local `name` declared before token j."""
        if fid is None:
            return None
        found = None
        for lname, ltyp, idx in self.fs.funcs[fid]:
            if lname == name and idx < j:
                found = (ltyp, idx)
        return found

    def local_type(self, fid, name, j):
        found = self.local_decl(fid, name, j)
        return found[0] if found else None

    def _expr_end(self, k, stops):
        """Last token index of the expression starting at k, before a depth-0 stop."""
        toks = self.tokens
        depth = 0
        while k < len(toks):
            t = toks[k]
            if t in ("(", "[", "{"):
                depth += 1
            elif t in (")", "]", "}"):
                if depth == 0:
                    return k - 1 if ")" in stops else None
                depth -= 1
            elif depth == 0 and t in stops:
                return k - 1
            k += 1
        return None

    def auto_type(self, idx, depth):
        """Type of `auto x = expr` from expr, or of `foreach (auto x : coll)`."""
        toks = self.tokens
        if idx + 1 >= len(toks):
            return None
        if toks[idx + 1] == "=":
            end = self._expr_end(idx + 2, (";", ","))
            if end is None or end < idx + 2:
                return None
            return self.resolve(end, idx, depth + 1)
        if toks[idx + 1] == ":":
            end = self._expr_end(idx + 2, (")",))
            if end is None or end < idx + 2:
                return None
            return self.r.element_type(self.resolve(end, idx, depth + 1))
        return None

    def resolve(self, j, i, depth=0):
        """Type tokens of the expression ending at token j, seen from use site i."""
        if j < 0 or depth > 12:
            return None
        toks = self.tokens
        cctx = self.fs.ctx_class[i]
        fid = self.fs.ctx_func[i]
        t = toks[j]
        if t == ")":
            k = _match_back(toks, j, "(", ")")
            if k is None:
                return None
            name = toks[k - 1] if k > 0 else ""
            if not name or not _is_ident(name) or name in NOT_TYPE:
                # Grouping parentheses: ( expr ).m()
                if j - 1 > k:
                    return self.resolve(j - 1, i, depth + 1)
                return None
            if k >= 2 and toks[k - 2] == "new":
                return (name,)
            if k >= 2 and toks[k - 2] == ".":
                rj = k - 3
                if (name == "Cast" and rj >= 0 and _is_ident(toks[rj])
                        and (rj == 0 or toks[rj - 1] != ".") and self.r.is_class(toks[rj])):
                    return (toks[rj],)
                return self.r.member_type(self.resolve(rj, i, depth + 1), name)
            if cctx:
                mt = self.r.member_type((cctx[0],), name)
                if mt:
                    return mt
            return self.r.global_type(name)
        if t == "]":
            k = _match_back(toks, j, "[", "]")
            if k is None:
                return None
            return self.r.element_type(self.resolve(k - 1, i, depth + 1))
        if _is_ident(t):
            if j > 0 and toks[j - 1] == ".":
                return self.r.member_type(self.resolve(j - 2, i, depth + 1), t)
            if t == "this":
                return (cctx[0],) if cctx else None
            if t == "super":
                if not cctx:
                    return None
                if cctx[1] or not cctx[2]:
                    return (cctx[0],)
                return (cctx[2],)
            ld = self.local_decl(fid, t, j)
            if ld:
                if ld[0] == ("auto",):
                    return self.auto_type(ld[1], depth)
                return ld[0]
            if cctx:
                mt = self.r.member_type((cctx[0],), t)
                if mt:
                    return mt
            if self.r.is_class(t):
                return (t,)
            return self.r.global_type(t)
        return None

    def run(self, errors, warnings):
        toks = self.tokens
        fs = self.fs
        for i, t in enumerate(toks):
            if not _is_ident(t) or t in RESERVED or i in fs.decl_idx:
                continue
            if i > 0 and toks[i - 1] == ".":
                self.member_use(i, t, errors, warnings)
            else:
                self.plain_use(i, t, errors)

    def report(self, sink, name, i, kind, where):
        sink.setdefault(name, {"kind": kind, "where": where, "uses": []})["uses"].append((self.rel, i))

    def judge(self, info, name, i, errors, owner=None):
        """info: {origin: (..., kind, type, path, line)}; True if it decided."""
        if not info:
            return False
        if "other" in info or "mod" in info:
            return True
        exp = info["exp"]
        kind, _typ, path, line = exp[-4:]
        if owner:
            kind = "%s on %s" % (kind, owner)
        self.report(errors, name, i, kind, (path, line))
        return True

    def plain_use(self, i, t, errors):
        cctx = self.fs.ctx_class[i]
        fid = self.fs.ctx_func[i]
        if self.local_type(fid, t, i):
            return
        if cctx:
            info = self.r.member(cctx[0], t)
            if info:
                owner = info.get("exp", (None,))[0]
                self.judge(info, t, i, errors, owner)
                return
        if self.judge(self.r.global_info(t), t, i, errors):
            return
        if RE_TEXT.search(t):
            self.report(errors, t, i, "unresolved", None)

    def member_use(self, i, t, errors, warnings):
        rt = self.resolve(i - 2, i)
        cls = self.r.class_of(rt) if rt else None
        if cls and self.r.is_class(cls):
            info = self.r.member(cls, t)
            if info:
                owner = info.get("exp", (None,))[0]
                self.judge(info, t, i, errors, owner)
                return
        anyinfo = self.r.member_any(t)
        if not anyinfo:
            if RE_TEXT.search(t):
                self.report(errors, t, i, "unresolved", None)
            return
        if "exp" not in anyinfo:
            return
        owner, info = anyinfo["exp"]
        if "other" in anyinfo or "mod" in anyinfo:
            other = anyinfo.get("other") or anyinfo.get("mod")
            self.report(warnings, t, i, "%s on %s (Expansion) and %s" % (info[0], owner, other[0]),
                        (info[2], info[3]))
        else:
            self.report(errors, t, i, "%s on %s" % (info[0], owner), (info[2], info[3]))


# --------------------------------------------------------------------------
# Text checks
# --------------------------------------------------------------------------

def is_allowed(rel, allow):
    base = rel.rsplit("/", 1)[-1]
    return any(fnmatch.fnmatch(rel, p) or fnmatch.fnmatch(base, p) for p in allow)


def scan_text_content(rel, text, prefix=""):
    """Return (errors, warnings) for one text file's content."""
    errors = []
    warnings = []
    is_config = rel.lower().endswith("config.cpp")
    for no, line in enumerate(text.splitlines(), 1):
        if not RE_TEXT.search(line):
            continue
        # Only conditions on Expansion defines are warnings; #define values,
        # #include paths and other directives stay errors.
        cond = RE_PREPROC_COND.match(line)
        if cond:
            body = cond.group(2).split("//", 1)[0]
            m = RE_EXP_DEFINE.search(body)
            if m and not RE_RESOURCE.search(line):
                warnings.append({
                    "category": prefix + "ifdef", "file": rel, "line": no,
                    "match": m.group(0), "detail": line.strip()[:120],
                })
                continue
        for m in RE_RESOURCE.finditer(line):
            errors.append({
                "category": prefix + "resource", "file": rel, "line": no,
                "match": m.group(0), "detail": line.strip()[:120],
            })
        words = sorted({m.group(0) for m in re.finditer(r"[\w:\\/.-]*expansion[\w\\/.-]*", line, re.I)})
        errors.append({
            "category": prefix + "text", "file": rel, "line": no,
            "match": ", ".join(words)[:80], "detail": line.strip()[:120],
        })
    if is_config:
        stripped = re.sub(r"//[^\n]*|/\*.*?\*/", _blank, text, flags=re.S)
        for block in RE_REQUIRED.finditer(stripped):
            for q in RE_QUOTED.finditer(block.group(1)):
                if q.group(1).lower().startswith("dayzexpansion"):
                    pos = block.start(1) + q.start()
                    errors.append({
                        "category": prefix + "required_addon", "file": rel,
                        "line": stripped.count("\n", 0, pos) + 1,
                        "match": q.group(1), "detail": "requiredAddons",
                    })
    return errors, warnings


# --------------------------------------------------------------------------
# PBO reader (uncompressed entries only)
# --------------------------------------------------------------------------

def read_pbo_entries(path):
    """Yield (name, bytes or None, compressed) for each entry of a PBO."""
    with open(path, "rb") as fh:
        data = fh.read()
    pos = 0
    entries = []

    def cstr():
        nonlocal pos
        end = data.find(b"\0", pos)
        if end < 0:
            raise InputError("%s: truncated PBO header" % path)
        s = data[pos:end]
        pos = end + 1
        return s

    while True:
        name = cstr()
        if pos + 20 > len(data):
            raise InputError("%s: truncated PBO header" % path)
        mime, _orig, _res, _ts, size = struct.unpack_from("<5I", data, pos)
        pos += 20
        if not name:
            if mime == PBO_VERS:
                while cstr():
                    pass
                continue
            break
        entries.append((name.decode("latin-1"), mime, size))
    offset = pos
    for name, mime, size in entries:
        if offset + size > len(data):
            raise InputError("%s: entry %s runs past end of file" % (path, name))
        blob = data[offset:offset + size]
        offset += size
        if mime == PBO_COMPRESSED:
            yield name, None, True
        else:
            yield name, blob, False


def scan_pbos(pbo_dir, allow):
    errors, warnings = [], []
    paths = list_files(pbo_dir, (".pbo",), "PBO folder")
    if not paths:
        raise InputError("no .pbo files under %s" % pbo_dir)
    for path in paths:
        base = os.path.relpath(path, pbo_dir).replace(os.sep, "/")
        for name, blob, compressed in read_pbo_entries(path):
            rel = base + ":" + name.replace("\\", "/")
            lname = name.lower()
            if not (lname.endswith(TEXT_EXTS) or lname.endswith(".bin")):
                continue
            if is_allowed(name.replace("\\", "/"), allow):
                continue
            if compressed:
                warnings.append({"category": "pbo:compressed", "file": rel, "line": 0,
                                 "match": "", "detail": "compressed entry not scanned"})
                continue
            text = blob.decode("latin-1").replace("\0", "\n")
            e, w = scan_text_content(rel, text, prefix="pbo:")
            errors.extend(e)
            warnings.extend(w)
    return errors, warnings


# --------------------------------------------------------------------------
# Main check
# --------------------------------------------------------------------------

def _symbol_rows(sink, category, ref_root, lines_of):
    rows = []
    for name in sorted(sink):
        entry = sink[name]
        locs = [(rel, lines_of[rel][i]) for rel, i in entry["uses"]]
        if entry["where"]:
            path, line = entry["where"]
            try:
                path = os.path.relpath(path, ref_root)
            except ValueError:
                pass
            where = "declared %s:%d" % (path.replace(os.sep, "/"), line)
        else:
            where = "Expansion-named, not declared in any reference"
        if category == "ambiguous_homonym":
            where = "receiver type not resolved; Expansion " + where
        rows.append({
            "category": category, "file": locs[0][0], "line": locs[0][1], "match": name,
            "detail": "%s, %d use(s), %s" % (entry["kind"], len(locs), where),
            "kind": entry["kind"], "uses": [{"file": f, "line": l} for f, l in locs],
        })
    return rows


def run_check(root, ref_root, vanilla, allow, pbo_dir=None):
    root = os.path.abspath(root)
    if not os.path.isdir(root):
        raise InputError("root is not a directory: %s" % root)
    mod_dirs = [os.path.join(root, d) for d in MOD_DIRS if os.path.isdir(os.path.join(root, d))]
    if not mod_dirs:
        raise InputError("none of %s found under %s" % ("/".join(MOD_DIRS), root))
    exp_dirs = [os.path.join(ref_root, d) for d in EXPANSION_REFS]
    required = [vanilla] + [os.path.join(ref_root, d) for d in REQUIRED_OTHER_REFS]
    for d in exp_dirs + required:
        if not os.path.isdir(d):
            raise InputError("reference source missing: %s" % d)
    optional = [os.path.join(ref_root, d) for d in OTHER_REFS if d not in REQUIRED_OTHER_REFS
                and os.path.isdir(os.path.join(ref_root, d))]

    errors, warnings = [], []
    mod_scripts = []
    for d in mod_dirs:
        for path in list_files(d, TEXT_EXTS, "mod folder"):
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            if path.lower().endswith(SCRIPT_EXTS):
                mod_scripts.append((rel, path))
            if is_allowed(rel, allow):
                continue
            e, w = scan_text_content(rel, read_text(path))
            errors.extend(e)
            warnings.extend(w)

    ref_model = build_reference_model(exp_dirs, required, optional)

    mod_model = Model()
    parsed = []
    for rel, path in mod_scripts:
        toks, lns = tokenize(read_text(path))
        fs = scan(toks, lns, track=True)
        mod_model.add(fs, "mod", path)
        parsed.append((rel, toks, lns, fs))

    resolver = Resolver([ref_model, mod_model])
    sym_errors, sym_warnings = {}, {}
    lines_of = {}
    for rel, toks, lns, fs in parsed:
        lines_of[rel] = lns
        UseAnalyzer(resolver, rel, toks, fs).run(sym_errors, sym_warnings)
    errors = _symbol_rows(sym_errors, "symbol", ref_root, lines_of) + errors
    warnings = warnings + _symbol_rows(sym_warnings, "ambiguous_homonym", ref_root, lines_of)

    if pbo_dir is not None:
        if not os.path.isdir(pbo_dir):
            raise InputError("--pbo-dir is not a directory: %s" % pbo_dir)
        e, w = scan_pbos(pbo_dir, allow)
        errors.extend(e)
        warnings.extend(w)

    exp_count = sum(1 for g in ref_model.globals.values() if "exp" in g) + \
        sum(1 for g in ref_model.member_any.values() if "exp" in g)
    return {
        "root": root,
        "errors": errors,
        "warnings": warnings,
        "stats": {"mod_scripts": len(mod_scripts), "reference_files": ref_model.files,
                  "expansion_declarations": exp_count},
    }


CATEGORY_ORDER = ("symbol", "required_addon", "resource", "text",
                  "pbo:required_addon", "pbo:resource", "pbo:text")


def format_table(report):
    out = []
    errs = report["errors"]
    by_cat = {}
    for f in errs:
        by_cat.setdefault(f["category"], []).append(f)
    cats = [c for c in CATEGORY_ORDER if c in by_cat] + sorted(set(by_cat) - set(CATEGORY_ORDER))
    for cat in cats:
        rows = by_cat[cat]
        out.append("== %s (%d)" % (cat, len(rows)))
        if cat == "symbol":
            w = max(len(r["match"]) for r in rows)
            for r in rows:
                out.append("  %-*s  %s  first use %s:%d" % (w, r["match"], r["detail"], r["file"], r["line"]))
        else:
            for r in rows:
                out.append("  %s:%d  %s  | %s" % (r["file"], r["line"], r["match"], r["detail"]))
    if report["warnings"]:
        out.append("== warnings (%d)" % len(report["warnings"]))
        for r in report["warnings"]:
            if r["category"] == "ambiguous_homonym":
                out.append("  [%s] %s  %s  first use %s:%d" % (r["category"], r["match"], r["detail"],
                                                               r["file"], r["line"]))
            else:
                out.append("  [%s] %s:%d  %s" % (r["category"], r["file"], r["line"], r["detail"]))
    strict = report.get("strict")
    unreviewed = report.get("unreviewed", [])
    if strict:
        out.append("== strict: unreviewed ambiguous homonyms (%d)" % len(unreviewed))
        if unreviewed:
            out.append("  Review each use below; if it is not an Expansion dependency, add the line")
            out.append("  with a reason to %s:" % report["reviewed_file"])
            for u in unreviewed:
                out.append("    %s:%d:%s  # <reason>" % (u["file"], u["line"], u["name"]))
        for u in report.get("stale_reviewed", []):
            out.append("  (stale reviewed entry, no matching warning) %s" % u)
    counts = ", ".join("%s=%d" % (c, len(by_cat[c])) for c in cats) or "none"
    s = report["stats"]
    if errs:
        verdict = "DEPENDS ON EXPANSION"
    elif strict and unreviewed:
        verdict = "UNREVIEWED HOMONYMS"
    else:
        verdict = "CLEAN"
    out.append("")
    out.append("Result: %s - %d error(s) [%s], %d warning(s)%s; %d mod scripts, %d reference files, %.1f s"
               % (verdict, len(errs), counts, len(report["warnings"]),
                  (", strict: %d unreviewed homonym use(s)" % len(unreviewed)) if strict else "",
                  s["mod_scripts"], s["reference_files"], s.get("seconds", 0.0)))
    return "\n".join(out)


def load_reviewed(path):
    """Parse the reviewed-homonym allowlist: `path:line:name  # reason` per line."""
    entries = {}
    try:
        with open(path, encoding="utf-8") as fh:
            raw = fh.read().splitlines()
    except OSError as exc:
        raise InputError("cannot read reviewed file %s: %s" % (path, exc))
    for no, line in enumerate(raw, 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        entry, sep, reason = text.partition("#")
        parts = entry.strip().rsplit(":", 2)
        if not sep or not reason.strip() or len(parts) != 3 or not parts[1].isdigit() or not parts[2]:
            raise InputError("%s:%d: expected 'path:line:name  # reason', got %r" % (path, no, line))
        entries[(parts[0].strip(), int(parts[1]), parts[2].strip())] = reason.strip()
    return entries


def apply_strict(report, reviewed, reviewed_file):
    unreviewed = []
    seen = set()
    for w in report["warnings"]:
        if w["category"] != "ambiguous_homonym":
            continue
        for u in w["uses"]:
            key = (u["file"], u["line"], w["match"])
            if key in seen:
                continue
            seen.add(key)
            if key not in reviewed:
                unreviewed.append({"file": u["file"], "line": u["line"], "name": w["match"]})
    report["strict"] = True
    report["reviewed_file"] = reviewed_file
    report["unreviewed"] = unreviewed
    report["stale_reviewed"] = ["%s:%d:%s" % k for k in sorted(set(reviewed) - seen)]


def main(argv=None):
    ap = argparse.ArgumentParser(description="Report BattleRoyale dependencies on DayZ Expansion.")
    ap.add_argument("root", help="repository root containing BattleRoyale/, BattleRoyale_VPP/, BattleRoyale_server/")
    ap.add_argument("--json", action="store_true", help="print the report as JSON")
    ap.add_argument("--ref-root", default=DEFAULT_REF_ROOT,
                    help="folder with exp_core_scripts, expansion_scripts, navigation_scripts, cf_scripts, dabs_scripts (cot, vpp optional)")
    ap.add_argument("--vanilla", default=DEFAULT_VANILLA, help="vanilla scripts folder")
    ap.add_argument("--allow", action="append", default=None, metavar="GLOB",
                    help="path glob excluded from text checks (repeatable; default: %s)" % ", ".join(DEFAULT_ALLOW))
    ap.add_argument("--pbo-dir", default=None, help="also scan built .pbo files under this folder")
    ap.add_argument("--strict", action="store_true",
                    help="gate mode: any ambiguous_homonym use not in the reviewed file fails (exit 1)")
    ap.add_argument("--reviewed", default=DEFAULT_REVIEWED,
                    help="reviewed-homonym allowlist for --strict (default: %(default)s)")
    try:
        args = ap.parse_args(argv)
    except SystemExit as exc:
        return 0 if exc.code == 0 else 2
    allow = tuple(args.allow) if args.allow is not None else DEFAULT_ALLOW
    t0 = time.monotonic()
    try:
        reviewed = load_reviewed(args.reviewed) if args.strict else {}
        report = run_check(args.root, args.ref_root, args.vanilla, allow, args.pbo_dir)
    except (InputError, OSError) as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2
    report["stats"]["seconds"] = round(time.monotonic() - t0, 2)
    if args.strict:
        apply_strict(report, reviewed, args.reviewed)
    code = 1 if report["errors"] or report.get("unreviewed") else 0
    report["exit_code"] = code
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(format_table(report))
    return code


if __name__ == "__main__":
    sys.exit(main())
