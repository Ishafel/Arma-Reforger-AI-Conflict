# Arma Reforger AI Conflict

Scripts-only прототип автономной войны `US` против `USSR` поверх штатного
режима Conflict. Проект использует существующий мир, базы, радио-граф, фракции
и prefab-каталоги Arma Reforger; собственных world/prefab/layout-ресурсов в
репозитории сейчас нет.

Игровая логика server-authoritative: сервер выбирает цели, создаёт и заменяет
группы, управляет транспортом, билетами, снабжением и победой. Клиент получает
реплицируемую сводку, показывает карту/командный интерфейс и отправляет
проверяемые сервером запросы.

## Текущее устройство

Загружаются два исходных аддона:

| Проект | GUID | Назначение |
|---|---|---|
| `AIConflictCore` | `9178E5822AFE48EA` | Карто-независимая модель войны, AI, техника, экономика, UI и диагностика |
| `AIConflictArland` | `B52C5F6AEDBF423E` | Тонкий bootstrap и политики stock Conflict для Arland |

Оба зависят от штатного проекта `58D0FB3206B6F859`; Arland дополнительно
зависит от Core. Рабочая точка входа —
`AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_ArlandCampaignBootstrap.c`.

Актуальные defaults, видимые в коде:

- 10 стабильных group slots на фракцию;
- роли `6 ATTACK / 3 DEFEND / 1 RESERVE`;
- первые четыре слота создаются по 10 бойцов, остальные шесть — по 4;
- стартовый состав — 64 бойца на фракцию, 128 всего;
- максимальный бюджет управляемых бойцов по умолчанию — 220;
- ground vehicles всегда включены;
- economy/supply pacing всегда включены; CLI opt-out для этих subsystems нет.

Названия Stage в коде отражают эволюцию реализации, но сами по себе не являются
статусом приёмки. Текущий проверочный baseline описан в
[`docs/TESTING.md`](docs/TESTING.md).

## Структура

```text
AIConflictCore/Scripts/Game/AIConflict/
  Bootstrap/     composition root и server loops
  Config/        defaults и aicf* CLI overrides
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
```

Репозиторий не содержит CI, `.pak`, Workshop metadata или готового
package/publish pipeline. Сейчас проект разрабатывается и запускается как
unpacked source addon.

## Документация

- [`AGENTS.md`](AGENTS.md) — постоянные инструкции для Codex.
- [`docs/SERVER_SETUP.md`](docs/SERVER_SETUP.md) — пользовательский запуск dedicated server и клиента.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — домены, lifecycle и trust boundaries.
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — Workbench, API reference и Diag-запуск.
- [`docs/TESTING.md`](docs/TESTING.md) — gates, команды, baseline и evidence.

Проект распространяется на условиях [`LICENSE`](LICENSE).
