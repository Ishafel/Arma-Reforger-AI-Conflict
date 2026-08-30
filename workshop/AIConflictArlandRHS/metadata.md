# AI Conflict Arland RHS — Workshop metadata

## Поля Publish Project

- Project Name: `AI Conflict Arland RHS`
- Version: `0.1.6`
- Visibility: `Test`
- Category: `Systems & Features`
- Tags: `AI CONFLICT ARLAND RHS USMC MSV`
- License: `Custom`
- Preview Image: `workshop/AIConflictArlandRHS/preview.jpg`

## Summary

Autonomous RHS USMC-versus-MSV campaign for the RHS Conflict mission on
Arland. Requires AI Conflict Core, Arland integration and RHS packages.

## Description

AI Conflict Arland RHS adapts the autonomous AI Conflict campaign to the RHS
Conflict mission on Arland. RHS USMC MEF fights RHS Russian MSV while both
sides select objectives, deploy stable ten-member squads, use supported
ground transport, spend tickets and supplies, capture bases and fight to a
server-authoritative match result.

Required Workshop dependencies:
- AI Conflict Core;
- AI Conflict Arland;
- RHS Content Pack 01;
- RHS Content Pack 02;
- RHS - Status Quo.

The current integration targets RHS - Status Quo 0.16.5150 and Arma Reforger
1.8.0.10. It uses only faction-catalog RHS characters and supported vehicles;
missing RHS roster or vehicle content fails closed instead of silently falling
back to stock US/USSR assets. Deployment-map identities are normalized for the
managed RHS factions.

How to use:
1. Install and enable AI Conflict Arland RHS; Workshop should download Core,
   Arland and all RHS packages as dependencies.
2. Open Scenarios and select `AI Conflict RHS - Arland`, or join a server that
   lists this addon in its required mods.
3. Do not enable this optional addon for the stock-only AI Conflict setup.
4. Session saves are intentionally disabled. Every local play or hosted server
   starts a new AI Conflict campaign.

No RHS assets are copied or redistributed by this addon. RHS packages retain
their own licenses; Apache License 2.0 applies to the AICF integration code.

Русский:
Интеграция автономной кампании AI Conflict со штатной RHS Conflict mission на
Arland: RHS USMC MEF против RHS Russian MSV, отряды по десять бойцов,
поддержанная наземная техника, билеты и снабжение. Мод не содержит ресурсов
RHS и требует отдельно установить перечисленные Workshop dependencies. Для
локального запуска откройте `Сценарии` и выберите именно
`AI Conflict RHS - Arland`, а не stock-плитку из dependency. Сохранение
прогрессии отключено: каждый локальный запуск или hosting начинает новую
кампанию.

License: Apache License 2.0 (Custom license; full text is included in the
addon).

## Change Notes

Version 0.1.6 disables session persistence for `AI Conflict RHS - Arland`, so
every local play or hosted server starts a new campaign instead of loading
previous progression.
