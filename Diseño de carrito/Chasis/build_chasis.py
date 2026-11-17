# -*- coding: utf-8 -*-
"""
Chasis paramétrico del carrito MiguelOHM.

Uso (desde la carpeta Chasis):
    freecadcmd build_chasis.py            -> construye el carrito completo (ETAPA=6 por defecto)
    Desde FreeCAD (Macro > Ejecutar) también colorea por zona y deja el modelo abierto.
    set ETAPA=2 & freecadcmd build_chasis.py   (Windows)
    ETAPA=2 freecadcmd build_chasis.py         (Linux/WSL)

Todas las medidas salen de params.json. El script no tiene números de diseño propios.
Salida: carrito_ensamble.FCStd (+ exportaciones por etapa).
"""
import json
import os
import sys

import FreeCAD as App
import sys
try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass
import Part

BASE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd()
P = json.load(open(os.path.join(BASE, "params.json"), encoding="utf-8"))
ETAPA = int(os.environ.get("ETAPA", "6"))
V = App.Vector


# ----------------------------------------------------------------------------- utilidades
def caja_real(shape):
    """Caja envolvente fiable en FreeCAD 1.1 (OCCT 7) y 26.3 (OCCT 8):
    intersección de BoundBox y optimalBoundingBox()."""
    a = shape.BoundBox
    try:
        b = shape.optimalBoundingBox()
    except Exception:
        return a
    return App.BoundBox(max(a.XMin, b.XMin), max(a.YMin, b.YMin), max(a.ZMin, b.ZMin),
                        min(a.XMax, b.XMax), min(a.YMax, b.YMax), min(a.ZMax, b.ZMax))


def seccion_2020():
    """Sección simplificada del perfil 2020 (cuadrado con 4 ranuras y agujero central), centrada en el origen, plano XY."""
    pf = P["perfil_2020"]
    s = pf["lado"]
    w = pf["ranura_abertura"]
    d = pf["ranura_profundidad"]
    sq = Part.makeBox(s, s, 1, V(-s / 2, -s / 2, 0))
    cortes = [
        Part.makeBox(w, d, 1, V(-w / 2, s / 2 - d, 0)),      # +Y
        Part.makeBox(w, d, 1, V(-w / 2, -s / 2, 0)),         # -Y
        Part.makeBox(d, w, 1, V(s / 2 - d, -w / 2, 0)),      # +X
        Part.makeBox(d, w, 1, V(-s / 2, -w / 2, 0)),         # -X
        Part.makeCylinder(pf["agujero_central"] / 2, 1),
    ]
    for c in cortes:
        sq = sq.cut(c)
    cara = [f for f in sq.Faces if abs(f.normalAt(0, 0).z + 1) < 1e-6][0]  # cara inferior z=0
    return cara


def perfil_2020(largo, eje, origen):
    """Perfil 2020 de 'largo' a lo largo de 'eje' ('X' o 'Y'), con la sección centrada en 'origen' (inicio del perfil)."""
    sol = seccion_2020().extrude(V(0, 0, largo))
    if eje == "X":
        sol.rotate(V(0, 0, 0), V(0, 1, 0), 90)    # +Z -> +X
    elif eje == "Y":
        sol.rotate(V(0, 0, 0), V(1, 0, 0), -90)   # +Z -> +Y
    sol.translate(origen)
    return sol


def agregar(doc, nombre, etiqueta, shape, grupo, zona):
    obj = doc.addObject("Part::Feature", nombre)
    obj.Label = etiqueta
    obj.Shape = shape
    obj.addProperty("App::PropertyString", "Zona", "Carrito", "Zona del carrito").Zona = zona
    obj.addProperty("App::PropertyString", "Fabricacion", "Carrito", "Comprada / impresa / laser").Fabricacion = ""
    grupo.addObject(obj)
    return obj


# ----------------------------------------------------------------------------- etapa 1
def etapa1_marco(doc):
    m = P["marco"]
    s = P["perfil_2020"]["lado"]
    zc = m["z_inferior"] + s / 2
    grupo = doc.addObject("App::DocumentObjectGroup", "Estructura")
    grupo.Label = "1_Estructura"
    objs = []

    # Perfiles longitudinales
    for lado, signo in (("Izq", 1), ("Der", -1)):
        sh = perfil_2020(m["largo_perfiles"], "X", V(0, signo * m["y_centro_perfiles"], zc))
        o = agregar(doc, "Perfil_" + lado, "Perfil 2020 %s L=%d" % (lado, m["largo_perfiles"]), sh, grupo, "estructura")
        o.Fabricacion = "comprada (corte a medida)"
        objs.append(o)

    # Travesaños
    y_int = m["y_centro_perfiles"] - s / 2
    for i, xc in enumerate(m["travesanos_x_centro"]):
        sh = perfil_2020(m["travesano_largo"], "Y", V(xc, -y_int, zc))
        o = agregar(doc, "Travesano_%d" % (i + 1), "Travesano 2020 x=%g L=%d" % (xc, m["travesano_largo"]), sh, grupo, "estructura")
        o.Fabricacion = "comprada (corte a medida)"
        objs.append(o)

    # Escuadras interiores (simplificadas), en las esquinas internas del marco
    e, t, h = m["escuadra_lado"], m["escuadra_espesor"], m["escuadra_alto"]
    z0 = m["z_inferior"] + (s - h) / 2
    xs = m["travesanos_x_centro"]
    n = 0
    for xc in xs:
        caras = []
        if xc - s / 2 - e >= 0:
            caras.append(-1)                       # lado delantero del travesaño
        if xc + s / 2 + e <= m["largo_perfiles"]:
            caras.append(+1)                       # lado trasero del travesaño
        for sx in caras:
            for sy in (1, -1):
                xf = xc + sx * s / 2               # cara lateral del travesaño
                yf = sy * y_int                    # cara interior del perfil
                pata_perfil = Part.makeBox(e, t, h, V(xf if sx > 0 else xf - e, yf - t if sy > 0 else yf, z0))
                pata_trav = Part.makeBox(t, e, h, V(xf if sx > 0 else xf - t, yf - e if sy > 0 else yf, z0))
                sh = pata_perfil.fuse(pata_trav).removeSplitter()
                n += 1
                o = agregar(doc, "Escuadra_%d" % n, "Escuadra 20 (%s, %s)" % ("tras" if sx > 0 else "del", "izq" if sy > 0 else "der"),
                            sh, grupo, "estructura")
                o.Fabricacion = "comprada"
                objs.append(o)

    # Placa base
    pb = P["placa_base"]
    zt = m["z_inferior"] + s
    placa = Part.makeBox(pb["largo"], pb["ancho"], pb["espesor"], V(0, -pb["ancho"] / 2, zt))
    aristas = [ed for ed in placa.Edges if abs(ed.Vertexes[0].Point.z - ed.Vertexes[1].Point.z) > 1e-6]
    placa = placa.makeFillet(pb["radio_esquinas"], aristas)
    for x in pb["agujeros_perfil_x"]:
        for sy in (1, -1):
            placa = placa.cut(Part.makeCylinder(pb["agujeros_perfil_d"] / 2, pb["espesor"] + 2,
                                                V(x, sy * m["y_centro_perfiles"], zt - 1)))
    for r in pb["ranuras_cable"]:
        ranura = Part.makeBox(r["largo_x"], r["ancho_y"], pb["espesor"] + 2,
                              V(r["x"] - r["largo_x"] / 2, r["y"] - r["ancho_y"] / 2, zt - 1))
        placa = placa.cut(ranura)
    o = agregar(doc, "Placa_base", "Placa base %dx%dx%g (%s)" % (pb["largo"], pb["ancho"], pb["espesor"], pb["material"]),
                placa, grupo, "estructura")
    o.Fabricacion = "laser" if pb["material"] == "acrilico" else "impresa (2 partes)"
    objs.append(o)
    return objs


# ----------------------------------------------------------------------------- etapa 2
def cil_y(r, y0, y1, x, z):
    """Cilindro de radio r a lo largo de Y, de y0 a y1, con eje en (x, z)."""
    return Part.makeCylinder(r, y1 - y0, V(x, y0, z), V(0, 1, 0))


def motor_jgb37(espejo):
    """Motor JGB37-520 con encoder. Cara frontal en Y=+cara_y, cuerpo hacia +Y menor (adentro). espejo=-1 para el lado derecho."""
    mo = P["motor_jgb37"]
    x = P["general"]["eje_trasero_x"]
    zs = mo["eje_z"]                  # eje de salida
    zc = zs + mo["eje_offset"]        # centro del motor (eje desplazado hacia abajo)
    yf = mo["cara_y"]
    y1 = yf - mo["caja_largo"]
    y2 = y1 - mo["motor_largo"]
    y3 = y2 - mo["encoder_largo"]
    caja = cil_y(mo["caja_d"] / 2, y1, yf, x, zc)
    cuerpo = cil_y(mo["motor_d"] / 2, y2, y1, x, zc)
    enc = cil_y(mo["motor_d"] / 2, y3, y2, x, zc)
    centrador = cil_y(mo["centrador_d"] / 2, yf, yf + mo["centrador_alto"], x, zs)
    eje = cil_y(mo["eje_d"] / 2, yf, yf + mo["eje_largo"], x, zs)
    # plano del eje en D
    corte = Part.makeBox(10, mo["eje_largo"], 10, V(x - 5, yf + mo["centrador_alto"], zs + mo["eje_plano"] - mo["eje_d"] / 2))
    eje = eje.cut(corte)
    sh = caja.fuse([cuerpo, enc, centrador, eje]).removeSplitter()
    if espejo < 0:
        sh = sh.mirror(V(0, 0, 0), V(0, 1, 0))
    return sh


def soporte_motor(espejo):
    """Escuadra impresa para JGB37-520 sobre perfil 2020, con holgura para que el motor encaje."""
    mo, so, m = P["motor_jgb37"], P["soporte_motor"], P["marco"]
    s = P["perfil_2020"]["lado"]
    hol = so["holgura_motor"]
    x = P["general"]["eje_trasero_x"]
    zs = mo["eje_z"]; zc = zs + mo["eje_offset"]
    yf = mo["cara_y"]                                    # cara exterior del perfil = cara del motor
    t, ta, L = so["espesor_placa"], so["espesor_ala"], so["largo_x"]
    z0, zr0, zr1 = so["z_inferior"], m["z_inferior"], m["z_inferior"] + s
    yr0 = m["y_centro_perfiles"] - s / 2                 # cara interior del perfil
    x0 = x - L / 2

    placa = Part.makeBox(L, t, zr1 - z0, V(x0, yf, z0))                     # placa vertical (sube hasta la parte superior del perfil)
    ala = Part.makeBox(L, yf - yr0, ta, V(x0, yr0, zr0 - ta))               # ala bajo el perfil
    rc = mo["caja_d"] / 2 + hol / 2                                         # radio interior de la cuna
    ancho_cuna = 2 * (rc + so["pared_cuna"])
    cuna = Part.makeBox(ancho_cuna, yf - so["cuna_y_min"], zc - z0, V(x - ancho_cuna / 2, so["cuna_y_min"], z0))
    cuna = cuna.cut(cil_y(rc, so["cuna_y_min"] - 1, yf + 1, x, zc))
    # nervios en los extremos, entre la placa y el ala
    nervios = []
    e, h = so["nervio_espesor"], so["nervio_alto"]
    for xn in (x0, x0 + L - e):
        tri = Part.makePolygon([V(xn, yf, zr0 - ta), V(xn, yf - h, zr0 - ta), V(xn, yf, zr0 - ta - h), V(xn, yf, zr0 - ta)])
        nervios.append(Part.Face(tri).extrude(V(e, 0, 0)))
    sh = placa.fuse([ala, cuna] + nervios).removeSplitter()

    # agujero del centrador (con holgura) y ranuras radiales M3
    sh = sh.cut(cil_y((mo["centrador_d"] + hol) / 2, yf - 1, yf + t + 1, x, zs))
    import math
    r0 = mo["agujeros_m3_pcd"] / 2
    for k in range(mo["agujeros_m3_n"]):
        a = math.radians(so["m3_angulo_inicial_deg"] + k * 360.0 / mo["agujeros_m3_n"])
        ranura = None
        n_pasos = 6
        for i in range(n_pasos + 1):
            r = r0 - hol / 2 + hol * i / n_pasos
            c = cil_y(so["agujero_m3"] / 2, yf - 1, yf + t + 1, x + r * math.cos(a), zc + r * math.sin(a))
            ranura = c if ranura is None else ranura.fuse(c)
        sh = sh.cut(ranura)
    # tornillos M5 al perfil: 2 en el ala (ranura inferior) y 2 en la placa (ranura lateral)
    for dx in so["m5_x_rel"]:
        sh = sh.cut(Part.makeCylinder(so["agujero_m5"] / 2, ta + 2, V(x + dx, m["y_centro_perfiles"], zr0 - ta - 1)))
        sh = sh.cut(cil_y(so["agujero_m5"] / 2, yf - 1, yf + t + 1, x + dx, zr0 + s / 2))
    sh = sh.removeSplitter()
    if espejo < 0:
        sh = sh.mirror(V(0, 0, 0), V(0, 1, 0))
    return sh


def rueda_y_acople(espejo, x):
    ru, ac = P["ruedas"], P["acople_hex"]
    zs = ru["diametro"] / 2
    y0 = ru["cara_interior_y"]
    rueda = cil_y(ru["diametro"] / 2, y0, y0 + ru["ancho"], x, zs)
    rueda = rueda.cut(cil_y(ru["buje_d"] / 2, y0 - 1, y0 + ru["ancho"] + 1, x, zs))
    acople = cil_y(ac["d"] / 2, ac["y_min"], ac["y_min"] + ac["largo"], x, zs)
    acople = acople.cut(cil_y(P["motor_jgb37"]["eje_d"] / 2, ac["y_min"] - 1, ac["y_min"] + ac["largo"] + 1, x, zs))
    if espejo < 0:
        rueda = rueda.mirror(V(0, 0, 0), V(0, 1, 0))
        acople = acople.mirror(V(0, 0, 0), V(0, 1, 0))
    return rueda, acople


def rueda_con_acople(espejo, x, ac):
    """Rueda + acople hexagonal con parámetros de acople dados (traseras usan acople_hex, delanteras acople_delantero)."""
    ru = P["ruedas"]
    zs = ru["diametro"] / 2
    y0 = ru["cara_interior_y"]
    rueda = cil_y(ru["diametro"] / 2, y0, y0 + ru["ancho"], x, zs)
    rueda = rueda.cut(cil_y(ru["buje_d"] / 2, y0 - 1, y0 + ru["ancho"] + 1, x, zs))
    acople = cil_y(ac["d"] / 2, ac["y_min"], ac["y_min"] + ac["largo"], x, zs)
    acople = acople.cut(cil_y(P["motor_jgb37"]["eje_d"] / 2, ac["y_min"] - 1, ac["y_min"] + ac["largo"] + 1, x, zs))
    if espejo < 0:
        rueda = rueda.mirror(V(0, 0, 0), V(0, 1, 0))
        acople = acople.mirror(V(0, 0, 0), V(0, 1, 0))
    return rueda, acople


def etapa2_motores(doc):
    grupo = doc.addObject("App::DocumentObjectGroup", "Traccion")
    grupo.Label = "2_Traccion_trasera"
    objs = []
    x = P["general"]["eje_trasero_x"]
    for lado, sg, letra in (("Izq", 1, "A"), ("Der", -1, "B")):
        o = agregar(doc, "Motor_" + letra, "Motor %s JGB37-520 (%s)" % (letra, lado), motor_jgb37(sg), grupo, "motor")
        o.Fabricacion = "comprada"; objs.append(o)
        o = agregar(doc, "Soporte_motor_" + letra, "Soporte motor %s (holgura %.1f)" % (letra, P["soporte_motor"]["holgura_motor"]),
                    soporte_motor(sg), grupo, "potencia")
        o.Fabricacion = "impresa"; objs.append(o)
        rueda, acople = rueda_y_acople(sg, x)
        o = agregar(doc, "Rueda_T" + lado, "Rueda trasera %s D%g" % (lado, P["ruedas"]["diametro"]), rueda, grupo, "rueda")
        o.Fabricacion = "comprada"; objs.append(o)
        o = agregar(doc, "Acople_T" + lado, "Acople hexagonal 6 mm %s" % lado, acople, grupo, "estructura")
        o.Fabricacion = "comprada"; objs.append(o)
    return objs


# ----------------------------------------------------------------------------- etapa 3
import math


def _espejar(sh, sg):
    return sh.mirror(V(0, 0, 0), V(0, 1, 0)) if sg < 0 else sh


def angulo_ackermann():
    d = P["direccion"]
    return math.degrees(math.atan(d["kingpin_y"] / P["general"]["batalla"]))


def mangueta(sg):
    """Mangueta izquierda (sg=+1) o derecha (sg=-1): boss del kingpin, cuerpo, boss de rodamientos y brazo Ackermann."""
    d = P["direccion"]; mg = d["mangueta"]; rod = d["rodamiento"]
    x = P["general"]["eje_delantero_x"]; yk = d["kingpin_y"]
    zs = P["motor_jgb37"]["eje_z"]
    z0, z1 = mg["z"]
    boss_k = Part.makeCylinder(mg["boss_kingpin_r"], z1 - z0, V(x, yk, z0))
    cx0, cx1 = mg["cuerpo_x"]
    y_b0, y_b1 = mg["boss_rod_y"]
    cuerpo = Part.makeBox(cx1 - cx0, y_b0 - yk, mg["cuerpo_z_max"] - z0, V(cx0, yk, z0))
    boss_r = cil_y(mg["boss_rod_r"], y_b0, y_b1, x, zs)
    # brazo Ackermann hacia atrás y hacia adentro
    L, w = mg["brazo_largo"], mg["brazo_ancho"]
    bz0, bz1 = mg["brazo_z"]
    brazo = Part.makeBox(L, w, bz1 - bz0, V(x, yk - w / 2, bz0))
    brazo = brazo.fuse(Part.makeCylinder(w / 2, bz1 - bz0, V(x + L, yk, bz0)))
    brazo.rotate(V(x, yk, 0), V(0, 0, 1), -angulo_ackermann())
    sh = boss_k.fuse([cuerpo, boss_r, brazo]).removeSplitter()
    # agujeros
    sh = sh.cut(Part.makeCylinder(d["kingpin_agujero"] / 2, z1 - z0 + 2, V(x, yk, z0 - 1)))
    sh = sh.cut(cil_y((rod["d_ext"] + rod["ajuste"]) / 2, y_b0 + 0.5, y_b1 + 1, x, zs))
    a = math.radians(-angulo_ackermann())
    xe, ye = x + L * math.cos(a), yk + L * math.sin(a)
    sh = sh.cut(Part.makeCylinder(mg["brazo_agujero"] / 2, bz1 - bz0 + 2, V(xe, ye, bz0 - 1)))
    return _espejar(sh.removeSplitter(), sg)


def extremo_brazo():
    d = P["direccion"]; mg = d["mangueta"]
    a = math.radians(-angulo_ackermann())
    return (P["general"]["eje_delantero_x"] + mg["brazo_largo"] * math.cos(a),
            d["kingpin_y"] + mg["brazo_largo"] * math.sin(a))


def bandeja_delantera():
    """Bandeja impresa bajo los perfiles: sostiene las horquillas del kingpin y el servo invertido."""
    d = P["direccion"]; b = d["bandeja"]; h = d["horquilla"]; m = P["marco"]
    sv = P["servos"][d["servo"]]
    x = P["general"]["eje_delantero_x"]; yk = d["kingpin_y"]
    zt = m["z_inferior"]
    hw = m["y_centro_perfiles"] + P["perfil_2020"]["lado"] / 2
    placa = Part.makeBox(b["x_max"] - b["x_min"], 2 * hw, b["espesor"], V(b["x_min"], -hw, zt - b["espesor"]))
    lado = []
    px0, px1 = h["pared_x"]; py0, py1 = h["pared_y"]
    zi, zsu, eo = h["oreja_inf_z"], h["oreja_sup_z"], h["oreja_espesor"]
    pared = Part.makeBox(px1 - px0, py1 - py0, zt - b["espesor"] - zi, V(px0, py0, zi))
    r = h["oreja_radio"]
    for z in (zi, zsu):
        oreja = Part.makeBox(2 * r, yk - py0, eo, V(x - r, py0, z)).fuse(Part.makeCylinder(r, eo, V(x, yk, z)))
        lado.append(oreja)
    izq = pared.fuse(lado)
    # la oreja superior se une a la placa con un tabique vertical en la pared
    sh = placa.fuse([izq, izq.mirror(V(0, 0, 0), V(0, 1, 0))]).removeSplitter()
    for sg in (1, -1):
        sh = sh.cut(Part.makeCylinder(d["kingpin_agujero"] / 2, zt + 2, V(x, sg * yk, 0)))
    # hueco del servo y agujeros de sus orejas
    sx0 = d["servo_x_min"]
    hol = P["impresion"]["holgura"]
    sh = sh.cut(Part.makeBox(sv["largo"] + 2 * hol, sv["ancho"] + 2 * hol, b["espesor"] + 2,
                             V(sx0 - hol, -sv["ancho"] / 2 - hol, zt - b["espesor"] - 1)))
    cxs = sx0 + sv["largo"] / 2
    ax, ay = sv["agujeros_orejas"]
    for dx in (-ax / 2, ax / 2):
        for dy in (-ay / 2, ay / 2):
            sh = sh.cut(Part.makeCylinder(sv["agujero"] / 2, b["espesor"] + 2, V(cxs + dx, dy, zt - b["espesor"] - 1)))
    # tornillos M5 a la ranura inferior de los perfiles
    for xm in b["m5_x"]:
        for sg in (1, -1):
            sh = sh.cut(Part.makeCylinder(P["impresion"]["m5"] / 2, b["espesor"] + 2,
                                          V(xm, sg * m["y_centro_perfiles"], zt - b["espesor"] - 1)))
    return sh.removeSplitter()


def servo_invertido():
    d = P["direccion"]; sv = P["servos"][d["servo"]]
    x0 = d["servo_x_min"]
    zb = d["brazo_servo"]["z"][1] + sv["boss_alto"]            # base del cuerpo (eje hacia abajo)
    cuerpo = Part.makeBox(sv["largo"], sv["ancho"], sv["cuerpo_alto"], V(x0, -sv["ancho"] / 2, zb))
    oz0, oz1 = sv["orejas_z"]
    orejas = Part.makeBox(sv["orejas_largo"], sv["ancho"], oz1 - oz0,
                          V(x0 + sv["largo"] / 2 - sv["orejas_largo"] / 2, -sv["ancho"] / 2, oz0))
    xe = x0 + d["servo_eje_desde_frente"]
    boss = Part.makeCylinder(sv["boss_d"] / 2, sv["boss_alto"], V(xe, 0, zb - sv["boss_alto"]))
    return cuerpo.fuse([orejas, boss]).removeSplitter(), xe


def etapa3_direccion(doc):
    grupo = doc.addObject("App::DocumentObjectGroup", "Direccion")
    grupo.Label = "3_Direccion"
    objs = []
    d = P["direccion"]; rod = d["rodamiento"]
    x = P["general"]["eje_delantero_x"]; zs = P["motor_jgb37"]["eje_z"]
    o = agregar(doc, "Bandeja_delantera", "Bandeja delantera (horquillas + servo)", bandeja_delantera(), grupo, "sensores")
    o.Fabricacion = "impresa"; objs.append(o)
    for lado, sg in (("Izq", 1), ("Der", -1)):
        o = agregar(doc, "Mangueta_" + lado, "Mangueta %s" % lado, mangueta(sg), grupo, "sensores")
        o.Fabricacion = "impresa"; objs.append(o)
        kp = Part.makeCylinder(d["kingpin_d"] / 2, d["kingpin_z"][1] - d["kingpin_z"][0], V(x, sg * d["kingpin_y"], d["kingpin_z"][0]))
        o = agregar(doc, "Kingpin_" + lado, "Kingpin M4 %s" % lado, kp, grupo, "estructura")
        o.Fabricacion = "comprada (tornillo M4x55 + tuerca)"; objs.append(o)
        y0 = d["mangueta"]["boss_rod_y"][0] + 0.5
        for i in range(2):
            anillo = cil_y(rod["d_ext"] / 2, y0 + i * rod["ancho"], y0 + (i + 1) * rod["ancho"], x, zs)
            anillo = anillo.cut(cil_y(rod["d_int"] / 2, y0 - 1, y0 + 3 * rod["ancho"], x, zs))
            o = agregar(doc, "Rodamiento_%s_%d" % (lado, i + 1), "Rodamiento %s %s %d" % (rod["modelo"], lado, i + 1),
                        _espejar(anillo, sg), grupo, "estructura")
            o.Fabricacion = "comprada"; objs.append(o)
        ey0, ey1 = d["eje_delantero"]["y"]
        o = agregar(doc, "Eje_D" + lado, "Eje delantero 6 mm %s" % lado,
                    _espejar(cil_y(d["eje_delantero"]["d"] / 2, ey0, ey1, x, zs), sg), grupo, "estructura")
        o.Fabricacion = "comprada (varilla 6 mm o tornillo M6)"; objs.append(o)
        rueda, acople = rueda_con_acople(sg, x, P["acople_delantero"])
        o = agregar(doc, "Rueda_D" + lado, "Rueda delantera %s D%g" % (lado, P["ruedas"]["diametro"]), rueda, grupo, "rueda")
        o.Fabricacion = "comprada"; objs.append(o)
        o = agregar(doc, "Acople_D" + lado, "Acople hexagonal 6 mm delantero %s" % lado, acople, grupo, "estructura")
        o.Fabricacion = "comprada"; objs.append(o)
    # barra de dirección entre los extremos de los brazos
    xe, ye = extremo_brazo()
    bz0, bz1 = d["barra"]["z"]; w = d["barra"]["ancho"]
    barra = Part.makeBox(w, 2 * ye + w, bz1 - bz0, V(xe - w / 2, -ye - w / 2, bz0))
    for sg in (1, -1):
        barra = barra.cut(Part.makeCylinder(d["mangueta"]["brazo_agujero"] / 2, bz1 - bz0 + 2, V(xe, sg * ye, bz0 - 1)))
    o = agregar(doc, "Barra_direccion", "Barra de direccion", barra, grupo, "sensores")
    o.Fabricacion = "impresa"; objs.append(o)
    # servo invertido y su brazo
    servo, xsv = servo_invertido()
    o = agregar(doc, "Servo", "Servo %s (invertido)" % d["servo"], servo, grupo, "motor")
    o.Fabricacion = "comprada"; objs.append(o)
    bz0, bz1 = d["brazo_servo"]["z"]; w = d["brazo_servo"]["ancho"]
    brazo = Part.makeBox(xsv - xe, w, bz1 - bz0, V(xe, -w / 2, bz0)).fuse(Part.makeCylinder(w, bz1 - bz0, V(xsv, 0, bz0)))
    o = agregar(doc, "Brazo_servo", "Brazo del servo (horn)", brazo.removeSplitter(), grupo, "motor")
    o.Fabricacion = "comprada (horn del servo)"; objs.append(o)
    return objs


# ----------------------------------------------------------------------------- etapa 4
AGUJEROS_PLACA = []   # (x, y, d): agujeros que se cortan en la placa base para fijar cada pieza


def z_placa():
    return P["marco"]["z_inferior"] + P["perfil_2020"]["lado"] + P["placa_base"]["espesor"]


def brida_con_m3(lado, esp, cx, cy, sep, z0):
    b = Part.makeBox(lado, lado, esp, V(cx - lado / 2, cy - lado / 2, z0))
    for dx in (-sep / 2, sep / 2):
        for dy in (-sep / 2, sep / 2):
            b = b.cut(Part.makeCylinder(P["impresion"]["m3"] / 2, esp + 2, V(cx + dx, cy + dy, z0 - 1)))
            AGUJEROS_PLACA.append((cx + dx, cy + dy, P["impresion"]["m3"]))
    return b


def morro():
    mo = P["sensores"]["morro"]
    caja = Part.makeBox(mo["x_max"] - mo["x_min"], 2 * mo["y"], mo["z_max"] - mo["z_min"], V(mo["x_min"], -mo["y"], mo["z_min"]))
    aristas = [e for e in caja.Edges if abs(e.Vertexes[0].Point.z - e.Vertexes[1].Point.z) > 1e-6
               and abs(e.Vertexes[0].Point.x - mo["x_min"]) < 1e-6]
    caja = caja.makeFillet(mo["radio_frente"], aristas)
    for sg in (1, -1):
        y = sg * mo["faro_y"]
        caja = caja.cut(Part.makeCylinder(mo["faro_d"] / 2, mo["faro_prof"] + 1, V(mo["x_min"] - 1, y, mo["faro_z"]), V(1, 0, 0)))
        caja = caja.cut(Part.makeCylinder(mo["cable_d"] / 2, mo["x_max"] - mo["x_min"] + 2, V(mo["x_min"] - 1, y, mo["faro_z"]), V(1, 0, 0)))
        caja = caja.cut(Part.makeCylinder(P["impresion"]["m5"] / 2, mo["x_max"] - mo["x_min"] + 2,
                                          V(mo["x_min"] - 1, sg * mo["m5_y"], mo["m5_z"]), V(1, 0, 0)))
    return caja.removeSplitter()


def mastil_lidar():
    s = P["sensores"]; li, ma = s["lidar"], s["mastil_lidar"]
    zp = z_placa()
    brida = brida_con_m3(ma["brida"], ma["brida_esp"], li["x"], li["y"], ma["m3_sep"], zp)
    col = Part.makeBox(ma["columna"], ma["columna"], li["z_base"] - ma["plato_esp"] - zp - ma["brida_esp"],
                       V(li["x"] - ma["columna"] / 2, li["y"] - ma["columna"] / 2, zp + ma["brida_esp"]))
    plato = Part.makeBox(ma["plato_largo"], ma["plato_ancho"], ma["plato_esp"],
                         V(li["x"] - ma["plato_largo"] / 2, li["y"] - ma["plato_ancho"] / 2, li["z_base"] - ma["plato_esp"]))
    sh = brida.fuse([col, plato]).removeSplitter()
    sh = sh.cut(Part.makeBox(ma["hueco"], ma["hueco"], li["z_base"] - zp + 2, V(li["x"] - ma["hueco"] / 2, li["y"] - ma["hueco"] / 2, zp - 1)))
    # salida lateral del cable en la base de la columna
    sh = sh.cut(Part.makeBox(ma["columna"], ma["hueco"], ma["hueco"], V(li["x"], li["y"] - ma["hueco"] / 2, zp + ma["brida_esp"] + 2)))
    return sh.removeSplitter()


def mastil_gps():
    g, ma = P["logica"]["gps"], P["logica"]["mastil_gps"]
    zp = z_placa()
    brida = brida_con_m3(ma["brida"], ma["brida_esp"], g["x"], g["y"], ma["m3_sep"], zp)
    col = Part.makeBox(ma["columna"], ma["columna"], g["z_base"] - ma["plato_esp"] - zp - ma["brida_esp"],
                       V(g["x"] - ma["columna"] / 2, g["y"] - ma["columna"] / 2, zp + ma["brida_esp"]))
    plato = Part.makeBox(g["lado"] + 2, g["lado"] + 2, ma["plato_esp"],
                         V(g["x"] - g["lado"] / 2 - 1, g["y"] - g["lado"] / 2 - 1, g["z_base"] - ma["plato_esp"]))
    return brida.fuse([col, plato]).removeSplitter()


def cuna_bateria():
    b, c, m = P["potencia"]["bateria"], P["potencia"]["cuna_bateria"], P["marco"]
    s = P["perfil_2020"]["lado"]
    x0 = b["x_min"] - c["holgura"]; x1 = b["x_min"] + b["ancho"] + c["holgura"]
    zr = m["z_inferior"]; yp = c["pared_y"]; hw = m["y_centro_perfiles"] + s / 2; e = c["espesor"]
    fondo = Part.makeBox(x1 - x0, 2 * (yp + e), e, V(x0, -(yp + e), c["z_fondo"]))
    partes = []
    for sg in (1, -1):
        y_pared = yp if sg > 0 else -(yp + e)
        partes.append(Part.makeBox(x1 - x0, e, zr - 4 - c["z_fondo"], V(x0, y_pared, c["z_fondo"])))   # pared lateral
        y_ala = yp if sg > 0 else -hw
        partes.append(Part.makeBox(x1 - x0, hw - yp, 4, V(x0, y_ala, zr - 4)))                       # ala bajo el perfil
    sh = fondo.fuse(partes).removeSplitter()
    xm = (x0 + x1) / 2
    for sg in (1, -1):
        sh = sh.cut(Part.makeCylinder(P["impresion"]["m5"] / 2, 6, V(xm, sg * m["y_centro_perfiles"], zr - 5)))
        rl, ra = c["ranura_correa"]
        sh = sh.cut(Part.makeBox(ra, rl, e + 2, V(xm - ra / 2, sg * 30 - rl / 2, c["z_fondo"] - 1)))
    return sh.removeSplitter()


def soporte_led(x, y):
    le = P["led_esquina"]; zp = z_placa()
    sh = Part.makeBox(le["base"], le["base"], le["alto"], V(x - le["base"] / 2, y - le["base"] / 2, zp))
    sh = sh.cut(Part.makeCylinder(P["impresion"]["m3"] / 2, le["alto"] + 2, V(x, y, zp - 1)))
    AGUJEROS_PLACA.append((x, y, P["impresion"]["m3"]))
    return sh


def etapa4_zonas(doc):
    objs = []
    zp = z_placa()
    gs = doc.addObject("App::DocumentObjectGroup", "Sensores"); gs.Label = "4a_Zona_sensores"
    gl = doc.addObject("App::DocumentObjectGroup", "Logica"); gl.Label = "4b_Zona_logica"
    gp = doc.addObject("App::DocumentObjectGroup", "Potencia"); gp.Label = "4c_Zona_potencia"

    def add(nombre, et, sh, grupo, zona, fab):
        o = agregar(doc, nombre, et, sh, grupo, zona); o.Fabricacion = fab; objs.append(o); return o

    # --- sensores
    s = P["sensores"]; mo = s["morro"]; fa = s["faro"]
    add("Morro", "Morro con faros", morro(), gs, "sensores", "impresa")
    for lado, sg in (("Izq", 1), ("Der", -1)):
        faro = Part.makeCylinder(fa["d"] / 2, fa["espesor"], V(mo["x_min"] + mo["faro_prof"] - fa["espesor"], sg * mo["faro_y"], mo["faro_z"]), V(1, 0, 0))
        add("Faro_" + lado, "Faro LED 1 W %s" % lado, faro, gs, "sensores", "comprada")
    add("Mastil_lidar", "Mastil del LiDAR", mastil_lidar(), gs, "sensores", "impresa")
    li = s["lidar"]
    add("Lidar", "LiDAR %s (espacio reservado)" % li["modelo"],
        Part.makeBox(li["largo"], li["ancho"], li["alto"], V(li["x"] - li["largo"] / 2, li["y"] - li["ancho"] / 2, li["z_base"])),
        gs, "sensores", "comprada")
    le = P["led_esquina"]
    for lado, sg in (("Izq", 1), ("Der", -1)):
        p_ = s["led_esquina_delanteros"]
        add("Soporte_LED_D" + lado, "Soporte LED RGB delantero %s" % lado, soporte_led(p_["x"], sg * p_["y"]), gs, "sensores", "impresa")
        add("LED_D" + lado, "LED RGB delantero %s" % lado,
            Part.makeBox(le["led"], le["led"], 1.5, V(p_["x"] - le["led"] / 2, sg * p_["y"] - le["led"] / 2, zp + le["alto"])), gs, "sensores", "comprada")

    # --- lógica
    pc = P["logica"]["pcb"]; g = P["logica"]["gps"]
    x0, x1 = pc["x_min"], pc["x_min"] + pc["largo"]; e = pc["agujero_borde"]
    zpcb = zp + pc["separadores"]
    pcb = Part.makeBox(pc["largo"], pc["ancho"], pc["espesor"], V(x0, -pc["ancho"] / 2, zpcb))
    for xh in (x0 + e, x1 - e):
        for yh in (-pc["ancho"] / 2 + e, pc["ancho"] / 2 - e):
            pcb = pcb.cut(Part.makeCylinder(pc["agujero"] / 2, pc["espesor"] + 2, V(xh, yh, zpcb - 1)))
            sep = Part.makeCylinder(pc["separador_d"] / 2, pc["separadores"], V(xh, yh, zp))
            add("Separador_%d_%d" % (int(xh), int(yh + 100)), "Separador M3 x %d" % pc["separadores"], sep, gl, "logica", "comprada")
            AGUJEROS_PLACA.append((xh, yh, P["impresion"]["m3"]))
    add("PCB", "PCB principal %dx%d (Tesla)" % (pc["largo"], pc["ancho"]), pcb, gl, "logica", "comprada (JLCPCB)")
    add("Mastil_GPS", "Mastil del GPS", mastil_gps(), gl, "logica", "impresa")
    add("GPS", "GPS %s" % g["modelo"], Part.makeBox(g["lado"], g["lado"], g["alto"], V(g["x"] - g["lado"] / 2, g["y"] - g["lado"] / 2, g["z_base"])),
        gl, "logica", "comprada")

    # --- potencia
    pt = P["potencia"]; b = pt["bateria"]
    add("Cuna_bateria", "Cuna de la bateria (colgada)", cuna_bateria(), gp, "potencia", "impresa")
    add("Bateria", "Bateria LiPo 3S %dx%dx%d" % (b["largo"], b["ancho"], b["alto"]),
        Part.makeBox(b["ancho"], b["largo"], b["alto"], V(b["x_min"], -b["largo"] / 2, b["z_min"])), gp, "potencia", "comprada")
    if pt["driver_externo"]:
        dr = pt["driver"]
        add("Driver", "Driver de motores %s" % dr["modelo"],
            Part.makeBox(dr["largo"], dr["ancho"], dr["alto"], V(dr["x_min"], -dr["ancho"] / 2, zp + dr["separador"])), gp, "potencia", "comprada")
        i = dr["m3_inset"]
        for xh in (dr["x_min"] + i, dr["x_min"] + dr["largo"] - i):
            for yh in (-dr["ancho"] / 2 + i, dr["ancho"] / 2 - i):
                add("Separador_drv_%d_%d" % (int(xh), int(yh + 100)), "Separador driver M3 x %d" % dr["separador"],
                    Part.makeCylinder(2.5, dr["separador"], V(xh, yh, zp)), gp, "potencia", "comprada")
                AGUJEROS_PLACA.append((xh, yh, P["impresion"]["m3"]))
    for clave, nombre in (("interruptor", "Interruptor"), ("portafusible", "Portafusible")):
        c = pt[clave]
        add(nombre, "%s (%s)" % (nombre, c["modelo"]),
            Part.makeBox(c["largo"], c["ancho"], c["alto"], V(c["x"] - c["largo"] / 2, c["y"] - c["ancho"] / 2, zp)), gp, "potencia", "comprada")
    for lado, sg in (("Izq", 1), ("Der", -1)):
        p_ = pt["led_esquina_traseros"]
        add("Soporte_LED_T" + lado, "Soporte LED RGB trasero %s" % lado, soporte_led(p_["x"], sg * p_["y"]), gp, "potencia", "impresa")
        add("LED_T" + lado, "LED RGB trasero %s" % lado,
            Part.makeBox(le["led"], le["led"], 1.5, V(p_["x"] - le["led"] / 2, sg * p_["y"] - le["led"] / 2, zp + le["alto"])), gp, "potencia", "comprada")
    an = pt["antena"]
    base = Part.makeBox(an["base"], an["base"], an["base_alto"], V(an["x"] - an["base"] / 2, an["y"] - an["base"] / 2, zp))
    base = base.cut(Part.makeCylinder(an["d"] / 2 + P["impresion"]["holgura"], an["base_alto"], V(an["x"], an["y"], zp + 3)))
    add("Soporte_antena", "Soporte de la antena", base, gp, "potencia", "impresa")
    AGUJEROS_PLACA.append((an["x"], an["y"], 3.0))
    add("Antena", "Antena", Part.makeCylinder(an["d"] / 2, an["z_top"] - zp - 3, V(an["x"], an["y"], zp + 3)), gp, "potencia", "comprada")

    # agujeros de fijación en la placa base
    placa = doc.getObject("Placa_base")
    sh = placa.Shape
    for (x, y, d) in AGUJEROS_PLACA:
        sh = sh.cut(Part.makeCylinder(d / 2, P["placa_base"]["espesor"] + 2, V(x, y, zp - P["placa_base"]["espesor"] - 1)))
    placa.Shape = sh
    return objs


# ----------------------------------------------------------------------------- etapa 5
def perfil_ala(x0, z0, cuerda, esp):
    """Perfil invertido (genera carga hacia abajo): extradós casi plano arriba, intradós curvo abajo. Cara en el plano XZ."""
    pts = [(0.00, 0.55), (0.04, 0.85), (0.15, 1.00), (0.35, 0.98), (0.60, 0.88), (1.00, 0.62),
           (1.00, 0.50), (0.60, 0.30), (0.35, 0.05), (0.15, 0.00), (0.04, 0.12), (0.00, 0.55)]
    vs = [V(x0 + cuerda * a, 0, z0 + esp * b) for a, b in pts]
    curva = Part.BSplineCurve()
    curva.interpolate(vs[:-1], PeriodicFlag=True)
    return Part.Face(Part.Wire(curva.toShape()))


def aleron():
    a = P["aleron"]; ep = a["endplate"]; pp = a["plano_principal"]; fl = a["flap"]; bl = a["barra_led"]
    cl = a["canal_led"]; pi = a["pilon"]; zp = z_placa()
    yi = a["envergadura"] / 2 - ep["espesor"]                    # cara interior de los endplates
    partes = []
    for sg in (1, -1):
        y0 = yi if sg > 0 else -yi - ep["espesor"]
        partes.append(Part.makeBox(ep["largo"], ep["espesor"], ep["alto"], V(a["x_min"], y0, a["z_min"])))
    plano = perfil_ala(pp["x_min"], pp["z"], pp["cuerda"], pp["espesor_max"])
    plano = plano.extrude(V(0, 2 * yi + 2, 0)); plano.translate(V(0, -yi - 1, 0))
    partes.append(plano)
    flap = Part.makeBox(fl["cuerda"], 2 * yi + 2, fl["espesor"], V(fl["x_min"], -yi - 1, fl["z"]))
    flap.rotate(V(fl["x_min"], 0, fl["z"]), V(0, 1, 0), -fl["angulo_deg"])
    partes.append(flap)
    barra = Part.makeBox(bl["x"][1] - bl["x"][0], 2 * yi + 2, bl["z"][1] - bl["z"][0], V(bl["x"][0], -yi - 1, bl["z"][0]))
    partes.append(barra)
    # pilón cuello de cisne
    fx0, fx1 = pi["pie_x"]; cx0, cx1 = pi["columna_x"]; bz0, bz1 = pi["brazo_z"]
    pie = Part.makeBox(fx1 - fx0, 2 * pi["pie_y"], pi["pie_esp"], V(fx0, -pi["pie_y"], zp))
    col = Part.makeBox(cx1 - cx0, 2 * pi["columna_y"], bz1 - zp, V(cx0, -pi["columna_y"], zp))
    brazo = Part.makeBox(pi["brazo_x_max"] - cx0, 2 * pi["columna_y"], bz1 - bz0, V(cx0, -pi["columna_y"], bz0))
    tab = Part.makeBox(pi["tab_x"][1] - pi["tab_x"][0], 2 * pi["columna_y"], bz0 - pp["z"] - 2, V(pi["tab_x"][0], -pi["columna_y"], pp["z"] + 2))
    partes += [pie, col, brazo, tab]
    sh = partes[0].fuse(partes[1:]).removeSplitter()
    # canal de la tira LED en la cara trasera de la barra
    sh = sh.cut(Part.makeBox(cl["profundidad"] + 1, cl["largo"], cl["ancho"],
                             V(bl["x"][1] - cl["profundidad"], -cl["largo"] / 2, (bl["z"][0] + bl["z"][1]) / 2 - cl["ancho"] / 2)))
    # hueco de cable: sube por la columna, recorre el brazo y baja por el flap hasta el canal
    hc = pi["hueco_cable_d"] / 2
    xc = (cx0 + cx1) / 2
    sh = sh.cut(Part.makeCylinder(hc, bz1 - zp - 1, V(xc, 0, zp - 1)))
    sh = sh.cut(Part.makeCylinder(hc, bl["x"][1] - xc - 1, V(xc, 0, (bz0 + bz1) / 2), V(1, 0, 0)))
    for (x, y) in pi["m3"]:
        sh = sh.cut(Part.makeCylinder(P["impresion"]["m3"] / 2, pi["pie_esp"] + 2, V(x, y, zp - 1)))
        AGUJEROS_PLACA.append((x, y, P["impresion"]["m3"]))
    return sh.removeSplitter()


def etapa5_aleron(doc):
    g = doc.addObject("App::DocumentObjectGroup", "Aleron_grupo"); g.Label = "5_Aleron_F1"
    o = agregar(doc, "Aleron", "Aleron F1 con pilon y canal LED", aleron(), g, "aleron"); o.Fabricacion = "impresa"
    cl = P["aleron"]["canal_led"]; bl = P["aleron"]["barra_led"]
    tira = Part.makeBox(1.5, cl["largo"], 10, V(bl["x"][1] - cl["profundidad"], -cl["largo"] / 2, (bl["z"][0] + bl["z"][1]) / 2 - 5))
    t = agregar(doc, "Tira_LED", "Tira WS2812 %d mm" % cl["largo"], tira, g, "aleron"); t.Fabricacion = "comprada"
    placa = doc.getObject("Placa_base"); zp = z_placa(); sh = placa.Shape
    for (x, y) in P["aleron"]["pilon"]["m3"]:
        sh = sh.cut(Part.makeCylinder(P["impresion"]["m3"] / 2, P["placa_base"]["espesor"] + 2, V(x, y, zp - P["placa_base"]["espesor"] - 1)))
    placa.Shape = sh
    return [o, t]


# ----------------------------------------------------------------------------- principal
def construir():
    doc = App.newDocument("carrito_ensamble")
    todos = []
    if ETAPA >= 1:
        todos += etapa1_marco(doc)
    if ETAPA >= 2:
        todos += etapa2_motores(doc)
    if ETAPA >= 3:
        todos += etapa3_direccion(doc)
    if ETAPA >= 4:
        todos += etapa4_zonas(doc)
    if ETAPA >= 5:
        todos += etapa5_aleron(doc)
    doc.recompute()
    ruta = os.path.join(BASE, "carrito_ensamble.FCStd")
    doc.saveAs(ruta)

    # Exportaciones
    exp = os.path.join(BASE, "export")
    os.makedirs(exp, exist_ok=True)
    Part.export([o for o in todos], os.path.join(exp, "carrito_etapa%d.step" % ETAPA))
    stl = os.path.join(BASE, "stl")
    os.makedirs(stl, exist_ok=True)
    import Mesh
    for o in todos:
        if o.Fabricacion.startswith("impresa") and o.Name != "Placa_base":
            # soportes: se exporta solo el lado izquierdo (el derecho es su espejo; se imprime espejado en el slicer)
            if o.Name.endswith("_B") or o.Name.endswith("Der"):
                continue
            Mesh.export([o], os.path.join(stl, o.Name + ".stl"))
    placa = doc.getObject("Placa_base")
    if placa is not None:
        # contorno de la placa para corte láser (sección a media altura)
        zt = P["marco"]["z_inferior"] + P["perfil_2020"]["lado"] + P["placa_base"]["espesor"] / 2
        sec = placa.Shape.slice(V(0, 0, 1), zt)
        comp = Part.Compound(sec)
        comp.translate(V(0, 0, -zt))
        try:
            import importDXF
            tmp = doc.addObject("Part::Feature", "Placa_contorno_DXF")
            tmp.Shape = comp
            importDXF.export([tmp], os.path.join(exp, "placa_base_corte.dxf"))
            doc.removeObject(tmp.Name)
        except Exception as ex:
            print("AVISO: no se pudo exportar DXF:", ex)
    if App.GuiUp:
        macro = os.path.join(BASE, "colorear_por_zona.FCMacro")
        if os.path.exists(macro):
            src = open(macro, encoding="utf-8").read().replace("colorear(App.ActiveDocument)", "colorear(doc)")
            exec(src, {"doc": doc, "__name__": "colorear"})
            doc.save()
    print("OK etapa %d: %d objetos -> %s" % (ETAPA, len(todos), ruta))
    return doc


DOC = construir()
