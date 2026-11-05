# HANDOFF — BattleRoyale

<!-- LIVE-STATE -->
- objetivo: 1.0 en 24 h (desde 2026-11-05 00:23Z): DayZ 1.30, Chernarus, 120 jugadores, carga mínima
- estado (2026-11-05 04:00Z): la rama `integration` es la candidata 1.0 (ver `git log -1 integration`). Seis lanes fusionadas sin conflictos, cada una revisada por Codex con sus rondas de corrección. Los arreglos del integrador (8cae221, 2142483, e83f8a0, 5ca2fec) y el contrato entre lanes pasan una revisión final de Codex: `C:\Users\guill\Repos\br-runs\final-review\codex_final_review.md`
- notas de versión y problemas conocidos: `CHANGELOG.md`
- build: `python -I tools/build.py` → `build/@BattleRoyale`, firmada con `C:\Users\guill\Repos\br-keys\BattleRoyaleCommunity.biprivatekey` (la privada no entra en el repo). Despliegue de prueba en `C:\Users\guill\Repos\br-deploy\@BattleRoyale`, enlazado como `!Workshop\@BattleRoyale`
- verificado: gate de lint = base; partida de 1 jugador en DayZDiag 1.30 con todos los estados y 0 excepciones en servidor y cliente (5ca2fec, run 81fe2664); `restart_loop.ps1` 20/20 en el arnés `br-wt\config-mission\build\rltest`
- pendiente: F6 prueba con jugadores (20-40, luego 100+); repo en GitHub (espera el sí de Guillermo); `gui.pbo` de 126 MB; airdrop de Expansion sin probar; licencia de la música «Apocalypse»; VPP en la 1.1
- plan: `BattleRoyale_dev/plans/2026-11-05-plan-1.0.md` (doc vivo: https://claude.ai/artifact/YU2C2QmggTzDR2sRpynFPe)
- informe: `BattleRoyale_dev/audit/2026-11-04-informe-auditoria.md`
- lint base conocido: scripts_client 11 FAIL ES-LAYOUT-PATH-PBOPREFIX-MISMATCH (falsos positivos: los layouts viven en el PBO gui), scripts_server 2 FAIL ES-OVERRIDE-PARAM-NAME-MISMATCH (falsos positivos: `RemovePlayer` es del propio BattleRoyaleState, no de vanilla). `bash tools/gate.sh .` falla si se supera
- entorno de prueba: dayz-mcp `dayz_test_run` proyecto DayZ_MCP con `extra_mods` @CF, @Dabs Framework, @BR-COT, @BR-Expansion-Core, @BR-Expansion, @BR-Expansion-Navigation, @BattleRoyale (junctions en `!Workshop`). Perfil `P:\DayZ_MCP_dev\_server\profiles\Vigrid-BattleRoyale\` con `minimum_players` 1; la copia del perfil 0.1.0 está en `Vigrid-BattleRoyale.bak-0.1.0-20261105`
- bloqueos: ninguno
<!-- /LIVE-STATE -->

## Decisiones tomadas sin preguntar (noche del 2026-11-05)

Guillermo dio permiso pleno para avanzar durante la noche. Todas se revierten con un cambio de config o un commit.

- Infectados: 150 como máximo, sin animales (`BattleRoyale_server\mpmissions\...\db\globals.xml`).
- UI de la 1.0: killfeed propio y fase en el HUD; el resto de mejoras de UI queda para una 1.0.x.
- Zona final: las ciudades grandes de la costa no pueden serlo (`end_avoid_city`, 130 entradas, en `zone_settings.json`).
- Config rota: se anota con `Error` y se usa el valor por defecto; el servidor no se para.
- `restart_loop.ps1` exige `-InstanceId` y solo toca `storage_<id>` de esa instancia: la aparta como `storage_<id>.prev-<fecha>`, guarda las 3 copias más nuevas (`-KeepPrevious`) y borra solo las más viejas de esa misma instancia.
- Sin migración automática de los JSON de perfil de la 0.1.0: el README pide borrarlos.
