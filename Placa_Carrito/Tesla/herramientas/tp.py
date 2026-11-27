import re, shutil, sys
sys.path.insert(0, "C:/Users/yenni/AppData/Local/Temp/claude/8eb49cfc81ebaa8e/7726eab3-0a2d-45a6-bcd0-fbc7d6abb388/scratchpad")
src = open("protect.py", encoding="utf8").read()
# reutiliza helpers (hasta la hoja de proteccion) sin ejecutar la regeneracion
head = src[:src.index("# ================= hoja")]
head = head.replace('for f in ("Tesla.kicad_sch", "Externo.kicad_sch", "Tesla.kicad_pro"):\n    shutil.copy(D + f, SC + "bk_prot_" + f)', 'shutil.copy(D + "Externo.kicad_sch", SC + "bk_tp_Externo.kicad_sch")')
exec(head)
import xml.etree.ElementTree as ET
root = ET.parse(SC + "nl.xml").getroot(); func2gpio = {}
for n in root.find("nets"):
    nm = n.get("name").lstrip("/")
    for nd in n:
        if nd.get("ref") == "U3":
            m = re.search(r"GPIO(\d+)", nd.get("pinfunction") or "")
            if m: func2gpio[nm] = "GPIO" + m.group(1)
e = open(D + "Externo.kicad_sch", encoding="utf8").read()
assert '"TP1"' not in e
tpsym = open("C:/Program Files/KiCad/10.0/share/kicad/symbols/Connector.kicad_sym", encoding="utf8").read()
i = tpsym.index('\t(symbol "TestPoint"'); tps = tpsym[i:block(tpsym, i)].replace('(symbol "TestPoint"', '(symbol "Connector:TestPoint"', 1)
LST = [("GND 0V", "g", "GND"), ("GND 0V", "g", "GND"), ("GND 0V", "g", "GND"), ("5V BUCK 5V", "g", "+5V"), ("3V3 BUCK 3.3V", "g", "+3V3"),
       ("EN 3.3V", "l", "EN"), ("BOOT 3.3V", "l", "GPIO0"), ("TX 3.3V", "l", "GPIO43"), ("RX 3.3V", "l", "GPIO44")]
add = '\t(text "Puntos de prueba (TP1 a TP9): GND, salidas de los buck, EN, BOOT y UART"\n\t\t(exclude_from_sim no)\n\t\t(at 318.77 33.02 0)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.905 1.905)\n\t\t\t\t(bold yes)\n\t\t\t)\n\t\t\t(justify left top)\n\t\t)\n\t\t(uuid "%s")\n\t)\n' % U()
for k, (val, kind, net) in enumerate(LST):
    col, row = divmod(k, 5); x = 322.58 + 25.4 * col; y = 52.07 + 20.32 * row
    sym = mk("Connector:TestPoint", x, y, 0, "TP%d" % (k + 1), val, "TestPoint:TestPoint_Pad_D1.5mm", "", EPATH, npins=1, tx=2.54, ty=-3.5)
    sym = sym.replace("(in_bom yes)", "(in_bom no)", 1)
    add += sym + W((x, y), (x, y + 5.08)) + (GL(net, x, y + 5.08, 0) if kind == "g" else LB(net, x, y + 5.08, 0))
ls = e.index("(lib_symbols"); le = block(e, ls); lib = e[ls:le]
if "Connector:TestPoint" not in lib: lib = lib[:-1].rstrip("\n\t") + "\n\t\t" + tps + "\n\t)"
e = e[:ls] + lib + e[le:]
k = e.index("\n\t(hierarchical_label ") + 1
e = e[:k] + add + e[k:]
open(D + "Externo.kicad_sch", "w", encoding="utf8", newline="").write(e)
print("ok", len(LST))
