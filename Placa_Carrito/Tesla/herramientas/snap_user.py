# uso: snap_user.py <pcb_del_usuario> <salida.json> [pcb_base_generado]
# Guarda posiciones y silk del usuario; con una base, calcula que piezas y referencias movio a mano.
import pcbnew, json, sys, math, os
src, out = sys.argv[1], sys.argv[2]; base = sys.argv[3] if len(sys.argv) > 3 else None
b = pcbnew.LoadBoard(src)
def tx(t):
    return dict(text=t.GetText(), layer=t.GetLayerName(), x=t.GetX(), y=t.GetY(), ang=t.GetTextAngleDegrees(),
                w=t.GetTextWidth(), h=t.GetTextHeight(), th=t.GetTextThickness(), bold=t.IsBold(), ital=t.IsItalic(),
                mir=t.IsMirrored(), vis=t.IsVisible(), hj=int(t.GetHorizJustify()), vj=int(t.GetVertJustify()), up=t.IsKeepUpright())
d = {"fp": {}, "texts": [], "locked": [], "handref": [], "rects": []}
if os.path.exists(out):
    old = json.load(open(out)); d["locked"] = old.get("locked", []); d["handref"] = old.get("handref", [])
for f in b.GetFootprints():
    d["fp"][f.GetReference()] = dict(x=f.GetX(), y=f.GetY(), rot=f.GetOrientationDegrees(), layer=f.GetLayerName(), ref=tx(f.Reference()))
for t in b.GetDrawings():
    if t.GetClass() == "PCB_TEXT": d["texts"].append(tx(t))
    elif t.GetClass() == "PCB_SHAPE" and t.GetShape() == pcbnew.SHAPE_T_RECT and t.GetLayerName() == "F.Silkscreen":
        d["rects"].append([min(t.GetStart().x, t.GetEnd().x), min(t.GetStart().y, t.GetEnd().y), max(t.GetStart().x, t.GetEnd().x), max(t.GetStart().y, t.GetEnd().y)])
if base:
    g = pcbnew.LoadBoard(base)
    for f in g.GetFootprints():
        r = f.GetReference(); u = b.FindFootprintByReference(r)
        if u is None or r.startswith("TP") or r.startswith("MH"): continue
        if math.hypot(f.GetX() - u.GetX(), f.GetY() - u.GetY()) > 5e4 or abs(f.GetOrientationDegrees() - u.GetOrientationDegrees()) > 0.1:
            if r not in d["locked"]: d["locked"].append(r)
        a, c = f.Reference(), u.Reference()
        ka = (a.GetLayerName(), round(a.GetTextAngleDegrees()), a.GetTextHeight(), a.GetX() - f.GetX(), a.GetY() - f.GetY())
        kc = (c.GetLayerName(), round(c.GetTextAngleDegrees()), c.GetTextHeight(), c.GetX() - u.GetX(), c.GetY() - u.GetY())
        if ka[:3] != kc[:3] or math.hypot(ka[3] - kc[3], ka[4] - kc[4]) > 1.5e5:
            if r not in d["handref"]: d["handref"].append(r)
json.dump(d, open(out, "w"), indent=1)
print(len(d["fp"]), len(d["texts"]), "bloqueadas", len(d["locked"]), "refs a mano", len(d["handref"]))
