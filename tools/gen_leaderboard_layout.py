"""Generate gui/layouts/hud/leaderboard.layout, the leaderboard panel of the HUD.

The panel is a fixed pool of widgets (three podium cards and 27 table rows in two columns) that
BattleRoyaleLeaderboard.c fills; nothing is placed or sized from the script. All the geometry is here,
in exact layout units (1/1080 of the screen height), and the names of the widgets are the contract
with the script.

Usage: python -I tools/gen_leaderboard_layout.py <output.layout> [--sample <players> [--own <rank>]] [--state loading|empty]
--sample writes a filled copy for the offline preview (sample names and numbers, English texts instead of
the stringtable keys); --state writes the loading or the empty state. Neither is a file for the game.
"""
import argparse

FONT = "gui/fonts/etelkatextpro"
TEX = "Vigrid-BattleRoyale/GUI/textures/hud/"

# Panel
W = 1200
PAD = 20
HEADER_H = 72
PODIUM_Y = 88
PODIUM_BOTTOM = 228
TABLE_Y = 244            # column headers
HEAD_H = 26
ROWS_Y = 274
ROW_H = 30
ROW_LINES = 14           # rows per table column
ROW_COUNT = 27           # places 4 to 30
COL_W = 572
COL_X = (PAD, W - PAD - COL_W)
BOTTOM_PAD = 16
PANEL_H = ROWS_Y + ROW_LINES * ROW_H + BOTTOM_PAD
MESSAGE_H = 224

# Podium cards: second, first and third place from left to right, bottoms level
CARD_W = 376
CARD_GAP = 16
CARDS = (
    # place index, x, height, numeral box height, name box height
    (0, PAD + CARD_W + CARD_GAP, 140, 84, 38),
    (1, PAD, 124, 72, 32),
    (2, PAD + 2 * (CARD_W + CARD_GAP), 112, 64, 32),
)
CARD_TEXT_X = 88
STAT_W = 92

# Table columns inside a 572 wide row: x, width
C_RANK = (8, 40)
C_NAME = (60, 232)
C_POINTS = (300, 84)
C_KILLS = (388, 80)
C_WINS = (468, 92)

# Colours
GOLD = (1, 0.77, 0)                 # the gold of the kill feed rows that name the local player
METAL = ((1, 0.77, 0), (0.80, 0.83, 0.88), (0.86, 0.55, 0.30))                 # gold, silver, bronze
CARD_BACK = ((0.17, 0.12, 0.02), (0.11, 0.12, 0.14), (0.16, 0.09, 0.04))       # dark tone of each metal
BACKDROP = (0.03, 0.035, 0.045, 0.74)
ZEBRA = ((0, 0, 0, 0.22), (0, 0, 0, 0.40))
WHITE = (1, 1, 1, 1)
SOFT = (0.86, 0.86, 0.86, 1)        # secondary numbers, period
DIM = (0.62, 0.62, 0.62, 1)         # labels, places, hints

SAMPLE_NAMES = (
    "Survivor_Kuro", "NWAF_Camper", "Willy92", "ElGatoConBotas", "xX_Zelenogorsk_Xx", "Marta", "TisyRunner",
    "OldManBerezino", "LongNameThatDoesNotFitAnywhere", "Pavlovo", "Chernogorsk_Sniper", "Bean", "Dr. Kuru",
    "Vybor_Vagabond", "Novaya", "Ghillie", "SkalistyBoat", "Altar", "Devils_Castle", "Green_Mountain",
    "Stary_Sobor", "Gorka", "Polana", "Krasnostav", "Svetlo", "Elektro_Legend", "Kamyshovo", "Solnichniy",
    "Nizhnoye", "Tulga",
)
ENGLISH = {
    "#STR_BR_LEADERBOARD_TITLE": "LEADERBOARD",
    "#STR_BR_LEADERBOARD_COL_PLAYER": "PLAYER",
    "#STR_BR_LEADERBOARD_COL_POINTS": "POINTS",
    "#STR_BR_LEADERBOARD_COL_KILLS": "KILLS",
    "#STR_BR_LEADERBOARD_COL_WINS": "WINS",
}


class Layout:
    def __init__(self, sample):
        self.lines = []
        self.depth = 0
        self.sample = sample

    def widget(self, cls, name, x, y, w, h, visible=True, color=None, align=(), tail=(), children=False):
        pad = " " * self.depth
        self.lines.append(f"{pad}{cls} {name} {{")
        attrs = []
        if not visible:
            attrs.append("visible 0")
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

    def frame(self, name, x, y, w, h, visible=True, align=()):
        self.widget("FrameWidgetClass", name, x, y, w, h, visible, align=align, children=True)

    def panel(self, name, x, y, w, h, color, visible=True, align=()):
        self.widget("PanelWidgetClass", name, x, y, w, h, visible, color, align, ("style rover_sim_colorable",))

    def image(self, name, x, y, w, h, image, color, visible=True):
        self.widget("ImageWidgetClass", name, x, y, w, h, visible, color, (), (
            f'image0 "{image}"', "mode blend", '"src alpha" 1', '"no wrap" 1', "stretch 1"))

    def text(self, name, x, y, w, h, color=WHITE, bold=True, halign="left", text="", visible=True, align=()):
        tail = []
        if text:
            if self.sample:
                text = ENGLISH.get(text, text)
            tail.append(f'text "{text}"')
        if bold:
            tail.append("style Bold")
        tail.extend((f'font "{FONT}"', '"exact text" 0', '"outline size" 1', '"outline color" 0 0 0 1',
                     f'"text halign" {halign}', '"text valign" center'))
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


def build(players, own, state):
    sample = players >= 0 or state != ""
    filled = max(players, 0)
    out = Layout(sample)

    # Root: the whole screen, proportional. The panel is centred, a little below the middle: the compass, the phase
    # lines and the first kill feed rows stay clear above it, the vanilla HUD below it
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
    out.frame("LeaderboardPanel", 0, 20, W, PANEL_H, align=centre)

    # Backdrop in pieces, shown by the script for what the panel holds: the top (title and podium), one of the
    # table pieces (as tall as the rows in use) or the short one of the loading and empty states
    show_content = state == "" and (filled > 0 or not sample)
    lines_used = min(ROW_LINES, max(0, filled - 3))
    out.panel("LeaderboardBackTop", 0, 0, W, TABLE_Y, BACKDROP, visible=show_content or not sample)
    for lines in range(1, ROW_LINES + 1):
        height = ROWS_Y - TABLE_Y + lines * ROW_H + BOTTOM_PAD
        out.panel(f"LeaderboardBackTable{lines}", 0, TABLE_Y, W, height, BACKDROP,
                  visible=sample and show_content and lines == lines_used)
    out.panel("LeaderboardBackMessage", 0, 0, W, MESSAGE_H, BACKDROP, visible=sample and not show_content)

    # Title band: darker than the body, a gold sheen that fades to the right, the gold bar of the HUD rows on the
    # left and a gold line under it
    out.panel("LeaderboardHeaderBack", 0, 0, W, HEADER_H, (0, 0, 0, 0.42))
    out.image("LeaderboardHeaderSheen", 0, 0, 520, HEADER_H, TEX + "br_lb_fade_h_ca.paa", rgba(GOLD, 0.28))
    out.panel("LeaderboardHeaderBar", 0, 0, 5, HEADER_H, rgba(GOLD))
    out.panel("LeaderboardHeaderLine", 0, HEADER_H, W, 2, rgba(GOLD, 0.85))
    out.text("LeaderboardTitle", 24, 11, 600, 50, text="#STR_BR_LEADERBOARD_TITLE")
    period = ""
    status = ""
    if sample:
        period = "Monthly season: 2026-11"
        status = "T: close"
    right = ("halign right_ref",)
    out.text("LeaderboardPeriod", 24, 13, 540, 26, SOFT, halign="right", text=period, align=right)
    out.text("LeaderboardStatus", 24, 41, 540, 22, DIM, bold=False, halign="right", text=status, align=right)

    # Podium
    out.frame("LeaderboardPodium", 0, 0, W, PODIUM_BOTTOM, visible=show_content or not sample)
    for place, x, height, rank_h, name_h in CARDS:
        metal = METAL[place]
        card = f"LeaderboardCard{place}"
        shown = sample and place < filled
        out.frame(card, x, PODIUM_BOTTOM - height, CARD_W, height, visible=shown)
        out.panel(card + "Back", 0, 0, CARD_W, height, rgba(CARD_BACK[place], 0.92))
        out.image(card + "Glow", 0, 4, CARD_W, height - 24, TEX + "br_lb_fade_v_ca.paa", rgba(metal, 0.30))
        out.panel(card + "Bar", 0, 0, CARD_W, 4, rgba(metal))
        out.text(card + "Rank", 14, (height - rank_h) / 2 + 2, 64, rank_h, rgba(metal), halign="center",
                 text=str(place + 1))
        name_w = CARD_W - CARD_TEXT_X - 16
        if place == 0:
            name_w -= 46
            out.image(card + "Crown", CARD_W - 16 - 38, 14, 38, 38, TEX + "br_lb_crown_ca.paa", rgba(metal))
        row = sample_row(place) if shown else ("", "", "", "")
        out.text(card + "Name", CARD_TEXT_X, 14, name_w, name_h, text=str(row[0]))
        stats = (("Points", "#STR_BR_LEADERBOARD_COL_POINTS", rgba(metal)),
                 ("Kills", "#STR_BR_LEADERBOARD_COL_KILLS", WHITE),
                 ("Wins", "#STR_BR_LEADERBOARD_COL_WINS", WHITE))
        for column, (stat, label, colour) in enumerate(stats):
            stat_x = CARD_TEXT_X + column * STAT_W
            out.text(card + stat, stat_x, height - 60, STAT_W - 4, 30, colour, text=str(row[column + 1]))
            out.text(card + stat + "Label", stat_x, height - 30, STAT_W - 4, 20, DIM, bold=False, text=label)
        # Gold outline of the card of the local player
        out.frame(card + "Own", 0, 0, CARD_W, height, visible=shown and own == place + 1)
        out.panel(card + "OwnTop", 0, 0, CARD_W, 3, rgba(GOLD))
        out.panel(card + "OwnBottom", 0, height - 3, CARD_W, 3, rgba(GOLD))
        out.panel(card + "OwnLeft", 0, 0, 3, height, rgba(GOLD))
        out.panel(card + "OwnRight", CARD_W - 3, 0, 3, height, rgba(GOLD))
        out.end()
        out.end()
    out.end()

    # Table: places 4 to 17 on the left, 18 to 30 on the right
    out.frame("LeaderboardTable", 0, 0, W, PANEL_H, visible=show_content or not sample)
    for column in range(2):
        head = f"LeaderboardHead{column}"
        first = 3 + column * ROW_LINES
        out.frame(head, COL_X[column], TABLE_Y, COL_W, ROWS_Y - TABLE_Y, visible=sample and filled > first)
        out.text(head + "Rank", C_RANK[0], 1, C_RANK[1], 24, DIM, bold=False, halign="center", text="#")
        out.text(head + "Name", C_NAME[0], 1, C_NAME[1], 24, DIM, bold=False, text="#STR_BR_LEADERBOARD_COL_PLAYER")
        out.text(head + "Points", C_POINTS[0], 1, C_POINTS[1], 24, DIM, bold=False, halign="right",
                 text="#STR_BR_LEADERBOARD_COL_POINTS")
        out.text(head + "Kills", C_KILLS[0], 1, C_KILLS[1], 24, DIM, bold=False, halign="right",
                 text="#STR_BR_LEADERBOARD_COL_KILLS")
        out.text(head + "Wins", C_WINS[0], 1, C_WINS[1], 24, DIM, bold=False, halign="right",
                 text="#STR_BR_LEADERBOARD_COL_WINS")
        out.panel(head + "Line", 0, HEAD_H, COL_W, 2, (1, 1, 1, 0.28))
        out.end()
    for index in range(ROW_COUNT):
        row_name = f"LeaderboardRow{index}"
        column = index // ROW_LINES
        line = index % ROW_LINES
        place = index + 4
        shown = sample and place <= filled
        is_own = shown and own == place
        values = sample_row(place - 1) if shown else ("", "", "", "")
        out.frame(row_name, COL_X[column], ROWS_Y + line * ROW_H, COL_W, ROW_H, visible=shown)
        out.panel(row_name + "Back", 0, 0, COL_W, ROW_H, rgba(GOLD, 0.26) if is_own else ZEBRA[line % 2])
        out.panel(row_name + "Own", 0, 0, 4, ROW_H, rgba(GOLD), visible=is_own)
        out.text(row_name + "Rank", C_RANK[0], 1, C_RANK[1], 28, DIM, bold=False, halign="center", text=str(place))
        out.text(row_name + "Name", C_NAME[0], 1, C_NAME[1], 28, rgba(GOLD) if is_own else WHITE, text=str(values[0]))
        out.text(row_name + "Points", C_POINTS[0], 1, C_POINTS[1], 28, halign="right", text=str(values[1]))
        out.text(row_name + "Kills", C_KILLS[0], 1, C_KILLS[1], 28, SOFT, bold=False, halign="right", text=str(values[2]))
        out.text(row_name + "Wins", C_WINS[0], 1, C_WINS[1], 28, SOFT, bold=False, halign="right", text=str(values[3]))
        out.end()
    out.end()

    # Loading and empty states: a line and, under it, a hint
    message = ""
    hint = ""
    if state == "loading":
        message = "Loading the leaderboard..."
    elif sample and not show_content:
        message = "Nobody has scored yet this season"
        hint = "Play a match to get on the board."
    out.frame("LeaderboardMessage", 0, HEADER_H + 2, W, MESSAGE_H - HEADER_H - 2, visible=sample and not show_content)
    out.text("LeaderboardMessageTitle", PAD, 40, W - 2 * PAD, 36, halign="center", text=message)
    out.text("LeaderboardMessageHint", PAD, 84, W - 2 * PAD, 24, DIM, bold=False, halign="center", text=hint)
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
    args = parser.parse_args()
    text = build(args.sample, args.own, args.state)
    if text.count("{") != text.count("}"):
        raise SystemExit("unbalanced braces")
    with open(args.output, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)
    print("wrote", args.output, text.count("WidgetClass "), "widgets")


if __name__ == "__main__":
    main()
