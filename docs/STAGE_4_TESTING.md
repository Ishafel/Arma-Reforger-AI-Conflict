# Stage 4 — экономика и снабжение

Статус реализации: **IMPLEMENTATION COMPLETE**. Статус runtime-приёмки: **PARTIAL — E1 PASS; E0/E2–E13 NOT RUN**.

Stage 4 реализован как отдельный authoritative server-only контур поверх stock Conflict supplies. Функция выключена по умолчанию: без явного `-aicfEconomyEnabled 1` reinforcement использует прежний Stage 3/3.5 путь — один ticket reservation и first-safe spawn-base selection без supply debit, request pacing или AICF shipments.

Development evidence текущего working tree:

- `tools/Test-Stage4Static.ps1`: **PASS**, включая negative fixture отсутствующего supply rollback;
- language audit `Invoke-AICFLanguageAudit`: **PASS**;
- Workbench 1.8.0.10: `.cache/stage4-implementation-validate-20260815-r5/console.log`, Game `5729/11223`, CRC32 `307e40e6`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`;
- direct ServerDiag E1: `C:\Users\retar\AppData\Local\AICF\Stage4-Probe-20260815-115400\logs\logs_2026-08-15_11-54-11\console.log`, `CONFIG enabled=1`, `ROSTER_READY 4+4`, `SUPPLY_PROBE=9`, both faction MOB `1000/1000`, `balance_delta=0`, `tools/Test-Stage4Log.ps1` **PASS**;
- replacement transaction, delivery, capture, JIP and soak evidence: **NOT RUN**.

## Нормативный контракт

1. Восемь initial-групп не расходуют tickets или supplies.
2. Уничтоженная managed-группа из пяти бойцов создаёт один durable reinforcement request.
3. Request накапливает readiness; временный logistics outage не сбрасывает уже накопленный progress.
4. Replacement attempt допускается только при одновременно доступных ticket, safe friendly spawn-base и stock supplies в размере group cost.
5. Reservation временно списывает supplies с выбранной базы и резервирует ticket. Permanent debit разрешён только после exact alive/faction roster `5/5`, current request/token/group generation, current graph revision и повторной owner/safety/supply-pool проверки.
6. Spawn/bind/roster/timeout/capture/stale/shutdown failure удаляет candidate group, возвращает ticket и supplies и оставляет request в очереди с новым attempt token.
7. Supply delivery не создаёт vehicle entity: cargo существует как bounded server shipment, списанный с stock HQ/SOURCE_BASE.
8. Для AICF shipments выполняется conservation: `dispatched = delivered + returned + in_transit`.
9. Player delivery и vanilla supply consumption используют тот же stock pool и немедленно влияют на последующий pacing/selection.
10. Репликация Stage 4 содержит только агрегаты; конкретные base supplies остаются stock Conflict state.

## Pacing

| Tier | Условие | Default pace при baseline 30 с |
|---|---|---:|
| `HEALTHY` | connected safe base имеет запас минимум на две группы | 100%, около 30 с |
| `STRAINED` | connected safe base оплачивает одну группу | 67%, около 45 с |
| `ISOLATED` | connected source отсутствует, но safe local stock оплачивает группу | 50%, около 60 с |
| `BLOCKED` | safe affordable base отсутствует либо graph rebuild pending | 0%, progress стоит |

## Выбор базы

Фильтр отклоняет ownerless/uninitialized, enemy-owned, contested/enemy-present, missing/disabled/inactive spawn point, faction mismatch и insufficient supplies. Оставшиеся базы ранжируются детерминированно:

1. connected к operational HQ/SOURCE_BASE;
2. меньше graph-hop до сохранённой target уничтоженной группы;
3. больше остаток supplies после reservation;
4. меньший stable node ID.

Перед entity spawn выполняется повторная spawn safety-проверка. Перед commit дополнительно проверяются request/token, group generation, graph revision, ticket/supply reservation, stock supply pool и текущий owner/safety state.

## Абстрактная доставка

- source: operational faction HQ или `SOURCE_BASE` с package сверх configured reserve;
- destination: operational connected `BASE` с минимальным stock, достаточной ёмкостью и без уже активного shipment;
- ETA: `base travel + hop count × per-hop travel`;
- broken route: shipment ставится на паузу без уменьшения remaining ETA;
- restored route: shipment продолжает движение;
- lost destination: cargo возвращается в актуальный friendly HQ/source; если return base временно отсутствует, shipment остаётся `RETURN_PENDING`;
- arrival overflow возвращается в friendly HQ/source и остаётся учтённым в conservation.

## CLI

| Параметр | Default | Назначение |
|---|---:|---|
| `aicfEconomyEnabled` | `0` | явный opt-in Stage 4 |
| `aicfReplacementSupplyCost` | `500` | supplies за replacement-группу `5/5`; calibrated по Arland probe 15.08.2026 |
| `aicfEconomyHealthyStockGroups` | `2` | запас групп для tier `HEALTHY` |
| `aicfEconomyHealthyPacePercent` | `100` | connected high-stock pace |
| `aicfEconomyStrainedPacePercent` | `67` | connected one-group pace |
| `aicfEconomyIsolatedPacePercent` | `50` | isolated local-stock pace |
| `aicfEconomyBlockedPacePercent` | `0` | no-affordable-base pace |
| `aicfEconomyRetryMs` | `5000` | retry после неуспешного attempt |
| `aicfSupplyDeliveryIntervalMs` | `60000` | dispatch cadence на фракцию |
| `aicfSupplyDeliveryPackage` | `500` | cargo одного shipment — стоимость одной replacement-группы |
| `aicfSupplyDeliveryBaseTravelMs` | `30000` | базовый ETA |
| `aicfSupplyDeliveryPerHopMs` | `15000` | ETA за graph-hop |
| `aicfMaxSupplyShipmentsPerFaction` | `2` | лимит in-flight/return-pending shipments |
| `aicfSupplySourceReserveGroups` | `1` | неприкосновенный source reserve в group-cost units |
| `aicfEconomyHeartbeatMs` | `60000` | bounded economy heartbeat |

Enabled runtime сохраняет все `[SUPPLY_PROBE]` строки только после инициализации stock pools обеих сторон. Финальный probe 15.08.2026 показал faction MOB `1000/1000`, стандартные non-relay capacities `1000` и более крупные pools `2150/3000`; поэтому default group cost откалиброван до `500`, package — до `500`, source reserve — до одной группы. MOB и стандартная база capacity `1000` тем самым держат две replacement-группы.

## Диагностический контракт

Префикс: `[AICF][STAGE4]` с тем же `run` и `t_ms`, что Stage 1.

Обязательные события:

- `CONFIG`, `SUPPLY_PROBE`, `STATE_REPLICATED`;
- `REINFORCEMENT_REQUESTED`, `REINFORCEMENT_PACING`;
- `BASE_CANDIDATE_REJECTED`, `BASE_SELECTED`;
- `DEPLOYMENT_RESERVED`, `RESERVATION_REVALIDATED`, `DEPLOYMENT_COMMITTED`, `DEPLOYMENT_ABORTED`;
- `SHIPMENT_DISPATCHED`, `SHIPMENT_PAUSED`, `SHIPMENT_RESUMED`, `SHIPMENT_DELIVERED`, `SHIPMENT_RETURNED`;
- bounded `HEARTBEAT` с `balance_delta=0`.

Любой `STAGE4 ERROR`, `SHIPMENT_BALANCE_FAILED`, `SCRIPT (E/F)`, `ENGINE (F)` или `Virtual Machine Exception` делает runtime-срез `FAIL`.

## Development-проверка

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Static.ps1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Log.ps1 `
  -LogPath <полный-server-console.log>
```

Для live capture незавершённого матча log-аудит запускается с `-AllowActiveAtEnd`; финальный остановленный прогон проверяется без этого флага.

## Runtime-матрица

| ID | Срез | Ожидаемый результат | Статус |
|---|---|---|---|
| E0 | economy off baseline | нет Stage 4 supply debit/shipments; Stage 3/3.5 reinforcement unchanged | NOT RUN |
| E1 | startup probe | supplies/max всех Arland graph bases зафиксированы после ready обеих faction pools | PASS |
| E2 | successful replacement | ровно один ticket debit + один supply debit после exact `5/5` | NOT RUN |
| E3 | timeout/invalid roster | permanent debit отсутствует; ticket/supplies возвращены | NOT RUN |
| E4 | base selection | supplied forward base побеждает MOB; tie-break детерминирован | NOT RUN |
| E5 | rejection | enemy/contested/inactive/insufficient base не используется | NOT RUN |
| E6 | pacing | observed `100/67/50/0%`, progress pause/resume без reset | NOT RUN |
| E7 | shipment | dispatch/delivery/return сохраняют `balance_delta=0` | NOT RUN |
| E8 | route break | shipment pause/resume без cargo duplication | NOT RUN |
| E9 | capture race | stale reservation/shipment не коммитится новой стороне | NOT RUN |
| E10 | player delivery | stock delivery игрока используется AICF без parallel currency | NOT RUN |
| E11 | JIP | клиент получает правильные агрегаты US/USSR | NOT RUN |
| E12 | M30 | нет request/reservation/shipment leak или log churn | NOT RUN |
| E13 | S120 | supply conservation, entity/memory/FPS устойчивы | NOT RUN |

Stage 4 не получает автоматический runtime PASS из static/Workbench evidence. Итог присваивается после review полного остановленного server/client log и заполнения матрицы на одном commit.
