# Stage 4 — экономика, снабжение и командный интерфейс

Статус реализации: **IMPLEMENTATION COMPLETE, включая расширение командования группами и отложенную выдачу техники**. Статус runtime-приёмки: **PARTIAL — E1 PASS; E0/E2–E13/U1–U11 NOT RUN**.

Экономика Stage 4 реализована как отдельный authoritative server-only контур поверх stock Conflict supplies. Функция выключена по умолчанию: без явного `-aicfEconomyEnabled 1` reinforcement использует прежний Stage 3/3.5 путь — один ticket reservation и first-safe spawn-base selection без supply debit, request pacing или AICF shipments. HUD, союзная карта и окно командования работают в обоих режимах; при выключенной экономике supply-поле явно показывает `OFF`.

Development evidence текущего working tree:

- `tools/Test-Stage4Static.ps1`: **PASS**, включая negative fixture отсутствующего supply rollback, strategic replication, allied map и order-authority gates;
- deferred vehicle-spawn contract (`TYPE_CHANGED → SITE_PLANNED → APPROACHING_SITE → STAGING_CONFIRMED → SPAWN_COMMIT → BOARDING`) проверен `Test-Stage4Static.ps1` и `Test-Stage35RecoveryPolicy.ps1`: **PASS**;
- Workbench 1.8.0.10 после all-member staging/reissue fix: `.cache/stage4-staging-waypoint-validate-20260816-r1/console.log`, Game `5731/11235`, CRC32 `9191335a`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`, VM/null `0`;
- runtime regression `Full-Stage3-4-20260816-094529` воспроизведён новым `Test-Stage4Log.ps1`: четыре reservation корректно классифицированы как `FAIL` за `6/17/50/60` повторных `APPROACH_REISSUE`; исправление задаёт waypoint completion `All`, один delayed reissue максимум и разрешает fenced MOB-egress recovery во время активного пешего staging-подхода;
- runtime regression `Full-Stage3-4-20260816-100909` классифицирован как `FAIL`: false `SPAWN_PAD_OCCUPIED` блокировал четыре US-группы, USSR A1/D2 получили `MOB_EGRESS_DEADLINE_MISSED`, а dynamic role-local slot names осложнили корреляцию. Исправление выполняет exact cylinder commit-probe, логирует blocker candidates и `stable_slot=S<n>`, выбирает разные MOB-egress candidates, передаёт recovery ownership от pending order repair и требует `All` для strategic Move/Defend; новый runtime ещё **NOT RUN**;
- replacement-cap regression: при `75/80` live agents четырёхместная A7 ошибочно проецировалась как `75 + MAX_GROUP_SIZE(10) = 85` и бесконечно получала `AI_LIMIT`. Capacity preflight теперь выполняется до `BeginReplacementSpawn`, резервирует `slot.GetDesiredSize()` (для A7: `75 + 4 = 79`) и не увеличивает `group_generation` при реальном ожидании лимита; одинаковый block логируется один раз до `REINFORCEMENT_CAPACITY_RELEASED`; runtime-ретест **NOT RUN**;
- Workbench 1.8.0.10 после exact blocker tracing, desired-roster replacement-cap fix и staging radius `50` м: `.cache/stage4-defects-validate-20260816-r4/console.log`, Game `5731/11235`, CRC32 `f53b70a8`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`, VM/null `0`;
- runtime regression A8: два engine callback `COMPLETED` за `2–3` секунды были получены примерно в `2343` м от objective и ошибочно исчерпали repair budget `2/2`; после vehicle fallback один выживший остался в `FIELD_HOLD`. Теперь completion подтверждается фактической позицией, `FALSE_COMPLETION` не расходует reliability budget, до трёх раз выбирается другая navmesh-точка, затем следует bounded hold и полный replan. `PLAYER_FENCE`/`COMBAT_THREAT` переводят MOB egress в `EGRESS_BLOCKED_BY_SAFETY` с приостановленным hard deadline. Vehicle handoff направляет единственного выжившего к союзной MOB, `2+` бойцов возвращает к пешему приказу, `0` оставляет replacement lifecycle; runtime-ретест **NOT RUN**;
- Workbench 1.8.0.10 после A8 false-completion/MOB-safety/lone-survivor fix: `.cache/stage4-a8-recovery-validate-20260816-r2/console.log`, Game `5731/11235`, CRC32 `169a9efc`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`, VM/null `0`, SHA-256 `B589F5BE88F8905F205154F82D1FC423B94539D8CCD04ECE65318C10F24399A5`;
- runtime regression `Full-Stage3-4-20260816-144051`: у A7 точная Turret reservation и `GetIn` были приняты, но DRIVER израсходовал общий boarding budget, поэтому GUNNER получил только около `9` секунд и был прерван в корректном compartment с `getting_in=1`. Каждая новая crew-фаза теперь получает полный bounded timeout в пределах immutable Trip deadline, а unlinked exact-role action без перехода получает один animated `GetIn` retry. False completion строит промежуточные точки через `NavmeshWorldComponent.GetReachablePoint` от текущего leader island; физически пройденные legs не расходуют no-progress budget. Staging center динамически отделён от `8`-метрового spawn cylinder на `radius + 10` м, commit отдельно запрещает бойцов requesting group в cylinder. Pre-lease trips входят в admission cap, поэтому при лимите `3` не создаются десять параллельных lifecycle;
- Workbench 1.8.0.10 после gunner/connected-route/staging/admission fix: `.cache/stage4-defect-bundle-validate-20260816-r3/console.log`, Game `5731/11235`, CRC32 `0902a014`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`, SHA-256 `F27BE782B1DB59FBD4BC3AFBD4E70CE014D372964D11907AC07BB3EB895A22BE`; `Test-Stage4Static.ps1` и `Test-Stage35RecoveryPolicy.ps1`: **PASS**; runtime-ретест **NOT RUN**;
- language audit `Invoke-AICFLanguageAudit`: **PASS**;
- Workbench 1.8.0.10 после critical runtime fixes: `.cache/stage4-critical-fixes-validate-20260815-r12/console.log`, Game `5731/11231`, CRC32 `6a07dbba`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`, VM `0`;
- direct ServerDiag MOB/accounting smoke `.cache/Stage4-CriticalFix-Smoke-R2-20260815-1647/logs/logs_2026-08-15_16-46-42/console.log`: 211 секунд, `10 + 10` групп, четыре `ATTEMPTED → SUBMITTED → ACCEPTED → PHYSICALLY_CONFIRMED`, `PARTIAL=0`, `SUBMISSION_FAILED=0`, `MOB_EGRESS_DEADLINE_MISSED=0`, VM/null `0`, order accounting invariant failures `0`, три heartbeat с `accounting_balanced=1`; vehicles были отключены, поэтому vehicle/armed-light runtime остаётся отдельным ретестом;
- финальный semantic smoke `.cache/Stage4-CriticalFix-Smoke-R3-20260815-1654/logs/logs_2026-08-15_16-54-13/console.log`: обе R0 подавлены как штатный `HQ_RESERVE` без recovery churn; all-member envelope обнаружил `inside_members=1` у US A1, перенёс только отставшего бойца и подтвердил `alive_members=4 outside_members=4`; `PARTIAL=0`, `MOB_EGRESS_DEADLINE_MISSED=0`, VM/null `0`, accounting invariant failures `0`, heartbeat `accounting_balanced=1`;
- direct ServerDiag E1: `C:\Users\retar\AppData\Local\AICF\Stage4-Probe-20260815-115400\logs\logs_2026-08-15_11-54-11\console.log`, `CONFIG enabled=1`, `ROSTER_READY 4+4`, `SUPPLY_PROBE=9`, both faction MOB `1000/1000`, `balance_delta=0`, `tools/Test-Stage4Log.ps1` **PASS**;
- client UI-retest 15.08.2026 подтвердил compact HUD, тёмное command menu, выбор групп, построение role-compatible целей и синхронный aggregate attack badge. Runtime-log `Full-Stage3-4-20260815-141111` подтверждает принятые сервером приказы A0/A1/D0; кратковременный отказ A2 при `ORDER_RECOVERY` корректно остановлен authoritative gate. Кнопка `AI COMMAND` дополнительно сдвинута вправо от stock map toolbar; post-shift client screenshot **PENDING**;
- replacement transaction, delivery, capture, JIP and soak evidence: **NOT RUN**.

## Нормативный контракт

1. Каждая фракция имеет десять stable numeric slots. Штатная раскладка — `6 ATTACK / 3 DEFEND / 1 RESERVE`; все группы по умолчанию `INFANTRY`, desired size `4`.
2. Initial-группы не расходуют tickets или supplies. Уничтоженная managed-группа создаёт один durable reinforcement request для своего текущего desired size.
3. Request накапливает readiness; временный logistics outage не сбрасывает уже накопленный progress.
4. Replacement attempt допускается только при одновременно доступных ticket, safe friendly spawn-base и stock supplies в размере group cost.
5. Reservation временно списывает supplies с выбранной базы и резервирует ticket. Цена пропорциональна desired size: настроенная стоимость относится к четырём бойцам и округляется вверх. Permanent debit разрешён только после exact alive/faction roster `N/N`, current request/token/group generation, current graph revision и повторной owner/safety/supply-pool проверки.
6. Spawn/bind/roster/timeout/capture/stale/shutdown failure удаляет candidate group, возвращает ticket и supplies и оставляет request в очереди с новым attempt token.
7. Supply delivery не создаёт vehicle entity: cargo существует как bounded server shipment, списанный с stock HQ/SOURCE_BASE.
8. Для AICF shipments выполняется conservation: `dispatched = delivered + returned + in_transit`.
9. Player delivery и vanilla supply consumption используют тот же stock pool и немедленно влияют на последующий pacing/selection.
10. Economy-репликация Stage 4 содержит только агрегаты; конкретные base supplies остаются stock Conflict state.
11. Стратегическая UI-проекция содержит текущую faction objective, десять bounded group summary, допустимые callsign-цели и общую численность, но не передаёт authority клиенту.
12. Клиентский приказ содержит только numeric slot и stock base callsign. Сервер выводит faction из player identity, применяет rate limit и повторно проверяет combat-ready state, pending recovery и role-specific target validity.
13. Явный приказ сохраняется до потери допустимости цели либо до lifecycle/recovery/reassignment, после чего штатный commander снова владеет выбором цели.
14. Конфигурационный RPC принимает только numeric slot, role, unit type и desired size. Сервер выводит faction из player identity, ограничивает частоту, проверяет диапазоны и запрещает менять size во время `SPAWNING`.
15. Роль и mobility type применяются к живой группе. Desired size `1..10` применяется только при следующем развёртывании, чтобы изменение настройки не удаляло живых бойцов.
16. Командная панель предлагает `INFANTRY`, `MOTORIZED_LIGHT`, `MOTORIZED_TRUCK` и `MOTORIZED_ARMED_LIGHT`. Light/truck используют faction-correct M998/UAZ-452 и M923/Ural с capacity fallback; armed-light использует M1025/UAZ-469 (для состава 2–4). Тяжёлая техника отсутствует.
17. Смена типа на motorized создаёт только desired vehicle request. Машина не создаётся в момент изменения настройки.
18. Сервер выбирает точную safe friendly площадку, резервирует её без fleet lease и выдаёт группе временный waypoint к отдельной staging-точке.
19. Spawn commit разрешён только после нахождения всех живых бойцов в staging radius в течение стабильных трёх секунд. Staging MOVE использует engine completion `All`; потеря waypoint допускает не более одного reissue после 15 секунд, поэтому снятие завершённого waypoint не создаёт command/voice churn. Перед entity spawn повторно проверяются owner/safety базы, весь живой состав, поверхность, свободное место и vehicle cap.
20. Type/target/base/group-generation change, timeout и shutdown удаляют только принадлежащий plan waypoint и освобождают pad reservation. Одну площадку одновременно удерживает не более одной группы.

## Стратегический интерфейс

- HUD появляется после выбора `US` или `USSR` и показывает tickets, connected/total supplies, READY squads, managed personnel и текущую цель A0/fallback-группы.
- Stock map получает только faction-streamed allied group markers. Подпись содержит динамический role-local identity (`A*`, `D*`, `R*`), роль, задачу, alive count, vehicle phase и compass/distance до цели.
- Каждая уникальная атакуемая база получает один компактный тёмный badge под stock base marker, без второго faction flag и без повторения названия базы. Оранжевая подпись агрегирует slot keys: `ATK  A0+A1+A2`.
- Кнопка `AI COMMAND` в правой верхней части карты открывает полноэкранную панель. Слева две колонки из десяти групп; справа — роль, тип, размер следующего развёртывания и только допустимые для выбранной роли цели.
- Карточка группы показывает slot/role/state, `alive/desired`, unit type, target, operational posture, vehicle phase и reinforcement ETA. Верхняя сводка показывает force size, logistics tier, supplies, pending reinforcements и shipments.
- Смена роли перестраивает role-local callsign и текущий приказ authoritative commander. Смена типа на motorized запускает отложенный vehicle request, а возврат к infantry завершает активный transport trip. Карточка и карта не показывают внутренние enum-коды: весь цикл переведён — «Ожидание площадки», «Площадка выбрана», «Следует к месту выдачи N/M», «Ожидание бойцов N/M», «Выдача техники», «Посадка», «Движение на технике», «Высадка», «Возврат к пешему приказу», «Задача техники завершена», «Переход на пеший порядок» и «Техника недоступна». Фактическая связь живых бойцов с любой машиной имеет приоритет над отсутствующей или терминальной записью trip: карточка и карта показывают «В технике N/M», а `SLOT_ACTIVITY` отдельно пишет `physical_vehicle_members`, поэтому retained/fallback-рассинхронизация больше не маскируется статусом «Пешком». Причина ожидания уточняется как поиск свободной площадки, ожидание безопасной базы, техники, готовности отряда или его прибытия к базе. Фактическая выдача машин требует включённого `aicfVehiclesEnabled=1`; при выключенном Stage 3 группа остаётся пешей.
- Нажатие цели сразу отправляет приказ. Серверный результат подтверждается последующей репликацией target/posture; authoritative журнал использует `PLAYER_ORDER_ACCEPTED` или `PLAYER_ORDER_REJECTED`.

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
| `aicfReplacementSupplyCost` | `500` | supplies за штатную replacement-группу `4/4`; для desired size 1–10 цена масштабируется и округляется вверх |
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
| `aicfVehicleSpawnStagingOffsetMeters` | `28` | расстояние от spawn pad до служебной точки сбора |
| `aicfVehicleSpawnStagingRadiusMeters` | `50` | радиус, в котором должны находиться все живые бойцы; CLI-границы `5–100` м |
| `aicfVehicleSpawnStagingHoldMs` | `3000` | время непрерывного удержания полного состава в staging radius |
| `aicfVehicleSpawnApproachTimeoutMs` | `300000` | предельное время подхода группы к зарезервированной площадке |

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
- `VEHICLE_TYPE_CHANGED` (`flow=TYPE_CHANGED`, `desired_only=1`, без lease/entity), `VEHICLE_SPAWN_PLAN_CREATED`, `VEHICLE_SPAWN_APPROACH_STARTED`, `VEHICLE_SPAWN_STAGING_PROGRESS`;
- `VEHICLE_SPAWN_COMMIT_REQUESTED`, `VEHICLE_SPAWN_PAD_PROBE`, `VEHICLE_SPAWN_COMMIT_REJECTED`, `VEHICLE_SPAWN_PLAN_COMPLETED`, `VEHICLE_SPAWN_PLAN_CANCELLED`. Для `SPAWN_PAD_OCCUPIED` probe обязан содержать reservation, `stable_slot`, exact cylinder geometry, фактические blocking trace hits и bounded nearby candidates с entity/Rpl identity, типом, расстоянием и фракцией.
- `FALSE_COMPLETION` обязан содержать `objective_distance_m`, `physical_progress_m`, `physical_confirmation=REJECTED` и `reliability_budget_consumed=0`; `MOB_EGRESS_BLOCKED_BY_SAFETY` — `hard_deadline=PAUSED acceptance_failure=0`. `MOB_EGRESS_DEADLINE_MISSED` с последним blocker `PLAYER_FENCE` или `COMBAT_THREAT` является regression failure.

Любой `AICF ERROR` любой фазы, `SHIPMENT_BALANCE_FAILED`, `SCRIPT (E/F)`, `ENGINE (F)`, `Virtual Machine Exception` или null-pointer делает runtime-срез `FAIL`.
Более одного `VEHICLE_SPAWN_APPROACH_STARTED reason=REISSUED` для одной reservation также является `FAIL`.
Role-local `slot=A#/D#/R#` считается display-key и может измениться после смены роли. Для correlation обязательна пара `stable_slot=S<n> numeric_slot=<n>` вместе с `group_generation`, `assignment_revision` и `operation_id`, где они применимы.

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
| E2 | successful replacement | ровно один ticket debit + один пропорциональный supply debit после exact `N/N` | NOT RUN |
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
| U0 | economy-off HUD | `SUPPLY OFF`, tickets/squads/personnel/objective обновляются без economy side effects | FAIL на первом срезе; FIXED CANDIDATE, RETEST NOT RUN |
| U1 | allied map US/USSR | каждая сторона видит только свои десять групп, их направления и атакуемые базы | NOT RUN |
| U2 | command composition | десять карточек соответствуют server slots, casualties, desired size, unit type, vehicle phase и replacement ETA | NOT RUN |
| U3 | valid order | ATTACK/DEFEND получает выбранную допустимую базу; target/posture реплицируется всем союзникам | NOT RUN |
| U4 | rejected order | enemy faction spoof, invalid slot/role target, recovery state и spam не меняют приказ | NOT RUN |
| U5 | JIP/reopen | поздний клиент и повторное открытие карты получают актуальный HUD/command state без duplicate widgets | NOT RUN |
| U6 | role change | ATTACK/DEFEND/RESERVE меняет role-local key, допустимые цели и приказ без spoof | NOT RUN |
| U7 | mobility change | INFANTRY ↔ LIGHT 4X4/TRUCK/ARMED 4X4 запускает либо завершает transport flow; armed-light ограничен составом 2–4, heavy asset не выдаётся | NOT RUN |
| U8 | roster size | default 4; значения 1 и 10 создают exact roster и пропорциональный debit; active SPAWNING size change отклоняется | NOT RUN |
| U9 | deferred vehicle spawn | смена типа не создаёт entity; группа получает staging waypoint; spawn происходит только после стабильного all-alive `N/N` | NOT RUN |
| U10 | concurrent vehicle requests | две группы не удерживают одну площадку; вторая ждёт освобождения либо получает отдельный pad | NOT RUN |
| U11 | vehicle plan cancellation | infantry/type/target/base change и уничтожение группы удаляют staging waypoint и reservation без lease/entity leak | NOT RUN |

Stage 4 не получает автоматический runtime PASS из static/Workbench evidence. Итог присваивается после review полного остановленного server/client log и заполнения матрицы на одном commit.
