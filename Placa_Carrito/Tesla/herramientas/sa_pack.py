# Empaquetado por recocido simulado de cada etapa: minimiza el area del grupo y la longitud de cables
import random
def pack_group(title, members, iters=14000, seed=1):
    anchor = members[0]
    mov = [r for r in members[1:] if r in fps and r not in LOCKED and r in placed]
    if not mov: return None
    random.seed(seed)
    allm = [anchor] + mov + [r for r in members[1:] if r in LOCKED and r in fps]
    _sv = {r: (fps[r].GetPosition(), fps[r].GetOrientationDegrees()) for r in allm}
    for r in allm:
        for rot_ in (0, 90, 180, 270): local_info(r, rot_)
    for r, (p_, o_) in _sv.items(): fps[r].SetOrientationDegrees(o_); fps[r].SetPosition(p_)
    # estado inicial
    pos = {}
    for r in allm:
        fp = fps[r]; c = fp.GetBoundingBox(False).GetCenter(); rot = int(round((fp.GetOrientationDegrees() % 360) / 90.0)) * 90 % 360
        pos[r] = [c.x, c.y, rot]
    fixed = [anchor] + [r for r in allm if r in LOCKED]
    own_ids = set(allm)
    gb = [fps[r].GetBoundingBox(False) for r in allm]
    gx1 = min(b.GetX() for b in gb) - mm(15); gy1 = min(b.GetY() for b in gb) - mm(15); gx2 = max(b.GetRight() for b in gb) + mm(15); gy2 = max(b.GetBottom() for b in gb) + mm(15)
    inf = mm(0.12)
    ext = []
    for o in placed:
        if o in own_ids: continue
        bb = fps[o].GetBoundingBox(False)
        if bb.GetRight() > gx1 and bb.GetX() < gx2 and bb.GetBottom() > gy1 and bb.GetY() < gy2:
            ext.append((bb.GetX() - inf, bb.GetY() - inf, bb.GetRight() + inf, bb.GetBottom() + inf))
    myrect = box_rect(title, [m for m in members if m in fps])
    ext += [q for q in legres if q[2] > gx1 and q[0] < gx2 and q[3] > gy1 and q[1] < gy2]
    ac = fps[anchor].GetBoundingBox(False).GetCenter()
    ext += [q for q in reserved if not (q[0] <= ac.x <= q[2] and q[1] <= ac.y <= q[3]) and q[2] > gx1 and q[0] < gx2 and q[3] > gy1 and q[1] < gy2]
    lo = (P(1.0, 1.0).x, P(1.0, 1.0).y); hi = (P(W - 1.0, H - 1.0).x, P(W - 1.0, H - 1.0).y)
    def info(r, rot): return local_info(r, rot)
    # pads fijos del ancla y bloqueadas en absolutos
    nets = {}
    for r in allm:
        for k, (n, dx, dy) in enumerate(info(r, pos[r][2])[0]):
            if n and n != "GND": nets.setdefault(n, []).append(r)
    wnet = {n: (0.25 if n in _POWER else 1.0) for n in nets}
    def rect_of(r):
        st = pos[r]; pads, hw, hh = info(r, st[2]); return (st[0] - hw, st[1] - hh, st[0] + hw, st[1] + hh)
    def hpwl():
        tot = 0.0
        for n, rs in nets.items():
            if len(rs) < 2: continue
            xs = []; ys = []
            for r in rs:
                st = pos[r]
                for nn, dx, dy in info(r, st[2])[0]:
                    if nn == n: xs.append(st[0] + dx); ys.append(st[1] + dy)
            tot += wnet[n] * ((max(xs) - min(xs)) + (max(ys) - min(ys)))
        return tot / MM
    def area():
        rs = [rect_of(r) for r in allm]
        w_ = (max(a[2] for a in rs) - min(a[0] for a in rs)) / MM + 2.0; h_ = (max(a[3] for a in rs) - min(a[1] for a in rs)) / MM + 2.0
        return w_ * h_
    def legal(r):
        a = rect_of(r)
        if a[0] < lo[0] or a[1] < lo[1] or a[2] > hi[0] or a[3] > hi[1]: return False
        for q in ext:
            if a[0] < q[2] and a[2] > q[0] and a[1] < q[3] and a[3] > q[1]: return False
        for o in allm:
            if o == r: continue
            b = rect_of(o)
            if a[0] < b[2] + inf and a[2] > b[0] - inf and a[1] < b[3] + inf and a[3] > b[1] - inf: return False
        return True
    if not all(legal(r) for r in allm): pass
    E0 = area() + 4.0 * hpwl(); E = E0; best = (E, {r: list(pos[r]) for r in allm})
    T0 = 6.0
    for it in range(iters):
        T = T0 * (1.0 - it / iters) ** 2 + 0.01
        r = random.choice(mov); old = list(pos[r]); u = random.random()
        sig = mm(0.4 + 2.5 * T / T0)
        if u < 0.65: pos[r][0] += int(random.gauss(0, sig)); pos[r][1] += int(random.gauss(0, sig))
        elif u < 0.85: pos[r][2] = (pos[r][2] + 90) % 360
        elif u < 0.93: pos[r][2] = (pos[r][2] + 180) % 360
        else:
            o = random.choice(mov)
            if o != r:
                oo = list(pos[o]); pos[r][0], pos[r][1] = oo[0], oo[1]; pos[o][0], pos[o][1] = old[0], old[1]
                if not (legal(r) and legal(o)): pos[r] = old; pos[o] = oo; continue
                E1 = area() + 4.0 * hpwl()
                if E1 < E or random.random() < math.exp(-(E1 - E) / T): E = E1
                else: pos[r] = old; pos[o] = oo
                if E < best[0]: best = (E, {k: list(v) for k, v in pos.items()})
                continue
        if not legal(r): pos[r] = old; continue
        E1 = area() + 4.0 * hpwl()
        if E1 < E or random.random() < math.exp(-(E1 - E) / T): E = E1
        else: pos[r] = old
        if E < best[0]: best = (E, {k: list(v) for k, v in pos.items()})
    for r in mov:
        cx, cy, rot = best[1][r]; fp = fps[r]
        fp.SetOrientationDegrees(rot); move_center(fp, 0, 0, rot)
        c = fp.GetBoundingBox(False).GetCenter(); fp.Move(pcbnew.VECTOR2I(int(cx - c.x), int(cy - c.y)))
    return (round(E0, 1), round(best[0], 1))
def pack_all():
    for title, members in cgroups:
        res = pack_group(title, members)
        if res: print("empaquetado %-16s energia %7.1f -> %7.1f" % (title, res[0], res[1]))
