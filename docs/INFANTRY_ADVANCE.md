# Продвижение пехоты при фоновой стрельбе

## Поведение

Пехота US и USSR во время перехода короче осматривает направление фоновой
стрельбы и раньше возвращается к приказу. Настройка действует одинаково в
stock/RHS, для автономных и player orders. Она не ускоряет анимации и не
принуждает солдата бежать сквозь прямой огонь.

| Параметр | Значение | Эффект |
|---|---:|---|
| Близкая угроза | до 75 м включительно | Штатная реакция |
| Дальняя угроза | от 150 м | Приоритетное наблюдение не дольше 1,5 с |
| Остальная фоновая угроза | больше 75, меньше 150 м | Приоритетное наблюдение не дольше 3 с |
| Сокращение штатного окна | ×0,5, с указанными верхними пределами | Более короткое штатное окно также сокращается |
| Приоритет после окна | штатный низкий приоритет наблюдения `4` | Обычное движение с приоритетом `30` может продолжиться |
| Зона цели | максимум из 30 м и completion radius waypoint | Прибывший боец сохраняет штатное удержание позиции |

Приоритеты — значения закреплённого Script Diff `1.8.0.13`. Время окна
отсчитывается от штатного начала/повторного включения высокой реакции.
Это предел приоритета одного поведения, а не гарантия начала движения в
указанную секунду: стрельба, лечение, строй и навигация могут требовать остановки.
Параметры пока фиксированы в `AICF_InfantryAdvancePolicy`, новых CLI flags нет.

## Область и защита

Policy каждый раз синхронно проверяет authority, живого AI, отсутствие player
control, exact controlled entity, актуального владельца group в одном из двух
faction states, `READY`, `INFANTRY` и совпадение текущего waypoint с owned waypoint
слота. Кешей identity, новых callbacks и server loops нет. Controller добавляет
только read-only lookup; waypoint и стратегия остаются у `AICF_OrderPlanner`.

Исключены `SYSTEM_HOLD`, route replan/field holds, отступление одиночного
выжившего, набор в казарме, SearchAndDestroy, relay smart action, vehicle
suspension, нахождение/посадка/выход из транспорта. FIA, строители, сторонние
группы и экипажи не входят в scope. Прибытие проверяется по позиции конкретного
бойца, поэтому ожидание остальных по `ALL` не ослабляет его оборону.

При наличии текущей боевой цели, injury factor или suppression выше `0.01`
результат остаётся штатным. Проверяются **оба** активных сектора угроз:
`DIRECTED_AT_ME`, `CAUSED_DAMAGE`, danger от `2.0` или расстояние до 75 м
запрещают послабление. Флаги прямого огня сохраняются столько же, сколько в
stock threat memory; policy их не обнуляет и не ускоряет забывание.
Поведение ухода от гранат и остальные combat actions не переопределяются.

Единственное изменение stock — результат `SCR_AIObserveThreatSystemBehavior.CustomEvaluate()`.
Сначала вызывается `super`, затем при безопасном текущем контексте и истечении
сокращённого окна возвращается штатный низкий приоритет наблюдения.
Штатные таймеры, threat memory, навык `EXPERT`, perception, combat mode,
стрельба на подавление и скорость не записываются.

## Проверки

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryAdvanceContracts.ps1`
проверяет границы scope и отклоняет 15 отрицательных мутаций. Это static gate,
не доказательство движения или выживаемости.

`tools/fixtures/AICF_InfantryAdvanceProbe.c` подключается только в изолированной
копии Core, в `Scripts/Game/AIConflict/Orders/`. Production Core её не содержит.
После `ROSTER_READY` fixture проверяет native observer настоящих бойцов обеих
сторон: короткое/длинное окно, близкий огонь, пролёт сбоку, suppression,
injury/damage, прибытие, stale group, suspension и crew. Для этих cases
синхронно моделируются возраст реакции и угрозы; test-only состояние
сбрасывается перед отдельным 30-секундным наблюдением за физическим движением.
Последний этап подаёт один далёкий gunshot event в секунду. Он моделирует
фоновую стрельбу и не заменяет полноценную перестрелку или A/B балансировку.

Запуск через canonical launcher на свежем profile после терминального Validate:

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot '<изолированная копия>' -AdditionalArguments @(
    '-aicfInfantryAdvanceProbe','1','-addr','127.0.0.1:22213',
    '-aicfRequirePlayerForResult','0','-aicfConstructionDecisionMs','3600000',
    '-aicfCommanderIntervalMs','60000')
```

Для RHS повторить с `-Variant EveronNorthRHS` после Validate его полного graph.
Fixture завершает процесс через `RequestClose`; Stop снимает оба callback.
`ADVANCE_PROBE_CHECK`, `ADVANCE_PROBE_MOVEMENT`, `ADVANCE_PROBE_FINISHED` — только
индекс: verdict требует полного остановленного console log и native exit.

## Результаты 2026-09-26

- Stage3Static, Stage35Static, Stage35RecoveryPolicy, Stage4Static,
  AICommanderModeStatic, MapPointOrdersStatic, InfantryRecruitmentStatic,
  BaseBuildersStatic и BaseBuilderDangerContracts: `PASS / 0` до и после.
  Baseline failures отсутствуют; новая проверка — `PASS`, 15/15 мутаций отклонены.
- Терминальный Workbench Validate production Arland и полного EveronRHS graph,
  а также обеих изолированных fixture graphs: `PASS / native exit 0`,
  `Script validation successful`, без SCRIPT E/F, ENGINE F и VM/null errors.
- Stock Arland и EveronNorthRHS dedicated runs: **33/33 функциональных checks**
  каждый, штатный `RequestClose`, native exit 0. Checks проверяют policy на
  обеих сторонах, а не успешность полного маршрута.
- Под синтетической фоновой стрельбой за 30 с stock US/USSR сократили расстояние
  до текущей цели на 83,98/75,92 м; RHS_USAF — на 76,67 м. Это наблюдение
  физического движения, не измерение ускорения относительно старого кода.
- Для RHS_AFRF перемещение составило 22,92 м, но штатный waypoint получил
  `GROUP_CALLBACK_COMPLETED/REMOVED` до конца окна. В момент измерения waypoint
  отсутствовал: `route_reduction_m=0` — значение невычисленной метрики,
  `eligible=0`. Затем production reliability выдала `ORDER_RECOVERY`.
  Непрерывное продвижение этой группы **не подтверждено**; новый policy
  корректно перестал действовать без текущего owned waypoint.

Полные остановленные console logs: stock 796 строк, RHS 1265. SCRIPT E/F,
ENGINE F, VM/null — 0 в обоих. Присутствуют engine/resource diagnostics:
stock `ENTITY=4, RESOURCES=3, WORLD=7`; RHS `ENTITY=3, MATERIAL=2,
PATHFINDING=10, RESOURCES=16, WORLD=167`. Workbench также пишет shutdown
resource leaks. Общий runtime не объявляется error-free.

Baseline outputs, команды, native exits, manifests, source hashes и полные logs:
`.codex-runtime/infantry-advance-20260926/`, итоговая передача — `result.md`.
145 production `.c` в тестовой копии совпадают по SHA-256 с рабочими;
fixture в production Core отсутствует. Первый sandbox-запуск Workbench
не имел доступа к Steam и не дал Validate verdict; засчитаны последующие
терминальные запуски с успешной валидацией и native exit.

Client/JIP, ручная визуальная оценка, сравнение скорости/потерь в настоящем
бою, десятиместные боевые составы и длительный soak — `NOT RUN`.
Статус `ACCEPTED` не присваивался. Для загрузки новой логики перезапустите
сервер/игру с обновлённым addon; работающую пользовательскую кампанию проверки не меняли.
