"""Build the zone wall panel: a double-sided 20 x 15 m vertical quad, origin at the bottom centre.

Writes, next to this script:
  zonewall_ca.png   red texture, opaque at the bottom and fading out upwards (convert to .paa)
  zonewall.rvmat    emissive red, Super shader with noZwrite like vanilla red glass
  zonewall.p3d      MLOD, one visual LOD, no collision, no shadow (binarize before shipping)
"""
import os
import py3d
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PREFIX = r"Vigrid-BattleRoyale\Models\Shapes"
WIDTH, HEIGHT = 20.0, 15.0

# Texture: rows top to bottom, alpha grows towards the ground
tw, th = 64, 256
img = Image.new("RGBA", (tw, th))
for y in range(th):
    t = y / (th - 1)  # 0 top, 1 bottom
    alpha = int(255 * (0.04 + 0.66 * t ** 1.6))
    for x in range(tw):
        img.putpixel((x, y), (235, 45, 35, alpha))
img.save(os.path.join(HERE, "zonewall_ca.png"))

RVMAT = r"""ambient[]={1,0.15,0.1,1};
diffuse[]={1,0.15,0.1,1};
forcedDiffuse[]={0,0,0,0};
emmisive[]={1.2,0.12,0.08,1};
specular[]={0,0,0,0};
specularPower=1;
PixelShaderID="Super";
VertexShaderID="Super";
renderFlags[]={"noZwrite"};
class Stage1 { texture="#(argb,8,8,3)color(0.5,0.5,1,1,NOHQ)"; uvSource="tex"; class uvTransform { aside[]={1,0,0}; up[]={0,1,0}; dir[]={0,0,0}; pos[]={0,0,0}; }; };
class Stage2 { texture="#(argb,8,8,3)color(0.5,0.5,0.5,0.5,DT)"; uvSource="tex"; class uvTransform { aside[]={1,0,0}; up[]={0,1,0}; dir[]={0,0,0}; pos[]={0,0,0}; }; };
class Stage3 { texture="#(argb,8,8,3)color(0,0,0,0,MC)"; uvSource="tex"; class uvTransform { aside[]={1,0,0}; up[]={0,1,0}; dir[]={0,0,0}; pos[]={0,0,0}; }; };
class Stage4 { texture="#(argb,8,8,3)color(1,1,1,1,AS)"; uvSource="tex"; class uvTransform { aside[]={1,0,0}; up[]={0,1,0}; dir[]={0,0,0}; pos[]={0,0,0}; }; };
class Stage5 { texture="#(argb,8,8,3)color(0,0,1,1,SMDI)"; uvSource="tex"; class uvTransform { aside[]={1,0,0}; up[]={0,1,0}; dir[]={0,0,0}; pos[]={0,0,0}; }; };
class Stage6 { texture="#(ai,32,128,1)fresnel(1,0)"; uvSource="tex"; class uvTransform { aside[]={1,0,0}; up[]={0,1,0}; dir[]={0,0,0}; pos[]={0,0,0}; }; };
class Stage7 { texture="dz\data\data\env_co.paa"; uvSource="tex"; class uvTransform { aside[]={1,0,0}; up[]={0,1,0}; dir[]={0,0,0}; pos[]={0,0,0}; }; };
"""
with open(os.path.join(HERE, "zonewall.rvmat"), "w", newline="\r\n") as f:
    f.write(RVMAT)

model = py3d.P3D()
lod = py3d.LOD()
lod.resolution = 0.0
hw = WIDTH / 2
corners = [(-hw, 0.0, 0.0), (hw, 0.0, 0.0), (hw, HEIGHT, 0.0), (-hw, HEIGHT, 0.0)]
uvs = [(0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)]
for c in corners:
    p = py3d.Point()
    p.coords = c
    p.flags = 0
    p.mass = None
    lod.points.append(p)
lod.facenormals.append((0.0, 0.0, 1.0))   # index 0: face seen from -Z
lod.facenormals.append((0.0, 0.0, -1.0))  # index 1: face seen from +Z

# Both sides: the vertex-order cross product points away from the side that sees the face
for order, normal in (((0, 1, 2, 3), 0), ((0, 3, 2, 1), 1)):
    face = py3d.Face(lod.points, lod.facenormals)
    face.flags = 0
    face.texture = PREFIX + r"\data\zonewall_ca.paa"
    face.material = PREFIX + r"\data\zonewall.rvmat"
    for i in order:
        v = py3d.Vertex(lod.points, lod.facenormals)
        v.point_index = i
        v.normal_index = normal
        v.uv = uvs[i]
        face.vertices.append(v)
    lod.faces.append(face)

sel = py3d.Selection(lod.points, lod.faces)
for p in lod.points:
    sel.points[p] = 1
for fc in lod.faces:
    sel.faces[fc] = 1
lod.selections["wall"] = sel
lod.properties["lodnoshadow"] = "1"
model.lods.append(lod)

out = os.path.join(HERE, "zonewall.p3d")
with open(out, "wb") as f:
    model.write(f)
with open(out, "rb") as f:
    back = py3d.P3D(f)
print("written", out, "lods", len(back.lods), "points", len(back.lods[0].points), "faces", len(back.lods[0].faces))
