# Context index for Codex

Этот индекс направляет агента к минимальному достаточному контексту. Он не
заменяет документы, на которые ссылается.

## Always read

Для implementation, verification или изменения продуктового статуса:

1. [`AGENTS.md`](../AGENTS.md)
2. Активную карточку из [`product/work-items/`](../product/work-items/)
3. [`product/CURRENT.md`](../product/CURRENT.md)
4. Только соответствующие строки таблиц ниже.

Для read-only вопроса или диагностики достаточно релевантного source path и
одного нормативного документа.

## Source routing

| Work type | Required product/contract context | Primary code paths | Verification |
|---|---|---|---|
| Product scope, MVP boundaries | [`PROJECT_VISION.md`](../PROJECT_VISION.md), особенно разделы 3-4, 9-10 и 14 | по work item | acceptance matrix из work item |
| Stage 0 bootstrap/probe | [`STAGE_0_TESTING.md`](STAGE_0_TESTING.md) | `AIConflictCore/.../Bootstrap`, Arland integration | Workbench + Stage 0 runtime contract |
| Stage 1 infantry/capture/replacement | [`STAGE_1_TESTING.md`](STAGE_1_TESTING.md) | `Forces`, `Objectives`, `Orders`, `State` | Stage 1 two-run matrix |
| Stage 2 reliability/load/stuck | [`STAGE_2_TESTING.md`](STAGE_2_TESTING.md) | `Diagnostics`, `Forces`, `Orders`, Stage 2 config | `Test-Stage2Log.ps1` + M30/S120 when required |
| Stage 3 vehicles | [`STAGE_3_TESTING.md`](STAGE_3_TESTING.md) | `Vehicles`, `State/Vehicles` | `Test-Stage3Static.ps1` + named runtime slice |
| Stage 3.5 force structure/QRF | [`STAGE_3_5_ACTIVE_FORCES.md`](STAGE_3_5_ACTIVE_FORCES.md), relevant slice in [`STAGE_3_5_TESTING.md`](STAGE_3_5_TESTING.md) | `Vehicles`, `Forces`, `Orders`, `State` | `Test-Stage35Static.ps1`, `Test-Stage35RecoveryPolicy.ps1`, named runtime slice |
| Stage 4 economy/supplies | [`STAGE_4_TESTING.md`](STAGE_4_TESTING.md): разделы «Нормативный контракт», «Pacing», «Выбор базы», «Абстрактная доставка», «Runtime-матрица» | `Economy`, Stage 4 config, campaign state | `Test-Stage4Static.ps1`, `Test-Stage4Log.ps1` for economy-enabled runs |
| Stage 4 HUD/map/command UI | [`STAGE_4_TESTING.md`](STAGE_4_TESTING.md): разделы «Стратегический интерфейс», «Диагностический контракт», «Runtime-матрица U0-U11» | `UI`, `Integration/AICF_CampaignState.c`, `Orders` | `Test-Stage4Static.ps1` + client/runtime evidence |
| Arland adapter/map-specific integration | [`PROJECT_VISION.md`](../PROJECT_VISION.md), раздел 6.2 и relevant Stage contract | `AIConflictArland/Scripts/.../Integration` | Workbench + relevant runtime |
| Reforger API/version question | Relevant section of [`API_REFERENCE.md`](API_REFERENCE.md) | exact symbol call sites | local API cache/reference before assumption |
| Owner acceptance/status | [`product/CURRENT.md`](../product/CURRENT.md), matching record in `product/acceptance/` or owner-decision doc | normally none | explicit owner decision |

## Tool routing

| Tool | Use |
|---|---|
| `tools/Test-Stage2Log.ps1` | Stage 2 stopped runtime log classification |
| `tools/Test-Stage3Static.ps1` | Stage 3 vehicle static contracts |
| `tools/Test-Stage35Static.ps1` | Stage 3.5 static contracts |
| `tools/Test-Stage35RecoveryPolicy.ps1` | Stage 3/3.5 recovery-policy invariants |
| `tools/Test-Stage4Static.ps1` | Stage 4 economy/UI/authority static contracts |
| `tools/Test-Stage4Log.ps1` | Economy-enabled Stage 4 stopped log; requires `CONFIG enabled=1` |
| `tools/fetch_reforger_api_reference.sh` | Refresh local Reforger API reference when explicitly required |

Не используй `Test-Stage4Log.ps1` как доказательство economy-off `U0`: его
обязательный input contract ожидает `enabled=1`.

## Current pilot: AICF-OPS-001

Минимальный контекст:

1. [`AICF-OPS-001.md`](../product/work-items/AICF-OPS-001.md)
2. [`product/CURRENT.md`](../product/CURRENT.md)
3. [`STAGE_4_TESTING.md`](STAGE_4_TESTING.md):
   - `Стратегический интерфейс`;
   - `Диагностический контракт`;
   - `Development-проверка`;
   - строка `U0` в `Runtime-матрица`.
4. `tools/Test-Stage4Static.ps1` только для запуска/результата; исходник читать
   лишь при его `FAIL` или при проверке самого analyzer.

Не нужны по умолчанию:

- Stage 3/3.5 repeat history;
- весь `README.md`;
- полный `API_REFERENCE.md`;
- economy-enabled `E1-E13` implementation history.

## Historical evidence

Открывай historical repeat/defect report только когда:

- work item ссылается на конкретный run/defect ID;
- новый лог воспроизводит тот же signature;
- нужно проверить, не был ли verdict уже явно принят или отклонён владельцем.

Не загружай несколько repeat-отчётов для общего ознакомления. Сначала найди
точный ID через `rg`, затем прочитай минимальный раздел.

## Context conflict rule

Если краткая сводка расходится с нормативным документом:

1. Не выбирай удобный вариант молча.
2. Проверь дату, commit и owner decision.
3. Сохрани исторический verdict.
4. Если конфликт влияет на scope или acceptance, остановись и запроси решение
   владельца.
