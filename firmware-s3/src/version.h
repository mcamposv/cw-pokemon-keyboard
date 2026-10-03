#pragma once

// En builds locales devuelve "dev".
// El workflow de GitHub Actions sobreescribe este fichero con la versión del tag.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
