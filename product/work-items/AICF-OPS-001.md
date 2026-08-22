---
id: AICF-OPS-001
title: Stage 4 U0 economy-off HUD retest
status: READY
owner: Product Owner
type: runtime-acceptance
created: 2026-08-20
updated: 2026-08-22
baseline_branch: main
baseline_commit: TO_BE_RECORDED_AT_START
product_candidate_observed_at: 650b0942fb51a8bdd879d5b9f530663a72989377
target_game_version: 1.8.0.10
writer: UNASSIGNED
parallel_work: forbidden
---

# `AICF-OPS-001` — Stage 4 U0 economy-off HUD retest

## Outcome

Выполнить чистый client/runtime retest Stage 4 gate `U0` и подготовить
воспроизводимый acceptance packet с точным verdict `PASS`, `FAIL` или
`BLOCKED`.

Эта work item проверяет и классифицирует существующий fixed candidate. Она не
исправляет product code в том же цикле.

## Product context

Stage 4 matrix сейчас фиксирует:

> `U0` economy-off HUD — first slice `FAIL`; `FIXED CANDIDATE, RETEST NOT RUN`.

Нормативное ожидание: при отключённой economy HUD показывает `SUPPLY OFF`, а
tickets, ready squads, managed personnel и current objective продолжают
обновляться без economy side effects.

## Scope

### Included

- Зафиксировать exact clean commit и версии Game/Tools/Server.
- Выполнить релевантный Stage 4 static audit перед runtime.
- Запустить Arland с `aicfEconomyEnabled=0` на чистых server/client profiles.
- Подключить observer client к проверяемой faction и отдельный stimulus client
  к противоположной стороне.
- Выполнить контролируемые authoritative изменения objective и lifecycle
  managed group по протоколу ниже.
- Подтвердить пары server/HUD values до и после изменения tickets, squads,
  personnel и objective.
- Проверить отсутствие Stage 4 supply debit и shipment side effects.
- Проверить server/client logs на AICF, script, engine, VM и null failures.
- Сохранить непрерывное client video, checkpoint screenshots и полные
  server/client logs.
- Создать `product/acceptance/AICF-OPS-001.md` по шаблону.

### Allowed paths

- `product/work-items/AICF-OPS-001.md` — status и execution metadata.
- `product/acceptance/AICF-OPS-001.md` — новый acceptance packet.

### Forbidden paths

- `AIConflictCore/**`
- `AIConflictArland/**`
- `tools/**`
- `docs/STAGE_4_TESTING.md`
- `README.md`
- Раздел `Recognized baseline` в `product/CURRENT.md`

### Non-goals

- Исправление U0 при новом `FAIL`.
- Изменение HUD layout или текста вне подтверждённого U0 defect.
- Проверка `U1-U11`.
- Проверка economy-enabled `E1-E13`.
- Vehicle, M30 или S120 runtime.
- Обновление нормативной matrix до решения владельца.

## Preconditions

1. Рабочее дерево чистое либо все отклонения перечислены и не затрагивают
   runtime product files.
2. Exact commit записан до Workbench/static/runtime checks.
3. Game, Tools и Server используют совместимую ветку; фактические версии
   записаны в acceptance packet.
4. Используются новые server/client profile directories и свободные порты.
5. `aicfEconomyEnabled=0` подтверждён фактическими launch parameters и логом.
6. Observer client остаётся в одной записанной faction на всех checkpoints;
   stimulus client использует отдельный чистый profile и противоположную
   faction.
7. Server и оба client clock синхронизированы либо для видео записан точный
   offset к server log.
8. Для event-based loss checkpoint используется
   `aicfReinforcementDelayMs=90000`; это test-only окно U0, а не evidence
   нормативного 30-секундного Stage 1 timing gate.
9. Tickets достаточно минимум для одного replacement, а observer video
   начинается до baseline checkpoint.

## Work-item completion gates

Эти gates оценивают качество выполненного retest. Product gate `U0` учитывается
отдельно и может законно получить `FAIL`.

| ID | Gate | Required verdict | Evidence |
|---|---|---|---|
| W1 | Exact commit, clean-tree status и environment metadata записаны | `PASS` | acceptance metadata |
| W2 | `Test-Stage4Static.ps1` завершён до runtime | `PASS` | command, exit code, output |
| W3 | Economy-off run завершил весь controlled U0 protocol через post-replacement checkpoint | `PASS` | correlated events, tuples, video/screenshots |
| W4 | Полные остановленные server и оба client logs сохранены | `PASS` | paths and hashes |
| W5 | `U0` получил обоснованный `PASS/FAIL` | `PASS` | product gate table |
| W6 | Acceptance packet не содержит неподтверждённых повышений статуса | `PASS` | packet self-check/read-only review |

Если environment не позволяет выполнить W3/W4, writer переводит карточку из
текущего execution status в `BLOCKED`, записывает последний checkpoint и
условие `BLOCKED -> READY`; `U0` остаётся `NOT RUN` или `BLOCKED` по
фактической границе.

## Product gate U0

| ID | Expected result | Method | Evidence |
|---|---|---|---|
| U0.1 | HUD показывает `SUPPLY OFF` на всех checkpoints | stable HUD samples | checkpoint screenshots/video |
| U0.2 | Tickets совпадают с authority до debit и меняются на exact `before -> after` после replacement | `HEARTBEAT`, `TICKET_DEBIT`, `TICKETS_REPLICATED` + HUD correlation | server/HUD tuple table, excerpts, video |
| U0.3 | Ready squads и managed personnel отражают loss и восстановление exact slot | `GROUP_EMPTY`/replacement chain, complete `HEARTBEAT` + `SLOT_ACTIVITY` batches, HUD correlation | baseline/loss/recovery tuples, screenshots/video |
| U0.4 | Current objective меняется с old target на принятый новый target | `PLAYER_ORDER_ACCEPTED old_target != target`, following `SLOT_ACTIVITY`, HUD correlation | objective before/after tuples, screenshots/video |
| U0.5 | Нет supply debit/shipments и parallel economy side effects | full stopped server log review | event counts/excerpts |
| U0.6 | Нет любого `AICF ERROR` любой фазы, `SHIPMENT_BALANCE_FAILED`, `SCRIPT (E/F)`, `ENGINE (F)`, `Virtual Machine Exception` или null-pointer | full stopped server and both client log review | counts/excerpts |

`U0 PASS` требует одновременного выполнения `U0.1-U0.6`. Визуальная
демонстрация без controlled state transitions, server/client correlation и
полного stopped-log review недостаточна.

## Controlled U0 protocol

Все значения записываются для одной observer faction `F`. Server tuple имеет
вид `tickets, ready_squads, managed_personnel, A0_target`; `managed_personnel`
равен сумме `alive` полного набора `SLOT_ACTIVITY faction=F`, а остальные поля
берутся из `HEARTBEAT` и `SLOT_ACTIVITY`. Client tuple — точные четыре значения
HUD плюс `SUPPLY`.

1. **B — baseline.** После `ROSTER_READY` дождаться одного `HEARTBEAT` и полного
   набора `SLOT_ACTIVITY` для всех configured slots faction `F`. Убедиться, что
   replacement не ожидается. Записать server tuple `B` и два одинаковых HUD
   sample не менее чем через 1 секунду друг от друга.
2. **O — objective stimulus.** Через существующий command UI отправить для A0
   допустимую цель, отличную от текущей. Требуется
   `PLAYER_ORDER_ACCEPTED faction=F old_target=<B target> target=<new>`.
   Дождаться первого полного `SLOT_ACTIVITY` batch после события и записать два
   одинаковых HUD sample с новым objective не менее чем через 1 секунду друг от
   друга. Между событием и checkpoint не допускается другая смена target A0.
   Этот stimulus доказывает только динамику U0.4 и не повышает `U3`.
3. **L — loss stimulus.** Выбрать отличный от A0 managed slot faction `F`,
   который в `B` имеет `state=READY`, и записать его numeric slot, generation и
   `alive`. Stimulus client уничтожает его полный состав обычным игровым
   уроном. Direct entity deletion, Game Master, console mutation и test RPC
   запрещены: обязательны штатные `OnEmpty`, `GROUP_EMPTY` и
   `REINFORCEMENT_SCHEDULED` для exact faction/slot.
4. До `REINFORCEMENT_SPAWNED` дождаться первого полного `HEARTBEAT` +
   `SLOT_ACTIVITY` checkpoint после `GROUP_EMPTY`. Записать tuple `L` и два
   одинаковых HUD sample. Ожидается: tickets без изменения, ready squads меньше
   `B` на один, а personnel меньше на exact baseline `alive` проверяемого slot.
5. **R — recovery.** Для той же faction/numeric slot дождаться цепочки
   `REINFORCEMENT_SPAWNED -> GROUP_ROSTER_READY -> SLOT_READY -> TICKET_DEBIT ->
   TICKETS_REPLICATED`, затем первого полного `HEARTBEAT` + `SLOT_ACTIVITY`
   checkpoint. Записать tuple `R` и два одинаковых HUD sample. Tickets должны
   совпасть с exact `TICKET_DEBIT after`, ready squads — восстановиться, а
   personnel увеличиться на exact `GROUP_ROSTER_READY initial_agents`.
6. Между `B` и `R` не допускаются другие loss/replacement/debit события faction
   `F` или изменения её personnel вне выбранного slot. При contamination run
   повторяется и не может дать `PASS`.
7. После `R` корректно остановить server и оба client, затем анализировать
   полные логи без временного cutoff.

Observation window начинается с server/HUD checkpoint `B` и завершается только
после authoritative recovery chain, следующего полного server checkpoint и
двух стабильных HUD samples `R`. Idle log, один screenshot или только конечное
значение не удовлетворяют протоколу.

Если authoritative stimulus завершился, но HUD не отразил соответствующее
значение на checkpoint, затронутый U0 subgate получает `FAIL`. Если order/loss/
replacement chain не удалось довести до authoritative checkpoint, `U0`
получает `BLOCKED` или `NOT RUN` по достигнутой границе, но не `PASS`; отдельный
product defect классифицируется вне этой retest work item.

## Required context

Читать:

- [`AGENTS.md`](../../AGENTS.md)
- [`product/CURRENT.md`](../CURRENT.md)
- [`docs/CONTEXT_INDEX.md`](../../docs/CONTEXT_INDEX.md), раздел про текущий пилот.
- [`docs/STAGE_4_TESTING.md`](../../docs/STAGE_4_TESTING.md):
  `Стратегический интерфейс`, `Диагностический контракт`,
  `Development-проверка` и строку `U0` в `Runtime-матрица`.
- [`docs/STAGE_1_TESTING.md`](../../docs/STAGE_1_TESTING.md): только таблицу
  событий и раздел `Пустая группа и replacement` для event correlation.

Не читать по умолчанию:

- Исторические Stage 3/3.5 repeat reports.
- Полный `README.md`.
- `E1-E13` и `U1-U11` implementation details, если они не нужны для
  классификации обнаруженного события.

## Verification

Static:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Static.ps1
```

`Test-Stage4Log.ps1` ожидает economy-enabled `CONFIG enabled=1` и поэтому не
является самостоятельным доказательством economy-off `U0`. Для этого slice
нужны полный stopped-log review и компактная таблица event counts/excerpts.

Минимально зафиксировать отсутствие:

- `[DEPLOYMENT_RESERVED]`, `[DEPLOYMENT_COMMITTED]` и supply debit;
- `[SHIPMENT_DISPATCHED]`, `[SHIPMENT_DELIVERED]` и других shipment mutations;
- любой `[AICF][...][ERROR]` любой фазы и `SHIPMENT_BALANCE_FAILED`;
- `SCRIPT (E/F)`, `ENGINE (F)`, `Virtual Machine Exception` и null-pointer.

Не интерпретировать ожидаемые economy-off suppression/info events как side
effect без проверки их семантики.

## Stop conditions

- Требуется изменить product code, analyzer или нормативный Stage-контракт.
- Launch parameters не доказывают `aicfEconomyEnabled=0`.
- Commit изменился между static и runtime.
- Observer client не дошёл до HUD или stimulus client не может выполнить
  ordinary-damage stimulus из-за внешней среды.
- Полный server или любой client log потерян либо run не был корректно
  остановлен.

При новом product defect создай предложение отдельной defect work item; не
исправляй его внутри `AICF-OPS-001`.

## Deliverable

`product/acceptance/AICF-OPS-001.md` должен содержать:

- work-item completion verdict;
- отдельный product gate `U0` verdict;
- exact commit и environment;
- commands, timestamps и exit codes;
- continuous video и checkpoint screenshot paths;
- server и оба client log paths и hashes;
- server/HUD tuple table `B/O/L/R` с event timestamps и clock offset;
- event-count table и релевантные excerpts;
- deviations, risks и owner decision `PENDING`.

## Status history

| Date | From | To | Actor | Reason |
|---|---|---|---|---|
| 2026-08-20 | — | `READY` | Product Owner | Pilot operational transformation and U0 retest |
