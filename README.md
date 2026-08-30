# Arma Reforger AI Conflict

Scripts-first прототип автономной войны `US` против `USSR` поверх штатного
режима Conflict. Проект использует существующий мир, базы, радио-граф, фракции
и prefab-каталоги Arma Reforger. Собственных world/prefab/layout-ресурсов в
репозитории нет; два тонких `MissionHeader` добавляют запуск из меню сценариев
и наследуют штатные stock/RHS missions.

Игровая логика server-authoritative: сервер применяет выбранную при запуске
политику командования, создаёт и заменяет группы, управляет транспортом,
билетами, снабжением и победой. Клиент получает реплицируемую сводку,
показывает карту/командный интерфейс и отправляет проверяемые сервером запросы.

## Текущее устройство

Stock-вариант загружает два исходных аддона; RHS-вариант добавляет третий:

| Проект | GUID | Назначение |
|---|---|---|
| `AIConflictCore` | `9178E5822AFE48EA` | Карто-независимая модель войны, AI, техника, экономика, UI и диагностика |
| `AIConflictArland` | `B52C5F6AEDBF423E` | Тонкий bootstrap и политики stock Conflict для Arland |
| `AIConflictArlandRHS` | `9F88011DA22B471C` | Опциональный RHS USMC против RHS MSV на штатной RHS Arland mission |

Core и обычный Arland не имеют RHS dependencies. `AIConflictArlandRHS` зависит
от них, RHS Content Pack 01 `1337C0DE5DABBEEF`, Content Pack 02
`BADC0DEDABBEDA5E` и RHS - Status Quo `595F2BF2F44836FB`. Его постоянный GUID —
`9F88011DA22B471C`. Рабочая точка входа обоих вариантов —
`AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_ArlandCampaignBootstrap.c`.
RHS-addon подменяет передаваемый через bootstrap content profile и содержит
локальные compatibility adapters для штатных RHS UI/services, не создавая
второй controller или server loops.

Stock profile сохраняет текущие `US`/`USSR` catalog mappings. RHS profile
разрешает `RHS_USAF` как стабильную сторону `US`, `RHS_AFRF` как `USSR`,
создаёт USMC MEF и MSV VKPO Demiseason rosters из faction `CHARACTER` catalogs
и выбирает только явно поддержанные faction `VEHICLE` candidates. RHS
character/source-roster и vehicle fallback к stock запрещён fail-closed.
Для малых казарм RHS-only building-browser adapter трактует как
`GROUPTYPE_ESSENTIAL` по одному уже зарегистрированному минимальному USMC/MSV
`SentryTeam` на сторону в локальной копии данных фильтра и в локальном массиве
server-side provider validation. Дорогие казармы сохраняют полный штатный
список групп; numeric IDs, faction, provider, budget и server placement
validation не меняются; после штатной проверки временный label удаляется.
Source runtime использует штатную mission
`{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf`; RHS world/mission assets
в репозиторий не копируются.

Актуальные defaults, видимые в коде:

- 10 стабильных group slots на фракцию;
- роли `6 ATTACK / 3 DEFEND / 1 RESERVE`;
- каждый слот по умолчанию создаётся с 10 бойцами;
- стартовый состав — 100 бойцов на фракцию, 200 всего;
- максимальный бюджет управляемых бойцов по умолчанию — 220;
- отдельные faction-scoped AI commanders активны для `US` и `USSR`; режим
  `-aicfAICommanderMode BOTH|US|USSR` выбирается один раз при запуске dedicated
  server, а без параметра используется `BOTH`;
- ground vehicles всегда включены;
- economy/supply pacing всегда включены; CLI opt-out для этих subsystems нет;
- общий rank policy отключает player-rank gates для строительства, заказа
  техники, арсенала, loadouts, групп, защитников, radial commands и Commander
  volunteer; scenario headers дополнительно выдают игрокам `GENERAL` при входе.

`US` и `USSR` в `aicfAICommanderMode` означают сторону, которой разрешено
автономно выбирать новые стратегические цели. Другая сторона сохраняет полный
roster, economy, vehicles, reliability и victory state, но до player order
остаётся в `AWAITING_PLAYER_COMMAND` и физически удерживается на своей HQ через
`SYSTEM_HOLD`. Значения регистрозависимы; `NONE`, пустое и неизвестное значение
отклоняются до создания roster и запуска server loops. Подробности запуска — в
[`docs/SERVER_SETUP.md`](docs/SERVER_SETUP.md#8-параметры-aicf).

Названия Stage в коде отражают эволюцию реализации, но сами по себе не являются
статусом приёмки. Текущий проверочный baseline описан в
[`docs/TESTING.md`](docs/TESTING.md).

## Структура

```text
AIConflictCore/Scripts/Game/AIConflict/
  Bootstrap/     composition root и server loops
  Command/       immutable authority policy и faction-scoped AI commanders
  Config/        defaults и aicf* CLI overrides
  Content/       stock profile и runtime faction -> stable side mapping boundary
  Diagnostics/   стабильные [AICF][STAGE...] события
  Economy/       supply network, транзакции и abstract deliveries
  Forces/        spawn, reinforcement, cohesion и managed AI LOD
  Integration/   адаптер stock Conflict и replicated campaign state
  Objectives/    radio graph и выбор целей
  Orders/        infantry waypoint ownership
  State/         faction/group/vehicle state
  UI/            allied map markers, HUD и strategic command UI
  Vehicles/      transport domain и physical cleanup

AIConflictArland/Scripts/Game/AIConflictArland/Integration/
  bootstrap, AI-only capture, victory override, radio normalization
AIConflictArland/Missions/
  игровая плитка AI Conflict - Arland поверх штатного Conflict

AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/
  RHS USMC/MSV content profile, bootstrap factory override и RHS-only adapters
AIConflictArlandRHS/Missions/
  игровая плитка AI Conflict RHS - Arland поверх штатной RHS mission

tools/
  статические аудиторы, анализаторы runtime-логов и API reference helper
```

Подробная карта компонентов и потоков: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Начало работы

Нужны Windows, Arma Reforger, Arma Reforger Server и Arma Reforger Tools одной
версии. Закреплённая в репозитории API-база — `1.8.0.10`; установленную версию
всё равно нужно записывать для каждого runtime-прогона.

Для Codex действует terminal-only workflow: без Launcher/Workbench GUI,
Computer Use и скриншотов. Workbench validation, server и client запускаются
командами из [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md); результат проверяется
по exit code и полным логам. Визуальные критерии остаются `NOT RUN`, пока их
вручную не проверит пользователь.

Для ручного source-запуска из самой игры открой Diag-клиент с нужным root
project и addon graph, затем выбери `Сценарии -> AI Conflict - Arland` либо
`Сценарии -> AI Conflict RHS - Arland`. Готовые команды и ограничения описаны
в [`docs/SERVER_SETUP.md`](docs/SERVER_SETUP.md#запуск-из-меню-сценариев).
После Workshop-публикации те же плитки появляются у включённых packaged addons.
Запуск из меню не задаёт CLI-параметры и поэтому использует default
`aicfAICommanderMode=BOTH`.

1. Запустите применимые статические проверки из
   [`docs/TESTING.md`](docs/TESTING.md).
2. Выполните терминальный Diag Workbench Validate/Compile.
3. При изменении поведения запустите server/client из терминала на свежих
   profiles и проанализируйте полные логи.

Быстрые статические команды:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35RecoveryPolicy.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-AICommanderModeStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RHSIntegrationStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-ScenarioHeadersStatic.ps1
```

Репозиторий не содержит CI, готового `.pak` или автоматической первой
публикации. Canonical Workshop metadata, preview assets и ручной release
workflow находятся в [`workshop/`](workshop/README.md) и
[`docs/PUBLISHING.md`](docs/PUBLISHING.md). До первой ручной публикации через
Workbench проект продолжает запускаться как unpacked source addon.

## Документация

- [`AGENTS.md`](AGENTS.md) — постоянные инструкции для Codex.
- [`docs/SERVER_SETUP.md`](docs/SERVER_SETUP.md) — пользовательский запуск dedicated server и клиента.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — домены, lifecycle и trust boundaries.
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — Workbench, API reference и Diag-запуск.
- [`docs/PUBLISHING.md`](docs/PUBLISHING.md) — подготовка, первый Workshop upload и проверка packaged build.
- [`docs/TESTING.md`](docs/TESTING.md) — gates, команды, baseline и evidence.

Проект распространяется на условиях [`LICENSE`](LICENSE).
