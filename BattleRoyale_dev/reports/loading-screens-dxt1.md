# Pantallas de carga en DXT1 (rama `opt/loading-screens`)

Las 12 pantallas de carga de la 0.1.0 son EDDS de 1920x1080 en BGRA de 32 bits sin comprimir (7-11 MB cada una) y son casi todo el peso de `gui.pbo`. `tools/compress_loading_screens.py` las vuelve a codificar en DXT1 con los mismos nombres, tamaño y 11 mips.

| | Original | DXT1 |
| --- | --- | --- |
| 12 pantallas | 120.147.552 bytes | 16.599.936 bytes |
| `gui.pbo` | 126.042.428 bytes | 22.494.942 bytes |
| Calidad (PSNR del mip 0 frente al original) | — | 30,5 a 35,4 dB |

- Comparación visual de los dos peores casos (`br_loading_2` y `br_loading_4`), imagen entera y recorte al 100 % de la zona con más error: `loading-screens-dxt1-comparison.png`.
- Formato: el mismo que vanilla 1.30 (por ejemplo `DZ\water_bliss\ponds\data\enoch_pond_nohq.edds`): cabecera DDS con `ENF1` en el segundo campo reservado, fourcc DXT1, tabla de bloques COPY del mip más pequeño al más grande. Sin `ENF1` el motor lee el fichero como DDS plano y la imagen sale desplazada (visto en el juego en la primera prueba).
- El lector de bloques LZ4 de los originales se comprobó contra los mips guardados: el mip 0 decodificado y reducido coincide con el mip 1 almacenado a 49-52 dB.
- En el juego (DayZDiag 1.30, run e3fac05f, build de esta rama desplegada aparte como `@BR-DXT1`): la pantalla de carga se ve bien, con el consejo del BR y la barra de progreso, y el RPT del cliente no tiene errores de textura nuevos frente a la build de integración.

## Cómo aplicarlo a la 1.0

1. `python tools/compress_loading_screens.py BattleRoyale/addons/gui/textures/loading_screens` en el árbol principal (sustituye los .edds; los originales siguen en `BattleRoyale_dev\extracted_v0.1.0-vigrid` y en esta rama no se versionan, porque los binarios no van en git).
2. `python -I tools/build.py` y desplegar.

Para volver atrás basta con copiar los .edds originales y reconstruir.
