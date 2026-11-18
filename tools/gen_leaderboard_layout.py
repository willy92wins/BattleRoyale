"""Generate gui/layouts/hud/leaderboard.layout, the leaderboard panel of the HUD.

The panel is a fixed pool of widgets (three podium cards and the table rows: two columns of 14 and 13,
and a centred column of 5 for a short table) that BattleRoyaleLeaderboard.c fills; nothing is placed or
sized from the script. All the geometry is here, in exact layout units (1/1080 of the screen height),
and the names of the widgets are the contract with the script.

Usage: python -I tools/gen_leaderboard_layout.py <output.layout> [--sample <players> [--own <rank>]]
       [--state loading|empty] [--lang en|es] [--self <rank>] [--period <text>] [--status <text>]
--self writes the footer: the place of the local player outside the top 30, or 0 for one without points.
--sample writes a filled copy for the offline preview (sample names and numbers, the texts of --lang
instead of the stringtable keys); --state writes the loading or the empty state. Neither is a file for
the game.
"""
import argparse

FONT = "gui/fonts/etelkatextpro"
TEX = "Vigrid-BattleRoyale/GUI/textures/hud/"

# Panel
W = 1240
PANEL_DOWN = 16          # below the middle of the screen: the top is at 215, under the fourth kill feed row
PAD = 20
HEADER_H = 72
PODIUM_BOTTOM = 228
TABLE_Y = 244            # column headers
HEAD_H = 26
ROWS_Y = 274
ROW_H = 28               # the texts of a row are as tall as the row
ROW_LINES = 14           # rows of the left table column; the right one has one less
SIDE_ROWS = 27           # rows of both columns: places 4 to 30
CENTRE_LINES = 5         # rows of the centred column, used alone for a table this short
COL_W = 592
COL_X = (PAD, W - PAD - COL_W, (W - COL_W) / 2)      # left, right, centred
BOTTOM_PAD = 16
PANEL_H = ROWS_Y + ROW_LINES * ROW_H + BOTTOM_PAD
MESSAGE_H = 224
BACK_COUNT = ROW_LINES + 2  # backdrops: loading/empty, podium alone, podium and 1 to 14 lines of rows
FOOTER_H = 44            # the place of the local player outside the top 30, under the backdrop: with the tallest
                         # one the panel ends at 941 of 1080, above the vanilla quickbar

# Podium cards: second, first and third place from left to right, bottoms level
CARD_W = 388
CARD_GAP = 18
CARDS = (
    # place index, x, height, numeral box height, name box height
    (0, PAD + CARD_W + CARD_GAP, 140, 84, 38),
    (1, PAD, 124, 72, 32),
    (2, PAD + 2 * (CARD_W + CARD_GAP), 112, 64, 32),
)
CARD_TEXT_X = 88
STAT_W = 96

# Table columns inside a 592 wide row: x, width. Measured in game at 1080p, the widest header (VICTORIAS, in a
# 24 high box) is 95 wide: the headers are in 22 high boxes and every column is wider than its widest header
C_RANK = (8, 40)
C_NAME = (60, 216)
C_POINTS = (284, 88)
C_KILLS = (380, 84)
C_WINS = (472, 108)
HEAD_TEXT_H = 22

# Palette. The engine may not draw an alpha under about 0.19 (0.07 measured invisible, 0.15 visible), so a
# faint light over the panel is a tone a little lighter than the background at an alpha it does draw, never
# white at 0.05
BACKGROUND = (0.06, 0.07, 0.09, 0.91)       # the glass of the panel: cool charcoal, dark enough over snow and sky
HEADER_SHADE = (0, 0, 0, 0.26)              # the title band, a little darker than the body
CARD = (0.115, 0.127, 0.155, 0.96)          # podium cards: the same family, a little lighter than the background
EDGE = (0.21, 0.225, 0.26, 0.90)            # 1 unit inner edge of the panel
LINE = (0.24, 0.255, 0.29, 0.95)            # line under the column headers
ZEBRA = (0.30, 0.32, 0.36, 0.20)            # every other row: lightens the background by about 0.045
TEXT = (0.95, 0.95, 0.95, 1)                # primary text
TEXT_STRONG = (1, 1, 1, 1)                  # the points of a row, its most prominent number
TEXT_VALUE = (0.72, 0.735, 0.76, 1)         # secondary numbers: kills and wins of a row
TEXT_MUTED = (0.58, 0.60, 0.635, 1)         # labels, headers, places, hints
GOLD = (1, 0.77, 0)                         # the gold of the kill feed rows that name the local player
SILVER = (0.78, 0.81, 0.86)
BRONZE = (0.82, 0.53, 0.30)
METAL = (GOLD, SILVER, BRONZE)              # accent of each podium place: line, glow, numeral and points
GLOW_ALPHA = 0.22                           # of the glow at its top; the texture fades it to nothing
OWN_TINT = (0.56, 0.43, 0.02, 0.22)         # the row or card of the local player: gold at about 0.12
OWN_NAME = (1, 0.93, 0.72, 1)               # its name: warm white
OWN_BAR_W = 3

SAMPLE_NAMES = (
    "Survivor_Kuro", "NWAF_Camper", "Willy92", "ElGatoConBotas", "xX_Zelenogorsk_Xx", "Marta", "TisyRunner",
    "OldManBerezino", "LongNameThatDoesNotFitAnywhere", "Pavlovo", "Chernogorsk_Sniper", "Bean", "Dr. Kuru",
    "Vybor_Vagabond", "Novaya", "Ghillie", "SkalistyBoat", "Altar", "Devils_Castle", "Green_Mountain",
    "Stary_Sobor", "Gorka", "Polana", "Krasnostav", "Svetlo", "Elektro_Legend", "Kamyshovo", "Solnichniy",
    "Nizhnoye", "Tulga",
)
SAMPLE_SELF = ("Dev", "12", "3", "0")    # the local player of the footer: name, points, kills, wins
TEXTS = {
    "en": {
        "#STR_BR_LEADERBOARD_TITLE": "LEADERBOARD",
        "#STR_BR_LEADERBOARD_COL_PLAYER": "PLAYER",
        "#STR_BR_LEADERBOARD_COL_POINTS": "POINTS",
        "#STR_BR_LEADERBOARD_COL_KILLS": "KILLS",
        "#STR_BR_LEADERBOARD_COL_WINS": "WINS",
        "#STR_BR_LEADERBOARD_SELF_LABEL": "YOUR PLACE",
        "#STR_BR_LEADERBOARD_SELF_NONE": "You have not scored yet this season",
        "period": "November 2026 season",
        "status": "Ends in 12 days - T: close",
        "loading": "Loading the leaderboard...",
        "empty": "Nobody has scored yet this season",
        "empty_hint": "Play a match to get on the board.",
    },
    "es": {
        "#STR_BR_LEADERBOARD_TITLE": "RANKING",
        "#STR_BR_LEADERBOARD_COL_PLAYER": "JUGADOR",
        "#STR_BR_LEADERBOARD_COL_POINTS": "PUNTOS",
        "#STR_BR_LEADERBOARD_COL_KILLS": "BAJAS",
        "#STR_BR_LEADERBOARD_COL_WINS": "VICTORIAS",
        "#STR_BR_LEADERBOARD_SELF_LABEL": "TU PUESTO",
        "#STR_BR_LEADERBOARD_SELF_NONE": "A\u00fan no has puntuado esta temporada",
        "period": "Temporada de noviembre de 2026",
        "status": "Termina en 12 d\u00edas - T: cerrar",
        "loading": "Cargando la clasificación...",
        "empty": "Nadie ha puntuado todavía esta temporada",
        "empty_hint": "Juega una partida para entrar en la clasificación.",
    },
}


class Layout:
    def __init__(self, texts):
        self.lines = []
        self.depth = 0
        self.texts = texts      # None for the layout of the game

    def widget(self, cls, name, x, y, w, h, visible=True, color=None, align=(), tail=(), children=False, inherit=True):
        pad = " " * self.depth
        self.lines.append(f"{pad}{cls} {name} {{")
        attrs = []
        if not visible:
            attrs.append("visible 0")
        # The script fades the panel in with the alpha of LeaderboardPanel: every widget under it takes it
        attrs.append("inheritalpha 1" if inherit else "inheritalpha 0")
        attrs.append("ignorepointer 1")
        if color:
            attrs.append("color " + " ".join(num(c) for c in color))
        attrs.append(f"position {num(x)} {num(y)}")
        attrs.append(f"size {num(w)} {num(h)}")
        attrs.extend(align)
        attrs.extend(("hexactpos 1", "vexactpos 1", "hexactsize 1", "vexactsize 1"))
        attrs.extend(tail)
        for attr in attrs:
            self.lines.append(f"{pad} {attr}")
        if children:
            self.lines.append(f"{pad} {{")
            self.depth += 2
        else:
            self.lines.append(f"{pad}}}")

    def end(self):
        self.depth -= 2
        pad = " " * self.depth
        self.lines.append(f"{pad} }}")
        self.lines.append(f"{pad}}}")

    def frame(self, name, x, y, w, h, visible=True, align=(), inherit=True):
        self.widget("FrameWidgetClass", name, x, y, w, h, visible, align=align, children=True, inherit=inherit)

    def panel(self, name, x, y, w, h, color, visible=True, align=()):
        self.widget("PanelWidgetClass", name, x, y, w, h, visible, color, align, ("style rover_sim_colorable",))

    def image(self, name, x, y, w, h, image, color, visible=True):
        self.widget("ImageWidgetClass", name, x, y, w, h, visible, color, (), (
            f'image0 "{image}"', "mode blend", '"src alpha" 1', '"no wrap" 1', "stretch 1"))

    def text(self, name, x, y, w, h, color=TEXT, bold=True, halign="left", text="", visible=True, align=()):
        tail = []
        if text:
            if self.texts:
                text = self.texts.get(text, text)
            tail.append(f'text "{text}"')
        if bold:
            tail.append("style Bold")
        # No outline: every text is over the dark panel, where an outline only makes the glyphs harsh
        tail.extend((f'font "{FONT}"', '"exact text" 0', f'"text halign" {halign}', '"text valign" center'))
        self.widget("TextWidgetClass", name, x, y, w, h, visible, color, align, tail)


def num(value):
    text = f"{value:.3f}".rstrip("0").rstrip(".")
    return text or "0"


def rgba(rgb, alpha=1):
    return (rgb[0], rgb[1], rgb[2], alpha)


def sample_row(index):
    """Name, points, kills and wins of the sample player in place index + 1."""
    points = int(round(418 * 0.9 ** index)) + 2
    return SAMPLE_NAMES[index], points, points // 6 + (index * 5) % 4, max(0, 9 - index) // 2 + (index == 0)


def table_split(players):
    """Rows of the left, right and centred columns for this many players: the script uses the same rule."""
    rows = max(0, players - 3)
    if rows <= CENTRE_LINES:
        return 0, 0, rows
    left = (rows + 1) // 2
    return left, rows - left, 0


def build(players, own, state, lang, self_rank=-1, period_text="", status_text=""):
    sample = players >= 0 or state != ""
    filled = max(players, 0)
    texts = TEXTS[lang] if sample else None
    out = Layout(texts)

    # Root: the whole screen, proportional. The panel is centred, a little below the middle: the compass, the phase
    # lines and the first four kill feed rows stay clear above it, the vanilla HUD below it
    out.lines += [
        "FrameWidgetClass LeaderboardRoot {",
        " visible 0" if not sample else " visible 1",
        " ignorepointer 1",
        " position 0 0",
        " size 1 1",
        " hexactpos 0",
        " vexactpos 0",
        " hexactsize 0",
        " vexactsize 0",
        " {",
    ]
    out.depth = 2
    centre = ("halign center_ref", "valign center_ref")
    out.frame("LeaderboardPanel", 0, PANEL_DOWN, W, PANEL_H, align=centre, inherit=False)

    # Backdrop: one piece for each height the panel can have, the glass with a 1 unit inner edge. The script shows
    # one: 0 the loading and empty states, 1 the podium alone, 1 + n the podium and n lines of table rows
    show_content = state == "" and (filled > 0 or not sample)
    left_rows, right_rows, centre_rows = table_split(filled)
    lines_used = max(left_rows, centre_rows)
    back_used = 1 + lines_used if show_content else 0
    for index in range(BACK_COUNT):
        if index == 0:
            height = MESSAGE_H
        elif index == 1:
            height = TABLE_Y
        else:
            height = ROWS_Y + (index - 1) * ROW_H + BOTTOM_PAD
        back = f"LeaderboardBack{index}"
        out.frame(back, 0, 0, W, height, visible=sample and index == back_used)
        out.panel(back + "Glass", 0, 0, W, height, BACKGROUND)
        out.panel(back + "Top", 0, 0, W, 1, EDGE)
        out.panel(back + "Bottom", 0, height - 1, W, 1, EDGE)
        out.panel(back + "Left", 0, 1, 1, height - 2, EDGE)
        out.panel(back + "Right", W - 1, 1, 1, height - 2, EDGE)
        out.end()

    # Title band: a little darker than the body, and under it the one accent of the panel, a thin gold line that
    # fades out to the right
    out.panel("LeaderboardHeaderBack", 1, 1, W - 2, HEADER_H - 1, HEADER_SHADE)
    out.image("LeaderboardHeaderLine", 1, HEADER_H, W - 2, 2, TEX + "br_lb_fade_h_ca.paa", rgba(GOLD))
    out.text("LeaderboardTitle", 24, 11, 600, 50, text="#STR_BR_LEADERBOARD_TITLE")
    period = "period" if sample else ""
    status = "status" if sample else ""
    if sample and period_text:
        period = period_text
    if sample and status_text:
        status = status_text
    right = ("halign right_ref",)
    out.text("LeaderboardPeriod", 24, 13, 640, 26, TEXT, halign="right", text=period, align=right)
    out.text("LeaderboardStatus", 24, 41, 640, 22, TEXT_MUTED, bold=False, halign="right", text=status, align=right)

    # Podium
    out.frame("LeaderboardPodium", 0, 0, W, PODIUM_BOTTOM, visible=show_content or not sample)
    for place, x, height, rank_h, name_h in CARDS:
        metal = METAL[place]
        card = f"LeaderboardCard{place}"
        shown = sample and place < filled
        out.frame(card, x, PODIUM_BOTTOM - height, CARD_W, height, visible=shown)
        # A dark card; the metal is only an accent: a thin line on top, a soft glow under it, the numeral and the points
        out.panel(card + "Back", 0, 0, CARD_W, height, CARD)
        out.image(card + "Glow", 0, 2, CARD_W, height * 0.6, TEX + "br_lb_fade_v_ca.paa", rgba(metal, GLOW_ALPHA))
        out.panel(card + "Line", 0, 0, CARD_W, 2, rgba(metal))
        # The card of the local player: the warm tint and the narrow gold bar of its row in the table
        out.frame(card + "Own", 0, 0, CARD_W, height, visible=shown and own == place + 1)
        out.panel(card + "OwnTint", 0, 2, CARD_W, height - 2, OWN_TINT)
        out.panel(card + "OwnBar", 0, 2, OWN_BAR_W, height - 2, rgba(GOLD))
        out.end()
        out.text(card + "Rank", 14, (height - rank_h) / 2 + 2, 64, rank_h, rgba(metal), halign="center",
                 text=str(place + 1))
        name_w = CARD_W - CARD_TEXT_X - 16
        if place == 0:
            name_w -= 46
            out.image(card + "Crown", CARD_W - 16 - 38, 14, 38, 38, TEX + "br_lb_crown_ca.paa", rgba(metal))
        row = sample_row(place) if shown else ("", "", "", "")
        out.text(card + "Name", CARD_TEXT_X, 14, name_w, name_h, text=str(row[0]))
        stats = (("Points", "#STR_BR_LEADERBOARD_COL_POINTS", rgba(metal)),
                 ("Kills", "#STR_BR_LEADERBOARD_COL_KILLS", TEXT),
                 ("Wins", "#STR_BR_LEADERBOARD_COL_WINS", TEXT))
        for column, (stat, label, colour) in enumerate(stats):
            stat_x = CARD_TEXT_X + column * STAT_W
            out.text(card + stat, stat_x, height - 60, STAT_W - 6, 30, colour, text=str(row[column + 1]))
            out.text(card + stat + "Label", stat_x, height - 30, STAT_W - 6, 20, TEXT_MUTED, bold=False, text=label)
        out.end()
    out.end()

    # Table. The script splits the rows evenly between the left and the right column (the left one takes the
    # odd row), or puts them all in the centred column when they are CENTRE_LINES or fewer. Rows 0-13 are the
    # left column, 14-26 the right one, 27-31 the centred one; the place of a row is written by the script
    out.frame("LeaderboardTable", 0, 0, W, PANEL_H, visible=show_content or not sample)
    used = (left_rows, right_rows, centre_rows)
    for column in range(3):
        head = f"LeaderboardHead{column}"
        out.frame(head, COL_X[column], TABLE_Y, COL_W, ROWS_Y - TABLE_Y, visible=sample and used[column] > 0)
        head_y = (HEAD_H - HEAD_TEXT_H) / 2
        out.text(head + "Name", C_NAME[0], head_y, C_NAME[1], HEAD_TEXT_H, TEXT_MUTED, bold=False,
                 text="#STR_BR_LEADERBOARD_COL_PLAYER")
        out.text(head + "Points", C_POINTS[0], head_y, C_POINTS[1], HEAD_TEXT_H, TEXT_MUTED, bold=False, halign="right",
                 text="#STR_BR_LEADERBOARD_COL_POINTS")
        out.text(head + "Kills", C_KILLS[0], head_y, C_KILLS[1], HEAD_TEXT_H, TEXT_MUTED, bold=False, halign="right",
                 text="#STR_BR_LEADERBOARD_COL_KILLS")
        out.text(head + "Wins", C_WINS[0], head_y, C_WINS[1], HEAD_TEXT_H, TEXT_MUTED, bold=False, halign="right",
                 text="#STR_BR_LEADERBOARD_COL_WINS")
        out.panel(head + "Line", 0, HEAD_H, COL_W, 2, LINE)
        out.end()
    first_place = (4, 4 + left_rows, 4)
    for index in range(SIDE_ROWS + CENTRE_LINES):
        row_name = f"LeaderboardRow{index}"
        if index < ROW_LINES:
            column, line = 0, index
        elif index < SIDE_ROWS:
            column, line = 1, index - ROW_LINES
        else:
            column, line = 2, index - SIDE_ROWS
        place = first_place[column] + line
        shown = sample and line < used[column]
        is_own = shown and own == place
        values = sample_row(place - 1) if shown else ("", "", "", "")
        out.frame(row_name, COL_X[column], ROWS_Y + line * ROW_H, COL_W, ROW_H, visible=shown)
        # Back: the alternate tone of the odd lines or the tint of the local player; the script shows and colours it
        out.panel(row_name + "Back", 0, 0, COL_W, ROW_H, OWN_TINT if is_own else ZEBRA,
                  visible=is_own or line % 2 == 1)
        out.panel(row_name + "Own", 0, 0, OWN_BAR_W, ROW_H, rgba(GOLD), visible=is_own)
        out.text(row_name + "Rank", C_RANK[0], 0, C_RANK[1], ROW_H, TEXT_MUTED, bold=False, halign="center",
                 text=str(place) if shown else "")
        out.text(row_name + "Name", C_NAME[0], 0, C_NAME[1], ROW_H, OWN_NAME if is_own else TEXT, text=str(values[0]))
        out.text(row_name + "Points", C_POINTS[0], 0, C_POINTS[1], ROW_H, TEXT_STRONG, halign="right", text=str(values[1]))
        out.text(row_name + "Kills", C_KILLS[0], 0, C_KILLS[1], ROW_H, TEXT_VALUE, bold=False, halign="right", text=str(values[2]))
        out.text(row_name + "Wins", C_WINS[0], 0, C_WINS[1], ROW_H, TEXT_VALUE, bold=False, halign="right", text=str(values[3]))
        out.end()
    out.end()

    # Footer: the local player outside the top 30 (place, name and numbers, with the bar and the tint of an own row)
    # or, without points yet, a line that says so. It hangs under the backdrop as one more piece of the same glass,
    # with the bottom edge of the backdrop as the line between them. One footer for each backdrop with a podium
    # (1 to BACK_COUNT - 1), at its height: the script shows the one of the backdrop in use
    for index in range(1, BACK_COUNT):
        if index == 1:
            footer_y = TABLE_Y
        else:
            footer_y = ROWS_Y + (index - 1) * ROW_H + BOTTOM_PAD
        footer = f"LeaderboardFooter{index}"
        footer_shown = sample and show_content and index == back_used and self_rank >= 0
        has_place = self_rank > 0
        out.frame(footer, 0, footer_y, W, FOOTER_H, visible=footer_shown)
        out.panel(footer + "Glass", 0, 0, W, FOOTER_H, BACKGROUND)
        out.panel(footer + "Bottom", 0, FOOTER_H - 1, W, 1, EDGE)
        out.panel(footer + "Left", 0, 0, 1, FOOTER_H - 1, EDGE)
        out.panel(footer + "Right", W - 1, 0, 1, FOOTER_H - 1, EDGE)
        out.frame(footer + "Self", 0, 0, W, FOOTER_H, visible=footer_shown and has_place)
        out.panel(footer + "SelfTint", 1, 0, W - 2, FOOTER_H - 1, OWN_TINT)
        out.panel(footer + "SelfBar", 1, 0, OWN_BAR_W, FOOTER_H - 1, rgba(GOLD))
        fill = footer_shown and has_place
        out.text(footer + "Label", 24, 11, 122, 22, TEXT_MUTED, bold=False, text="#STR_BR_LEADERBOARD_SELF_LABEL")
        out.text(footer + "Rank", 150, 7, 96, 30, TEXT_STRONG, text=str(self_rank) if fill else "")
        out.text(footer + "Name", 256, 8, 440, 28, OWN_NAME, text=SAMPLE_SELF[0] if fill else "")
        # Numbers and their labels from the right edge: wins, kills, points
        x = W - 24 - 108
        out.text(footer + "WinsLabel", x, 11, 108, 22, TEXT_MUTED, bold=False, text="#STR_BR_LEADERBOARD_COL_WINS")
        x -= 8 + 56
        out.text(footer + "Wins", x, 8, 56, 28, TEXT, halign="right", text=SAMPLE_SELF[3] if fill else "")
        x -= 20 + 64
        out.text(footer + "KillsLabel", x, 11, 64, 22, TEXT_MUTED, bold=False, text="#STR_BR_LEADERBOARD_COL_KILLS")
        x -= 8 + 56
        out.text(footer + "Kills", x, 8, 56, 28, TEXT, halign="right", text=SAMPLE_SELF[2] if fill else "")
        x -= 20 + 80
        out.text(footer + "PointsLabel", x, 11, 80, 22, TEXT_MUTED, bold=False, text="#STR_BR_LEADERBOARD_COL_POINTS")
        x -= 8 + 72
        out.text(footer + "Points", x, 8, 72, 28, TEXT_STRONG, halign="right", text=SAMPLE_SELF[1] if fill else "")
        out.end()
        out.text(footer + "None", 24, 9, W - 48, 26, TEXT_MUTED, bold=False, text="#STR_BR_LEADERBOARD_SELF_NONE",
                 visible=footer_shown and not has_place)
        out.end()

    # Loading and empty states: a line and, under it, a hint
    message = ""
    hint = ""
    if state == "loading":
        message = "loading"
    elif sample and not show_content:
        message = "empty"
        hint = "empty_hint"
    out.frame("LeaderboardMessage", 0, HEADER_H + 2, W, MESSAGE_H - HEADER_H - 2, visible=sample and not show_content)
    out.text("LeaderboardMessageTitle", PAD, 40, W - 2 * PAD, 36, halign="center", text=message)
    out.text("LeaderboardMessageHint", PAD, 84, W - 2 * PAD, 24, TEXT_MUTED, bold=False, halign="center", text=hint)
    out.end()

    out.end()
    out.lines += [" }", "}"]
    return "\r\n".join(out.lines) + "\r\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output")
    parser.add_argument("--sample", type=int, default=-1)
    parser.add_argument("--own", type=int, default=0)
    parser.add_argument("--state", default="", choices=("", "loading", "empty"))
    parser.add_argument("--lang", default="en", choices=("en", "es"))
    parser.add_argument("--self", dest="self_rank", type=int, default=-1)
    parser.add_argument("--period", default="")
    parser.add_argument("--status", default="")
    args = parser.parse_args()
    text = build(args.sample, args.own, args.state, args.lang, args.self_rank, args.period, args.status)
    if text.count("{") != text.count("}"):
        raise SystemExit("unbalanced braces")
    with open(args.output, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)
    print("wrote", args.output, text.count("WidgetClass "), "widgets")


if __name__ == "__main__":
    main()
