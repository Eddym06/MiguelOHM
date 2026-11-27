# Genera Tesla.kicad_pcb desde la netlist del esquema (con la API pcbnew de KiCad).
# Uso: "C:/Program Files/KiCad/10.0/bin/python.exe" pcb_build.py
import pcbnew, re, math, os, json, shutil, sys
sys.path.insert(0, "C:/Users/yenni/AppData/Local/Temp/claude/8eb49cfc81ebaa8e/7726eab3-0a2d-45a6-bcd0-fbc7d6abb388/scratchpad")
import partsdb
import xml.etree.ElementTree as ET
MM = 1000000
PRJ = "C:/Users/yenni/Documents/.Github Repositorios/MiguelOHM/Placa_Carrito/Tesla/"
SC = "C:/Users/yenni/AppData/Local/Temp/claude/8eb49cfc81ebaa8e/7726eab3-0a2d-45a6-bcd0-fbc7d6abb388/scratchpad/"
STD = "C:/Program Files/KiCad/10.0/share/kicad/footprints/"
LIBS = {"Tesla_Parts": PRJ + "Tesla_Parts.pretty",
        "Espressif_S3": PRJ + "../Componentes/ESP32-S3/KiCad/Espressif_S3.pretty"}
OX, OY = 100.0, 60.0              # origen de la placa en la hoja (mm)
W, H = 130.0, 170.0               # ancho (X) y largo (Y) de la placa
def P(x, y): return pcbnew.VECTOR2I(int(round((OX + x) * MM)), int(round((OY + y) * MM)))
def mm(v): return int(round(v * MM))

# ---------------- netlist
tree = ET.parse(SC + "nl.xml"); root = tree.getroot()
comps = {}
for c in root.find("components"):
    ref = c.get("ref")
    if ref.startswith("#"): continue
    fp = c.findtext("footprint"); val = c.findtext("value")
    sp = c.find("sheetpath").get("tstamps"); ts = c.findtext("tstamps")
    sheet = c.find("sheetpath").get("names")
    lcsc = ""
    for fld in (c.find("fields") if c.find("fields") is not None else []):
        if fld.get("name") == "LCSC": lcsc = fld.text or ""
    comps[ref] = dict(fp=fp, val=val, path=sp + ts if ts else sp, sheet=sheet, lcsc=lcsc)
nets = {}
for n in root.find("nets"):
    name = n.get("name")
    if name.startswith(("Net-(", "unconnected-(")): name = name.replace("/", "{slash}")
    nets[name] = [(nd.get("ref"), nd.get("pin")) for nd in n if not nd.get("ref").startswith("#")]

def load_fp(fpid):
    lib, name = fpid.split(":")
    path = LIBS.get(lib, STD + lib + ".pretty")
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None: raise Exception("huella no encontrada: " + fpid)
    return fp

board = pcbnew.CreateEmptyBoard()
board.SetCopperLayerCount(4)
ds = board.GetDesignSettings()
ds.SetBoardThickness(mm(1.6))
ds.m_TrackMinWidth = mm(0.127); ds.m_ViasMinSize = mm(0.45); ds.m_MinThroughDrill = mm(0.2)
ds.m_ViasMinAnnularWidth = mm(0.125); ds.m_MinClearance = mm(0.127); ds.m_CopperEdgeClearance = mm(0.3)
ds.m_HoleToHoleMin = mm(0.25); ds.m_HoleClearance = mm(0.25)
board.SetLayerName(pcbnew.In1_Cu, "GND"); board.SetLayerName(pcbnew.In2_Cu, "PWR")
board.SetLayerType(pcbnew.In1_Cu, pcbnew.LT_POWER); board.SetLayerType(pcbnew.In2_Cu, pcbnew.LT_POWER)

# ---------------- redes
netitems = {}
for name in nets:
    it = pcbnew.NETINFO_ITEM(board, name); board.Add(it); netitems[name] = it
pin2net = {}
for name, nodes in nets.items():
    for ref, pin in nodes: pin2net[(ref, pin)] = name

# ---------------- huellas
fps = {}
_cc = {x.get("ref"): x for x in root.find("components")}
for ref, c in comps.items():
    cc = _cc[ref]
    fp = load_fp(c["fp"])
    fp.SetReference(ref); fp.SetValue(c["val"] or "")
    if ref in partsdb.P: fp.SetField("LCSC", partsdb.P[ref][2])
    elif ref.startswith("TP"): fp.SetField("LCSC", "")
    c["flds"] = {}
    for fld in (cc.find("fields") if cc.find("fields") is not None else []): c["flds"][fld.get("name")] = fld.text or ""
    for nm in ("Datasheet", "Description"):
        if nm in c["flds"]: fp.SetField(nm, c["flds"][nm])
    try: fp.SetPath(pcbnew.KIID_PATH(c["path"]))
    except Exception as e: pass
    fp.SetFPID(pcbnew.LIB_ID(*c["fp"].split(":")))
    for _f in fp.GetFields():
        if _f.GetName() != "Reference": _f.SetVisible(False)
    board.Add(fp); fps[ref] = fp
    for pad in fp.Pads():
        nm = pin2net.get((ref, pad.GetNumber()))
        if nm: pad.SetNet(netitems[nm])
        elif ref == "U1" and pad.GetNumber().isdigit() and int(pad.GetNumber()) > 28: pad.SetNet(netitems["GND"])
    fp.Value().SetVisible(False)

def bbox(fp, include_text=False):
    b = fp.GetBoundingBox(include_text); return b
def move_center(fp, cx, cy, rot=None):
    if rot is not None: fp.SetOrientationDegrees(rot)
    b = fp.GetBoundingBox(False)
    c = b.GetCenter()
    fp.Move(pcbnew.VECTOR2I(P(cx, cy).x - c.x, P(cx, cy).y - c.y))
def align(fp, side, v, rot=None, along=None):
    """pega el contorno del footprint al borde: side en L,R,T,B (v = coordenada del borde); along = centro del eje paralelo"""
    if rot is not None: fp.SetOrientationDegrees(rot)
    b = fp.GetBoundingBox(False); c = b.GetCenter()
    dx = dy = 0
    if side == "L": dx = P(v, 0).x - b.GetX()
    if side == "R": dx = P(v, 0).x - b.GetRight()
    if side == "T": dy = P(0, v).y - b.GetY()
    if side == "B": dy = P(0, v).y - b.GetBottom()
    if along is not None:
        if side in "LR": dy = P(0, along).y - c.y
        else: dx = P(along, 0).x - c.x
    fp.Move(pcbnew.VECTOR2I(dx, dy))

# ---------------- anclas (conectores de borde y piezas grandes)
placed = []
def fix(ref): placed.append(ref)
# frente (y = 0)
for ref, x in (("J22", 28), ("J21", 45), ("J24", 76), ("J23", 92)):
    align(fps[ref], "T", 0.8, rot=180, along=x); fix(ref)
align(fps["J7"], "T", 0.0, rot=90, along=60); fix("J7")
# lateral izquierdo
align(fps["U2"], "L", 0.0, rot=180, along=20); fix("U2")
align(fps["J8"], "L", 0.0, rot=180, along=42); fix("J8")
align(fps["J5"], "L", 0.0, rot=0, along=66); fix("J5")
# lateral derecho
align(fps["J9"], "R", W, rot=0, along=42); fix("J9")
align(fps["J13"], "R", W, rot=180, along=66); fix("J13")
align(fps["J10"], "R", W, rot=90, along=H - 29); fix("J10")
# trasera (y = H)
align(fps["J6"], "B", H, rot=0, along=22); fix("J6")
align(fps["J16"], "B", H, rot=270, along=58); fix("J16")
align(fps["J11"], "B", H - 1, rot=0, along=72); fix("J11")
align(fps["J2"], "B", H, rot=0, along=86); fix("J2")
align(fps["J3"], "B", H, rot=0, along=100); fix("J3")
# ESP32: antena hacia el lateral derecho, al fondo
u3 = fps["U3"]
best = None
for rot in (90, 270):
    u3.SetOrientationDegrees(rot)
    pads = [p for p in u3.Pads() if p.GetNumber() in ("17", "20", "23")]
    cx = sum(p.GetPosition().x for p in pads) / len(pads); fc = u3.GetBoundingBox(False).GetCenter().x
    best = (rot, cx - fc) if (best is None or (cx - fc) < best[1]) else best
align(u3, "R", W, rot=best[0], along=H - 44); fix("U3")
# interior
move_center(fps["J20"], 60, 46, rot=90); fix("J20")
move_center(fps["J15"], 36, 90, rot=0); fix("J15")
move_center(fps["U1"], 60, 88, rot=0); fix("U1")

# ---------------- colocador por espiral para el resto
def overlap(a, b, margin=mm(0.8)):
    ba = a.GetBoundingBox(False); bb = b.GetBoundingBox(False)
    ba.Inflate(margin); return ba.Intersects(bb)
def inside(fp):
    b = fp.GetBoundingBox(False)
    return b.GetX() >= P(1.0, 0).x and b.GetRight() <= P(W - 1.0, 0).x and b.GetY() >= P(0, 1.0).y and b.GetBottom() <= P(0, H - 1.0).y
def pad_target(anchor, ref):
    """posicion del pad del ancla que comparte red (no GND) con la pieza; si no hay, None"""
    fa = fps[anchor]; fr = fps[ref]
    nets_r = [p.GetNetname() for p in fr.Pads() if p.GetNetname() not in ("", "GND")]
    best = None
    for p in fa.Pads():
        n = p.GetNetname()
        if n in nets_r and n not in ("+3V3", "+5V", "/Externo/VBAT") or (n in nets_r):
            if best is None or n in ("+3V3", "+5V", "/Externo/VBAT"):
                best = p
            if n not in ("+3V3", "+5V", "/Externo/VBAT"): best = p; break
    if best is None: return None
    q = best.GetPosition(); return (q.x / MM - OX, q.y / MM - OY)
reserved = []; cgroups = []; legres = []
GM = {'DESACOPLE nRF24': (mm(0.8), mm(0.6), mm(2.8))}
SIDE_M, TOP_M, BOT_M = mm(1.3), mm(2.8), mm(1.3)
LEGC = ['J5', 'J6', 'J7', 'J8', 'J9', 'J10', 'J11', 'J13', 'J15', 'J16', 'J21', 'J22', 'J23', 'J24']
def _hit(a, b): return a[0] < b[2] and a[2] > b[0] and a[1] < b[3] and a[3] > b[1]
def place_near(ref, ax, ay, rot=0):
    fp = fps[ref]; fp.SetOrientationDegrees(rot)
    for r in [i * 0.5 for i in range(0, 200)]:
        n = max(1, int(2 * math.pi * r / 1.0)) if r > 0 else 1
        for k in range(n):
            a = 2 * math.pi * k / n
            move_center(fp, ax + r * math.cos(a), ay + r * math.sin(a), rot)
            bb = fp.GetBoundingBox(False); bb = (bb.GetX(), bb.GetY(), bb.GetRight(), bb.GetBottom())
            if inside(fp) and not any(overlap(fp, fps[o]) for o in placed) and not any(_hit(bb, rr) for rr in reserved + legres):
                placed.append(ref); return True
    raise Exception("sin sitio para " + ref)
def small_cap(ref):
    if not ref.startswith("C"): return False
    b = fps[ref].GetBoundingBox(False); return max(b.GetWidth(), b.GetHeight()) < mm(5)
_loc = {}
def local_info(ref, rot):
    k = (ref, rot)
    if k not in _loc:
        fp = fps[ref]; move_center(fp, 0, 0, rot)
        b = fp.GetBoundingBox(False); c = b.GetCenter()
        _loc[k] = ([(p.GetNetname(), p.GetPosition().x - c.x, p.GetPosition().y - c.y) for p in fp.Pads()], b.GetWidth() / 2, b.GetHeight() / 2)
    return _loc[k]
def _pdist(px, py, r):
    dx = max(r[0] - px, 0, px - r[2]); dy = max(r[1] - py, 0, py - r[3]); return math.hypot(dx, dy)
def hug(ref, anchor, hint=None, R=7.0):
    """pone el condensador pegado a los pads del ancla de sus mismas redes, con la orientacion que los acerca mas"""
    fp = fps[ref]; fa = fps[anchor]
    prect = {}
    for p in fa.Pads():
        bb = p.GetBoundingBox(); prect.setdefault(p.GetNetname(), []).append((bb.GetX(), bb.GetY(), bb.GetRight(), bb.GetBottom()))
    cnets = [p.GetNetname() for p in fp.Pads()]
    cand = [r for n in cnets if n != "GND" for r in prect.get(n, [])] or [r for n in cnets for r in prect.get(n, [])]
    if not cand: return False
    if hint is None:
        hc = fa.GetBoundingBox(False).GetCenter(); hint = (hc.x, hc.y)
    else: hint = (P(hint[0], hint[1]).x, P(hint[0], hint[1]).y)
    t = min(cand, key=lambda r: (hint[0] - (r[0] + r[2]) / 2) ** 2 + (hint[1] - (r[1] + r[3]) / 2) ** 2)
    tx, ty = (t[0] + t[2]) // 2, (t[1] + t[3]) // 2
    inf = mm(0.2)
    obs = []
    for o in placed:
        bb = fps[o].GetBoundingBox(False)
        r = (bb.GetX() - inf, bb.GetY() - inf, bb.GetRight() + inf, bb.GetBottom() + inf)
        if r[2] > tx - mm(R + 4) and r[0] < tx + mm(R + 4) and r[3] > ty - mm(R + 4) and r[1] < ty + mm(R + 4): obs.append(r)
    obs += [rr for rr in reserved if rr[2] > tx - mm(R + 4) and rr[0] < tx + mm(R + 4) and rr[3] > ty - mm(R + 4) and rr[1] < ty + mm(R + 4)]
    lo = (P(1.0, 1.0).x, P(1.0, 1.0).y); hi = (P(W - 1.0, H - 1.0).x, P(W - 1.0, H - 1.0).y)
    def score(cx, cy, rot):
        pads, hw, hh = local_info(ref, rot)
        r = (cx - hw, cy - hh, cx + hw, cy + hh)
        if r[0] < lo[0] or r[1] < lo[1] or r[2] > hi[0] or r[3] > hi[1]: return None
        for o in obs:
            if r[0] < o[2] and r[2] > o[0] and r[1] < o[3] and r[3] > o[1]: return None
        sc = 0.0
        for n, dx, dy in pads:
            rs = prect.get(n)
            if rs: sc += min(_pdist(cx + dx, cy + dy, q) for q in rs) / MM
        return sc + 0.002 * math.hypot(cx - tx, cy - ty) / MM
    best = None
    def scan(c0x, c0y, rad, step):
        nonlocal best
        n = int(rad / step)
        for i in range(-n, n + 1):
            for j in range(-n, n + 1):
                cx = c0x + mm(i * step); cy = c0y + mm(j * step)
                for rot in (0, 90, 180, 270):
                    s_ = score(cx, cy, rot)
                    if s_ is not None and (best is None or s_ < best[0]): best = (s_, cx, cy, rot)
    scan(tx, ty, R, 0.5)
    if best is None: return False
    scan(best[1], best[2], 0.6, 0.1)
    fp.SetOrientationDegrees(best[3]); move_center(fp, 0, 0, best[3])
    c = fp.GetBoundingBox(False).GetCenter()
    fp.Move(pcbnew.VECTOR2I(int(best[1] - c.x), int(best[2] - c.y)))
    placed.append(ref); return True
def box_rect(title, members):
    rs = [fps[x].GetBoundingBox(False) for x in members]
    sm, tm, bm = GM.get(title, (SIDE_M, TOP_M, BOT_M))
    sm += mm(1.4); tm += mm(1.0); bm += mm(1.4)  # sitio para las referencias pegadas
    x1 = min(b.GetX() for b in rs) - sm; x2 = max(b.GetRight() for b in rs) + sm
    x2 = max(x2, x1 + mm(1.0 * len(title) + 2.8))
    return (x1, min(b.GetY() for b in rs) - tm, x2, max(b.GetBottom() for b in rs) + bm)
def _build(anchor, ax, ay, refs, arot, move_anchor):
    if move_anchor:
        move_center(fps[anchor], ax, ay, arot); placed.append(anchor)
    a = fps[anchor].GetBoundingBox(False).GetCenter()
    cx, cy = (a.x / MM - OX), (a.y / MM - OY)
    for r in refs:
        if small_cap(r): continue
        t = pad_target(anchor, r)
        place_near(r, t[0] if t else cx, t[1] if t else cy)
    for r in refs:
        if not small_cap(r): continue
        if not hug(r, anchor):
            t = pad_target(anchor, r); place_near(r, t[0] if t else cx, t[1] if t else cy)
_why = []
def cluster(anchor, ax, ay, refs, arot=0, title=None):
    moved = anchor not in placed
    offs = [(0, 0)] + ([(r * math.cos(k * math.pi / 4), r * math.sin(k * math.pi / 4)) for r in (3, 6, 9, 12, 16, 20, 24) for k in range(8)] if moved else [])
    members = [anchor] + refs
    for (dx, dy) in offs:
        n0 = len(placed)
        try:
            _build(anchor, ax + dx, ay + dy, refs, arot, moved)
        except Exception as _e:
            del placed[n0:]
            if not moved: raise
            _why.append(('exc', str(_e)[:60])); continue
        if title is None: break
        rect = box_rect(title, members)
        clash = any(_hit(rect, rr) for rr in reserved + legres) or any(_hit(rect, (lambda b: (b.GetX(), b.GetY(), b.GetRight(), b.GetBottom()))(fps[o].GetBoundingBox(False))) for o in placed if o not in members)
        inb = rect[0] >= P(0.5, 0).x and rect[2] <= P(W - 0.5, 0).x and rect[1] >= P(0, 0.5).y and rect[3] <= P(0, H - 0.5).y
        if (not clash and inb) or not moved: break
        _why.append((round(dx), round(dy), clash, inb))
        del placed[n0:]
    else:
        print(_why[-12:]); raise Exception("sin sitio para el grupo " + str(title))
    if title:
        print('grupo', title, 'movido' if moved else 'fijo', [round(v / MM, 1) for v in box_rect(title, members)])
        reserved.append(box_rect(title, members)); cgroups.append((title, members))

for _r in LEGC:
    _b = fps[_r].GetBoundingBox(False); legres.append((_b.GetX() - mm(3.4), _b.GetY() - mm(3.4), _b.GetRight() + mm(3.4), _b.GetBottom() + mm(3.4)))
for _x, _y in ((6, 6), (W - 6, 6), (6, H - 6), (W - 6, H - 6)):
    legres.append((P(_x - 4, _y - 4).x, P(_x - 4, _y - 4).y, P(_x + 4, _y + 4).x, P(_x + 4, _y + 4).y))
# racimos por funcion (anclas y piezas cercanas)
def near_fp(ref, anchor, dx=0.0, dy=0.0, rot=0, dohug=True):
    c = fps[anchor].GetBoundingBox(False).GetCenter()
    if dohug and small_cap(ref) and hug(ref, anchor, (c.x / MM - OX + dx, c.y / MM - OY + dy), R=10.0): return
    place_near(ref, c.x / MM - OX + dx, c.y / MM - OY + dy, rot)
cluster("U7", 30, 118, ["L5", "D18", "C69", "C70", "C71", "C72", "C75", "C77", "R67", "R68", "R69", "R78", "C139"], title="BUCK 5V")
cluster("U6", 62, 130, ["L4", "C45", "C46", "C47", "C48", "C146", "R79", "R80"], title="BUCK 3V3")
cluster("D8", 40, 150, ["D17", "C154"], title="TVS VBAT")
cluster("U1", 60, 86, ["C1", "C2", "C3", "C4", "C5", "C6", "R1", "R2", "R3", "R4", "R64", "R65", "R66"], title="DRIVER MOTORES")
cluster("U9", 42, 30, ["L7", "D7", "R25", "R26", "C88", "C89", "C153"], title="DRIVER FARO")
cluster("U15", 62, 31, ["L8", "D11", "R70", "R72", "C141", "C143"], title="DRIVER INT IZQ")
cluster("U16", 84, 30, ["L9", "D12", "R71", "R73", "C142", "C144"], title="DRIVER INT DER")
cluster("U10", 82, 104, ["R30", "C94", "C95"], title="NIVEL TIRA LED")
cluster("U14", 90, 82, ["C138", "R63", "C162"], title="EXPANSOR I/O")
cluster("U3", 0, 0, ["C131", "C132", "C133", "R21", "R22", "R52"])
cluster("U17", 80, 139, ["U18", "D16", "R74", "R75", "R76", "R77", "Q1", "Q2", "C145"], title="USB UART")
cluster("U13", 102, 140, ["D15", "R61", "R62"], title="USB JTAG")
place_near("SW4", 113, 90)
_c = fps["SW4"].GetBoundingBox(False).GetCenter()
place_near("SW1", _c.x / MM - OX + 10.5, _c.y / MM - OY, fps["SW4"].GetOrientationDegrees())
cluster("D1", 94, 60, ["D2", "R17", "R18"], title="LED RUEDA A"); cluster("D3", 26, 60, ["D4", "R19", "R20"], title="LED RUEDA B")
cluster("U2", 0, 0, ["C147", "C150"], title="DESACOPLE nRF24")
# desacople repartido y arreglos ESD cerca de lo que protegen
near_fp("C151", "J5", 16, 14, dohug=False); near_fp("C152", "J13", -16, 14, dohug=False)
near_fp("C155", "J7", 0, 9); near_fp("C156", "J8", 9, 0); near_fp("C157", "J9", -9, 0)
near_fp("C158", "J16", 0, -9); near_fp("C159", "J11", 0, -8); near_fp("C160", "J20", 0, 8)
near_fp("C161", "J20", 0, -8); near_fp("C163", "J10", -8, 0); near_fp("C164", "J15", 6, 0)
near_fp("U19", "J7", -9, 9); near_fp("U20", "J10", -12, 0); near_fp("U21", "J13", -16, 6); near_fp("U22", "J11", 0, -12)

# ---------------- ajustes del usuario (ajustes_usuario.json): solo se respetan las piezas que movio a mano
import json, os
AJ = None; LOCKED = set()
if os.path.exists(SC + 'ajustes_usuario.json'):
    AJ = json.load(open(SC + 'ajustes_usuario.json', encoding='utf8'))
    _prev = {t: box_rect(t, [x for x in m if x in fps]) for t, m in cgroups}
    for _r in AJ.get("locked", []):
        if _r not in fps: continue
        _d = AJ["fp"][_r]; _f = fps[_r]
        _f.SetOrientationDegrees(_d["rot"]); _f.SetPosition(pcbnew.VECTOR2I(_d["x"], _d["y"])); LOCKED.add(_r)
    for _t, _m in cgroups:                      # las reservas de las etapas movidas a mano siguen su nueva posicion
        _n = box_rect(_t, [x for x in _m if x in fps])
        if _n != _prev[_t] and _prev[_t] in reserved: reserved[reserved.index(_prev[_t])] = _n
    print("piezas movidas a mano por el usuario (se respetan):", sorted(LOCKED))
HANDREF = AJ.get("handref", []) if AJ else []

# ---------------- compactar: cada R/C/Q lo mas cerca posible de los pads a los que se conecta
_POWER = ("+3V3", "+5V", "VBAT", "/Externo/VBAT", "NRF_VCC")
GRP_ANCHOR = {}
for _t, _m in cgroups:
    for _x in _m[1:]: GRP_ANCHOR[_x] = _m[0]
def _movable(r):
    return r in placed and r not in LOCKED and r not in ("C151", "C152") and (r in GRP_ANCHOR or (r[0] in "RCQ" and max(fps[r].GetBoundingBox(False).GetWidth(), fps[r].GetBoundingBox(False).GetHeight()) < mm(6)))
def compact_all(passes=5):
    def _dist_anchor(r):
        a = fps[GRP_ANCHOR[r]].GetBoundingBox(False).GetCenter(); c = fps[r].GetBoundingBox(False).GetCenter(); return math.hypot(a.x - c.x, a.y - c.y)
    mov = sorted([r for r in fps if _movable(r) and r in GRP_ANCHOR], key=_dist_anchor) + [r for r in fps if _movable(r) and r not in GRP_ANCHOR]
    save = {r: (fps[r].GetPosition(), fps[r].GetOrientationDegrees()) for r in fps}
    for r in mov:
        for rot in (0, 90, 180, 270): local_info(r, rot)
    for r, (p_, o_) in save.items(): fps[r].SetOrientationDegrees(o_); fps[r].SetPosition(p_)
    lo = (P(1.0, 1.0).x, P(1.0, 1.0).y); hi = (P(W - 1.0, H - 1.0).x, P(W - 1.0, H - 1.0).y)
    inf = mm(0.15); moved = 0; gain = 0.0
    for _pass in range(passes):
        for ref in mov:
            fp = fps[ref]
            # pads objetivo por red (otras piezas)
            tg = {}
            for o in placed:
                if o == ref: continue
                for p in fps[o].Pads():
                    n = p.GetNetname()
                    if not n or n == "GND": continue
                    if n in _POWER and o[0] in "RC": continue
                    q = p.GetPosition(); tg.setdefault(n, []).append((q.x, q.y))
            anc = fps[GRP_ANCHOR[ref]].GetBoundingBox(False) if ref in GRP_ANCHOR else None
            def cost(cx, cy, rot):
                pads, hw, hh = local_info(ref, rot); sc = 0.0
                for n, dx, dy in pads:
                    ts = tg.get(n)
                    if ts:
                        px, py = cx + dx, cy + dy; sc += min(math.hypot(px - a, py - b) for a, b in ts)
                sc /= MM
                if anc is not None:
                    gx = max(anc.GetX() - (cx + hw), 0, (cx - hw) - anc.GetRight()); gy = max(anc.GetY() - (cy + hh), 0, (cy - hh) - anc.GetBottom())
                    sc += 0.6 * math.hypot(gx, gy) / MM
                return sc
            c0 = fp.GetBoundingBox(False); c0c = c0.GetCenter(); cur = (c0c.x, c0c.y)
            cur_rot = fp.GetOrientationDegrees() % 360
            curr = (c0.GetX() - 0, c0.GetY(), c0.GetRight(), c0.GetBottom())
            obs = []
            for o in placed:
                if o == ref: continue
                bb = fps[o].GetBoundingBox(False)
                if abs((bb.GetX() + bb.GetRight()) / 2 - cur[0]) < mm(12) + bb.GetWidth() / 2 and abs((bb.GetY() + bb.GetBottom()) / 2 - cur[1]) < mm(12) + bb.GetHeight() / 2:
                    obs.append((bb.GetX() - inf, bb.GetY() - inf, bb.GetRight() + inf, bb.GetBottom() + inf))
            hit0 = [k for k, q in enumerate(reserved) if _hit(curr, q)]
            inside0 = [k for k in hit0 if reserved[k][0] <= curr[0] and reserved[k][1] <= curr[1] and reserved[k][2] >= curr[2] and reserved[k][3] >= curr[3]]
            def ok(cx, cy, rot):
                pads, hw, hh = local_info(ref, rot)
                r_ = (cx - hw, cy - hh, cx + hw, cy + hh)
                if r_[0] < lo[0] or r_[1] < lo[1] or r_[2] > hi[0] or r_[3] > hi[1]: return False
                for o in obs:
                    if r_[0] < o[2] and r_[2] > o[0] and r_[1] < o[3] and r_[3] > o[1]: return False
                for q in legres:
                    if _hit(r_, q): return False
                for k, q in enumerate(reserved):
                    if k not in hit0 and _hit(r_, q): return False
                for k in inside0:
                    q = reserved[k]
                    if not (r_[0] >= q[0] and r_[1] >= q[1] and r_[2] <= q[2] and r_[3] <= q[3]): return False
                return True
            rot0 = int(round(cur_rot / 90.0)) * 90 % 360
            base = cost(cur[0], cur[1], rot0); best = (base, cur[0], cur[1], rot0)
            def scan(c0x, c0y, rad, step):
                nonlocal best
                n = int(rad / step)
                for i in range(-n, n + 1):
                    for j in range(-n, n + 1):
                        cx = c0x + mm(i * step); cy = c0y + mm(j * step)
                        for rot in (0, 90, 180, 270):
                            sc = cost(cx, cy, rot) + 0.003 * math.hypot(cx - cur[0], cy - cur[1]) / MM
                            if sc < best[0] - 1e-6 and ok(cx, cy, rot): best = (sc, cx, cy, rot)
            scan(cur[0], cur[1], 8.0 if ref in GRP_ANCHOR else 4.5, 0.5 if ref in GRP_ANCHOR else 0.45)
            if best[1:] != (cur[0], cur[1], rot0): scan(best[1], best[2], 0.5, 0.1)
            if best[0] < base - 0.05:
                fp.SetOrientationDegrees(best[3]); move_center(fp, 0, 0, best[3])
                c = fp.GetBoundingBox(False).GetCenter(); fp.Move(pcbnew.VECTOR2I(int(best[1] - c.x), int(best[2] - c.y)))
                moved += 1; gain += base - best[0]
            else:
                fp.SetOrientationDegrees(save[ref][1]); fp.SetPosition(save[ref][0])
    print("compactadas:", moved, "piezas; distancia total a sus pads reducida", round(gain, 1), "mm")
compact_all()
exec(open(SC + 'sa_pack.py', encoding='utf8').read())
pack_all()
def legalizar():
    inf = mm(0.08); n = 0
    refs_ = [r for r in placed if r in fps]
    for i, a in enumerate(refs_):
        for b_ in refs_[i + 1:]:
            ba = fps[a].GetBoundingBox(False); bb = fps[b_].GetBoundingBox(False)
            ba2 = pcbnew.BOX2I(ba.GetPosition(), ba.GetSize()); ba2.Inflate(inf)
            if not ba2.Intersects(bb): continue
            mv = b_ if _movable(b_) else (a if _movable(a) else None)
            if mv is None: continue
            fp = fps[mv]; p0 = fp.GetPosition(); done = False
            for r in [0.1 * k for k in range(1, 40)]:
                for k in range(16):
                    ang = 2 * math.pi * k / 16
                    fp.SetPosition(pcbnew.VECTOR2I(int(p0.x + mm(r) * math.cos(ang)), int(p0.y + mm(r) * math.sin(ang))))
                    bm = fp.GetBoundingBox(False); bm2 = pcbnew.BOX2I(bm.GetPosition(), bm.GetSize()); bm2.Inflate(inf)
                    if not any(o != mv and bm2.Intersects(fps[o].GetBoundingBox(False)) for o in refs_) and not any(_hit((bm.GetX(), bm.GetY(), bm.GetRight(), bm.GetBottom()), q) for q in legres):
                        done = True; break
                if done: break
            if done: n += 1
            else: fp.SetPosition(p0)
    print("solapes corregidos tras compactar:", n)
legalizar()

# puntos de prueba: junto a lo que miden, sin pisar piezas
_txt = []
def _place_tp(ref, ax, ay, title=None):
    fp = fps[ref]; fp.SetOrientationDegrees(0)
    own = box_rect(title, [m for t, m in cgroups if t == title][0]) if title else None
    for r in [i * 0.5 for i in range(0, 120)]:
        n = max(1, int(2 * math.pi * r / 1.0)) if r > 0 else 1
        for k in range(n):
            a = 2 * math.pi * k / n
            move_center(fp, ax + r * math.cos(a), ay + r * math.sin(a), 0)
            bb = fp.GetBoundingBox(False); rr = (bb.GetX(), bb.GetY(), bb.GetRight(), bb.GetBottom()); c = bb.GetCenter()
            if not inside(fp) or any(overlap(fp, fps[o], mm(0.5)) for o in placed): continue
            if own is not None and not (rr[0] >= own[0] and rr[1] >= own[1] and rr[2] <= own[2] and rr[3] <= own[3]): continue
            if any(_hit(rr, (q[0] - mm(3), q[1] - mm(3), q[2] + mm(3), q[3] + mm(3)) if own is None else q) for q in reserved + legres if q != own): continue
            if any(math.hypot(c.x - tx_, c.y - ty_) < tr_ for tx_, ty_, tr_ in _txt): continue
            placed.append(ref)
            if title: [m for t, m in cgroups if t == title][0].append(ref)
            return True
    raise Exception("sin sitio para " + ref)
def _c(ref): b = fps[ref].GetBoundingBox(False).GetCenter(); return b.x / MM - OX, b.y / MM - OY
TPS = [("TP1", "C77", 0, 7, None), ("TP2", "C48", 0, 5, None), ("TP3", "J20", 16, 2, None), ("TP4", "C71", 6, 0, None), ("TP5", "C47", 6, 0, None),
       ("TP6", "SW4", -8, 0, None), ("TP7", "SW1", 8, 0, None), ("TP8", "U17", 10, 0, None), ("TP9", "U17", 10, 4, None)]
for _tp, _an, _dx, _dy, _gt in TPS:
    if _tp in fps:
        _x, _y = _c(_an); _place_tp(_tp, _x + _dx, _y + _dy, _gt)
        fps[_tp].SetAttributes(fps[_tp].GetAttributes() | pcbnew.FP_EXCLUDE_FROM_BOM)
left = [r for r in fps if r not in placed]
print("sin colocar:", left)

# ---------------- contorno, agujeros de montaje, zonas
def seg(x1, y1, x2, y2, layer=pcbnew.Edge_Cuts, w=0.1):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetLayer(layer)
    s.SetStart(P(x1, y1)); s.SetEnd(P(x2, y2)); s.SetWidth(mm(w)); board.Add(s)
def arc(cx, cy, sx, sy, ex, ey, layer=pcbnew.Edge_Cuts):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_ARC); s.SetLayer(layer)
    s.SetCenter(P(cx, cy)); s.SetStart(P(sx, sy)); s.SetEnd(P(ex, ey)); s.SetWidth(mm(0.1)); board.Add(s)
R = 3.0
seg(R, 0, W - R, 0); seg(W, R, W, H - R); seg(W - R, H, R, H); seg(0, H - R, 0, R)
arc(R, R, 0, R, R, 0); arc(W - R, R, W - R, 0, W, R); arc(W - R, H - R, W, H - R, W - R, H); arc(R, H - R, R, H, 0, H - R)

def zone_poly(layerset, pts, net=None, priority=0, rule=False, keepout=(False, False, False), name=""):
    z = pcbnew.ZONE(board); z.SetLayerSet(layerset)
    if net: z.SetNet(netitems[net])
    z.SetAssignedPriority(priority)
    if rule:
        z.SetIsRuleArea(True)
        z.SetDoNotAllowZoneFills(keepout[0]); z.SetDoNotAllowTracks(keepout[1]); z.SetDoNotAllowVias(keepout[2])
        z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    else:
        z.SetMinThickness(mm(0.2)); z.SetLocalClearance(mm(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL); z.SetThermalReliefGap(mm(0.3)); z.SetThermalReliefSpokeWidth(mm(0.4))
    if name: z.SetZoneName(name)
    o = pcbnew.SHAPE_LINE_CHAIN()
    for (x, y) in pts: o.Append(P(x, y).x, P(x, y).y)
    o.SetClosed(True)
    z.Outline().AddOutline(o)
    board.Add(z); return z
full = [(0, 0), (W, 0), (W, H), (0, H)]
def lset(*ls):
    s = pcbnew.LSET()
    for l in ls: s.AddLayer(l)
    return s
zone_poly(lset(pcbnew.In1_Cu), full, "GND", 0, name="GND_L2")
zone_poly(lset(pcbnew.F_Cu), full, "GND", 0, name="GND_F")
zone_poly(lset(pcbnew.B_Cu), full, "GND", 0, name="GND_B")
# capa interna 2: VBAT en toda la placa, +5V y +3V3 en las zonas donde se usan
zone_poly(lset(pcbnew.In2_Cu), full, "/Externo/VBAT", 0, name="VBAT_L3")
zone_poly(lset(pcbnew.In2_Cu), [(30, 4), (90, 4), (90, 76), (30, 76)], "+5V", 1, name="5V_L3")
zone_poly(lset(pcbnew.In2_Cu), [(62, 80), (118, 80), (118, 147), (62, 147)], "+3V3", 2, name="3V3_L3")

# zona prohibida bajo la antena del ESP32 (todas las capas de cobre) y del nRF24
def keepout_for(ref, extra=1.5):
    b = fps[ref].GetBoundingBox(False)
    return b
b = fps["U3"].GetBoundingBox(False)
ax1 = (b.GetX() / MM - OX); ay1 = (b.GetY() / MM - OY); ax2 = b.GetRight() / MM - OX; ay2 = b.GetBottom() / MM - OY
# la antena ocupa 6 mm (7.5 con margen) del extremo del modulo que mira al borde derecho
if AJ is None: zone_poly(pcbnew.LSET.AllCuMask(), [(ax2 - 8.0, ay1), (W + 1, ay1), (W + 1, ay2), (ax2 - 8.0, ay2)], None, 0, rule=True, keepout=(True, True, True), name="Antena_ESP32")  # con ajustes del usuario la antena sale del borde y no hay zona
b = fps["U2"].GetBoundingBox(False)
nx1 = b.GetX() / MM - OX; ny1 = b.GetY() / MM - OY; ny2 = b.GetBottom() / MM - OY
zone_poly(pcbnew.LSET.AllCuMask(), [(nx1 - 1, ny1), (nx1 + 12, ny1), (nx1 + 12, ny2), (nx1 - 1, ny2)], None, 0, rule=True, keepout=(True, True, True), name="Antena_nRF24")

# agujeros de montaje M3
mh = STD + "MountingHole.pretty"
for i, (x, y) in enumerate(((6, 6), (W - 6, 6), (6, H - 6), (W - 6, H - 6))):
    f = pcbnew.FootprintLoad(mh, "MountingHole_3.2mm_M3"); f.SetReference("MH%d" % (i + 1)); f.SetPosition(P(x, y)); f.Value().SetVisible(False); f.Reference().SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8))); board.Add(f)

# vias termicas bajo los pads expuestos
def via(x, y, net, d=0.45, drill=0.2):
    v = pcbnew.PCB_VIA(board); v.SetPosition(pcbnew.VECTOR2I(x, y)); v.SetViaType(pcbnew.VIATYPE_THROUGH)
    v.SetWidth(mm(d)); v.SetDrill(mm(drill)); v.SetNet(netitems[net]); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); board.Add(v)
def ep_vias(ref, padnum, cols, rows, pitch, drill=0.2, d=0.45):
    fp = fps[ref]
    for p in fp.Pads():
        if p.GetNumber() == padnum and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD and p.GetSize().x > mm(1.5) and p.GetSize().y > mm(1.5):
            c = p.GetPosition(); rot = fp.GetOrientationDegrees()
            for i in range(cols):
                for j in range(rows):
                    dx = (i - (cols - 1) / 2) * pitch; dy = (j - (rows - 1) / 2) * pitch
                    a = math.radians(-rot); rx = dx * math.cos(a) - dy * math.sin(a); ry = dx * math.sin(a) + dy * math.cos(a)
                    via(c.x + mm(rx), c.y + mm(ry), "GND", d, drill)
            return True
    return False
res = {}
res["U1"] = "24 vias termicas de 0,2 mm ya incluidas en la huella (TI PWP0028M)"
res["U7"] = ep_vias("U7", "9", 2, 3, 1.0)       # AP64500: pad expuesto 2,41x3,3
res["U3"] = False
print("vias EP:", res)

exec(open(SC + 'silk_build.py', encoding='utf8').read())

if AJ is not None:
    from collections import Counter
    def _apply_tx(t, d):
        t.SetText(d["text"]); t.SetLayer(board.GetLayerID(d["layer"])); t.SetTextSize(pcbnew.VECTOR2I(d["w"], d["h"]))
        t.SetTextThickness(d["th"]); t.SetBold(d["bold"]); t.SetItalic(d["ital"]); t.SetMirrored(d["mir"]); t.SetVisible(d["vis"])
        t.SetHorizJustify(d["hj"]); t.SetVertJustify(d["vj"]); t.SetKeepUpright(d["up"]); t.SetTextAngleDegrees(d["ang"])
        t.SetPosition(pcbnew.VECTOR2I(d["x"], d["y"]))
    TPVAL = {fps[r].GetValue() for r in fps if r.startswith("TP")}
    gen = [d for d in board.GetDrawings() if d.GetClass() == "PCB_TEXT"]
    cg = Counter((t.GetText(), t.GetLayerName()) for t in gen); ca = Counter((d["text"], d["layer"]) for d in AJ["texts"])
    nrem = nadd = 0
    for t in reversed(gen):                      # textos que el usuario borro
        k = (t.GetText(), t.GetLayerName())
        if cg[k] > ca[k] and t.GetText() not in TPVAL: board.Remove(t); cg[k] -= 1; nrem += 1
    for d in AJ["texts"]:                        # textos que el usuario agrego o reescribio
        k = (d["text"], d["layer"])
        if ca[k] > cg[k]:
            t = pcbnew.PCB_TEXT(board); _apply_tx(t, d); board.Add(t); cg[k] += 1; nadd += 1
    print("silk del usuario: %d textos quitados, %d agregados" % (nrem, nadd))
try:
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
except Exception as e:
    print("relleno de zonas:", e)
board.Save(SC + "Tesla_new.kicad_pcb")
print("guardado")
