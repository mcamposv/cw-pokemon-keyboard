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
USB_WL = WL * 0.5;  // grosor de pared en la zona USB (50% de WL)

// ── RANURA JACK TRS (pared X=OL, forma U abierta arriba) ─────────────────────

JACK_R     = 2.0;   // radio ranura → ancho total 4 mm
JACK_DEPTH = 6.0;   // profundidad del canal desde el borde superior (mm)

// ── TAPA — labio de ajuste ────────────────────────────────────────────────────

LIP_D   =  4;   // profundidad del labio de la tapa
LIP_GAP = 0.3;  // holgura labio ↔ exterior de caja

// ── TAPA ─────────────────────────────────────────────────────────────────────

LID_T   = 2.0;  // grosor panel de tapa
LIP_WL  = 1.8;  // grosor del labio de la tapa

// ── CALCULADOS ────────────────────────────────────────────────────────────────

// Centro absoluto (Z) del conector USB desde el exterior del suelo
usb_z_abs = WL + FLOOR + USB_Z;


// ─────────────────────────────────────────────────────────────────────────────
// MÓDULOS
// ─────────────────────────────────────────────────────────────────────────────

// Ranura en U: semicírculo en la base + slot rectangular hasta el borde superior
// Se usa para el hueco del jack en el cuerpo y en la tapa.
// Orientación: la apertura queda en +Z (hacia arriba).
// La geometría se extrude en el eje Y (rotation -90° para cortar pared en X).
// U vertical: apertura en el borde superior (módulo X=0 → mundo Z=OH),
// paredes rectas hasta depth-r, semicírculo en el fondo.
// Ancho total = 2*r  |  Profundidad total = depth
module u_slot_xz(r, thick, depth=0) {
    d = max(r * 2, depth);   // mínimo suficiente para que el semicírculo quepa
    linear_extrude(thick + 0.2)
        union() {
            // Canal recto: desde la apertura hasta el inicio del semicírculo
            translate([0, -r]) square([d - r, r * 2]);
            // Semicírculo en el fondo
            translate([d - r, 0]) circle(r = r);
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

        // ── Apertura USB-C (pared X=0) — hueco continuo para ambos puertos ─────
        translate([-0.1, OW/2 - USB_CC/2 - USB_W/2, usb_z_abs - USB_H/2])
            cube([WL + 0.2, USB_CC + USB_W, USB_H]);

        // Rebaje exterior que deja la pared USB con grosor USB_WL (50% de WL)
        translate([-0.1, OW/2 - USB_CC/2 - USB_W/2 - 2, usb_z_abs - USB_H/2 - 2])
            cube([WL - USB_WL + 0.1, USB_CC + USB_W + 4, USB_H + 4]);

        // ── Ranura U jack TRS (pared X=OL) ───────────────────────────────
        // Ranura U: +0.1 sobre OH para evitar cara coplanar en el borde superior
        translate([OL - WL - 0.1, OW/2, OH + 0.1])
            rotate([0, 90, 0])
                u_slot_xz(JACK_R, WL, depth=JACK_DEPTH + 0.1);

    }

    // Topes esquina lado USB (solo soporte de altura, FLOOR mm)
    for (y = [WL + 1, OW - WL - 5])
        translate([WL + 1, y, WL])
            cube([4, 4, FLOOR]);

    // Topes esquina lado jack/antena (7×4×4 mm, encajan la placa lateralmente)
    for (y = [WL, OW - WL - 4])
        translate([OL - WL - 7, y, WL])
            cube([7, 4, 4]);
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
