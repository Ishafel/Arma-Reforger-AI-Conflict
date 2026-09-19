# Light Factory на пологом участке — 2026-09-19

Продолжение [проверки поиска площадок](CONSTRUCTION_SEARCH_20260919.md).
HEAD: `8cdc67a93e2611ad498a3ae02f696c1bf8e8f034`. Game/Server/Tools: `1.8.0.13`.
Evidence: `.codex-runtime/light-factory-20260919/`.

## Наблюдение в пользовательской сессии

В Stock Arland, profile `Server-20260919-024353-623`, на главной базе USSR
`0x20000000000000F6` поиск Light Depot завершился двумя `NO_SAFE_SITE`:
201 и 256 candidates. В первом поиске 123 отказа `PHYSICAL_OBSTRUCTION`,
39 `SLOPE_OR_FOUNDATION_GAP`; во втором — 153 и 31 соответственно.
Это не доказательство отсутствия свободной площадки.

Пользователь сообщил, что разместил Light Factory вручную. Затем server log
показал подход строителя к новому `target=0x40000000000079CF`, работу
с `tool_active=1 item_using=1` и `BUILDER_COMPLETED` в 03:05:42.
Лог содержит позицию работника `<1590.31,6.31059,915.978>`, а не точный transform
поставленного пользователем объекта. Поэтому точное повторение этой точки
и единственная причина её пропуска **не доказаны**.

Сессия остановлена через identity-checked `Stop-Process` после просьбы
пользователя выключить компьютер по завершении работы. Полные остановленные
логи сохранены как `play-server-stopped.log` и `play-client-stopped.log`;
это принудительная остановка процессов, не штатный runtime PASS.

## Исправление

Поиск ошибочно ограничивал перепад **на всей композиции** величиной 0.8 м.
В закреплённом Script Diff `1.8.0.13` штатный
`SCR_CampaignBuildingLayoutComponent.SpawnComposition()` применяет
`SCR_RefPreviewEntity.SpawnAndApplyReference` с `EEditorTransformVertical.TERRAIN`.
Общий перепад между удалёнными частями композиции не равен местному уклону.

Теперь проверяются обе соседние точки сетки: по строке и по столбцу.
Предел — 0.8 м на 3 м, с пересчётом по фактическому расстоянию.
Высоты сохраняются между ticks и сбрасываются при смене кандидата.
Вода, дороги, объекты, navmesh и отдельные выезды depot по-прежнему проверяются.
Измеренные min/max высоты сохраняются в receipt и консервативно расширяют
вертикальные physics bounds при commit/completion, сохраняя исходные объёмы.
Квоты queries, candidates, placement и deadline не увеличены.

Это исправление одного излишне строгого ограничения. Полное совпадение с
ручным placement, точная геометрия каждого наклонённого дочернего mesh и
нахождение каждого доступного места не заявляются.

Изменены в этом продолжении:

- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionSiteSearch.c`;
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_ConstructionOrder.c`;
- `tools/Test-ConstructionStatic.ps1`, `tools/Test-ConstructionContracts.ps1`;
- `tools/fixtures/AICF_ConstructionRuntimeProbe.c`;
- `docs/ARCHITECTURE.md`, `docs/CONSTRUCTION_VALIDATION.md`, `docs/TESTING.md`, этот отчёт.

## Проверки

До/после выполнены:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-BaseBuildersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-AICommanderModeStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage4Static.ps1
```

ConstructionStatic, ConstructionContracts, BaseBuildersStatic и Stage4Static:
**PASS / 0**. AICommanderModeStatic сохраняет исходный
**FAIL / 1, AI_COMMANDER_UI_STATE**. Contracts: 13 log inputs, positive и
шесть negative static inputs, включая потерю проверки соседней строки и
height envelope при completion. Native helpers: прежние 24 и шесть terrain cases.

Production Workbench Validate без fixture: **PASS / exit 0** на всех четырёх
graphs — Arland, Everon, ArlandRHS, EveronRHS. Команда:
`& .\.codex-runtime\light-factory-20260919\Validate-Production.ps1`.
Точные arguments, exit и полные console logs сохранены в `wb-production-*/`;
сводка — `production-workbench.json`. SCRIPT/VM/fatal errors отсутствуют.

Первая промежуточная compile-попытка обнаружила недопустимый `+=` для vector
accessor; исправлено на явное присваивание. Неудачный лог сохранён в
`wb-probe/`. Повторный fixture compile сообщил `Script validation successful`,
но его native exit не был надёжно захвачен; окончательные четыре production
gates имеют сохранённый native exit 0.

Попытка отдельного runtime с `-bindPort 2002` не прошла запуск replication:
режим прямого `-server world` фактически слушал занятый 2001. Native exit 0
этой попытки **не считается PASS**; есть startup/shutdown errors.
Полный output — `probe-launch.txt`. В [документации Bohemia](https://community.bistudio.com/wiki/Arma_Reforger%3AStartup_Parameters)
`bindPort` описан как override server config; перенос этого поведения на
прямой запуск не подтвердился.

Runtime fixture запускается из отдельной source-копии `runtime-source`, через
канонический `tools/Start-AICFRuntime.ps1 -RepositoryRoot <runtime-source>`.
Production source не содержит временной fixture. Она обеспечивает supplies;
решения, placement, оплата и работа строителя выполняются production path.

## Ограничения

Ручная проверка вида построенных ботом объектов и проезда, новый client/JIP,
Everon/RHS runtime, вся матрица типов и длительный soak — **NOT RUN**.
Успех отдельных зданий не является количественным benchmark частоты строительства.
Результаты завершённых runtime-прогонов приведены ниже.

## Завершённый focused runtime

Profile `Server-20260919-031413-837`; полный путь сохранён в
`runtime-manifest.json` и `probe-final-logpath.txt`.
Вызов canonical launcher дополнен:

```powershell
-AdditionalArguments @('-aicfConstructionProbe','1',
  '-aicfConstructionProbeRefill','1','-aicfConstructionProbeMs','360000',
  '-aicfConstructionProbeType','2','-aicfConstructionProbeRepeatType','2',
  '-aicfConstructionProbeTrace','1','-aicfRequirePlayerForResult','0')
```

Штатное завершение fixture, native exit 0. На HQ USSR `0x20000000000000F6`
Light Depot найден за 7 кандидатов, оплачен `1000 → 850` и достроен:
`CONSTRUCTION_COMPLETED service_online=1`, около 64 с от начала controller log.
Также достроены Light Depot US на HQ и захваченной базе. После Light Depot
USSR построены малые казармы на `<1622.16,4.85747,922.911>`:
`terrain_delta=1.39552`, один кандидат, supplies `1300 → 1050`,
`BUILDER_COMPLETED` и online service. Этот участок ранее не прошёл бы
общий порог 0.8 м. Все 30 native helper checks прошли;
max window 96 queries, candidate-budget нарушений нет.
Max tick 292 мс включает синхронный fixture test 400 collision volumes.

Весь остановленный `probe-final-console.log` проверен:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-ConstructionLog.ps1 `
  -LogPath '<evidence>/probe-final-console.log' -ExpectedMode BOTH -RequireCompletion
```

Общий log audit — **FAIL / 1**, при четырёх placement и четырёх completion:

- `CONSTRUCTION_ENGINE_ERROR`: две известные shutdown ошибки stock
  `SCR_BaseResupplySupportStationComponent needs a entity catalog manager!`;
- `CONSTRUCTION_DECISION_TOO_EARLY`: между первыми двумя decisions одной базы
  по log timestamps 59998 мс вместо 60000. Planner использует `now`, взятый
  до работы tick, тогда как event записывается позже; этот код в данном
  продолжении не менялся. Исходный аудит не ослаблялся, finding сохранён.

В полном логе 17 error lines: 4 ENTITY, 8 WORLD, 3 RESOURCES, 2 SCRIPT на
shutdown; VM/fatal отсутствуют. Индекс — `probe-final-errors.txt`, интервалы
decisions — `decision-gaps.json`. Успешные завершения конкретных построек
не повышают общий runtime gate до PASS. Первый тип в этом тесте Light Depot,
поэтому он не повторяет последовательность зданий пользовательской сессии.

## Обычный порядок зданий

Profile `Server-20260919-032037-732`; flags `aicfConstructionProbe=1`,
`aicfConstructionProbeRefill=1`, `aicfConstructionProbeMs=180000`,
`aicfConstructionProbeTrace=1`, `aicfRequirePlayerForResult=0`.
Override типа отсутствует. Manifest — `default-manifest.json`.
Faction allocation на этот раз обратный: физическая база `0x20000000000000F6`
принадлежит US, а `0x20000000000000FA` — USSR. Поэтому это не точное
повторение пользовательской HQ USSR.

Сначала достроены малые казармы обеих сторон, затем Light Depot US на
`<1621.89,4.81528,922.458>`, yaw 270, 49 candidates; supplies `1100 → 950`,
`CONSTRUCTION_COMPLETED service_online=1`. Казарма US имела общий
`terrain_delta=1.42401`. У USSR поиск следующего Light Depot на другой HQ
не завершился до остановки fixture: 207 candidates, `reason=STOP`.
Успешную последовательность «казарма → Light Depot» для USSR на той же
пользовательской HQ этот run не доказывает.

Штатная остановка, native exit 0; 3 placement, 3 completion, 30/30 helper
checks, max window 96. Полный `probe-default-console.log`:
`Test-ConstructionLog -ExpectedMode BOTH -RequireCompletion` — **FAIL / 1**,
только `CONSTRUCTION_ENGINE_ERROR` из-за тех же двух stock shutdown ошибок.
Всего 17 error lines, та же классификация, что в focused run; VM/fatal нет.
Интервальный finding первого прогона здесь не повторился. Полный вывод
аудитора — `probe-default-audit.txt`.

`git diff --check` и проверка относительных Markdown links — **PASS / 0**.
SHA-256 изменённых production search/order совпадают с runtime source-копией
(`source-hashes.json`). Test fixture в production отсутствует. Клиент, оба
тестовых сервера и Workbench завершены. Пользователь запросил выключение
компьютера после завершения работы; итоговые артефакты сохранены до этой команды.
