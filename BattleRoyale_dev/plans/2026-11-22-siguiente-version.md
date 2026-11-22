# Siguiente versión tras la 2.0.1: encargos de Guillermo (2026-11-22)

Estado de partida: 2.0.1 en GitHub (`main` `656508e`) y en el Workshop.

## 1. Paracaídas propio (quitar la dependencia de Admiral's)
Pedido: «mirar a ver si podemos implementar los parachutes dentro de nuestro mod».
- Hoy es dependencia dura: `requiredAddons` con `ADM_Parachute_Mod` en `BattleRoyale/addons/scripts_server/config.cpp:9` y `scripts_client/config.cpp:10`; mochila `ADM_ParachuteBag_Fabric` (`BattleRoyalePlaneFlight.c:6`); física de Admiral's (`HumanCommandScript_Parachute`, modded en `scripts_client/4_World/BattleRoyale/BattleRoyaleParachute.c`).
- No se puede copiar su modelo ni su código sin permiso (memoria `dayz-mod-publishing-licence`).
- Primer paso: análisis de viabilidad sin código. Piezas: mochila, modelo de tela con animación, comando de vuelo propio, HUD. Hipótesis sin comprobar: la física es corta (vertical = -0,5 x multiplicador; -1,5 con W) y lo caro es el modelo y su animación.

## 2. `legacy_spawn` sustituye a `enable_plane_deployment`
Pedido: parámetro de spawn antiguo opcional, por defecto 0; con 1, sin avión, como antes. Guillermo aprobó el nombre `legacy_spawn` con migración.
- El modo antiguo ya existe: `enable_plane_deployment = 0` (`4_BattleRoyalePrepare.c:47`).
- Config `general_settings.json` v5: `legacy_spawn` (0 por defecto) sustituye a `enable_plane_deployment`; migración v4->v5: `legacy_spawn = 1` si `enable_plane_deployment` era 0. R7: todos los usos de `enable_plane_deployment` (servidor, cliente, VPP compat, README, ServerFiles, CHANGELOG).
- Con el punto 1 hecho, un servidor en `legacy_spawn = 1` no necesitaría Admiral's.

## 3. Borde del espectador con otra tonalidad
Pedido: «el borde que hay en los extremos de la vista, que tiene tonalidad distinta».
- Causa probable (de leer el layout, sin verlo en el juego): `BattleRoyale/addons/scripts_client/layouts/br_spectator.layout`. Barras negras al 55 %: arriba 72 px, abajo 48 px y laterales de 24 px. Las laterales empiezan en y=72 con alto relativo 1 (pantalla entera), así que se pasan 72 px y pisan la barra de abajo: en las esquinas inferiores se suman dos capas y salen más oscuras. Las laterales además hacen un marco que no casa con el resto del HUD.
- Propuesta: quitar las barras laterales (quedan solo arriba y abajo), o ajustarlas a la altura exacta entre las dos. Verificar con captura in-game.
