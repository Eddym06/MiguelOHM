# -*- coding: utf-8 -*-
"""
Rehace las piezas propias del carrito en PartDesign: un Body por pieza, croquis totalmente
restringidos (cotas a origen) y operaciones Pad / Pocket / Fillet. Se ejecuta DESPUÉS de build_chasis.py:

    freecadcmd build_chasis.py
    freecadcmd build_partdesign.py

Abre carrito_ensamble.FCStd, construye cada Body, compara su volumen con la pieza original
(tolerancia 0,5 %) y reemplaza la pieza. Las piezas compradas (motores, ruedas, LiDAR...) quedan como sólidos.
"""
import json, math, os, sys
import FreeCAD as App
import Part, Sketcher

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

BASE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd()
P = json.load(open(os.path.join(BASE, "params.json"), encoding="utf-8"))
V = App.Vector
doc = App.openDocument(os.path.join(BASE, "carrito_ensamble.FCStd"))
REPORTE = []
AX = {"XY": V(0, 0, 1), "XZ": V(0, 1, 0), "YZ": V(1, 0, 0)}


# ----------------------------------------------------------------------------- utilidades de croquis
class Croquis:
    """Croquis sobre un plano del Body en la cota global 'c' (z para XY, y para XZ, x para YZ).
    Toda la geometría se da en coordenadas GLOBALES y se convierte a las locales del croquis."""

    def __init__(self, body, plano, c, nombre):
        self.body = body
        roles = {f.Role: f for f in body.Origin.OriginFeatures}
        sk = body.newObject("Sketcher::SketchObject", nombre)
        sk.AttachmentSupport = [(roles[plano + "_Plane"], "")]
        sk.MapMode = "FlatFace"
        sk.recompute()
        n = sk.Placement.Rotation.multVec(V(0, 0, 1))
        signo = 1 if n.dot(AX[plano]) > 0 else -1
        sk.AttachmentOffset = App.Placement(V(0, 0, c * signo), App.Rotation())
        sk.recompute()
        self.sk = sk
        self.inv = sk.Placement.inverse()

    def L(self, p):
        q = self.inv.multVec(V(*p))
        return V(round(q.x, 6), round(q.y, 6), 0)

    def poly(self, pts):
        sk = self.sk
        q = [self.L(p) for p in pts]
        ids = [sk.addGeometry(Part.LineSegment(q[i], q[(i + 1) % len(q)]), False) for i in range(len(q))]
        for i in range(len(ids)):
            sk.addConstraint(Sketcher.Constraint("Coincident", ids[i], 2, ids[(i + 1) % len(ids)], 1))
        for i, g in enumerate(ids):
            sk.addConstraint(Sketcher.Constraint("DistanceX", g, 1, q[i].x))
            sk.addConstraint(Sketcher.Constraint("DistanceY", g, 1, q[i].y))
        return ids

    def rect(self, p0, p1, plano_pts=None):
        """Rectángulo entre dos esquinas opuestas (en globales); las otras dos esquinas se deducen en el plano."""
        a, b = self.L(p0), self.L(p1)
        g = [self.inv.inverse().multVec(V(x, y, 0)) for x, y in ((a.x, a.y), (b.x, a.y), (b.x, b.y), (a.x, b.y))]
        return self.poly([(p.x, p.y, p.z) for p in g])

    def circ(self, c, r):
        sk = self.sk
        q = self.L(c)
        g = sk.addGeometry(Part.Circle(q, V(0, 0, 1), r), False)
        sk.addConstraint(Sketcher.Constraint("DistanceX", g, 3, q.x))
        sk.addConstraint(Sketcher.Constraint("DistanceY", g, 3, q.y))
        sk.addConstraint(Sketcher.Constraint("Diameter", g, 2 * r))
        return g

    def ranura(self, c1, c2, r):
        """Ranura (obround) entre dos centros con radio r."""
        sk = self.sk
        a, b = self.L(c1), self.L(c2)
        d = b - a; d.normalize(); nrm = V(-d.y, d.x, 0)
        ang = math.atan2(d.y, d.x)
        arc1 = sk.addGeometry(Part.ArcOfCircle(Part.Circle(a, V(0, 0, 1), r), ang + math.pi / 2, ang + 3 * math.pi / 2), False)
        arc2 = sk.addGeometry(Part.ArcOfCircle(Part.Circle(b, V(0, 0, 1), r), ang - math.pi / 2, ang + math.pi / 2), False)
        l1 = sk.addGeometry(Part.LineSegment(a + nrm * r, b + nrm * r), False)
        l2 = sk.addGeometry(Part.LineSegment(a - nrm * r, b - nrm * r), False)
        # arco1 va de +n a -n; arco2 de -n a +n
        sk.addConstraint(Sketcher.Constraint("Tangent", arc1, 1, l1, 1))
        sk.addConstraint(Sketcher.Constraint("Tangent", arc1, 2, l2, 1))
        sk.addConstraint(Sketcher.Constraint("Tangent", arc2, 1, l2, 2))
        sk.addConstraint(Sketcher.Constraint("Tangent", arc2, 2, l1, 2))
        for g, q in ((arc1, a), (arc2, b)):
            sk.addConstraint(Sketcher.Constraint("DistanceX", g, 3, q.x))
            sk.addConstraint(Sketcher.Constraint("DistanceY", g, 3, q.y))
        sk.addConstraint(Sketcher.Constraint("Radius", arc1, r))
        sk.addConstraint(Sketcher.Constraint("Equal", arc1, arc2))


def body(nombre, etiqueta):
    b = doc.addObject("PartDesign::Body", nombre)
    b.Label = etiqueta
    return b


def pad(b, cq, largo, nombre):
    f = b.newObject("PartDesign::Pad", nombre)
    f.Profile = cq.sk; f.Length = largo; f.SideType = "Symmetric"
    cq.sk.Visibility = False
    return f


def pocket(b, cq, largo, nombre, pasante=False):
    f = b.newObject("PartDesign::Pocket", nombre)
    f.Profile = cq.sk
    if pasante:
        f.Type = "ThroughAll"
    else:
        f.Length = largo
    f.SideType = "Symmetric"
    cq.sk.Visibility = False
    return f


def fillet_vertical(b, r, condicion, nombre):
    """Redondea aristas verticales que cumplen 'condicion(arista)' sobre el sólido actual del Body."""
    doc.recompute()
    tip = b.Tip
    nombres = []
    for i, e in enumerate(tip.Shape.Edges):
        p0, p1 = e.Vertexes[0].Point, e.Vertexes[-1].Point
        if abs(p0.x - p1.x) < 1e-6 and abs(p0.y - p1.y) < 1e-6 and condicion(p0):
            nombres.append("Edge%d" % (i + 1))
    f = b.newObject("PartDesign::Fillet", nombre)
    f.Base = (tip, nombres); f.Radius = r
    return f


def reemplazar(viejo, b, zona, fab):
    """Comprueba volumen y reemplaza la pieza Part::Feature por el Body, en el mismo grupo."""
    doc.recompute()
    o = doc.getObject(viejo)
    ok_croquis = all(getattr(s, "FullyConstrained", True) for s in b.Group if s.TypeId == "Sketcher::SketchObject")
    vb, vo = b.Shape.Volume, o.Shape.Volume
    dv = abs(vb - vo) / vo * 100 if vo else 0
    REPORTE.append((o.Label, len([s for s in b.Group if s.TypeId == "Sketcher::SketchObject"]), ok_croquis, vb, vo, dv))
    grupo = [g for g in doc.Objects if g.TypeId == "App::DocumentObjectGroup" and o in g.Group][0]
    etiqueta = o.Label
    doc.removeObject(o.Name)
    b.Label = etiqueta
    grupo.addObject(b)
    for prop, val in (("Zona", zona), ("Fabricacion", fab)):
        b.addProperty("App::PropertyString", prop, "Carrito")
        setattr(b, prop, val)


# ----------------------------------------------------------------------------- piezas
g = P["general"]; m = P["marco"]; pf = P["perfil_2020"]; s = pf["lado"]; zc = m["z_inferior"] + s / 2
zp = m["z_inferior"] + s + P["placa_base"]["espesor"]


def seccion_2020(cq, centro, plano):
    """Sección del 2020 con ranuras, como un polígono de 20 vértices, más el agujero central."""
    u0, v0 = centro
    h = s / 2; w = pf["ranura_abertura"] / 2; d = pf["ranura_profundidad"]
    pts2 = [(-h, -h), (-w, -h), (-w, -h + d), (w, -h + d), (w, -h), (h, -h), (h, -w), (h - d, -w), (h - d, w), (h, w),
            (h, h), (w, h), (w, h - d), (-w, h - d), (-w, h), (-h, h), (-h, w), (-h + d, w), (-h + d, -w), (-h, -w)]
    def G(u, v):
        if plano == "YZ": return (0, u0 + u, v0 + v)
        return (u0 + u, 0, v0 + v)   # XZ
    cq.poly([G(u, v) for u, v in pts2])
    cq.circ(G(0, 0), pf["agujero_central"] / 2)


def perfiles():
    for lado, sg in (("Izq", 1), ("Der", -1)):
        b = body("PD_Perfil_" + lado, "x")
        cq = Croquis(b, "YZ", m["largo_perfiles"] / 2, "Croquis_seccion_2020")
        seccion_2020(cq, (sg * m["y_centro_perfiles"], zc), "YZ")
        pad(b, cq, m["largo_perfiles"], "Extrusion_perfil")
        reemplazar("Perfil_" + lado, b, "estructura", "comprada (corte a medida)")
    for i, xc in enumerate(m["travesanos_x_centro"]):
        b = body("PD_Travesano_%d" % (i + 1), "x")
        cq = Croquis(b, "XZ", 0, "Croquis_seccion_2020")
        seccion_2020(cq, (xc, zc), "XZ")
        pad(b, cq, m["travesano_largo"], "Extrusion_travesano")
        reemplazar("Travesano_%d" % (i + 1), b, "estructura", "comprada (corte a medida)")


def escuadras():
    for o in [o for o in doc.Objects if o.Name.startswith("Escuadra_")]:
        bb = o.Shape.BoundBox
        # L: reconstruir desde las dos patas (cajas) leyendo la cara inferior del sólido original
        cara = [f for f in o.Shape.Faces if abs(f.normalAt(0, 0).z + 1) < 1e-6][0]
        pts = [(v.X, v.Y, bb.ZMin) for v in cara.OuterWire.OrderedVertexes]
        b = body("PD_" + o.Name, "x")
        cq = Croquis(b, "XY", (bb.ZMin + bb.ZMax) / 2, "Croquis_L")
        cq.poly([(x, y, (bb.ZMin + bb.ZMax) / 2) for x, y, z in pts])
        pad(b, cq, bb.ZLength, "Extrusion_escuadra")
        reemplazar(o.Name, b, "estructura", "comprada")


def placa_base():
    o = doc.getObject("Placa_base"); pb = P["placa_base"]
    zm = zp - pb["espesor"] / 2
    b = body("PD_Placa_base", "x")
    cq = Croquis(b, "XY", zm, "Croquis_contorno")
    cq.rect((0, -pb["ancho"] / 2, zm), (pb["largo"], pb["ancho"] / 2, zm))
    pad(b, cq, pb["espesor"], "Extrusion_placa")
    fillet_vertical(b, pb["radio_esquinas"], lambda p: True, "Redondeo_esquinas")
    # agujeros y ranuras leídos de la cara superior de la placa original
    cara = [f for f in o.Shape.Faces if abs(f.normalAt(0, 0).z - 1) < 1e-6 and abs(f.CenterOfMass.z - zp) < 1e-6][0]
    cq2 = Croquis(b, "XY", zm, "Croquis_agujeros_y_ranuras")
    for w in cara.Wires:
        if w.isSame(cara.OuterWire):
            continue
        if len(w.Edges) == 1 and hasattr(w.Edges[0].Curve, "Radius"):
            c = w.Edges[0].Curve.Center
            cq2.circ((c.x, c.y, zm), w.Edges[0].Curve.Radius)
        else:
            cq2.poly([(v.X, v.Y, zm) for v in w.OrderedVertexes])
    pocket(b, cq2, 0, "Vaciado_agujeros", pasante=True)
    reemplazar("Placa_base", b, "estructura", "laser")


def soporte_motor(letra, sg):
    mo, so = P["motor_jgb37"], P["soporte_motor"]
    x = g["eje_trasero_x"]; zs = mo["eje_z"]; zc_m = zs + mo["eje_offset"]; yf = mo["cara_y"]
    t, ta, L = so["espesor_placa"], so["espesor_ala"], so["largo_x"]; x0 = x - L / 2
    z0, zr0, zr1 = so["z_inferior"], m["z_inferior"], m["z_inferior"] + s
    yr0 = m["y_centro_perfiles"] - s / 2
    Y = lambda y: sg * y
    b = body("PD_Soporte_motor_" + letra, "x")
    c = Croquis(b, "XZ", Y(yf + t / 2), "Croquis_placa")
    c.rect((x0, 0, z0), (x0 + L, 0, zr1)); pad(b, c, t, "Placa")
    c = Croquis(b, "XY", zr0 - ta / 2, "Croquis_ala")
    c.rect((x0, Y(yr0), 0), (x0 + L, Y(yf + t), 0)); pad(b, c, ta, "Ala")
    hol = so["holgura_motor"]; rc = mo["caja_d"] / 2 + hol / 2; ac = rc + so["pared_cuna"]
    yc0 = so["cuna_y_min"]
    c = Croquis(b, "XZ", Y((yc0 + yf) / 2), "Croquis_cuna")
    c.rect((x - ac, 0, z0), (x + ac, 0, zc_m)); pad(b, c, yf - yc0, "Cuna")
    c = Croquis(b, "XZ", Y((yc0 + yf) / 2), "Croquis_hueco_cuna")
    c.circ((x, 0, zc_m), rc); pocket(b, c, yf - yc0, "Hueco_cuna")
    e, h = so["nervio_espesor"], so["nervio_alto"]
    for i, xn in enumerate((x0 + e / 2, x0 + L - e / 2)):
        c = Croquis(b, "YZ", xn, "Croquis_nervio_%d" % (i + 1))
        c.poly([(0, Y(yf), zr0 - ta), (0, Y(yf - h), zr0 - ta), (0, Y(yf), zr0 - ta - h)])
        pad(b, c, e, "Nervio_%d" % (i + 1))
    c = Croquis(b, "XZ", Y(yf + t / 2), "Croquis_centrador_M3_M5")
    c.circ((x, 0, zs), (mo["centrador_d"] + hol) / 2)
    r0 = mo["agujeros_m3_pcd"] / 2
    for k in range(mo["agujeros_m3_n"]):
        a = math.radians(so["m3_angulo_inicial_deg"] + k * 360.0 / mo["agujeros_m3_n"])
        p1 = (x + (r0 - hol / 2) * math.cos(a), 0, zc_m + (r0 - hol / 2) * math.sin(a))
        p2 = (x + (r0 + hol / 2) * math.cos(a), 0, zc_m + (r0 + hol / 2) * math.sin(a))
        c.ranura(p1, p2, so["agujero_m3"] / 2)
    for dx in so["m5_x_rel"]:
        c.circ((x + dx, 0, zr0 + s / 2), so["agujero_m5"] / 2)
    pocket(b, c, t + 2, "Agujeros_placa")
    c = Croquis(b, "XY", zr0 - ta / 2, "Croquis_M5_ala")
    for dx in so["m5_x_rel"]:
        c.circ((x + dx, Y(m["y_centro_perfiles"]), 0), so["agujero_m5"] / 2)
    pocket(b, c, ta + 2, "Agujeros_ala")
    reemplazar("Soporte_motor_" + letra, b, "potencia", "impresa")


def bandeja():
    d = P["direccion"]; bd = d["bandeja"]; h = d["horquilla"]; sv = P["servos"][d["servo"]]
    x = g["eje_delantero_x"]; yk = d["kingpin_y"]; zt = m["z_inferior"]; hw = m["y_centro_perfiles"] + s / 2
    b = body("PD_Bandeja_delantera", "x")
    c = Croquis(b, "XY", zt - bd["espesor"] / 2, "Croquis_placa")
    c.rect((bd["x_min"], -hw, 0), (bd["x_max"], hw, 0)); pad(b, c, bd["espesor"], "Placa")
    px0, px1 = h["pared_x"]; py0, py1 = h["pared_y"]; zi, zsu, eo, r = h["oreja_inf_z"], h["oreja_sup_z"], h["oreja_espesor"], h["oreja_radio"]
    zpar1 = zt - bd["espesor"]
    c = Croquis(b, "XY", (zi + zpar1) / 2, "Croquis_paredes")
    for sg in (1, -1):
        c.rect((px0, sg * py0, 0), (px1, sg * py1, 0))
    pad(b, c, zpar1 - zi, "Paredes")
    for nivel, z in (("inf", zi), ("sup", zsu)):
        c = Croquis(b, "XY", z + eo / 2, "Croquis_orejas_" + nivel)
        for sg in (1, -1):
            c.rect((x - r, sg * py0, 0), (x + r, sg * yk, 0))
        pad(b, c, eo, "Orejas_" + nivel)
        c = Croquis(b, "XY", z + eo / 2, "Croquis_puntas_orejas_" + nivel)
        for sg in (1, -1):
            c.circ((x, sg * yk, 0), r)
        pad(b, c, eo, "Puntas_orejas_" + nivel)
    c = Croquis(b, "XY", zt - bd["espesor"] / 2, "Croquis_agujeros")
    for sg in (1, -1):
        c.circ((x, sg * yk, 0), d["kingpin_agujero"] / 2)
    hol = P["impresion"]["holgura"]; sx0 = d["servo_x_min"]
    c.rect((sx0 - hol, -sv["ancho"] / 2 - hol, 0), (sx0 + sv["largo"] + hol, sv["ancho"] / 2 + hol, 0))
    cxs = sx0 + sv["largo"] / 2; ax, ay = sv["agujeros_orejas"]
    for dx in (-ax / 2, ax / 2):
        for dy in (-ay / 2, ay / 2):
            c.circ((cxs + dx, dy, 0), sv["agujero"] / 2)
    for xm in bd["m5_x"]:
        for sg in (1, -1):
            c.circ((xm, sg * m["y_centro_perfiles"], 0), P["impresion"]["m5"] / 2)
    pocket(b, c, 200, "Agujeros_y_hueco_servo")
    reemplazar("Bandeja_delantera", b, "sensores", "impresa")


def mangueta(lado, sg):
    d = P["direccion"]; mg = d["mangueta"]; rod = d["rodamiento"]
    x = g["eje_delantero_x"]; yk = d["kingpin_y"]; zs = P["motor_jgb37"]["eje_z"]; z0, z1 = mg["z"]
    Y = lambda y: sg * y
    b = body("PD_Mangueta_" + lado, "x")
    c = Croquis(b, "XY", (z0 + z1) / 2, "Croquis_boss_kingpin"); c.circ((x, Y(yk), 0), mg["boss_kingpin_r"]); pad(b, c, z1 - z0, "Boss_kingpin")
    cx0, cx1 = mg["cuerpo_x"]; yb0, yb1 = mg["boss_rod_y"]
    c = Croquis(b, "XY", (z0 + mg["cuerpo_z_max"]) / 2, "Croquis_cuerpo"); c.rect((cx0, Y(yk), 0), (cx1, Y(yb0), 0)); pad(b, c, mg["cuerpo_z_max"] - z0, "Cuerpo")
    c = Croquis(b, "XZ", Y((yb0 + yb1) / 2), "Croquis_boss_rodamientos"); c.circ((x, 0, zs), mg["boss_rod_r"]); pad(b, c, yb1 - yb0, "Boss_rodamientos")
    ang = math.atan(yk / g["batalla"]); L, w = mg["brazo_largo"], mg["brazo_ancho"]; bz0, bz1 = mg["brazo_z"]
    ux, uy = math.cos(-ang), sg * math.sin(-ang); nx, ny = -uy, ux
    xe, ye = x + L * ux, Y(yk) + L * uy
    c = Croquis(b, "XY", (bz0 + bz1) / 2, "Croquis_brazo")
    c.poly([(x + nx * w / 2, Y(yk) + ny * w / 2, 0), (xe + nx * w / 2, ye + ny * w / 2, 0),
            (xe - nx * w / 2, ye - ny * w / 2, 0), (x - nx * w / 2, Y(yk) - ny * w / 2, 0)])
    pad(b, c, bz1 - bz0, "Brazo")
    c = Croquis(b, "XY", (bz0 + bz1) / 2, "Croquis_punta_brazo"); c.circ((xe, ye, 0), w / 2); pad(b, c, bz1 - bz0, "Punta_brazo")
    c = Croquis(b, "XY", (z0 + z1) / 2, "Croquis_agujeros_kingpin_brazo")
    c.circ((x, Y(yk), 0), d["kingpin_agujero"] / 2); c.circ((xe, ye, 0), mg["brazo_agujero"] / 2)
    pocket(b, c, 0, "Agujeros_kingpin_brazo", pasante=True)
    lr = yb1 + 1 - (yb0 + 0.5)
    c = Croquis(b, "XZ", Y(yb0 + 0.5 + lr / 2), "Croquis_alojamiento_rodamientos"); c.circ((x, 0, zs), (rod["d_ext"] + rod["ajuste"]) / 2)
    pocket(b, c, lr, "Alojamiento_rodamientos")
    reemplazar("Mangueta_" + lado, b, "sensores", "impresa")


def barra():
    d = P["direccion"]; mg = d["mangueta"]; ang = math.atan(d["kingpin_y"] / g["batalla"])
    xe = g["eje_delantero_x"] + mg["brazo_largo"] * math.cos(ang); ye = d["kingpin_y"] - mg["brazo_largo"] * math.sin(ang)
    bz0, bz1 = d["barra"]["z"]; w = d["barra"]["ancho"]
    b = body("PD_Barra_direccion", "x")
    c = Croquis(b, "XY", (bz0 + bz1) / 2, "Croquis_barra")
    c.rect((xe - w / 2, -ye - w / 2, 0), (xe + w / 2, ye + w / 2, 0))
    for sg in (1, -1):
        c.circ((xe, sg * ye, 0), mg["brazo_agujero"] / 2)
    pad(b, c, bz1 - bz0, "Barra")
    reemplazar("Barra_direccion", b, "sensores", "impresa")


def morro():
    mo = P["sensores"]["morro"]; zm = (mo["z_min"] + mo["z_max"]) / 2
    b = body("PD_Morro", "x")
    c = Croquis(b, "XY", zm, "Croquis_planta"); c.rect((mo["x_min"], -mo["y"], 0), (mo["x_max"], mo["y"], 0))
    pad(b, c, mo["z_max"] - mo["z_min"], "Cuerpo")
    fillet_vertical(b, mo["radio_frente"], lambda p: abs(p.x - mo["x_min"]) < 1e-6, "Redondeo_frente")
    c = Croquis(b, "YZ", mo["x_min"] + mo["faro_prof"] / 2, "Croquis_faros")
    for sg in (1, -1):
        c.circ((0, sg * mo["faro_y"], mo["faro_z"]), mo["faro_d"] / 2)
    pocket(b, c, mo["faro_prof"], "Alojamiento_faros")
    c = Croquis(b, "YZ", (mo["x_min"] + mo["x_max"]) / 2, "Croquis_cables_M5")
    for sg in (1, -1):
        c.circ((0, sg * mo["faro_y"], mo["faro_z"]), mo["cable_d"] / 2)
        c.circ((0, sg * mo["m5_y"], mo["m5_z"]), P["impresion"]["m5"] / 2)
    pocket(b, c, 0, "Pasos_cable_y_M5", pasante=True)
    reemplazar("Morro", b, "sensores", "impresa")


def mastil(nombre, cx, cy, brida, esp, sep, col, z_top, plato_xy, plato_esp, hueco=None, salida=None):
    b = body("PD_" + nombre, "x")
    c = Croquis(b, "XY", zp + esp / 2, "Croquis_brida")
    c.rect((cx - brida / 2, cy - brida / 2, 0), (cx + brida / 2, cy + brida / 2, 0))
    for dx in (-sep / 2, sep / 2):
        for dy in (-sep / 2, sep / 2):
            c.circ((cx + dx, cy + dy, 0), P["impresion"]["m3"] / 2)
    pad(b, c, esp, "Brida")
    z1 = z_top - plato_esp
    c = Croquis(b, "XY", (zp + esp + z1) / 2, "Croquis_columna")
    c.rect((cx - col / 2, cy - col / 2, 0), (cx + col / 2, cy + col / 2, 0)); pad(b, c, z1 - zp - esp, "Columna")
    c = Croquis(b, "XY", z_top - plato_esp / 2, "Croquis_plato")
    c.rect((cx - plato_xy[0] / 2, cy - plato_xy[1] / 2, 0), (cx + plato_xy[0] / 2, cy + plato_xy[1] / 2, 0)); pad(b, c, plato_esp, "Plato")
    if hueco:
        c = Croquis(b, "XY", (zp + z_top) / 2, "Croquis_hueco_cable")
        c.rect((cx - hueco / 2, cy - hueco / 2, 0), (cx + hueco / 2, cy + hueco / 2, 0)); pocket(b, c, 0, "Hueco_cable", pasante=True)
    if salida:
        (x0, x1, z0, z1s) = salida
        c = Croquis(b, "XZ", cy, "Croquis_salida_cable")
        c.rect((x0, 0, z0), (x1, 0, z1s)); pocket(b, c, hueco, "Salida_cable")
    return b


def mastiles():
    li, ma = P["sensores"]["lidar"], P["sensores"]["mastil_lidar"]
    b = mastil("Mastil_lidar", li["x"], li["y"], ma["brida"], ma["brida_esp"], ma["m3_sep"], ma["columna"], li["z_base"],
               (ma["plato_largo"], ma["plato_ancho"]), ma["plato_esp"], hueco=ma["hueco"],
               salida=(li["x"], li["x"] + ma["columna"], zp + ma["brida_esp"] + 2, zp + ma["brida_esp"] + 2 + ma["hueco"]))
    reemplazar("Mastil_lidar", b, "sensores", "impresa")
    gp, mg = P["logica"]["gps"], P["logica"]["mastil_gps"]
    b = mastil("Mastil_GPS", gp["x"], gp["y"], mg["brida"], mg["brida_esp"], mg["m3_sep"], mg["columna"], gp["z_base"],
               (gp["lado"] + 2, gp["lado"] + 2), mg["plato_esp"])
    reemplazar("Mastil_GPS", b, "logica", "impresa")


def cuna():
    bt, cn = P["potencia"]["bateria"], P["potencia"]["cuna_bateria"]
    x0 = bt["x_min"] - cn["holgura"]; x1 = bt["x_min"] + bt["ancho"] + cn["holgura"]
    zr = m["z_inferior"]; yp = cn["pared_y"]; hw = m["y_centro_perfiles"] + s / 2; e = cn["espesor"]; zf = cn["z_fondo"]
    xm = (x0 + x1) / 2; rl, ra = cn["ranura_correa"]
    b = body("PD_Cuna_bateria", "x")
    c = Croquis(b, "XY", zf + e / 2, "Croquis_fondo")
    c.rect((x0, -(yp + e), 0), (x1, yp + e, 0))
    for sg in (1, -1):
        c.rect((xm - ra / 2, sg * 30 - rl / 2, 0), (xm + ra / 2, sg * 30 + rl / 2, 0))
    pad(b, c, e, "Fondo")
    c = Croquis(b, "XY", (zf + zr - 4) / 2, "Croquis_paredes")
    for sg in (1, -1):
        c.rect((x0, sg * yp, 0), (x1, sg * (yp + e), 0))
    pad(b, c, zr - 4 - zf, "Paredes")
    c = Croquis(b, "XY", zr - 2, "Croquis_alas")
    for sg in (1, -1):
        c.rect((x0, sg * yp, 0), (x1, sg * hw, 0))
        c.circ((xm, sg * m["y_centro_perfiles"], 0), P["impresion"]["m5"] / 2)
    pad(b, c, 4, "Alas")
    reemplazar("Cuna_bateria", b, "potencia", "impresa")


def pequenos():
    le = P["led_esquina"]
    for nombre, zona in (("Soporte_LED_DIzq", "sensores"), ("Soporte_LED_DDer", "sensores"), ("Soporte_LED_TIzq", "potencia"), ("Soporte_LED_TDer", "potencia")):
        o = doc.getObject(nombre); cc = o.Shape.BoundBox.Center
        b = body("PD_" + nombre, "x")
        c = Croquis(b, "XY", zp + le["alto"] / 2, "Croquis_soporte")
        c.rect((cc.x - le["base"] / 2, cc.y - le["base"] / 2, 0), (cc.x + le["base"] / 2, cc.y + le["base"] / 2, 0))
        c.circ((cc.x, cc.y, 0), P["impresion"]["m3"] / 2)
        pad(b, c, le["alto"], "Soporte")
        reemplazar(nombre, b, zona, "impresa")
    an = P["potencia"]["antena"]
    b = body("PD_Soporte_antena", "x")
    c = Croquis(b, "XY", zp + an["base_alto"] / 2, "Croquis_base")
    c.rect((an["x"] - an["base"] / 2, an["y"] - an["base"] / 2, 0), (an["x"] + an["base"] / 2, an["y"] + an["base"] / 2, 0))
    pad(b, c, an["base_alto"], "Base")
    c = Croquis(b, "XY", zp + 3 + (an["base_alto"] - 3) / 2, "Croquis_alojamiento_antena")
    c.circ((an["x"], an["y"], 0), an["d"] / 2 + P["impresion"]["holgura"])
    pocket(b, c, an["base_alto"] - 3, "Alojamiento_antena")
    reemplazar("Soporte_antena", b, "potencia", "impresa")


def naca_invertido(x0, z0, cuerda, esp, m_=0.04, p_=0.4, n=14):
    """Perfil NACA de 4 dígitos con la curvatura hacia abajo (carga aerodinámica), como polígono cerrado."""
    t = esp / cuerda
    xs = [0.5 * (1 - math.cos(math.pi * i / n)) for i in range(n + 1)]
    sup, inf = [], []
    for x in xs:
        yt = 5 * t * (0.2969 * math.sqrt(x) - 0.1260 * x - 0.3516 * x ** 2 + 0.2843 * x ** 3 - 0.1036 * x ** 4)
        yc = m_ / p_ ** 2 * (2 * p_ * x - x ** 2) if x < p_ else m_ / (1 - p_) ** 2 * ((1 - 2 * p_) + 2 * p_ * x - x ** 2)
        yc = -yc
        sup.append((x, yc + yt)); inf.append((x, yc - yt))
    pts = sup[::-1] + inf[1:-1]
    zmin = min(v for _, v in pts)
    return [(x0 + cuerda * u, 0, z0 + cuerda * (v - zmin)) for u, v in pts]


def aleron():
    a = P["aleron"]; ep = a["endplate"]; pp = a["plano_principal"]; fl = a["flap"]; bl = a["barra_led"]; cl = a["canal_led"]; pi = a["pilon"]
    yi = a["envergadura"] / 2 - ep["espesor"]
    b = body("PD_Aleron", "x")
    for sg, nm in ((1, "izq"), (-1, "der")):
        c = Croquis(b, "XZ", sg * (yi + ep["espesor"] / 2), "Croquis_endplate_" + nm)
        c.rect((a["x_min"], 0, a["z_min"]), (a["x_min"] + ep["largo"], 0, a["z_min"] + ep["alto"]))
        pad(b, c, ep["espesor"], "Endplate_" + nm)
    c = Croquis(b, "XZ", 0, "Croquis_perfil_NACA")
    c.poly(naca_invertido(pp["x_min"], pp["z"], pp["cuerda"], pp["espesor_max"]))
    pad(b, c, 2 * yi + 2, "Plano_principal")
    rot = App.Rotation(V(0, 1, 0), -fl["angulo_deg"]); o = V(fl["x_min"], 0, fl["z"])
    esq = [V(fl["x_min"], 0, fl["z"]), V(fl["x_min"] + fl["cuerda"], 0, fl["z"]), V(fl["x_min"] + fl["cuerda"], 0, fl["z"] + fl["espesor"]), V(fl["x_min"], 0, fl["z"] + fl["espesor"])]
    q = [o + rot.multVec(p - o) for p in esq]
    c = Croquis(b, "XZ", 0, "Croquis_flap"); c.poly([(p.x, 0, p.z) for p in q]); pad(b, c, 2 * yi + 2, "Flap")
    c = Croquis(b, "XZ", 0, "Croquis_barra_LED"); c.rect((bl["x"][0], 0, bl["z"][0]), (bl["x"][1], 0, bl["z"][1])); pad(b, c, 2 * yi + 2, "Barra_LED")
    fx0, fx1 = pi["pie_x"]; cx0, cx1 = pi["columna_x"]; bz0, bz1 = pi["brazo_z"]; tx0, tx1 = pi["tab_x"]
    c = Croquis(b, "XY", zp + pi["pie_esp"] / 2, "Croquis_pie_pilon")
    c.rect((fx0, -pi["pie_y"], 0), (fx1, pi["pie_y"], 0))
    for (x, y) in pi["m3"]:
        c.circ((x, y, 0), P["impresion"]["m3"] / 2)
    pad(b, c, pi["pie_esp"], "Pie_pilon")
    c = Croquis(b, "XY", (zp + bz1) / 2, "Croquis_columna_pilon")
    c.rect((cx0, -pi["columna_y"], 0), (cx1, pi["columna_y"], 0)); pad(b, c, bz1 - zp, "Columna_pilon")
    c = Croquis(b, "XZ", 0, "Croquis_cuello_cisne")
    c.poly([(cx0, 0, bz0), (tx0, 0, bz0), (tx0, 0, pp["z"] + 2), (tx1, 0, pp["z"] + 2), (tx1, 0, bz0), (pi["brazo_x_max"], 0, bz0),
            (pi["brazo_x_max"], 0, bz1), (cx0, 0, bz1)])
    pad(b, c, 2 * pi["columna_y"], "Cuello_cisne")
    zl = (bl["z"][0] + bl["z"][1]) / 2
    c = Croquis(b, "XZ", 0, "Croquis_canal_LED")
    c.rect((bl["x"][1] - cl["profundidad"], 0, zl - cl["ancho"] / 2), (bl["x"][1] + 1, 0, zl + cl["ancho"] / 2))
    pocket(b, c, cl["largo"], "Canal_LED")
    hc = pi["hueco_cable_d"] / 2; xc = (cx0 + cx1) / 2
    c = Croquis(b, "XY", (zp - 1 + bz1 - 1) / 2, "Croquis_cable_vertical"); c.circ((xc, 0, 0), hc); pocket(b, c, bz1 - zp, "Cable_vertical")
    c = Croquis(b, "YZ", (xc + bl["x"][1] - 1) / 2, "Croquis_cable_horizontal"); c.circ((0, 0, (bz0 + bz1) / 2), hc); pocket(b, c, bl["x"][1] - 1 - xc, "Cable_horizontal")
    reemplazar("Aleron", b, "aleron", "impresa")


for fn in (perfiles, escuadras, placa_base, lambda: soporte_motor("A", 1), lambda: soporte_motor("B", -1), bandeja,
           lambda: mangueta("Izq", 1), lambda: mangueta("Der", -1), barra, morro, mastiles, cuna, pequenos, aleron):
    try:
        fn()
    except Exception:
        import traceback; traceback.print_exc(); raise
doc.recompute()
doc.save()

print("\n%-40s %8s %10s %12s %12s %8s" % ("Pieza", "Croquis", "Restring.", "Vol. nuevo", "Vol. orig.", "Dif %"))
for et, n, ok, vb, vo, dv in REPORTE:
    print("%-40s %8d %10s %12.0f %12.0f %8.2f" % (et[:40], n, "TOTAL" if ok else "FALTA", vb, vo, dv))
print("PIEZAS:", len(REPORTE), " CROQUIS:", sum(r[1] for r in REPORTE),
      " RESTRINGIDAS:", sum(1 for r in REPORTE if r[2]), " DIF>0.5%:", [r[0] for r in REPORTE if r[5] > 0.5])
