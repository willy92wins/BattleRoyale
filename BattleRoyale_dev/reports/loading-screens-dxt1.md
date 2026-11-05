# Pantallas de carga comprimidas (rama `opt/loading-screens`)

Las 12 pantallas de carga de la 0.1.0 son EDDS de 1920x1080 en BGRA de 32 bits sin comprimir (7-11 MB cada una) y son casi todo el peso de `gui.pbo`. `tools/compress_loading_screens.py` las vuelve a codificar con los mismos nombres, tamaño y 11 mips: en DXT1, o en DXT5 cuando algún píxel tiene un alfa por debajo de 230, para conservar la transparencia.

| | Original | Comprimido |
| --- | --- | --- |
| 12 pantallas | 120.147.552 bytes | 22.132.384 bytes (8 en DXT1, 4 en DXT5) |
| `gui.pbo` | 126.042.428 bytes | 28.027.390 bytes |
| Calidad (PSNR del mip 0 frente al original, RGBA) | — | 31,7 a 36,7 dB |

- Comparación visual de un caso DXT1 (`br_loading_4`) y uno DXT5 (`br_loading_6`), imagen entera y recorte al 100 % de la zona con más error: `loading-screens-dxt1-comparison.png`.
- Alfa: las pantallas llevan una marca de agua con bordes suaves en la esquina inferior derecha. Las de alfa mínimo 195-226 (2, 11, 12) y 146 (6) van en DXT5; en las demás el alfa más bajo es 235 o más y se vuelve opaco, una diferencia menor que el ruido de DXT1.
- Formato: el mismo que vanilla 1.30 (por ejemplo `DZ\water_bliss\ponds\data\enoch_pond_nohq.edds` para DXT1 y `enoch_pond_moss_ca.edds` para DXT5): cabecera DDS con `ENF1` en el segundo campo reservado, tabla de bloques COPY del mip más pequeño al más grande. Sin `ENF1` el motor lee el fichero como DDS plano y la imagen sale desplazada (visto en el juego en la primera prueba).
- El lector de bloques LZ4 de los originales se comprobó contra los mips guardados: el mip 0 decodificado y reducido coincide con el mip 1 almacenado a 49-52 dB.
- En el juego (DayZDiag 1.30, build de esta rama desplegada aparte como `@BR-DXT1`): una pantalla DXT1 (run e3fac05f) y una DXT5 (run 57aa82b9, con un `data.pbo` de prueba que solo listaba pantallas DXT5) se ven bien, con el consejo del BR y la barra de progreso. El RPT del cliente no tiene errores de textura nuevos frente a la build de integración.
- Revisión de Codex en dos rondas (`C:\Users\guill\Repos\br-runs\loading-screens\codex_review.md` y `codex_review2.md`): aprobar con cambios. Aplicados: no escribe nada hasta comprobar todos los ficheros, rechaza por debajo de `--min-psnr` (28 dB por defecto) y un umbral no finito, escribe a un temporal, lo relee y lo renombra, exige `--backup` para sustituir en sitio y rechaza un `--out` que sea la carpeta de origen (también por alias, enlace o unidad `subst`), valida el entramado LZ4 (límites, consumo exacto y flags 0x00/0x80) y conserva el alfa con DXT5. Tras los últimos arreglos la salida es idéntica byte a byte a la probada en el juego.

## Cómo aplicarlo a la 1.0

1. En el árbol principal: `python tools/compress_loading_screens.py BattleRoyale/addons/gui/textures/loading_screens --backup build/loading_screens_original` (la copia queda fuera del addon; los originales también siguen en `BattleRoyale_dev\extracted_v0.1.0-vigrid`).
2. `python -I tools/build.py` y desplegar.

Para volver atrás basta con copiar los .edds originales y reconstruir.
