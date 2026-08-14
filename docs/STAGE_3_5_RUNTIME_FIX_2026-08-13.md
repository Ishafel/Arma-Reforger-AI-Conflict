# Stage 3.5 runtime fix-candidate — 13.08.2026

## Статус

- Исходный Transport run `stage1-server-11245`: **FAIL**.
- Текущий dirty-working-tree fix-candidate: static/compile **PASS**, runtime transport **NOT VERIFIED**.
- Stage 3 / 3.5: **FAILED / NOT ACCEPTED** до нового длительного server+client прогона.

Исходный server log: `C:\Users\retar\AppData\Local\AICF\Stage35-Rewrite-T-20260813-211511\logs\logs_2026-08-13_21-15-12\console.log`.
Сохранённый immutable snapshot: `.cache/stage35-rewrite-t-11245-capture-20260813/server-console-live-snapshot.log`, SHA-256 `2C9F37B06185B72165AE68DFF97B9E70B6EDBAAC61E9B7E65EF0A0A85E7AC777`.

## Подтверждённые причины

1. USSR A0 не завершал terminal transition. Exit сначала удалял waypoint entity, затем пытался очистить state по уже инвалидированной ссылке. Trip оставался `TRANSIT`, а scheduler создавал следующий route. В snapshot: 370 `VEHICLE_ROUTE_ASSIGNED`, 369 `WAYPOINT_REMOVED`; один USSR A0 дал 347 циклов.
2. `TRANSIT_WAYPOINT_RECONCILIATION_REJECTED` был следствием premature reconciliation после route suspension и того же неправильного порядка ownership cleanup. Подтверждено пять committed failures.
3. Managed terminal clearance исчерпывал три асинхронные exit-попытки и переходил в бесконечный `WAIT_MANAGED_CLEARANCE`. Фактически был один exact logical occupant и два physical-only участника внутри bounds.
4. У USSR A2 два exact Cargo action оставались `RUNNING` без link/transition и без retry на 3.04/3.90 м. Старый supervisor повторял только `COMPLETED/FAILED`.
5. Большая часть `MEANINGFUL_TASK_LOST/RECOVERED` была вторичной: около 92% churn создавали terminal route loops. Оставшийся аудит объявлял loss после первого секундного gap и работал даже при `alive=0`.
6. `stuck_detected=15` смешивал 2 физических stuck с 13 unstable order-binding cases; `stuck_recovered` означал успешную выдачу приказа, а не движение.

## Внесённые исправления

- Transit проверяет destroyed/on-fire/overturned asset до reconciliation/route creation.
- Transit и Dismount waypoint освобождаются строго `queue detach → exact state confirm → entity delete`; superseded cleanup и bind разделены по scheduler ticks.
- Detach exact phase-owned waypoint не зависит от живости lease/entity, поэтому asset-loss terminal transition может закоммититься; произвольный или принадлежащий другому Trip waypoint не проходит pointer fence.
- Coordinator явно latch-ит `UNCOMMITTED_TERMINAL_OUTCOME`, если flow вернул terminal result без committed terminal phase.
- Terminal clearance по deadline передаёт asset в независимый CleanupManager scan. Cleanup сохраняет player/occupant/transition protection, stable-clear 5 s и immediate rescan. Его собственный absolute deadline заканчивается `PROTECTED_CLEARANCE_DEADLINE_EXCEEDED` с retained fail-closed asset; release/delete без clearance не выполняются. Trip отсоединяется только после exact acknowledgement, что unreleased `FAILED_CLOSED` lease всё ещё принадлежит Fleet и удерживает slot/cap; manager продолжает владеть asset независимо. Ошибка до создания release snapshot попадает в acceptance через immutable cleanup fence.
- Exact Cargo supervisor повторяет любой unlinked/non-transitioning `RUNNING` token после 15 s без прогресса ≥2 м, включая orphaned action reference. `Fail()` разрешён только для всё ещё utility-owned exact action authoritative AI, не player/possessed/proxy; orphaned action не получает `Fail()`. Retry заново доказывает exact compartment vehicle, reservation owner, доступность/lock, generation/owner fences, бюджет 2 и не более одного reissue за tick.
- Meaningful-task audit теперь игнорирует `alive=0`, имеет минимум 5 s grace, identity/revision episode fence и пишет RECOVERED только после реально emitted LOST.
- MOB telemetry разделяет присутствие в зоне и движение: `MOB_EGRESS_DEADLINE_MISSED`, `mob_presence_ms`, `motion_age_ms`.
- Physical stuck recovery отделён от order issue: `stuck_recovered` растёт только после подтверждённого displacement или route reduction; unstable binding имеет отдельный outcome и не увеличивает physical counter.

## Development evidence

- `tools/Test-Stage3Static.ps1`: PASS, negative fixtures PASS.
- `tools/Test-Stage35Static.ps1`: PASS, negative fixtures PASS.
- `git diff --check`: PASS (только существующие line-ending warnings).
- Workbench validate: `.cache/stage35-runtime-report-fix-validate-20260813-r4/console.log`, SHA-256 `451687360B6A5742135E6C82F26BDCD1D8FDED2A770D324D1B9A23D894A4A826`.
  - Reforger `1.8.0.10`; Game: 5719 files / 11200 classes, CRC32 `859d2690`.
  - `Game successfully created`.
  - `SCRIPT(E/F)=0`, `ENGINE(F)=0`, VM exceptions `0`.
  - 25 shutdown `RESOURCES(E)` сохранены как Workbench resource-leak caveat, не runtime evidence.

Focused dedicated attempt `.cache/stage35-runtime-fix-smoke-20260813-220401` загрузил Game, но был остановлен после повторяющихся backend `SSL peer certificate` / `BAD_REQUEST` до AICF bootstrap (`AICF=0`). Его статус — **BLOCKED external backend**; он не подтверждает и не опровергает transport fixes.

## Обязательный repeat

Нужен новый длинный server+client Transport run на чистых profile. Помимо исходных критериев проверить:

- ровно один terminal transition после `DESTROYED`/`ON_FIRE`, без новых route для того lifecycle;
- отсутствие `UNCOMMITTED_TERMINAL_OUTCOME` и `TRANSIT_WAYPOINT_RECONCILIATION_REJECTED`;
- exact Cargo либо 5/5 settled два polls, либо точная bounded terminal-причина с distance/progress/seat/action/door fields;
- `WAIT_MANAGED_CLEARANCE` и `WAIT_PROTECTED_CLEARANCE` имеют конечный исход; release/delete только при `logical_occupants=0`, `inside_bounds=0`, без transitions и защищённых игроков;
- task churn не периодический, `alive=0` не генерирует loss/recovery;
- `order_issue_succeeded`, `movement_resumed`, `route_progress_resumed`, `recovery_failed/repeated` считаются независимо;
- несколько полных Acquisition → Boarding → Transit → Dismount → Handoff → Cleanup циклов для обеих фракций.
