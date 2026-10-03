// ─────────────────────────────────────────────────────────────────────────────
// CWPokeKey — Caja para ESP32-S3 DevKitC-1
// OpenSCAD paramétrico · tapa a presión (4 clips en los lados largos)
//
// EJES
//   X : eje largo de la placa  (X=0 → pared USB-C  /  X=OL → pared jack TRS)
//   Y : eje ancho (headers)
//   Z : altura
//
// CÓMO USAR
//   F5 = vista previa   F6 = render final   Archivo › Exportar › Export as STL
//
//   Cuerpo : deja  render_box();  y comenta  render_lid();
//   Tapa   : deja  render_lid();  y comenta  render_box();
//
// ORIENTACIÓN DE IMPRESIÓN
//   Cuerpo : suelo hacia abajo — sin soportes
//   Tapa   : cara exterior hacia abajo (girar 180° en el slicer) — sin soportes
//
// AJUSTE
//   TOL     : aumenta si la placa entra muy justa
//   LIP_GAP : aumenta si la tapa no desliza
//   SNAP_D  : reduce si los clips van muy duros
// ─────────────────────────────────────────────────────────────────────────────

$fn = 48;

// ── PLACA ────────────────────────────────────────────────────────────────────

BL = 65;   // largo con headers  (eje X: pared USB-C ↔ pared jack)
BW = 28;   // ancho con headers  (eje Y)
BH = 14;   // alto total (PCB + componentes + pines)

// ── GEOMETRÍA DE CAJA ────────────────────────────────────────────────────────

TOL   = 0.4;  // holgura por lado
WL    = 2.0;  // grosor de pared
FLOOR = 1.5;  // espacio libre bajo la placa

// Dimensiones interiores
IL = BL + 2*TOL;
IW = BW + 2*TOL;
IH = BH + FLOOR;

// Dimensiones exteriores del cuerpo
OL = IL + 2*WL;
OW = IW + 2*WL;
OH = IH + WL;   // WL = suelo sólido

// ── APERTURAS USB-C (pared X=0, extremo corto) ───────────────────────────────
// Dos conectores USB-C centrados en el ancho de la placa

USB_W  = 9.5;   // ancho de cada apertura
USB_H  = 4.0;   // alto de cada apertura
USB_CC = 16.0;  // distancia centro a centro entre los dos conectores
USB_Z  =  2.0;  // altura del centro del conector sobre el fondo de la placa

// ── RANURA JACK TRS (pared X=OL, forma U abierta arriba) ─────────────────────

JACK_R = 2.5;   // radio ranura: cable 4 mm + 0.5 mm holgura

// ── CLIPS DE CIERRE ──────────────────────────────────────────────────────────
// 2 clips en la pared Y=0, 2 en la pared Y=OW (lados largos)

CLIP_W  = 10;   // ancho del clip
LIP_D   =  4;   // profundidad del labio de la tapa
LIP_GAP = 0.3;  // holgura labio ↔ exterior de caja
SNAP_D  = 0.8;  // profundidad del resalte de enganche

// ── TAPA ─────────────────────────────────────────────────────────────────────

LID_T   = 2.0;  // grosor panel de tapa
LIP_WL  = 1.8;  // grosor del labio de la tapa

// ── CALCULADOS ────────────────────────────────────────────────────────────────

// Centro absoluto (Z) del conector USB desde el exterior del suelo
usb_z_abs = WL + FLOOR + USB_Z;

// X de los dos clips por cara larga (en OL*0.25 y OL*0.75)
clip_x = [OL*0.25, OL*0.75];

// ─────────────────────────────────────────────────────────────────────────────
// MÓDULOS
// ─────────────────────────────────────────────────────────────────────────────

// Ranura en U: semicírculo en la base + slot rectangular hasta el borde superior
// Se usa para el hueco del jack en el cuerpo y en la tapa.
// Orientación: la apertura queda en +Z (hacia arriba).
// La geometría se extrude en el eje Y (rotation -90° para cortar pared en X).
module u_slot_xz(r, thick) {
    // Llamada: translate al centro, rotate para orientar, luego extruye en Y
    linear_extrude(thick + 0.2)
        union() {
            circle(r = r);
            translate([-r, -r]) square([r*2, r + 0.1]);
        }
}

// ─────────────────────────────────────────────────────────────────────────────
// CUERPO
// ─────────────────────────────────────────────────────────────────────────────

module box_body() {
    difference() {
        // Bloque exterior sólido
        cube([OL, OW, OH]);

        // Vaciado interior (abierto por arriba, +1 para limpiar cara superior)
        translate([WL, WL, WL])
            cube([IL, IW, IH + 1]);

        // ── Aperturas USB-C (pared X=0) ──────────────────────────────────
        for (yc = [OW/2 - USB_CC/2, OW/2 + USB_CC/2])
            translate([-0.1, yc - USB_W/2, usb_z_abs - USB_H/2])
                cube([WL + 0.2, USB_W, USB_H]);

        // ── Ranura U jack TRS (pared X=OL) ───────────────────────────────
        // El semicírculo queda centrado en Y=OW/2, Z=OH (borde superior)
        translate([OL - WL - 0.1, OW/2, OH])
            rotate([0, 90, 0])                // extruye en +X (atraviesa la pared)
                u_slot_xz(JACK_R, WL);

        // ── Muescas para clips (pared Y=0 y Y=OW) ────────────────────────
        for (cx = clip_x) {
            // Pared Y=0
            translate([cx - CLIP_W/2, -0.1, OH - LIP_D])
                cube([CLIP_W, SNAP_D + 0.1, LIP_D + 0.1]);
            // Pared Y=OW
            translate([cx - CLIP_W/2, OW - SNAP_D, OH - LIP_D])
                cube([CLIP_W, SNAP_D + 0.1, LIP_D + 0.1]);
        }
    }

    // Topes de esquina: elevan la placa FLOOR mm del suelo
    for (x = [WL + 1, OL - WL - 5])
        for (y = [WL + 1, OW - WL - 5])
            translate([x, y, WL])
                cube([4, 4, FLOOR]);
}

// ─────────────────────────────────────────────────────────────────────────────
// TAPA
// ─────────────────────────────────────────────────────────────────────────────

// El labio envuelve el exterior del cuerpo con holgura LIP_GAP.
// Modelada "al derecho" (cara exterior arriba); para imprimirla,
// girar 180° en el slicer para que la cara exterior quede sobre la cama.

module lid() {
    // Dimensiones exteriores del labio
    ll = OL + 2*(LIP_GAP + LIP_WL);
    lw = OW + 2*(LIP_GAP + LIP_WL);
    // Offset del labio respecto al origen del cuerpo
    lx = -(LIP_GAP + LIP_WL);
    ly = -(LIP_GAP + LIP_WL);

    difference() {
        union() {
            // Panel superior de la tapa
            translate([lx, ly, 0])
                cube([ll, lw, LID_T]);

            // Labio perimetral (baja LIP_D desde el panel)
            translate([lx, ly, LID_T])
                difference() {
                    cube([ll, lw, LIP_D]);
                    // Vaciado interior (deja solo la pared LIP_WL)
                    translate([LIP_WL, LIP_WL, -0.1])
                        cube([ll - 2*LIP_WL, lw - 2*LIP_WL, LIP_D + 0.2]);
                }
        }

        // ── Ranura U en el labio (lado jack, X=OL) ───────────────────────
        // La ranura debe alinearse con la del cuerpo:
        // centro en Y=OW/2, parte inferior en Z=LID_T+(LIP_D-JACK_R) para
        // que el semicírculo quede a la misma cota que en el cuerpo.
        translate([OL + LIP_GAP + LIP_WL + 0.1, OW/2, LID_T + LIP_D])
            rotate([0, -90, 0])                // extruye en -X
                u_slot_xz(JACK_R, LIP_WL + LIP_GAP);
    }

    // ── Resaltes de enganche en el interior del labio (lados largos Y) ───
    for (cx = clip_x) {
        // Lado Y=0: resalte en cara interior Y+ del labio
        translate([cx - CLIP_W/2, lx + LIP_WL, LID_T])
            snap_bump();
        // Lado Y=OW: resalte en cara interior Y- del labio
        translate([cx - CLIP_W/2, OW + LIP_GAP, LID_T])
            mirror([0, 1, 0])
                snap_bump();
    }
}

// Resalte triangular — engancha en la muesca del cuerpo
module snap_bump() {
    linear_extrude(CLIP_W)
        polygon([
            [0, 0],
            [0, LIP_D * 0.6],
            [SNAP_D, LIP_D * 0.4],
            [SNAP_D, 0]
        ]);
}

// ─────────────────────────────────────────────────────────────────────────────
// RENDER
// ─────────────────────────────────────────────────────────────────────────────

module render_box() {
    box_body();
}

module render_lid() {
    // Para preview conjunto: tapa flotando 8 mm sobre el cuerpo
    translate([0, 0, OH + 8])
        lid();
}

// ┌──────────────────────────────────────────────────────┐
// │  DESCOMENTA LO QUE QUIERAS EXPORTAR (uno cada vez)  │
// └──────────────────────────────────────────────────────┘

// ── Posición de impresión: cuerpo y tapa uno al lado del otro ────────────────
// Cuerpo: suelo hacia abajo, apertura arriba — sin soportes
render_box();

// Tapa: panel exterior hacia abajo (Z=0), labio hacia arriba — sin soportes
// Se desplaza OL+10 mm en X para dejar 10 mm de espacio entre piezas
translate([OL + 10 + (LIP_GAP + LIP_WL), 0, 0])
    lid();

// Vista de conjunto montado (tapa flotando sobre el cuerpo):
// render_box(); render_lid();
