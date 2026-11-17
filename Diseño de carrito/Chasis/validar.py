# -*- coding: utf-8 -*-
"""
Validación del ensamble del carrito. Uso: freecadcmd validar.py
Lee carrito_ensamble.FCStd y params.json, imprime una tabla PASA/FALLA.
Las comprobaciones de piezas que aún no existen salen como PENDIENTE.
"""
import itertools
import json
import os

import FreeCAD as App
import sys
try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

BASE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd()
P = json.load(open(os.path.join(BASE, "params.json"), encoding="utf-8"))
doc = App.openDocument(os.path.join(BASE, "carrito_ensamble.FCStd"))
V = App.Vector
R = []  # (comprobación, estado, detalle)


def caja_real(shape):
    a = shape.BoundBox
    try:
        b = shape.optimalBoundingBox()
    except Exception:
        return a
    return App.BoundBox(max(a.XMin, b.XMin), max(a.YMin, b.YMin), max(a.ZMin, b.ZMin),
                        min(a.XMax, b.XMax), min(a.YMax, b.YMax), min(a.ZMax, b.ZMax))


def ok(nombre, cond, detalle):
    R.append((nombre, "PASA" if cond else "FALLA", detalle))


def obj(n):
    return doc.getObject(n) or doc.getObject("PD_" + n)


solidos = [o for o in doc.Objects if o.TypeId in ("Part::Feature", "PartDesign::Body") and o.Shape.Solids
           and not (o.TypeId == "Part::Feature" and o.getParentGeoFeatureGroup() is not None)]
# nombre lógico: los Body se llaman PD_<pieza>
NOMBRE = {o: (o.Name[3:] if o.Name.startswith("PD_") else o.Name) for o in solidos}

# 1. Interferencias
lim = P["validacion"]["interferencia_max_mm3"]
choques = []
for a, b in itertools.combinations(solidos, 2):
    if not a.Shape.BoundBox.intersect(b.Shape.BoundBox):
        continue
    v = a.Shape.common(b.Shape).Volume
    if v > lim:
        choques.append("%s / %s: %.2f mm3" % (a.Label, b.Label, v))
ok("Sin interferencias entre sólidos", not choques,
   "%d sólidos revisados" % len(solidos) if not choques else "; ".join(choques))

# 2. Marco
m = P["marco"]; s = P["perfil_2020"]["lado"]; pb = P["placa_base"]
est = [o for o in solidos if NOMBRE[o].split("_")[0] in ("Perfil", "Travesano", "Escuadra", "Placa")]
if est:
    import Part
    bb = caja_real(Part.makeCompound([o.Shape for o in est]))
    esp = (0, m["largo_perfiles"], -(m["y_centro_perfiles"] + s / 2), m["y_centro_perfiles"] + s / 2,
           m["z_inferior"], m["z_inferior"] + s + pb["espesor"])
    real = (bb.XMin, bb.XMax, bb.YMin, bb.YMax, bb.ZMin, bb.ZMax)
    dif = max(abs(x - y) for x, y in zip(esp, real))
    ok("Marco: caja envolvente", dif < 0.05,
       "X %.1f..%.1f  Y %.1f..%.1f  Z %.1f..%.1f" % real)

placa = obj("Placa_base")
perfiles = [obj("Perfil_Izq"), obj("Perfil_Der")]
if placa and all(perfiles):
    d = min(placa.Shape.distToShape(p.Shape)[0] for p in perfiles)
    ok("Placa apoyada sobre los perfiles", d < 0.01, "distancia %.3f mm" % d)
    # cada agujero de la placa cae sobre la ranura superior del perfil
    malos = 0
    for x in pb["agujeros_perfil_x"]:
        for p in perfiles:
            y = p.Shape.BoundBox.Center.y
            if not (0 <= x <= m["largo_perfiles"]) or abs(abs(y) - m["y_centro_perfiles"]) > 0.01:
                malos += 1
    ok("Agujeros de la placa sobre la ranura del perfil", malos == 0, "%d agujeros" % (2 * len(pb["agujeros_perfil_x"])))
    cama = P["impresion"]["cama"]
    if pb["material"] == "acrilico":
        R.append(("Placa base fabricable", "PASA", "acrílico %dx%dx%g en láser (DXF en export/)" % (pb["largo"], pb["ancho"], pb["espesor"])))
    else:
        cabe = pb["largo"] <= cama[0] and pb["ancho"] <= cama[1]
        ok("Placa base cabe en la cama", cabe, "si se imprime hay que dividirla en 2 partes" if not cabe else "")

# 3. Tracción trasera (etapa 2)
g = P["general"]
import Part
ruedas = [o for o in solidos if o.Name.startswith("Rueda")]
traseras = [o for o in ruedas if o.Name.startswith("Rueda_T")]
if traseras:
    zmin = min(caja_real(o.Shape).ZMin for o in traseras)
    ok("Ruedas traseras apoyan en el suelo", abs(zmin) < 0.05, "Z mín %.2f" % zmin)
    cy = [caja_real(o.Shape).Center.y for o in traseras]
    via = max(cy) - min(cy)
    ok("Vía trasera", abs(via - g["via"]) <= g["tolerancia_medidas"], "%.1f mm (objetivo %d)" % (via, g["via"]))
    otros = [o for o in solidos if not o.Name.startswith("Rueda")]
    zmin_o = min(caja_real(o.Shape).ZMin for o in otros)
    bajo = min(otros, key=lambda o: caja_real(o.Shape).ZMin).Label
    ok("Distancia al suelo ≥ %d" % P["validacion"]["z_min_suelo"], zmin_o >= P["validacion"]["z_min_suelo"],
       "punto más bajo %.1f mm (%s)" % (zmin_o, bajo))
    so, mo = P["soporte_motor"], P["motor_jgb37"]
    for letra in ("A", "B"):
        mot, sop = obj("Motor_" + letra), obj("Soporte_motor_" + letra)
        if not (mot and sop):
            continue
        # holgura real en la cuna: distancia entre la caja reductora y el soporte, sin contar la cara de apoyo
        yf = mo["cara_y"] * (1 if letra == "A" else -1)
        x = g["eje_trasero_x"]; zc = mo["eje_z"] + mo["eje_offset"]
        largo = mo["caja_largo"] - 4   # tramo central de la caja, lejos de la cara de apoyo
        y0 = (yf - mo["caja_largo"] + 1.5) if letra == "A" else (yf + 1.5)
        tramo = Part.makeCylinder(mo["caja_d"] / 2, largo, V(x, y0, zc), V(0, 1, 0))
        d = tramo.distToShape(sop.Shape)[0]
        ok("Holgura motor %s en la cuna" % letra, abs(d - so["holgura_motor"] / 2) < 0.05,
           "%.2f mm por lado (diametral %.1f)" % (d, 2 * d))
        r = caja_real(sop.Shape); cama = P["impresion"]["cama"]
        dims = sorted([r.XLength, r.YLength, r.ZLength])
        ok("Soporte %s cabe en la cama" % letra, all(a <= b for a, b in zip(dims, sorted(cama))),
           "%.0f × %.0f × %.0f mm" % (r.XLength, r.YLength, r.ZLength))
    # el eje del motor debe salir por fuera del soporte para recibir el acople
    acop = obj("Acople_TIzq")
    if acop:
        d = acop.Shape.distToShape(obj("Soporte_motor_A").Shape)[0]
        ok("Acople libre del soporte", d >= 1.0, "separación %.1f mm" % d)

# 4. Dirección (etapa 3)
delanteras = [o for o in ruedas if o.Name.startswith("Rueda_D")]
if delanteras and traseras:
    cy = [caja_real(o.Shape).Center.y for o in delanteras]
    via_d = max(cy) - min(cy)
    ok("Vía delantera", abs(via_d - g["via"]) <= g["tolerancia_medidas"], "%.1f mm (objetivo %d)" % (via_d, g["via"]))
    bat = caja_real(traseras[0].Shape).Center.x - caja_real(delanteras[0].Shape).Center.x
    ok("Batalla", abs(bat - g["batalla"]) <= g["tolerancia_medidas"], "%.1f mm (objetivo %d)" % (bat, g["batalla"]))
    d = P["direccion"]; x = g["eje_delantero_x"]
    fijos_excl = ("Barra_direccion", "Brazo_servo")
    for lado, sg in (("Izq", 1), ("Der", -1)):
        mov = [o for o in solidos if NOMBRE[o] in ("Mangueta_" + lado, "Eje_D" + lado, "Acople_D" + lado, "Rueda_D" + lado,
                                                 "Rodamiento_%s_1" % lado, "Rodamiento_%s_2" % lado)]
        otros = [o for o in solidos if o not in mov and NOMBRE[o] not in fijos_excl and NOMBRE[o] != "Kingpin_" + lado
                 and not NOMBRE[o].endswith("Der" if lado == "Izq" else "Izq")]
        peor = None
        for ang in (-d["giro_max_deg"], d["giro_max_deg"]):
            for mo_ in mov:
                sh = mo_.Shape.copy(); sh.rotate(V(x, sg * d["kingpin_y"], 0), V(0, 0, 1), ang)
                for ot in otros:
                    if not sh.BoundBox.intersect(ot.Shape.BoundBox):
                        continue
                    vol = sh.common(ot.Shape).Volume
                    if vol > lim:
                        peor = "%s choca con %s a %+d° (%.1f mm3)" % (mo_.Label, ot.Label, ang, vol)
        ok("Giro ±%d° rueda %s sin choques" % (d["giro_max_deg"], lado), peor is None, peor or "libre")
    cama = sorted(P["impresion"]["cama"])
    for o in solidos:
        if o.Fabricacion.startswith("impresa") and NOMBRE[o] != "Placa_base" and not NOMBRE[o].endswith(("_B", "Der")):
            r = caja_real(o.Shape); dims = sorted([r.XLength, r.YLength, r.ZLength])
            ok("%s cabe en la cama" % o.Label, all(a <= b for a, b in zip(dims, cama)),
               "%.0f × %.0f × %.0f mm" % (r.XLength, r.YLength, r.ZLength))

# 5. Zonas (etapa 4)
gps = obj("GPS")
if gps:
    dmin = min(gps.Shape.distToShape(obj(n).Shape)[0] for n in ("Motor_A", "Motor_B"))
    ok("GPS a ≥ %d mm de los motores" % P["logica"]["gps"]["dist_min_motores"], dmin >= P["logica"]["gps"]["dist_min_motores"],
       "%.0f mm (por la brújula QMC5883L)" % dmin)
lid = obj("Lidar")
if lid:
    zlim = P["validacion"]["z_max_bajo_lidar"]
    altos = ["%s (%.0f)" % (o.Label, caja_real(o.Shape).ZMax) for o in solidos
             if NOMBRE[o] not in ("Lidar", "Mastil_lidar") and caja_real(o.Shape).ZMax > zlim + 0.01]
    ok("Plano del LiDAR despejado (nada sobre Z=%d)" % zlim, not altos, "; ".join(altos) if altos else "libre a 360°")
bat, cuna = obj("Bateria"), obj("Cuna_bateria")
if bat and cuna:
    d = bat.Shape.distToShape(cuna.Shape)[0]
    ok("Batería entra en su cuna", 0 <= d <= 0.01, "apoyada en el fondo; holgura lateral %g mm" % P["potencia"]["cuna_bateria"]["holgura"])
zona_pcb = obj("PCB")
if zona_pcb:
    xs = caja_real(zona_pcb.Shape)
    ok("PCB dentro de la zona lógica", 80 <= xs.XMin and xs.XMax <= 215, "X %.0f..%.0f" % (xs.XMin, xs.XMax))

# 6. Comprobaciones de etapas posteriores
pend = []
if not [o for o in ruedas if o.Name.startswith("Rueda_D")]:
    pend += ["Batalla y vía delantera", "Giro de ruedas ±%d°" % P["direccion"]["giro_max_deg"]]
if not obj("Aleron"):
    pend += ["Medidas totales (largo/ancho/alto)"]
else:
    tot = caja_real(Part.makeCompound([o.Shape for o in solidos]))
    for nombre, real, objetivo in (("Largo total", tot.XLength, g["largo_total"]), ("Ancho total", tot.YLength, g["ancho_total"]),
                              ("Alto total", tot.ZLength, g["alto_total"])):
        ok(nombre, abs(real - objetivo) <= g["tolerancia_medidas"], "%.1f mm (objetivo %d)" % (real, objetivo))
    al = obj("Aleron"); r = caja_real(al.Shape)
    ok("Alerón dentro del ancho de las ruedas", r.YLength <= g["via"], "envergadura %.0f mm" % r.YLength)
if not obj("Lidar"):
    pend += ["Plano del LiDAR despejado"]
if not obj("GPS"):
    pend += ["GPS a ≥ %d mm de los motores" % P["logica"]["gps"]["dist_min_motores"]]
if not traseras:
    pend += ["Distancia al suelo ≥ %d" % P["validacion"]["z_min_suelo"]]
for c in pend:
    R.append((c, "PENDIENTE", "etapas siguientes"))

ancho = max(len(r[0]) for r in R)
print("\n" + "=" * (ancho + 60))
for n, e, d in R:
    print("%-*s  %-9s  %s" % (ancho, n, e, d))
print("=" * (ancho + 60))
print("RESUMEN: %d PASA, %d FALLA, %d PENDIENTE" % tuple(sum(1 for r in R if r[1] == k) for k in ("PASA", "FALLA", "PENDIENTE")))
