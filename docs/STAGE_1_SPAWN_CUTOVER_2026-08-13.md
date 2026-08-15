# Stage 1 spawn regression после Reforger 1.8 cutover — 2026-08-13

## Исходный FAIL

- Run: `stage1-server-30017`.
- Runtime: Arma Reforger `1.8.0.10`, engine `191843`.
- Лог: `C:\Users\retar\AppData\Local\AICF\Stage35-T-20260813-193121\logs\logs_2026-08-13_19-31-22\console.log`.
- Все восемь group entities были созданы и bound, generation `1`: US `0x4000000000000132`–`0x4000000000000135`, USSR `0x4000000000000136`–`0x4000000000000139`.
- На 10/20/30 сек engine stats оставались `AI:13, AIChar:0`; после fatal cleanup стало `AI:5, AIChar:0`. Это подтверждает восемь существовавших group controllers и отсутствие materialized characters у всех групп. `US A0` оказался первым timeout только из-за порядка обхода slots.
- Клиент начал соединение после `GROUP_SPAWN_TIMEOUT`, поэтому connection path не участвовал в дефекте.

## Причина

Исторический успешный roster-run выполнялся на `1.7.0.54` / engine `190965`. В 1.7 `SCR_AIGroup.SpawnUnits()` создавал per-group delayed queue и повторял transient navmesh failures по кадрам.

В 1.8:

- `SpawnUnits()` вызывает одноразовый синхронный `CreateUnitEntities()`;
- `SpawnGroupMember()` может вернуть `false`, пока navmesh tile не загружен;
- синхронный caller не ставит такую попытку в очередь повторно;
- `GetSpawnQueueSize()` оставлен только как compatibility stub и всегда возвращает `0`;
- поддерживаемый runtime path — `SetNumberOfMembersToSpawn(expected)` + `RequestSpawn(expected)`, передающий retry/budget ownership глобальной очереди `SCR_AIWorld`.

AICF вызывал `SpawnUnits()` для всех восьми групп до первого world frame. Это полностью объясняет потерю пяти transient attempts на группу и наблюдавшийся roster `0/5`. Vehicle cutover не менял `AICF_GroupSpawner` и не владел initial-spawn callback; совпадение по времени связано с переходом engine/API 1.7 → 1.8, а не с новой транспортной FSM.

## Исправление и диагностика

- Group entity создаётся и exact prefab roster формируется отдельно.
- `MatchController` сначала bind-ит group к slot/generation и подписывает `GetOnAgentAdded()`/`GetOnAllDelayedEntitySpawned()`.
- Затем AICF фиксирует request intent и вызывает `RequestSpawn(5)`.
- READY остаётся authoritative polling gate: ровно пять живых faction-correct agents. Completion callback используется только как telemetry.
- `GROUP_SPAWN_AUDIT` пишет фактический roster во время SPAWNING.
- `GROUP_SPAWN_TIMEOUT` строит полный immutable snapshot до detach/delete/`MarkDestroyed`; при initial failure перед cleanup дополнительно снимаются остальные семь SPAWNING slots.
- Snapshot включает group EntityID/generation/existence/faction/Rpl state, expected/configured/requested/actual/alive/faction counts, AICF-owned pending shortfall, callback/progress age, AIWorld limit telemetry, classified reason и per-member EntityID/age/alive/faction/replication/reason.
- `spawning_pending` больше не выдаётся за native queue size; это `expected - actual` с `pending_source=AICF_EXPECTED_MINUS_ACTUAL` и `engine_queue_observable=0`.

## Проверка candidate

- `tools/Test-Stage3Static.ps1`: PASS.
- `tools/Test-Stage35Static.ps1`: PASS, включая negative-fixture self-check и новый Reforger 1.8 spawn contract.
- Workbench 1.8 validation: `.cache/stage1-spawn-request-validate-20260813-r3/console.log`; `Game successfully created`, Game CRC32 `7c4d4436`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`.
- Dedicated vehicles-off smoke: `.cache/stage1-roster-request-smoke-20260813-r3/logs/logs_2026-08-13_21-04-39/console.log`, run `stage1-server-8212`.
  - `ROSTER_SPAWN_REQUESTED=8`;
  - `GROUP_ROSTER_READY=8`, каждый `initial_agents=5 expected_agents=5 faction_correct=1`;
  - общий `ROSTER_READY` в `t_ms=5986`;
  - `GROUP_SPAWN_TIMEOUT=0`, `AICF ERROR/FAIL=0`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`.

Этот smoke подтверждает только устранение initial roster blocker на engine `191843`. Он выполнен без клиента и с `aicfVehiclesEnabled=0`, поэтому не является Repeat T, Repeat-T2 или техническим runtime PASS транспортной архитектуры. На дату этого отчёта Stage 3/3.5 оставались `FAILED / NOT ACCEPTED`; текущий продуктовый статус позднее изменён [решением владельца от 15.08.2026](STAGE_3_3_5_OWNER_ACCEPTANCE_2026-08-15.md) без переклассификации данного evidence.
