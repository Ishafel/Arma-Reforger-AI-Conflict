# AI Conflict Everon — Workshop metadata

Этот файл — canonical source для ручного заполнения `Publish Project`. Workbench
не импортирует Markdown автоматически: поля ниже копируются владельцем при
первой публикации и при каждом обновлении Workshop entry.

## Поля Publish Project

- Project Name: `AI Conflict Everon`
- Version: `0.1.0`
- Visibility: `Test`
- Category: `Systems & Features`
- Tags: `AI CONFLICT EVERON COLDWAR US USSR`
- License: `Custom`
- Preview Image: `workshop/AIConflictEveron/preview.jpg`

## Обязательные зависимости

Проверь эти зависимости в Workbench до публикации:

- Arma Reforger: `58D0FB3206B6F859`;
- AI Conflict Core: `9178E5822AFE48EA`;
- AI Conflict Arland: `B52C5F6AEDBF423E`.

`AI Conflict Arland` используется как проверенный общий stock Conflict
integration addon. Он не меняет карту Everon: сценарий по-прежнему наследует
официальную миссию Conflict — Everon.

## Summary

Autonomous US-versus-USSR war for the official Conflict mode on Everon,
powered by AI Conflict Core.

## Description

AI Conflict Everon turns the official Conflict scenario on Everon into an
autonomous US-versus-USSR campaign. It reuses the original world, bases, radio
network, factions and Conflict systems; no vanilla world or mission assets are
copied into this addon.

Features:

- separate server-authoritative AI commanders for US and USSR;
- ten stable force slots per faction with infantry deployment, replacement and
  order recovery;
- objective selection and frontline movement based on the live stock radio
  graph;
- an Everon-specific, data-driven escape from an isolated friendly radio
  component: only after all reachable targets are exhausted, one owned non-HQ
  base is deterministically linked to the nearest useful relay;
- base capture, tickets, supplies and a server-authoritative match result;
- ground transport with guarded acquisition, boarding, transit, dismount and
  cleanup;
- replicated campaign summaries, allied markers and strategic command UI;
- player-issued `MOVE AND HOLD` orders for selected squads from the stock map.

Required Workshop dependencies:

- AI Conflict Core (`9178E5822AFE48EA`);
- AI Conflict Arland (`B52C5F6AEDBF423E`).

How to use:

1. Install and enable AI Conflict Everon. Let Workshop download and enable both
   required dependencies.
2. Open `Scenarios` and select `AI Conflict - Everon`, or join a server that
   lists the complete dependency set.
3. Both factions use AI commanders by default. Dedicated-server operators can
   instead enable autonomous target selection for only US or only USSR with the
   exact `aicfAICommanderMode` startup setting.
4. Session saves are intentionally disabled. Every local play or hosted server
   starts a new campaign.

Compatibility and scope:

- designed for the official Conflict — Everon mission inherited from
  `{ECC61978EDCC2B5A}Missions/23_Campaign.conf`;
- current development baseline: Arma Reforger `1.8.0.13`;
- uses the stock Cold War US and USSR content catalogs;
- does not require or include RHS packages;
- not intended to be loaded into unrelated worlds without a separate
  compatibility review.

Русский:

AI Conflict Everon превращает официальный Conflict на Everon в автономную
кампанию US против USSR. Мод использует штатный мир, базы, radio graph, фракции
и системы Conflict; копий vanilla world или mission assets в addon нет.

Обе стороны получают по десять стабильных отрядов, самостоятельно выбирают
доступные цели, захватывают базы, используют наземный транспорт, билеты и
снабжение. Специальная политика Everon устраняет тупик, когда связанный с HQ
радиокомпонент исчерпал все доступные цели: только в этот момент одна союзная
база детерминированно получает связь с ближайшим полезным relay, после чего
граф перестраивается и командир получает новые допустимые цели.

Для запуска установите `AI Conflict Everon` вместе с автоматически
подтягиваемыми зависимостями `AI Conflict Core` и `AI Conflict Arland`, затем
откройте `Сценарии -> AI Conflict - Everon`. По умолчанию обеими сторонами
управляют AI commanders. Сохранение прогрессии отключено: каждый локальный
запуск или hosting начинает новую кампанию.

License: Apache License 2.0 (`Custom`; full text is included in
`AIConflictEveron/license.txt`).

## Change Notes

Version 0.1.0 introduces:

- a dedicated Everon root addon and inherited `AI Conflict - Everon` scenario;
- explicit Core and stock-integration dependencies;
- autonomous US-versus-USSR Conflict gameplay on the official Everon world;
- deterministic recovery for isolated friendly radio components after their
  reachable objectives are exhausted;
- fresh-session behavior with campaign persistence disabled.
