# Serigrafia: rotulos sin colisiones, cuadros de etapas, leyendas de pines.
# Se ejecuta dentro de pcb_build.py (usa board, fps, P, mm, MM, OX, OY, W, H, pcbnew).
def R_(b): return (b.GetX(), b.GetY(), b.GetRight(), b.GetBottom())
def infl(r, m): return (r[0] - m, r[1] - m, r[2] + m, r[3] + m)
def hit(a, b): return a[0] < b[2] and a[2] > b[0] and a[1] < b[3] and a[3] > b[1]
EDGE = mm(0.9)
BRD = (P(0, 0).x + EDGE, P(0, 0).y + EDGE, P(W, H).x - EDGE, P(W, H).y - EDGE)
def in_board(r): return r[0] >= BRD[0] and r[1] >= BRD[1] and r[2] <= BRD[2] and r[3] <= BRD[3]
fprect = {ref: R_(f.GetBoundingBox(False)) for ref, f in fps.items()}
obst = [("fp:" + ref, infl(r, mm(0.1))) for ref, r in fprect.items()]
for f in board.GetFootprints():
    if f.GetReference().startswith("MH"): obst.append(("MH", infl(R_(f.GetBoundingBox(False)), mm(0.6))))
def blocked(r, ignore=()):
    for tag, o in obst:
        if tag in ignore: continue
        if hit(r, o): return True
    return False

def mk_text(txt, size, rot):
    t = pcbnew.PCB_TEXT(board); t.SetText(txt); t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size))); t.SetTextThickness(mm(max(0.15, round(size * 0.2, 2))))
    t.SetBold(True); t.SetTextAngleDegrees(rot); t.SetKeepUpright(False)
    return t
def box_at(t, cx, cy):
    t.SetPosition(pcbnew.VECTOR2I(int(cx), int(cy)))
    return R_(t.GetBoundingBox())
unplaced = []
def commit(t, r, tag):
    obst.append((tag, infl(r, mm(0.08 if tag == "ref" else 0.2))))

def put_around(item, base, txt, size, ignore=(), tag="txt", rots=(0, 90), gaps=None, offs=(0, 0.35, -0.35)):
    """coloca 'item' alrededor del rectangulo base sin chocar; prefiere texto horizontal"""
    gaps = gaps or [0.3 + 0.4 * i for i in range(16)]
    near_g = [g for g in gaps if g <= 2.6]
    stages = [(0, near_g), (90, near_g), (0, gaps), (90, gaps)] if len(rots) > 1 else [(rots[0], gaps)]
    for rot, gl in stages:
        for gap in gl:
            for side in ("T", "B", "R", "L"):
                item.SetTextAngleDegrees(rot)
                r0 = box_at(item, 0, 0); w = r0[2] - r0[0]; h = r0[3] - r0[1]
                bw = base[2] - base[0]; bh = base[3] - base[1]
                for off in offs:
                    if side in "TB":
                        cx = (base[0] + base[2]) / 2 + off * bw
                        cy = base[1] - mm(gap) - h / 2 if side == "T" else base[3] + mm(gap) + h / 2
                    else:
                        cy = (base[1] + base[3]) / 2 + off * bh
                        cx = base[0] - mm(gap) - w / 2 if side == "L" else base[2] + mm(gap) + w / 2
                    r = box_at(item, cx, cy)
                    if in_board(r) and not blocked(r, ignore):
                        commit(item, r, tag); return True
    return False

def put_ray(item, start, d, size, ignore=(), tag="leg", kmax=12.0):
    """coloca una leyenda siguiendo un rayo desde 'start' (nm) en la direccion d=(dx,dy)"""
    rot = 0 if d[0] != 0 else 90
    item.SetTextAngleDegrees(rot)
    r0 = box_at(item, 0, 0); half = ((r0[2] - r0[0]) if d[0] else (r0[3] - r0[1])) / 2
    start = (start[0] + d[0] * half, start[1] + d[1] * half)
    k = 0.0
    while k <= kmax:
        r = box_at(item, start[0] + d[0] * mm(k), start[1] + d[1] * mm(k))
        if in_board(r) and not blocked(r, ignore): commit(item, r, tag); return True
        k += 0.3
    return False

# ---------------- punto de pin 1 grande, junto al pad 1 (circulo relleno de 0,9 mm)
DOT = 0.9
ndot = 0
for ref, f in fps.items():
    if not ref[0] in "UJ" or f.GetPadCount() < 4: continue
    p1 = f.FindPadByNumber("1")
    if p1 is None: continue
    pb = R_(p1.GetBoundingBox()); fc = f.GetBoundingBox(False).GetCenter()
    pcx, pcy = (pb[0] + pb[2]) / 2, (pb[1] + pb[3]) / 2
    dirs = []
    dx, dy = pcx - fc.x, pcy - fc.y
    primary = (1 if dx > 0 else -1, 0) if abs(dx) >= abs(dy) else (0, 1 if dy > 0 else -1)
    dirs = [primary] + [d for d in ((-1, 0), (1, 0), (0, -1), (0, 1)) if d != primary]
    done_ = False
    for dd in dirs:
        for gap in (0.45, 0.7, 1.0, 1.4):
            hw = (pb[2] - pb[0]) / 2 if dd[0] else (pb[3] - pb[1]) / 2
            cx = pcx + dd[0] * (hw + mm(gap) + mm(DOT) / 2); cy = pcy + dd[1] * (hw + mm(gap) + mm(DOT) / 2)
            r = (cx - mm(DOT) / 2, cy - mm(DOT) / 2, cx + mm(DOT) / 2, cy + mm(DOT) / 2)
            if in_board(r) and not blocked(r, ("fp:" + ref,)):
                c_ = pcbnew.PCB_SHAPE(board); c_.SetShape(pcbnew.SHAPE_T_CIRCLE); c_.SetLayer(pcbnew.F_SilkS)
                c_.SetStart(pcbnew.VECTOR2I(int(cx), int(cy))); c_.SetEnd(pcbnew.VECTOR2I(int(cx + mm(DOT) / 2), int(cy)))
                c_.SetWidth(mm(0.1)); c_.SetFilled(True); board.Add(c_); commit(c_, r, "dot"); ndot += 1; done_ = True; break
        if done_: break
print("puntos de pin 1:", ndot)

# ---------------- referencias de piezas de etapas (primero, pegadas a su pieza)

SIZES_BIG = (0.7, 0.6, 0.5, 0.4)
SIZES_SMALL = (0.6, 0.5, 0.4)
def apply_handref(ref):
    d = AJ["fp"][ref]["ref"]; fp = fps[ref]; t = fp.Reference()
    rx = d["x"] - AJ["fp"][ref]["x"]; ry = d["y"] - AJ["fp"][ref]["y"]
    t.SetLayer(board.GetLayerID(d["layer"])); t.SetTextSize(pcbnew.VECTOR2I(d["w"], d["h"])); t.SetTextThickness(d["th"]); t.SetBold(d["bold"])
    t.SetMirrored(d["mir"]); t.SetVisible(d["vis"]); t.SetKeepUpright(False); t.SetTextAngleDegrees(d["ang"])
    t.SetHorizJustify(d["hj"]); t.SetVertJustify(d["vj"])
    t.SetPosition(pcbnew.VECTOR2I(int(fp.GetX() + rx), int(fp.GetY() + ry)))
    if t.GetLayer() == pcbnew.F_SilkS: commit(t, R_(t.GetBoundingBox()), "ref")
def place_ref(ref, gapmax=32, step=0.4, g0=0.3, ign=(), sizes=None):
    if AJ is not None and ref in HANDREF and ref in AJ["fp"]:
        apply_handref(ref); return True
    f = fps[ref]; item = f.Reference()
    item.SetLayer(pcbnew.F_SilkS); item.SetVisible(True); item.SetKeepUpright(False); item.SetBold(True)
    small = (fprect[ref][2] - fprect[ref][0]) < mm(6) and (fprect[ref][3] - fprect[ref][1]) < mm(6)
    sl = sizes or (SIZES_SMALL if small else SIZES_BIG)
    near = [0.08 + 0.1 * i for i in range(14)]
    far = [g0 + step * i for i in range(gapmax)]
    for gl in (near, far):
        for size in sl:
            item.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size))); item.SetTextThickness(mm(max(0.1, round(size * 0.2, 2))))
            if put_around(item, fprect[ref], ref, size, ignore=("fp:" + ref,) + ign, tag="ref", gaps=gl):
                return True
    return False
refrect = {}; refdone = set()
for _t, _m in cgroups:
    _br = box_rect(_t, [x for x in _m if x in fps])
    obst.append(("res:" + _t, _br))
    _tw = mm(1.0 * len(_t) + 3.6)
    if _t in GM: obst.append(("ttl", (_br[0], _br[3] - mm(4.4), _br[0] + _tw, _br[3])))
    else: obst.append(("ttl", (_br[0] - mm(4), _br[1] - mm(8), _br[0] + _tw, _br[1] + mm(3.9))))
_grp = [r for _, rr_ in cgroups for r in rr_ if r in fps]
_own = {r: tuple("res:" + t for t, m in cgroups if r in m) for r in _grp}
def _slabs(rc, big=mm(400)):
    return [("tmp", (rc[0] - big, rc[1] - big, rc[0], rc[3] + big)), ("tmp", (rc[2], rc[1] - big, rc[2] + big, rc[3] + big)),
            ("tmp", (rc[0] - big, rc[1] - big, rc[2] + big, rc[1])), ("tmp", (rc[0] - big, rc[3], rc[2] + big, rc[3] + big))]
_grect = {t: box_rect(t, [x for x in m if x in fps]) for t, m in cgroups}
for ref in sorted(set(_grp), key=lambda r: -((fprect[r][2] - fprect[r][0]) * (fprect[r][3] - fprect[r][1]))):
    gt = [t for t, m in cgroups if ref in m][0]
    sl = _slabs(_grect[gt]); n0 = len(obst); obst.extend(sl)
    okp = place_ref(ref, 12, 0.3, 0.2, _own[ref])
    obst[:] = [o for o in obst if o[0] != "tmp"]
    if okp:
        refdone.add(ref); refrect[ref] = R_(fps[ref].Reference().GetBoundingBox())
for ref in sorted(set(_grp) - refdone):
    gt = [t for t, m in cgroups if ref in m][0]
    obst.extend(_slabs(_grect[gt]))
    okp = place_ref(ref, 30, 0.3, 0.2, _own[ref])
    obst[:] = [o for o in obst if o[0] != "tmp"]
    if okp or place_ref(ref, 30, 0.3, 0.2, _own[ref], (0.5, 0.4)) or place_ref(ref, 30, 0.3, 0.2, _own[ref]):
        refdone.add(ref); refrect[ref] = R_(fps[ref].Reference().GetBoundingBox())
print("refs de grupo sin sitio cerca:", sorted(set(_grp) - refdone))
obst[:] = [o for o in obst if not o[0].startswith("res:") and o[0] != "ttl"]

# ---------------- cuadros de etapas
GROUPS = cgroups
SIDE = mm(0.7); TOP = mm(2.0); BOT = mm(0.7)
boxes = []
tt_size = 0.8
for title, refs in GROUPS:
    refs = [r for r in refs if r in fps]
    rs = [fprect[r] for r in refs]; qs = [refrect[r] for r in refs if r in refrect]
    sm, tm, bm = GM.get(title, (SIDE, TOP, BOT)); q = mm(0.4)
    x1 = min([r[0] - sm for r in rs] + [r[0] - q for r in qs]); y1 = min([r[1] - tm for r in rs] + [r[1] - q for r in qs])
    x2 = max([r[2] + sm for r in rs] + [r[2] + q for r in qs]); y2 = max([r[3] + bm for r in rs] + [r[3] + q for r in qs])
    t = mk_text(title, tt_size, 0); r0 = box_at(t, 0, 0); tw = r0[2] - r0[0]
    if x2 - x1 < tw + mm(2.4): x2 = x1 + tw + mm(2.4)
    user_box = None
    if AJ is not None and AJ.get("rects") and any(r in LOCKED for r in refs):
        best_ = None
        for rc in AJ["rects"]:
            n_ = sum(1 for r in refs if rc[0] <= (fprect[r][0] + fprect[r][2]) / 2 <= rc[2] and rc[1] <= (fprect[r][1] + fprect[r][3]) / 2 <= rc[3])
            if n_ >= max(2, len(refs) * 0.6) and (best_ is None or n_ > best_[0]): best_ = (n_, rc)
        if best_: user_box = tuple(best_[1]); x1, y1, x2, y2 = user_box
    boxes.append(dict(title=title, refs=set(refs), rect=(x1, y1, x2, y2), t=t, tw=tw, user=user_box))
box_issues = []
for b in boxes:
    x1, y1, x2, y2 = b["rect"]
    sh = pcbnew.PCB_SHAPE(board); sh.SetShape(pcbnew.SHAPE_T_RECT); sh.SetLayer(pcbnew.F_SilkS)
    sh.SetStart(pcbnew.VECTOR2I(int(x1), int(y1))); sh.SetEnd(pcbnew.VECTOR2I(int(x2), int(y2)))
    sh.SetWidth(mm(0.2)); sh.SetFilled(False); board.Add(sh)
    bottom = b["title"] in GM
    t = b["t"]; r = box_at(t, x1 + mm(1.0) + b["tw"] / 2, (y2 - mm(2.2) if bottom else y1 + mm(0.9)) + (R_(t.GetBoundingBox())[3] - R_(t.GetBoundingBox())[1]) / 2)
    ut = [d for d in AJ["texts"] if d["text"] == b["title"] and d["layer"] == "F.Silkscreen"] if (b.get("user") and AJ is not None) else []
    if ut:                                      # titulo como lo dejo el usuario (posicion y giro)
        d = ut[0]; t.SetTextSize(pcbnew.VECTOR2I(d["w"], d["h"])); t.SetTextThickness(d["th"]); t.SetTextAngleDegrees(d["ang"])
        t.SetHorizJustify(d["hj"]); t.SetVertJustify(d["vj"]); t.SetPosition(pcbnew.VECTOR2I(d["x"], d["y"])); r = R_(t.GetBoundingBox())
    board.Add(t)
    b["tr"] = r
    # el borde y el titulo son obstaculos para los demas rotulos
    w = mm(0.45)
    for seg_ in ((x1, y1, x2, y1), (x1, y2, x2, y2), (x1, y1, x1, y2), (x2, y1, x2, y2)):
        obst.append(("box", (min(seg_[0], seg_[2]) - w, min(seg_[1], seg_[3]) - w, max(seg_[0], seg_[2]) + w, max(seg_[1], seg_[3]) + w)))
    obst.append(("title", infl(r, mm(0.2))))
    obst.append(("boxfill:" + b["title"], (x1, y1, x2, y2)))
    for ref, rr in fprect.items():
        if ref in b["refs"]: continue
        if hit(rr, (x1, y1, x2, y2)): box_issues.append((b["title"], ref))
for i_ in range(len(boxes)):
    for j_ in range(i_ + 1, len(boxes)):
        if hit(boxes[i_]["rect"], boxes[j_]["rect"]): box_issues.append((boxes[i_]["title"], boxes[j_]["title"]))
print("cuadros que se cruzan con piezas ajenas o entre si:", box_issues)

def mine(ref): return tuple('boxfill:' + b['title'] for b in boxes if ref in b['refs'])
# ---------------- nombre y voltaje esperado de cada punto de prueba (al lado del TP)
ntp = 0
for ref in sorted(r for r in fps if r.startswith("TP")):
    txt = fps[ref].GetValue()
    if not txt: continue
    ok_ = False
    for size in (0.6, 0.5):
        t = mk_text(txt, size, 0)
        if put_around(t, fprect[ref], txt, size, ignore=("fp:" + ref,), tag="tplab", gaps=[0.2 + 0.25 * i for i in range(16)]):
            board.Add(t); ok_ = True; ntp += 1; break
    if not ok_: unplaced.append("etiqueta " + ref)
print("etiquetas de puntos de prueba:", ntp)

# ---------------- leyendas de pines (siguiendo un rayo perpendicular a la fila de pads)
LEG = {
 "J7": {"1": "-", "2": "+", "3": "S"}, "J8": {"1": "-", "2": "+", "3": "S"}, "J9": {"1": "-", "2": "+", "3": "S"},
 "J16": {"1": "S", "2": "+", "3": "-"},
 "J5": {"1": "M1", "2": "GND", "3": "C1", "4": "C2", "5": "3V3", "6": "M2"},
 "J13": {"1": "M1", "2": "GND", "3": "C1", "4": "C2", "5": "3V3", "6": "M2"},
 "J6": {"1": "+", "2": "-"},
 "J21": {"1": "+", "2": "-"}, "J22": {"1": "+", "2": "-"}, "J23": {"1": "+", "2": "-"}, "J24": {"1": "+", "2": "-"},
 "J10": {"1": "SCL", "2": "SDA", "3": "RX", "4": "TX", "5": "5V", "6": "GND"},
 "J11": {"1": "RX", "2": "PWM", "3": "GND", "4": "5V"},
 "J15": {"1": "3V3", "2": "GND", "3": "SCL", "4": "SDA"},
}
cb = (P(W / 2, H / 2).x, P(W / 2, H / 2).y)
nleg = 0
def collide_pair(rs):
    for i in range(len(rs)):
        for j in range(i + 1, len(rs)):
            if hit(infl(rs[i], mm(0.1)), rs[j]): return True
    return False
for ref, m in LEG.items():
    f = fps[ref]; fr = fprect[ref]
    pads = {}
    for p in f.Pads(): pads.setdefault(p.GetNumber(), []).append(p)
    pts = [p.GetPosition() for ps in pads.values() for p in ps]
    xs = [q.x for q in pts]; ys = [q.y for q in pts]
    row_vertical = (max(ys) - min(ys)) >= (max(xs) - min(xs))
    cen = {}
    for num in m:
        if num in pads:
            cen[num] = (sum(p.GetPosition().x for p in pads[num]) / len(pads[num]), sum(p.GetPosition().y for p in pads[num]) / len(pads[num]))
    order = sorted(cen.values(), key=lambda q: q[1] if row_vertical else q[0])
    pitch = min([abs((b_[1] - a_[1]) if row_vertical else (b_[0] - a_[0])) for a_, b_ in zip(order, order[1:])] or [mm(2.54)])
    SZ = 0.6
    ws = [R_(mk_text(txt, SZ, 0).GetBoundingBox()) for txt in m.values()]
    wmax = max(r[2] - r[0] for r in ws)
    rot = 0 if (row_vertical or wmax <= pitch - mm(0.3)) else 90   # misma orientacion para todos los pines del conector
    cand = [(-1, 0), (1, 0)] if row_vertical else [(0, -1), (0, 1)]
    cand.sort(key=lambda d: ((sum(q[0] for q in cen.values()) / len(cen)) + d[0] * mm(8) - cb[0]) ** 2 + ((sum(q[1] for q in cen.values()) / len(cen)) + d[1] * mm(8) - cb[1]) ** 2)
    done = False
    for d in cand:
        if done: break
        for kk in [0.3 * i for i in range(0, 30)]:
            items = []; rects = []; okk = True
            for num, txt in m.items():
                if num not in cen: continue
                px, py = cen[num]
                t = mk_text(txt, SZ, rot); r0 = box_at(t, 0, 0)
                half = ((r0[2] - r0[0]) if d[0] else (r0[3] - r0[1])) / 2
                if d[0] > 0: sx, sy = fr[2], py
                elif d[0] < 0: sx, sy = fr[0], py
                elif d[1] > 0: sx, sy = px, fr[3]
                else: sx, sy = px, fr[1]
                r = box_at(t, sx + d[0] * (mm(0.4 + kk) + half), sy + d[1] * (mm(0.4 + kk) + half))
                if not in_board(r) or blocked(r, ("fp:" + ref,) + mine(ref)): okk = False; break
                items.append((t, r)); rects.append(r)
            if okk and not collide_pair(rects):
                for t, r in items: commit(t, r, "leg"); board.Add(t); nleg += 1
                done = True; break
    if not done:
        for num, txt in m.items():
            if num not in cen: continue
            t = mk_text(txt, SZ, 0); pb = R_(pads[num][0].GetBoundingBox())
            if put_around(t, pb, txt, SZ, ignore=mine(ref), tag="leg", gaps=[0.2 + 0.3 * i for i in range(30)]):
                board.Add(t); nleg += 1
            else: unplaced.append(ref + ":" + txt)
print("leyendas colocadas:", nleg)

# ---------------- nombres descriptivos de conectores y modulos
LABELS = {"J22": "INT IZQ", "J21": "FARO 1", "J24": "FARO 2", "J23": "INT DER", "J7": "SERVO DIR", "J8": "SERVO 1", "J9": "SERVO 2",
          "J5": "MOTOR B", "J13": "MOTOR A", "J6": "BATERIA 12V", "J16": "TIRA LED",
          "J11": "LIDAR", "J10": "GPS", "J15": "IMU", "J20": "EXPANSION", "U2": "nRF24L01+", "U3": "ESP32-S3", "SW1": "BOOT", "SW4": "RESET"}
nlab = 0
for ref, txt in LABELS.items():
    t = mk_text(txt, 0.9, 0)
    if put_around(t, fprect[ref], txt, 0.9, ignore=("fp:" + ref,) + mine(ref), tag="lab", gaps=[0.4 + 0.5 * i for i in range(24)]):
        board.Add(t); nlab += 1
    else: unplaced.append("etiqueta " + ref)
print("etiquetas:", nlab)
# nombres en la cara inferior (B.SilkS) bajo los conectores USB
for ref, txt in (("J2", "UART"), ("J3", "JTAG")):
    c = fps[ref].GetBoundingBox(False).GetCenter()
    t = pcbnew.PCB_TEXT(board); t.SetText(txt); t.SetLayer(pcbnew.B_SilkS); t.SetMirrored(True)
    t.SetTextSize(pcbnew.VECTOR2I(mm(1.6), mm(1.6))); t.SetTextThickness(mm(0.3)); t.SetBold(True)
    t.SetPosition(pcbnew.VECTOR2I(c.x, c.y)); board.Add(t)
# titulo de la placa
t = mk_text("TESLA - carrito S3", 1.6, 0)
done = False
for (cx, cy) in ((W / 2, 68), (W / 2, 70), (W / 2, 64), (W / 2, 76), (W / 2, 58)):
    r = box_at(t, P(cx, cy).x, P(cy and cx, cy).y)
    if in_board(r) and not blocked(r):
        commit(t, r, "lab"); board.Add(t); done = True; break
if not done: unplaced.append("titulo")

# ---------------- referencias de todas las piezas (campo Reference sobre F.SilkS)
order = sorted(fps, key=lambda r: -((fprect[r][2] - fprect[r][0]) * (fprect[r][3] - fprect[r][1])))
nref = 0
for ref in order:
    if ref in refdone: nref += 1; continue
    if place_ref(ref): nref += 1
    else:
        unplaced.append("ref " + ref); fps[ref].Reference().SetLayer(pcbnew.F_Fab)
print("referencias colocadas:", nref, "de", len(fps))
print("sin sitio:", unplaced)
