# Архитектура

## Общая модель

Проект расширяет штатный Conflict преимущественно скриптами. Три собственных
`MissionHeader` являются только точками запуска и не владеют world topology:

```text
Arma Reforger / stock Conflict
             |
             v
       AIConflictCore
             |
             v
       AIConflictArland
       /      |       \
 Arland   AIConflictEveron   AIConflictArlandRHS
 header   + Everon header    + RHS dependencies
```

`AIConflictCore` не должен знать о конкретных именах или координатах карт.
Он получает базы, фракции, spawn points, radio reachability и prefab-каталоги
через stock API. `AIConflictArland` содержит единый проверенный stock Conflict
bootstrap и data-driven stock radio override; bootstrap разрешает только Arland
и Everon. `AIConflictEveron` не добавляет второй lifecycle: он содержит
inherited header и подменяет только radio-normalizer factory на Everon policy.

В production path нет собственного мира. `Missions/AICF_Conflict_Arland.conf`
наследует штатный `{C41618FD18E9D714}Missions/23_Campaign_Arland.conf`,
`Missions/AICF_Conflict_Everon.conf` — штатный
`{ECC61978EDCC2B5A}Missions/23_Campaign.conf`, а
`Missions/AICF_RHS_Conflict_Arland.conf` — штатный RHS
`{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf`. Presentation overrides
ограничены metadata и `m_bShowInScenarioMenu`; world, systems config, базы и
параметры Conflict остаются у родителей. Единственный behavior override самих
headers — `m_eSaveTypes 0`: AICF state не имеет persistence serializers и
resume lifecycle, поэтому все плитки fail-closed отключают штатный session
save/load и при каждом запуске создают новую кампанию.

Stock-проекты для Workbench и Diag — `AIConflictArland/addon.gproj` и
`AIConflictEveron/addon.gproj`; Everon зависит от Arland integration после
явного compatibility review и использует raw world
`worlds/MP/CTI_Campaign_Eden.ent`.
Опциональный `AIConflictArlandRHS/addon.gproj` использует установленные RHS
world/mission без копирования ресурсов и зависит от обычного Arland, Core и
RHS. Обычные проекты RHS не знают.

## Content profile boundary

`AICF_ContentProfile` — узкая composition dependency для трёх вещей:

- отображения runtime faction key в стабильную сторону AICF `US`/`USSR`;
- детерминированных role candidates из faction `CHARACTER` catalog;
- vehicle candidate suffixes и подтверждённой conservative metadata.

Stock implementation находится в Core и воспроизводит прежние stock suffixes и
fallback. `AICF_RHSContentProfile` целиком находится в RHS-addon: runtime
`RHS_USAF -> US`, `RHS_AFRF -> USSR`, USMC/MSV paths и RHS vehicle metadata не
попадают в Core. Runtime key остаётся частью entity/trip/fleet identity guards;
stable key используется только политикой, CLI, UI и публичными Stage fields.
Одно событие `[AICF][CONTENT][INFO][PROFILE_SELECTED]` публикует обе стороны
mapping как startup evidence.

## Bootstrap

Точка входа — modded `SCR_GameModeCampaign` в
`AIConflictArland/.../AICF_ArlandCampaignBootstrap.c`.

Последовательность запуска:

```text
SCR_GameModeCampaign.OnGameStart()
  -> super.OnGameStart()
  -> fail-closed world gate: Arland или Everon
  -> server/master command-mode preflight: создание и strict validation
     AICF_Stage1Config до любых AICF subscriptions/callqueue
  -> bootstrap factory выбирает и активирует один AICF_ContentProfile
  -> AICF_StrategicUIController.Start() на игровых peers
  -> server/master guard
  -> ожидание campaign.HasStarted()
  -> ожидание baseManager.IsBasesInitDone()
  -> повторная fail-closed проверка того же config instance
  -> один radio normalizer из map factory:
     stock one-way policy; Everon дополнительно чинит полностью изолированный frontier
  -> next-frame AICF_MatchController.Start(same config instance, same profile,
     resolved map key)
```

`AICF_MatchController.Start()`:

1. Проверяет play mode и server/master authority, принимает тот же
   prevalidated `AICF_Stage1Config` и повторно проверяет его fail-closed.
2. Материализует из этого config immutable `AICF_CommandAuthorityPolicy`, не
   перечитывая CLI.
3. Создаёт Stage 2–4 configs и диагностические каналы.
4. Собирает stock bases и строит `AICF_ObjectiveGraph`.
5. Разрешает только runtime-фракции выбранного profile; stock требует
   `US`/`USSR`, RHS — `RHS_USAF`/`RHS_AFRF`.
6. Создаёт обе faction state, общий planner, spawner, reliability, victory,
   markers, economy и vehicle subsystem, а также разрешённые политикой
   faction-scoped `AICF_AICommander`.
7. Подписывается на stock events и инициирует полный initial roster: отправляет
   запросы на двадцать group controllers, а их group/agent rosters становятся
   готовыми асинхронно.
8. Запускает периодические server loops, которые обслуживают readiness gates и
   остальные общие подсистемы.
9. Только когда все двадцать initial slots фактически перешли в `READY`,
   публикует replicated authority availability flags и `ROSTER_READY`.

Strict CLI preflight принадлежит stock bootstrap, потому что на Arland normalizer сам
подписывается на смену владельца базы и может немедленно менять replicated
radio state. Invalid mode возвращает `CONFIG_INVALID` до
`AICF_StockRadioBridgeNormalizer.Start()`, его subscription/normalization и до
deferred `AICF_MatchController`; readiness-subscriptions stock Conflict к этому
моменту уже сняты. Production path передаёт в controller тот же предварительно
проверенный объект config; fallback-создание config существует только для
прямого вызова без bootstrap.

`AICF_Stage0Controller` и классы `AICF_Test*` являются исследовательскими
утилитами и не вызываются из текущего production bootstrap.

## Периодические циклы

| Цикл | Частота | Ответственность |
|---|---:|---|
| `Update()` | 1 секунда | group lifecycle, reinforcement/economy, vehicle tick, task audit, replicated UI state, victory |
| `CommanderTick()` | `aicfCommanderIntervalMs`, default 15 секунд | стабильный обход `US -> USSR`: AI commander выбирает цели только для своей стороны, player-commanded сторона поддерживает player intent/`SYSTEM_HOLD` без автономного retarget |
| `ReliabilityTick()` | `aicfReliabilityIntervalMs`, default 5 секунд | order binding, stuck recovery, lifecycle и accounting invariants |
| `Heartbeat()` | 60 секунд | агрегированное диагностическое состояние |

Смена владельца базы обрабатывается отдельно: событие сохраняется, после
короткой задержки граф пересобирается, его revision увеличивается, а приказы,
экономика и transport assignments проходят revalidation.

## Граф и приказы

```text
SCR_MilitaryBaseSystem
  -> AICF_ConflictAdapter.CollectBases()
  -> AICF_ObjectiveGraph.Build()
  -> AICF_AICommander(faction) -> AICF_TargetSelector
  -> AICF_OrderPlanner
  -> SCR_AIGroup waypoint queue
```

- `AICF_ConflictAdapter` — единственная точка адаптации stock Conflict для
  сбора обычных баз, HQ/source bases и relays, разрешения фракций и проверки
  безопасной reinforcement base.
- `AICF_ObjectiveGraph` строит направленные edges через stock radio
  reachability, предоставляет BFS/friendly path и собственный revision.
- `AICF_TargetSelector` ранжирует attack/defend/loss-response цели
  детерминированно.
- `AICF_AICommander` является faction-scoped boundary для нового автономного
  target selection. Он привязан к одной `AICF_FactionState` и не владеет
  spawn, tickets, economy, transport или waypoint entities.
- `AICF_OrderPlanner` владеет AICF infantry waypoints: создаёт, связывает,
  заменяет, восстанавливает и удаляет их, а также применяет player intent и
  безопасный `SYSTEM_HOLD`. Vehicle flows могут продолжить или восстановить
  уже выбранное назначение, но не имеют права выбирать другую базу.
- Стратегическое назначение имеет явный `AICF_EOrderTargetKind`: `BASE` хранит
  stock `SCR_CampaignMilitaryBaseComponent`, `POSITION` — подтверждённый
  сервером world endpoint. Для `POSITION` planner создаёт Defend waypoint с
  семантикой `MOVE_AND_HOLD`; такой приказ не участвует в radio graph,
  ownership/capture, relay smart action или SearchAndDestroy promotion.
- Проверка новой base-кандидатуры и проверка уже назначенной destination имеют
  разные boundaries. `IsStrategicTargetValid()` всегда применяет role/ownership
  правила к переданной `BASE`; `IsCurrentStrategicDestinationValid()` учитывает
  текущий `BASE|POSITION`. Поэтому point intent representative-slot не может
  раскрыть в UI все узлы objective graph как допустимые base targets.
- Если stock Move waypoint завершён вне физического радиуса цели,
  reliability сохраняет `faction + slot`, group generation и assignment
  revision и строит следующий endpoint только на navmesh, связанной с
  фактической позицией группы. Query origin сначала привязывается к ближайшей
  navmesh-точке без перемещения entity. Ограниченный боковой detour разрешает
  обойти ограду или другой локальный минимум, после чего каждый реально
  пройденный route leg продолжает тот же false-completion episode. Отсутствие
  физического прогресса по-прежнему ограничено endpoint budget и завершается
  temporary field hold/full replan, а не бесконечной выдачей waypoint. Если
  recovery уже поместил группу ближе минимальной длины route leg, planner
  выдаёт конечный objective endpoint; его всё равно принимает только отдельная
  физическая проверка радиуса цели.

## Command authority

`aicfAICommanderMode` имеет три допустимых exact-case значения:

| Mode | `US` | `USSR` |
|---|---|---|
| `BOTH` | AI commander | AI commander |
| `US` | AI commander | player command |
| `USSR` | player command | AI commander |

Параметр читается только при создании `AICF_Stage1Config` в Arland bootstrap;
без него действует `BOTH`. `NONE`, пустое, lowercase и любое неизвестное
значение не получают fallback и завершают startup до radio normalizer,
MatchController composition, roster и loops. Тот же предварительно проверенный
объект config передаётся в `AICF_MatchController`, а эффективная policy не
перечитывает CLI и меняется только перезапуском server process. Она независима от
`aicfExpectedPlayerFaction`, который относится к проверке результата.

Один `AICF_MatchController` сохраняется при любом mode: graph, lifecycle,
economy, vehicles, victory, replication, subscriptions и callqueue остаются
общими. Опциональны только `AICF_AICommander(US)` и
`AICF_AICommander(USSR)`. Явный valid player order имеет приоритет и на
AI-controlled стороне; запрет player RPC для неё не входит в текущую политику.
Этот приоритет обязателен на всех commander boundaries: initial assignment,
периодическом `ReconcileAICommanderOrder()` и loss response после смены
владельца базы. Они могут удержать или восстановить valid `POSITION`, включая
состояние с waypoint под управлением vehicle domain, но не заменить его `BASE`.

Player-commanded сторона без valid player intent получает стратегическое
состояние `AWAITING_PLAYER_COMMAND`. Физический приказ при этом — созданный
`AICF_OrderPlanner` waypoint `SYSTEM_HOLD` типа Defend на собственной HQ,
независимо от slot role. Он не является новой attack/defend целью, vehicle
assignment, task loss или stuck episode. Valid player order снимает ожидание.
Reliability вправе повторно выпустить waypoint к тому же valid player target,
но не выбирать другую цель. `BASE`, ставшая недопустимой после capture,
очищается и приводит обратно к `SYSTEM_HOLD`; `POSITION` остаётся role-agnostic
durable intent, переживает commander tick и несвязанную смену владельца базы и
проходит обычные waypoint/progress/stuck gates.

Map-point RPC передаёт только `slotId` и untrusted vector. Сервер повторно
определяет player/faction/stable slot, применяет общий player-order rate limit,
отбрасывает non-finite и выходящие за terrain bounds X/Z, игнорирует client Y,
получает `BaseWorld.GetSurfaceY()` и принимает только ближайший navmesh endpoint
в ограниченном окне и с ограниченным смещением. Ни target kind, ни faction,
ни marker label, ни waypoint identity клиент не выбирает.

Для удалённой точки `NavmeshWorldComponent` может ещё не держать streamable
tile в памяти. Resolver сначала вызывает `LoadTileIn()` и возвращает внутреннее
transient-состояние `NAVMESH_TILE_LOADING`; `AICF_MatchController` повторяет
проверку каждые 100 ms не дольше 6 s. Pending request сохраняет requester,
player id, stable faction, numeric slot, immutable group entity, group
generation, assignment/intent revisions и request token. Каждый retry заново
сверяет всю identity и текущую faction, а при несовпадении завершается
fail-closed до waypoint/intent mutation. `Stop()` явно снимает callback и
возвращает terminal owner response всем оставшимся запросам.

Результат этой проверки возвращается только владельцу player controller через
reliable `RplRcver.Owner`: `slotId`, исходная X/Z, `accepted`, exact rejection
reason и server-resolved endpoint. Это ephemeral acknowledgement, а не новый
источник gameplay state. UI сопоставляет его с pending request и немедленно
показывает server rejection; accepted destination и JIP по-прежнему читаются
из replicated campaign summary/static marker. Отсутствие owner response и
явный server rejection имеют разные UI-состояния.

Все причины нового выбора проходят через тот же boundary: initial/replacement
deployment, player role change, graph rebuild, reliability/stuck/lone-survivor
recovery и vehicle replan/fallback. Это особенно важно для vehicle domain: он
не должен становиться скрытым вторым командиром.

В Core нельзя добавлять координатные списки или специальные имена баз. Если
карта требует коррекции stock поведения, она оформляется в её integration
addon.

## Состояние фракции и группы

`AICF_FactionState` хранит фиксированный набор `AICF_GroupSlot` и
`AICF_TicketLedger`.

Важные идентичности:

- стабильная: `faction + slotId`, диагностическая форма `S0..S9`;
- presentation: role-local `A0..`, `D0..`, `R0..` — меняется при смене роли;
- конкретное воплощение группы: stable slot + `spawn/group generation`;
- стратегический приказ: slot + `assignment revision`;
- долговечное стратегическое намерение: target kind + base/position, role, posture,
  `decision_authority` и собственный intent revision;
- карта/снабжение: `graph revision`;
- асинхронная операция: request/trip token и immutable entity identity.

Slot переживает уничтожение и замену группы. Его основной lifecycle:

```text
EMPTY -> SPAWNING -> READY -> DESTROYED -> WAITING -> SPAWNING
```

`AWAITING_PLAYER_COMMAND` не добавляется в этот lifecycle: combat-ready slot
остаётся `READY`, а ожидание описывает command state. Runtime target/waypoint и
group reference очищаются при replacement, но durable intent на стабильном
`faction + slotId` сохраняется. После появления новой group generation intent
повторно проверяется по текущим role, ownership и graph и только затем
восстанавливается. `AICF_EStrategicDecisionAuthority` различает
`AI_COMMANDER`, `PLAYER_COMMAND`, `SYSTEM_HOLD` и отсутствие назначения.

`AICF_GroupSpawner` создаёт controller entity и настраивает roster через
выбранный content profile. Profile возвращает ordered suffixes, но итоговый
prefab обязательно должен принадлежать faction `CHARACTER` catalog и успешно
загружаться. RHS запрещает source-roster fallback; отсутствие любой роли
удаляет ещё пустой controller fail-closed. Spawn queue
Reforger 1.8 асинхронна: готовность доказывается фактическим составом,
faction/replication checks и callback/generation fencing, а не самим вызовом
`RequestSpawn()`.

Перед initial roster authoritative `AICF_MatchController` проверяет глобальный
active-AI limit `AIWorld` и при необходимости поднимает его до настроенного
`max_managed_agents`, не уменьшая более высокий world limit. Если effective
limit всё ещё недостаточен, запуск завершается fail-closed до создания групп.

Для физического положения группы используется `AICF_GroupRuntime`: живой
leader, а пока promotion не завершён — живой участник. Origin
`SCR_AIGroup` не считается положением бойцов.

После полного bounded MOB-egress deadline разрешён identity-preserving hidden
recovery живых участников в свободную точку рядом с текущей целью (с базовым
отступом 15 м). Привязка к цели, а не фиксированный шаг от HQ, не оставляет
formation на следующем изолированном участке navmesh. Recovery по-прежнему
требует server authority, exact group/generation/assignment identity,
отсутствие vehicle transition и
отдельные player proximity, LOS и combat fences; небезопасный перенос
отклоняется fail-closed.

## Строители баз

`AICF_BaseBuilderService` — отдельный server/master support domain в Core.
`AICF_MatchController` только создаёт службу, обновляет после `ROSTER_READY`
и вызывает `Stop()`. Армейские десять slots, их задания, reinforcement и
ticket accounting не меняются. Это отдельное архитектурное решение для
вспомогательного персонала: служба владеет единственным service slot каждой
stock base, а `AICF_BaseBuilderSpawner` создаёт для него одиночный faction
`RIFLEMAN` из active content profile. RHS не получает stock character fallback.
`AICF_OrderPlanner` сохраняет исключительное владение waypoint entities.

Готовность после `RequestSpawn(1)` подтверждается фактическим ровно одним
живым бойцом нужной фракции. Стабильная identity — faction + numeric service
slot, выделенный базе из диапазона после армейских slots и не переиспользуемый
для другой базы. Group/entity identity и spawn generation
сохраняются до удаления; повторные polling ticks не создают второго бота.
Строители имеют отдельную ограниченную квоту активного AI — не более числа
баз сверх `max_managed_agents`; она не уменьшает более высокий world limit.

Очередь состоит из уже размещённых stock `SCR_CampaignBuildingCompositionComponent`.
Источник — `GetBuildingCompositions()` и server event
`GetOnEntitySpawnedByProvider()`: второй покрывает размещение внутри provider
radius, но за пределами меньшего base radius. Перед работой повторно
проверяются текущий владелец базы, связь provider с этой базой, его актуальный
building radius и unfinished layout. Neutral props допускаются, чужая faction
composition — нет. Цель выбирается по расстоянию с устойчивым `EntityID` tie-break.

Бот подходит к ближайшей стороне world bounds layout с отступом 2 метра;
arrival tolerance — 1 метр, отдельно проверяется нахождение вне footprint.
Большой stock `Danger_UnsafeArea` больше не задаёт рабочую дистанцию.
`AICF_BaseBuilderDangerEvent` сохраняет source layout, character identity и
generation. Только работающий с этим layout бот снаружи footprint получает
исключение из этой реакции. Остальные AI и все чужие danger events сохраняют
штатную реакцию; после completion исключение перестаёт действовать.
Прогресс начинается только после физического прибытия;
боевой контакт, бессознательное состояние и нахождение в машине приостанавливают
работу. Сначала бот достаёт строительный gadget через штатный AI equip path,
ожидает hand attachment и запускает `CMD_Item_Action`. Progress требует
`OnItemUseBegan`, `IsUsingItem` и инструмент в левой руке. Только после
3 секунд непрерывной работы вызывается `AddBuildingValue` со значением
инструмента; fallback-прогресса без инструмента/анимации нет. `OnItemUseEnded`
сбрасывает таймер, `StopTool` симметрично снимает оба invoker и убирает gadget.
Supplies за размещение, стоимость,
replication и активация готового service остаются в stock building pipeline;
строитель не выдаёт player XP и повторно не списывает стоимость проекта.

Недостижимая за 120 секунд цель откладывается на 60 секунд, освобождая очередь
для других проектов. После завершения или отмены работы бот идёт к master
provider (главной палатке); удаление после 30 секунд простоя разрешено только
при физическом нахождении не далее 8 метров от неё. Новая работа прерывает
возврат/ожидание. Потеря базы, смерть или spawn timeout отменяют работу,
очищают stock spawn queue и owned entities; следующая попытка ограничена
60 секундами. Player-controlled или потерявший identity бот не удаляется,
а slot удерживается fail-closed без замены. `Stop()` снимает placement event
и завершает службу без новых callbacks. Runtime probe находится только в
`tools/fixtures/` и не загружается обычным addon.

## Vehicle domain

Vehicle subsystem всегда включён. Его задача — временно ускорить существующий
infantry assignment, а не стать вторым стратегическим командиром.

`AICF_VehicleCatalog` не владеет content paths: он получает ordered candidates
и metadata от active profile, затем принимает только faction `VEHICLE` catalog
entries. Acquisition проверяет metadata до spawn и повторно измеряет live
accessible pilot/cargo/turret compartments до fleet binding. Неизвестный prefab,
нехватка мест, pilot/turret или невалидный AI usage дают fail-closed/fallback,
а не скрытую подмену stock asset.

Фазы одной поездки:

```text
WAITING_FOR_SITE
  -> SITE_PLANNED
  -> APPROACHING_SITE
  -> STAGING_CONFIRMED
  -> SPAWN_COMMIT
  -> BOARDING
  -> TRANSIT
  -> DISMOUNT
  -> HANDOFF
  -> COMPLETE

Любая фаза -> FALLBACK -> infantry order restore
             или FAILED_CLOSED
```

Разделение владельцев:

| Компонент | Что ему разрешено |
|---|---|
| `AICF_VehicleCoordinator` | composition, scheduling, aggregate queries |
| `AICF_TransportTrip` | identity и состояние одной поездки |
| `AICF_TransportTripController` | единственная точка перехода фаз и интерпретации `AICF_TripOutcome` |
| `AICF_VehicleAcquisitionFlow` | acquisition-local phases/outcome; в `SPAWN_COMMIT` вызывает spawner |
| `AICF_VehicleSpawner` | site selection/reservation и authoritative создание entity |
| `AICF_VehicleBoardingFlow` | локальное состояние посадки и typed outcome |
| `AICF_VehicleTransitFlow` | движение, progress/recovery evidence и typed outcome |
| `AICF_VehicleDismountFlow` | высадка и clearance observations, но не release/delete |
| `AICF_VehicleTaskHandoff` | vehicle waypoint attach/detach и возврат infantry order |
| `AICF_FactionFleet` | lease, accepted generation, per-faction cap и world pool |
| `AICF_VehicleCleanupManager` | occupancy/clearance, release, retirement и identity-safe delete |

Flows возвращают данные и не вызывают друг друга, controller или cleanup
напрямую. `WAITING_FOR_SITE` не должен создавать entity или резервировать
lease. Cleanup живёт независимо от terminal trip: поездка может закончиться, а
защищённая проверка пассажиров и подтверждение удаления продолжиться.

Предспавновая staging geometry по умолчанию использует offset `43 м` и radius
`25 м` как guidance для штатного group waypoint. Готовность доказывается не
принадлежностью к этому кругу, а фактическим положением каждого живого member в
безопасном pad envelope `10..90 м`: spawn clearance сохраняется, а рельеф и
растянутая формация не создают ложный результат `9/10`. Настоящему outlier вне
envelope acquisition выдаёт видимый tracked `SCR_AIMoveIndividuallyBehavior` к
staging point. Action fenced по trip/request/entity/Rpl identity, progress
проверяется live, retry ограничен одной попыткой, а plan cancellation завершает
только всё ещё принадлежащие utility actions. Если ровно один живой member
остаётся вне envelope после исчерпания обеих move-actions, а остальные `N-1`
уже собраны, acquisition выполняет обязательную наземную relocation этого exact
authoritative AI к свободной navmesh-позиции staging. Это намеренно видимый
administrative recovery без player/LOS fence: он не меняет compartment state,
проверяет trip/request/entity/Rpl/faction identity и vehicle transitions, а
mutation разрешена только одному member за scheduler tick. Waypoint-only
`assignment revision` и глобальный `base revision` при неизменных destination и
`intent revision` принимаются новым runtime snapshot без сброса site
reservation/spawn plan; пригодность exact выбранной базы и pad всё равно
перепроверяется live. Смена стратегического intent по-прежнему инвалидирует
acquisition context. Boarding использует согласованный threshold `90 м` и
применяет отдельные immutable phase budgets
для `APPROACH`, `DRIVER`, `GUNNER` и `PASSENGERS`; grace пересчитывается в каждой
фазе, но никогда не продлевает общий hard trip deadline.

`AICF_VehicleBoardingFlow` выбирает mandatory crew по расстоянию до exact door
с детерминированным `EntityID` tie-break. В passenger phase один раз создаётся
immutable план `member EntityID -> exact Cargo manager/slot`. Временная
occupancy, accessibility или чужая reservation блокирует только свою пару:
готовые пары получают action, а уже корректные reservations не откатываются.
После normal action bounded recovery заменяет зависший behavior новым
отслеживаемым `SCR_AIGetInVehicle` к тому же exact seat; прямой параллельный
`CompartmentAccessComponent.GetInVehicle(... forceTeleport=false ...)` здесь
запрещён, потому что он может снять ownership исходного action и reservation без
начала transition. Только после tracked retry разрешён forced
`GetInVehicle(... forceTeleport=true ...)`. Hidden
mutation разрешена по одному member за scheduler tick после повторной проверки
authority, group/trip/lease/entity identity, vehicle state, combat, player
radius и LOS. Для Cargo не прошедший fence означает `WAIT` до phase deadline.
Для mandatory Pilot/Turret flow сначала детерминированно исключает исчерпавшего
recovery бойца. Старый action завершается в текущем scheduler tick, а видимый
tracked exact-seat action следующему ближайшему неиспытанному кандидату выдаётся
не раньше следующего tick: это не позволяет отложенному teardown старого action
снять новую reservation того же seat (ABA race). Первая rotation получает
bounded observation window, не выходя за общий hard trip deadline. При terminal
fallback всё ещё принадлежащий utility `SCR_AIGetInVehicle` завершается даже
если engine уже потерял его reservation; иначе водитель может сесть после
восстановления infantry order. Перебор ограничен живыми members и общими
phase/trip deadlines; только отсутствие кандидата оставляет `WAIT` до deadline.

Deadline не может преобразовать уже физически завершённую посадку в
`FALLBACK`. Если все живые members связаны с vehicle, имеют compartment,
находятся без `getting_in/getting_out` и проходят settled-проверку, flow ждёт
следующий scheduler tick для обязательного второго settled poll. Любая
физическая регрессия сразу снимает это подавление; при сохранённом `10/10`
следующий tick коммитит `TRANSIT`, а не запускает terminal force-disembark.

Явное изменение игроком live-группы с `INFANTRY` на motorized unit type создаёт
в `AICF_VehicleCoordinator` одноразовый exact admission intent. Он сохраняется
до создания одного trip и обходит только эвристический
`minimum_route_m`: authority, group generation, unit type, meaningful order,
minimum roster, faction cap и lease gates остаются обязательными. Поэтому
повторное включение транспорта после fallback работает и внутри прежнего
порога `400 м`, но не создаёт бесконечный цикл автоматических retry.

Для dismount завершение trip и безопасность cleanup являются разными
доказательствами. `AICF_VehicleDismountFlow` после двух последовательных polls
без logical occupants, compartment transitions и живых members внутри vehicle
bounds возвращает completion, позволяя controller раньше восстановить infantry
order. Target-side guidance, clearance radius и непрерывный `5 с` stable-clear
остаются у независимого cleanup/clearance lifecycle и по-прежнему обязательны
перед release/delete. Exact occupant escalation начинается animated retry через
`5 с`, forced fenced exit через `10 с`; relocation физического ghost выполняется
отдельно на следующем poll и только если transform всё ещё внутри hull.

Vehicle snapshot переносит target kind, base/position и две разные версии:
`assignment revision` защищает
runtime waypoint/entity identity, а `intent revision` обозначает только смену
target/posture/authority/role. Если новый intent приходит в уже начатый
`HANDOFF` или terminal restore, `AICF_VehicleHandoffState` выдаёт свежий
ограниченный order-restore budget по новому intent revision. При этом lease,
clearance и cleanup evidence не сбрасываются; простая замена waypoint не может
переармить budget.

Перед удалением vehicle Cleanup Manager сохраняет immutable snapshot,
проверяет `EntityID`/replicated identity, выдерживает stable-clear window и
повторяет occupancy scan непосредственно перед `DeleteRplEntity`. При stale
identity или неполном доказательстве состояние удерживается fail-closed.

## Reinforcement и экономика

`AICF_EconomySystem` всегда ведёт ticket-based reinforcement transaction,
добавляя stock supplies и connectivity pacing.

Транзакция replacement:

```text
request
  -> накопление progress с учётом network tier
  -> выбор safe/affordable stock base
  -> reserve ticket + debit supplies
  -> spawn attempt
  -> revalidate request token, slot generation, graph revision и base safety
  -> commit ticket и finalize supply debit

ошибка до завершения -> ticket/supply rollback + bounded retry
```

Supply delivery абстрактна: `AICF_SupplyDeliverySystem` переносит учитываемый
cargo между stock supply pools по доступному friendly path, не создавая
физический convoy. Баланс dispatched/delivered/returned/in-transit является
диагностическим контрактом.

## Replication, UI и trust boundary

`Integration/AICF_CampaignState.c` расширяет уже реплицируемый
`SCR_GameModeCampaign`:

- authority availability flags для `US` и `USSR`;
- tickets;
- Stage 4 supply/tier/request/shipment totals;
- strategic objective и допустимые order targets;
- summaries десяти групп на каждую сторону.

Сводки UI сериализуются компактными строками с разделителями `|`, `~` и `;`.
После прежних десяти полей summary содержит `target kind`, округлённые X/Z и
`intent revision`; старые первые десять полей и их parser contract сохранены.
Это внутренний wire contract между server state и программно построенным UI;
изменение формата требует одновременной правки producer и parser.

`AICF_StrategicUIController` только отображает состояние и вызывает методы
player-owned `SCR_PlayerController`. Reliable RPC передаёт slot/selection, но
server заново получает `GetPlayerId()`, faction и authoritative slot и проверяет
role, target, size, rate limit и текущий lifecycle.

Выбор `POSITION` использует stock `SCR_MapCommandCursor`, поэтому pan/zoom и
положительный cursor остаются штатными. На время выбора command panel/scrim
скрыты, отдельный compact prompt имеет явную отмену; cursor callbacks и
повторяющийся update снимаются при выборе, отмене, закрытии карты и `Stop()`.
UI считает приказ принятым только после изменения authoritative group summary.
Создание `SCR_MapCommandCursor` откладывается за пределы input event кнопки:
клик `MOVE TO MAP POINT` не может одновременно стать координатой приказа.
Отложенная активация снимается тем же lifecycle cleanup. Цвета prompt/cancel
повторно устанавливаются через `SetRectColor()` после Enfusion widget
initialization, а map-point action сохраняет отдельный синий стиль и не
попадает под янтарный стиль base targets.

Authority flags входят в JIP snapshot, но не являются хранилищем immutable
policy. `false/false` означает, что authoritative command state ещё недоступен
или controller уже остановлен; поскольку `NONE` не является valid mode, UI
безопасно показывает `COMMAND SYNC`. Controller оставляет sentinel во время
composition и всего асинхронного initial spawn, а policy flags публикует в
`TryLogRosterReady()` только после перехода всех двадцати slots обеих фракций в
`READY`. Эта готовность доказывается `GROUP_ROSTER_READY`/`ROSTER_READY`.

`AICF_MatchController.Stop()` возвращает flags в `false/false` до снятия
subscriptions и очистки domain state. Если flags действительно
изменились, authority вызывает `Replication.BumpMe()`, поэтому connected proxy
и поздний snapshot больше не показывают действующий command mode. Опубликованные
valid flags позволяют UI отличить `AI COMMANDER` от `PLAYER COMMAND`, но не дают
клиенту право решать, кто управляет фракцией: источником истины остаются server
policy и per-assignment `decision_authority`.

Map markers получают faction streaming и показывают союзные группы/цели.
Маркер следует за живым leader и перепривязывается после замены группы.
Для active player `POSITION` marker system дополнительно создаёт faction-filtered
stock static server marker. Его static-marker serialization обеспечивает JIP;
новый point/base intent и `Stop()` удаляют прежний marker.

## Rank policy

Оба scenario headers задают `m_eStartingRank GENERAL`. В Reforger 1.8
stock/default `SCR_RankContainer` сериализует ранги только до `MAJOR`, поэтому
штатный `SCR_GameModeCampaign.SetStartingRank()` не может сам разрешить порог
несуществующей записи `GENERAL`. Modded `SCR_PlayerXPHandlerComponent` в Core
использует faction-specific `GENERAL` threshold, когда он существует, иначе
максимальный настроенный non-renegade threshold активного container как
effective `GENERAL` floor. Все штатные награды, штрафы, reconnect restore и
persistence deserialize сходятся в `AddPlayerXP()`; floor применяется до
`UpdatePlayerRank()`, который authoritative записывает именно `GENERAL` в
replicated character state. Поэтому character не успевает перейти в `RENEGADE`
даже временно.

XP выше порога не обрезается: положительные награды и статистика сохраняются,
а штрафы уменьшают сначала накопленный избыток. При фактической коррекции server
меняет owner-only XP, вызывает stock XP listeners/RPC и делает
`Replication.BumpMe()` только после изменения. До назначения фракции в
authoritative map `SCR_FactionManager` floor fail-closed не выбирает
`SCR_RankContainer`; после назначения container разрешается через штатный
`SCR_FactionManager.GetFactionRanks()`, включая stock fallback для нестандартной
RHS faction. Map обновляется до `OnPlayerFactionSet_S`, тогда как affiliation
component во время callback ещё может возвращать предыдущее значение. Назначение
фракции server публикует через authoritative
`SCR_FactionManager.OnPlayerFactionSet_S()`; после обновления faction state этот
callback повторно вызывает `UpdatePlayerRank()`, а
повторную проверку при создании character даёт штатный spawn-вызов
`UpdatePlayerRank()`. Поэтому join/JIP, reconnect и persistence restore
покрываются `AddPlayerXP()` и faction/spawn lifecycle hooks без раннего polling
неготового `PlayerController`. Override `GetCharacterRank()` и
`GetPlayerRankByXP()` остаются дополнительной защитой чтения. Глобальный
override `SCR_RankContainer.GetRankByXP()` не используется.

## Stock campaign integration: Arland и Everon

`AIConflictArland` содержит четыре чувствительных расширения stock классов,
проверенных для штатных Arland и Everon Conflict missions:

- bootstrap сразу после stock `super.OnGameStart()` выполняет server/master
  command-mode preflight, только после valid результата ждёт готовности Conflict
  и bases, повторно проверяет и передаёт тот же immutable config в
  MatchController;
- `AICF_ArlandSeizingPolicy` разрешает AI-only capture;
- `AICF_ArlandVictoryPolicy` отключает stock territorial winner, чтобы
  `AICF_VictorySystem` завершал матч по ticket contract;
- `AICF_StockRadioBridgeNormalizer` исправляет релевантные односторонние
  radio links после valid preflight и при смене владельца. Bootstrap создаёт
  ровно один normalizer через `AICF_CreateRadioBridgeNormalizer()`.

Отдельно `AIConflictArland/Missions/AICF_Conflict_Arland.conf` предоставляет
плитку `AI Conflict - Arland` и отключает для неё persistence через
`m_eSaveTypes 0`. Это inherited config, а не пятое расширение класса и не копия
мира.

Bootstrap сверяет `GetGame().GetWorldFile()` и fail-closed запускается только
для `CTI_Campaign_Arland`, RHS Arland или `CTI_Campaign_Eden`. Поэтому загрузка
dependency рядом с иной mission не создаёт controller/UI/subscriptions.
Порядок `super` и очистка event subscriptions остаются обязательными.

`AIConflictEveron` добавляет
`Missions/AICF_Conflict_Everon.conf`: плитка `AI Conflict - Everon` наследует
официальный Everon Conflict header и отключает persistence через
`m_eSaveTypes 0`. Его `AICF_EveronRadioBridgeNormalizer` расширяет stock
normalizer: только когда достижимый от HQ friendly-компонент исчерпал все
допустимые цели, он детерминированно расширяет ближайшую обычную базу до
полезного relay. Имена и координаты баз не задаются. Factory override не
добавляет `OnGameStart`, subscription, controller или server loop, поэтому
dependency на `AIConflictArland` по-прежнему использует один lifecycle.

`AIConflictArlandRHS` добавляет ровно одно пятое расширение того же campaign
class: override `AICF_CreateContentProfile()`. В нём нет `OnGameStart`, event
subscription, `CallLater` или второго `AICF_MatchController`, поэтому Arland
policies и lifecycle исполняются один раз. Отдельный RHS-only compatibility
override `SCR_Faction.GetIndentityVoiceSignal()` сопоставляет только
`RHS_AFRF` со штатным русским voice signal СССР (`1`); для остальных фракций
результат без изменений делегируется исходной реализации.

RHS-only adapter `SCR_ContentBrowserEditorComponent.FilterEntries()` сохраняет
штатные blacklist, search, filtered-event behavior и числовые prefab IDs.
Stock small `LivingArea` показывает только `GROUP` entries с
`GROUPTYPE_ESSENTIAL`; RHS USMC/MSV groups этого vanilla UI label не содержат,
хотя уже присутствуют в placeable registry и видны в большой казарме. RHS-only
adapter в `FilterEntries()` добавляет label в локальную копию labels только для
двух минимальных `SentryTeam` — по одному на поддержанную faction. Исходные UI
info, numeric prefab IDs, faction labels, provider traits, budgets и authoritative
placement не меняются. Authoritative `CanPlaceEntityServer()` повторяет ту же
проекцию только вокруг штатного `AreLabelsMatching()`, сохраняя faction,
provider, blacklist и budget gates; затем временный label удаляется.
`PERSONNEL_BROWSER_BOUND`
публикует выбранные groups и итоговый filter count; отсутствие обеих entries
даёт `PERSONNEL_BROWSER_BIND_FAILED`, а проекция label не применяется частично.
`PERSONNEL_SERVER_VALIDATED` фиксирует server-side проекцию и итоговый verdict.
Клиентский `SCR_LoadoutButton` дополнительно не рисует badge, если UI info RHS
Campaign loadout не содержит faction, предотвращая vanilla null dereference.

Его `Missions/AICF_RHS_Conflict_Arland.conf` аналогично добавляет плитку
`AI Conflict RHS - Arland`, наследуя RHS mission. Поскольку RHS root-addon
зависит от обычного Arland, при одновременной загрузке видны обе плитки; для RHS
runtime пользователь выбирает именно RHS-вариант, а для stock запускает addon
graph без `AIConflictArlandRHS`.

## Диагностика как интерфейс

Каналы `[AICF][STAGE1]`, `[STAGE2]`, `[STAGE3]`, `[STAGE3.5]` и `[STAGE4]`
используют общий `run` и `t_ms`. Анализаторы зависят от event name и полей
`key=value`; свободный текст вторичен.

Следствия:

- переименование event/field — изменение интерфейса;
- новый failure path должен оставлять достаточную identity и causation
  информацию;
- отфильтрованные события не заменяют полный server/client log;
- static audit, Workbench compile и runtime доказывают разные свойства.

Command-authority contract добавляет следующие поля и события, не переименовывая
существующие:

- `CONFIG ai_commander_mode=... ai_commander_us=... ai_commander_ussr=...`;
- один `COMMAND_AUTHORITY_SET faction=... authority=AI|PLAYER` на сторону;
- edge-triggered `COMMAND_WAITING` при переходе в ожидание player order;
- `STRATEGIC_ASSIGNMENT decision_authority=AI_COMMANDER|PLAYER_COMMAND|SYSTEM_HOLD`.

По этим строкам можно доказать server decision path, но UI/JIP всё равно
требуют отдельного client evidence, а визуальный вид — ручного verdict.

## Зоны повышенного риска

- `AICF_MatchController`, `AICF_GroupSlot` и vehicle cleanup/flows очень
  stateful; локальная правка может задеть несколько scheduled loops.
- `AICF_CorpseRetentionPolicy` расширяет stock garbage system и требует
  отдельного внимания при долгих soak-прогонах.
- `AICF_GroupSpawner` использует stock/internal roster и global spawn controls;
  version upgrade требует повторной проверки всех exit paths.
- Campaign replicated state жёстко представляет десять slots на сторону.
- Protected/internal Enfusion API делает обновление версии игры отдельной
  migration-задачей, а не обычной перекомпиляцией.
