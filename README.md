# CWPokeKey — Adaptador de teclado USB para CW Pokemon

Firmware para ESP32-S3 que convierte la entrada de un teclado USB estándar al protocolo PS/2, permitiendo usarlo con el jack TRS 3.5mm del [CW Pokemon](https://github.com/admvip/CW-Pokemon-Infomation).

## Objetivo

El [CW Pokemon](https://github.com/admvip/CW-Pokemon-Infomation) es un dispositivo hardware para radioaficionados que decodifica y genera código Morse (CW). Solo acepta teclados PS/2 a través de su jack TRS 3.5mm. Este adaptador soluciona esa limitación usando un ESP32-S3 como host USB HID + transmisor PS/2 por bit-bang.

```
Teclado USB  →  ESP32-S3 (USB OTG Host)  →  PS/2 bit-bang  →  Jack TRS 3.5mm  →  CW Pokemon
```

**Proyecto CW Pokemon (hardware original):** [github.com/admvip/CW-Pokemon-Infomation](https://github.com/admvip/CW-Pokemon-Infomation)

## Lista de materiales

| Cantidad | Componente | Notas |
|---|---|---|
| 1 | ESP32-S3-DevKitC-1 N16R8 | 16 MB flash, 8 MB PSRAM |
| 1 | Jack TRS 3.5mm hembra estéreo (para soldar) | **Imprescindible estéreo** — ver aviso abajo |
| 1 | Cable TRS 3.5mm estéreo (macho-macho) | Para conectar el adaptador al CW Pokemon |
| 1 | Conector USB-A hembra (para soldar) | Puerto donde se enchufa el teclado USB |
| 4 | Cable fino (~28 AWG, ~10 cm c/u) | D+, D−, 5V, GND del USB-OTG a la USB-A hembra |
| 1 | Cable USB-C | Alimentación del ESP32 (puerto derecho / UART) |
| 1 | Fuente o cargador USB 5V | Para alimentar el ESP32 |
| — | Estaño + soldador | Soldaduras en la placa |
| — | Pistola de hot glue | Sujetar y proteger las soldaduras |

> **¿Por qué no vale un adaptador USB-C OTG estándar?**
> El puerto USB-OTG del DevKitC-1 (conector izquierdo) **no suministra VBUS (5V)** por sí solo —
> necesita un circuito externo (transistor PMOS) para habilitar la alimentación.
> La modificación descrita abajo suelda los cuatro pines (D+, D−, 5V, GND) directamente
> a los pads del conector OTG, tomando el 5V del pin de alimentación de la propia placa.

> **Atención:** El jack TRS debe ser **estéreo** (3 contactos: TIP, RING, SLEEVE). Un conector mono (TS, 2 contactos) cortocircuitaría DATA con GND y podría dañar el CW Pokemon.

## Conexión del jack TRS (PS/2)

| Contacto TRS | Señal PS/2 | GPIO ESP32-S3 |
|---|---|---|
| TIP | DATA | GPIO5 |
| RING | CLK | GPIO6 |
| SLEEVE | GND | GND |

**Activar modo teclado en el CW Pokemon:** Menú → tipo de electrónico → **KeyB**

## Modificación hardware — USB Host (VBUS)

El conector USB-C izquierdo del DevKitC-1 (marcado como **USB-OTG**) no suministra alimentación VBUS por sí solo sin circuitería adicional (transistor PMOS). La solución es soldar 4 cables directamente a los pads del USB-OTG en la PCB:

| Pad PCB (conector izquierdo) | Color cable | Destino |
|---|---|---|
| D+ | — | GPIO20 |
| D- | — | GPIO19 |
| VBUS (5V) | Rojo | Pin 5V del DevKitC-1 |
| GND | Negro/blanco | Pin GND del DevKitC-1 |

Al otro extremo de estos 4 cables se suelda un conector USB-A hembra donde se conecta el teclado USB.

Los cables se sujetan con hot glue para evitar que se despeguen los pads al mover el conector.

**Ver fotos en `s3Pictures/` para detalles de las soldaduras.**

### Fotos del hardware

| Archivo | Descripción |
|---|---|
| `s3Pictures/photo_2026-10-03_12-49-05.jpg` | Vista general del módulo ESP32-S3 N16R8 con ambos USB-C |
| `s3Pictures/photo_2026-10-03_12-49-09.jpg` | Vista trasera: USB-OTG, cables soldados y hot glue |
| `s3Pictures/photo_2026-10-03_12-48-46.jpg` | Placa sobre teclado, círculos rojos en los puntos de soldadura |
| `s3Pictures/photo_2026-10-03_12-48-56.jpg` | Detalle de los cables del jack TRS (rojo/blanco) |
| `s3Pictures/photo_2026-10-03_12-48-09.jpg` | Primer plano del conector USB-OTG con hot glue y cables |
| `s3Pictures/photo_2026-09-26_18-58-44.jpg` | Vista adicional de las soldaduras USB-OTG |

## Notas eléctricas importantes

- El CW Pokemon usa lógica **3.3V** — no es necesario convertidor de nivel.
- El pin **CLK (RING)** tiene pull-up interno a **3.2V** en el CW Pokemon.
- El pin **DATA (TIP)** tiene un pull-down interno de **~8 kΩ a GND** en el CW Pokemon.
- **CRÍTICO — modo push-pull en DATA:** Con el pull-up interno de 45 kΩ del ESP32 en modo open-drain, la línea DATA solo alcanza ~0.5 V porque el pull-down de 8 kΩ del CW Pokemon gana. El CW Pokemon lee DATA como LOW constante y todo llega corrupto. Configurando el GPIO en **push-pull** (salida activa), el ESP32 conduce activamente 3.3 V y supera el pull-down. Implementado en `firmware-s3/src/ps2.cpp`.

## Indicador LED (WS2812 RGB, GPIO48)

| Estado | Comportamiento LED |
|---|---|
| Teclado USB conectado | Apagado |
| BLE conectado | Azul, parpadeo lento (~800 ms) |
| Buscando / esperando | Azul, parpadeo rápido (~120 ms) |

## Compilar y flashear

Requiere [PlatformIO](https://platformio.org/) (extensión VS Code o CLI).

```bash
cd firmware-s3
pio run              # compilar
pio run -t upload    # flashear (puerto /dev/ttyACM0)
pio device monitor   # monitor serie (115200 baud)
```

### Selección de layout de teclado

Editar `firmware-s3/platformio.ini`:

```ini
build_flags =
    -DLAYOUT_ES   ; QWERTY español (por defecto)
;   -DLAYOUT_US   ; QWERTY inglés americano
```

## Estructura del proyecto

```
firmware-s3/              — Firmware principal (USB HID Host + PS/2 bit-bang + BLE)
├── platformio.ini
└── src/
    ├── config.h          — Asignación de pines y constantes
    ├── ps2.cpp/hpp       — Driver PS/2 bit-bang (modo push-pull)
    ├── usb_kbd.cpp/hpp   — USB HID Host (ESP-IDF usb_host)
    ├── ble_hid.cpp/hpp   — BLE HID Host (NimBLE) — en desarrollo
    ├── keymap.c/h        — Tabla HID keycode → PS/2 Set 2
    └── main.cpp
scanner/                  — Herramienta de diagnóstico: escáner WiFi + BLE
s3Pictures/               — Fotos del hardware y soldaduras
debug/                    — Capturas de pantalla de depuración
```

## Ramas del repositorio

| Rama | Contenido |
|---|---|
| `usb-bt` | Firmware completo USB + BLE (BLE en desarrollo), herramienta de diagnóstico |
| `only-usb` | Solo firmware USB→PS/2, sin código BLE ni herramientas extra |

## Timing PS/2

- Semi-período del CLK: 40 μs → ~12.5 kHz (especificación PS/2: período 60–100 μs, ≤ 16.7 kHz)
- Verificado con osciloscopio: 928 μs para 11 bits (un byte completo)
- El CW Pokemon resetea su máquina de estados PS/2 si hay más de ~2 ms entre flancos de CLK durante una transmisión de byte — con 40 μs estamos dentro del límite

## Licencia

GNU General Public License v3.0

Copyright (C) 2026 mcamposv
