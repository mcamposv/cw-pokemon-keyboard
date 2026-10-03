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
| 1 | Cable USB-C a USB-A OTG (macho-hembra) | Para conectar el teclado al puerto OTG del ESP32 |
| 2 | Cable fino (~28 AWG, ~5 cm c/u) | 5V y GND para alimentar el teclado — ver mod abajo |
| 1 | Cable USB-C | Alimentación del ESP32 (puerto derecho / UART) |
| 1 | Fuente o cargador USB 5V | Para alimentar el ESP32 |
| — | Estaño + soldador | Soldaduras en la placa |
| — | Pistola de hot glue | Sujetar y proteger las soldaduras |

> **Atención:** El jack TRS debe ser **estéreo** (3 contactos: TIP, RING, SLEEVE). Un conector mono (TS, 2 contactos) cortocircuitaría DATA con GND y podría dañar el CW Pokemon.

## Conexión del jack TRS (PS/2)

| Contacto TRS | Señal PS/2 | GPIO ESP32-S3 |
|---|---|---|
| TIP | DATA | GPIO5 |
| RING | CLK | GPIO6 |
| SLEEVE | GND | GND |

**Activar modo teclado en el CW Pokemon:** Menú → tipo de electrónico → **KeyB**

## Modificación hardware — alimentación VBUS para el teclado

El cable USB-C a USB-A OTG se encarga de las líneas de datos (D+/D−) entre el puerto OTG del ESP32-S3 y el teclado. Sin embargo, el teclado USB necesita alimentación (5V VBUS) y el puerto OTG del DevKitC-1 no la suministra por sí solo.

La solución es soldar **2 cables cortos** directamente en la placa:

| Pad a soldar | Cable | Destino |
|---|---|---|
| VBUS del conector USB-OTG (izquierdo) | Rojo (5V) | Pin 5V del header del DevKitC-1 |
| GND del conector USB-OTG (izquierdo) | Negro (GND) | Pin GND del header del DevKitC-1 |

Los pads exactos están marcados con **círculos rojos** en las fotos de referencia.

Los cables se sujetan con hot glue para evitar que las soldaduras cedan al conectar/desconectar el cable OTG.

### Fotos del hardware

| Archivo | Descripción |
|---|---|
| `s3Pictures/photo_2026-10-03_12-48-56.jpg` | **Pads marcados con círculos rojos** — puntos exactos donde soldar los 2 cables de alimentación |
| `s3Pictures/photo_2026-10-03_12-49-09.jpg` | Vista trasera: cables soldados y hot glue sobre el conector OTG |
| `s3Pictures/photo_2026-10-03_12-49-05.jpg` | Vista general del módulo ESP32-S3 N16R8 con ambos USB-C |
| `s3Pictures/photo_2026-10-03_12-49-00.jpg` | Vista lateral con el cable OTG conectado |
| `s3Pictures/photo_2026-10-03_12-48-09.jpg` | Primer plano del conector USB-OTG con hot glue |
| `s3Pictures/photo_2026-09-26_18-58-44.jpg` | Detalle adicional de las soldaduras |

## Notas eléctricas importantes

- El CW Pokemon usa lógica **3.3V** — no es necesario convertidor de nivel.
- El pin **CLK (RING)** tiene pull-up interno a **3.2V** en el CW Pokemon.
- El pin **DATA (TIP)** tiene un pull-down interno de **~8 kΩ a GND** en el CW Pokemon.
- **CRÍTICO — modo push-pull en DATA:** Con el pull-up interno de 45 kΩ del ESP32 en modo open-drain, la línea DATA solo alcanza ~0.5 V porque el pull-down de 8 kΩ del CW Pokemon gana. El CW Pokemon lee DATA como LOW constante y todo llega corrupto. Configurando el GPIO en **push-pull** (salida activa), el ESP32 conduce activamente 3.3 V y supera el pull-down. Implementado en `firmware-s3/src/ps2.cpp`.

## Indicador LED (WS2812 RGB, GPIO48)

| Estado | Comportamiento LED |
|---|---|
| Teclado USB conectado | Apagado |
| Esperando teclado | Azul, parpadeo rápido (~120 ms) |

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
firmware-s3/              — Firmware (USB HID Host + PS/2 bit-bang)
├── platformio.ini
└── src/
    ├── config.h          — Asignación de pines y constantes
    ├── ps2.cpp/hpp       — Driver PS/2 bit-bang (modo push-pull)
    ├── usb_kbd.cpp/hpp   — USB HID Host (ESP-IDF usb_host)
    ├── keymap.c/h        — Tabla HID keycode → PS/2 Set 2
    └── main.cpp
s3Pictures/               — Fotos del hardware y soldaduras
debug/                    — Capturas de pantalla de depuración
```

## Licencia

GNU General Public License v3.0

Copyright (C) 2026 mcamposv
