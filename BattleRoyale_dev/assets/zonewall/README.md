# Zone wall panel

There are two variants with the same geometry: `zonewall` (red, the zone in force) and `zonewall_yellow`
(the announced zone until it locks). The steps below name the red files; the yellow ones are the same
with the `zonewall_yellow` stem.

The repository does not version binary assets (`.gitignore`), so the three files per variant the mod
ships in `BattleRoyale/addons/models_shapes` are rebuilt from this folder:

1. `python make_zonewall.py` writes `zonewall_ca.png`, `zonewall.rvmat` and the MLOD `zonewall.p3d`.
2. `ImageToPAA.exe zonewall_ca.png zonewall_ca.paa` (DayZ Tools, `Bin\ImageToPAA`).
3. Binarize the model. The source must sit under `P:\` at its prefix so binarize resolves the texture
   and marks the section as alpha (`0x2C100` in the ODOL section table):
   - copy `zonewall.p3d` to `P:\Vigrid-BattleRoyale\Models\Shapes\` and the `.paa` and `.rvmat` to
     its `data\` folder (a junction to a staging folder is enough);
   - `binarize.exe -always -silent -maxProcesses=0 -norecurse -addon=P:\Vigrid-BattleRoyale\Models\Shapes P:\Vigrid-BattleRoyale\Models\Shapes <out> *.p3d`
     (without `-maxProcesses=0` it hung for minutes).
4. Copy the binarized `zonewall.p3d` to `BattleRoyale/addons/models_shapes/`, and `zonewall_ca.paa`
   and `zonewall.rvmat` to `BattleRoyale/addons/models_shapes/data/`.

A face texture written as a procedural `#(argb,...)` string binarizes into a section the engine never
draws, which is why the panel uses a real `.paa`.
