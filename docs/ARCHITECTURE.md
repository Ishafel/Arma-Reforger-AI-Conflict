# Архитектура

## Общая модель

Проект расширяет штатный Conflict преимущественно скриптами. Два собственных
`MissionHeader` являются только точками запуска и не владеют world topology:

```text
Arma Reforger / stock Conflict
             |
             v
       AIConflictCore
             |
             v
       AIConflictArland
          /       \
 stock mission    AIConflictArlandRHS
                  + RHS dependencies
```

`AIConflictCore` не должен знать о конкретных именах или координатах Arland.
Он получает базы, фракции, spawn points, radio reachability и prefab-каталоги
через stock API. `AIConflictArland` подключает Core к конкретной stock-миссии и
содержит неизбежные map-specific overrides.

В production path нет собственного мира. `Missions/AICF_Conflict_Arland.conf`
наследует штатный `{C41618FD18E9D714}Missions/23_Campaign_Arland.conf`, а
`Missions/AICF_RHS_Conflict_Arland.conf` — штатный RHS
`{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf`. Presentation overrides
ограничены metadata и `m_bShowInScenarioMenu`; world, systems config, базы и
параметры Conflict остаются у родителей. Единственный behavior override самих
headers — `m_eSaveTypes 0`: AICF state не имеет persistence serializers и
resume lifecycle, поэтому обе плитки fail-closed отключают штатный session
save/load и при каждом запуске создают новую кампанию.

Stock-проект для Workbench и Diag — `AIConflictArland/addon.gproj`.
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
  -> server/master command-mode preflight: создание и strict validation
     AICF_Stage1Config до любых AICF subscriptions/callqueue
  -> bootstrap factory выбирает и активирует один AICF_ContentProfile
  -> AICF_StrategicUIController.Start() на игровых peers
  -> server/master guard
  -> ожидание campaign.HasStarted()
  -> ожидание baseManager.IsBasesInitDone()
  -> повторная fail-closed проверка того же config instance
  -> AICF_ArlandRadioBridgeNormalizer.Start()
  -> next-frame AICF_MatchController.Start(same config instance, same profile)
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

Strict CLI preflight принадлежит Arland bootstrap, потому что normalizer сам
подписывается на смену владельца базы и может немедленно менять replicated
radio state. Invalid mode возвращает `CONFIG_INVALID` до
`AICF_ArlandRadioBridgeNormalizer.Start()`, его subscription/normalization и до
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

## Arland integration

`AIConflictArland` содержит четыре чувствительных расширения stock классов:

- bootstrap сразу после stock `super.OnGameStart()` выполняет server/master
  command-mode preflight, только после valid результата ждёт готовности Conflict
  и bases, повторно проверяет и передаёт тот же immutable config в
  MatchController;
- `AICF_ArlandSeizingPolicy` разрешает AI-only capture;
- `AICF_ArlandVictoryPolicy` отключает stock territorial winner, чтобы
  `AICF_VictorySystem` завершал матч по ticket contract;
- `AICF_ArlandRadioBridgeNormalizer` исправляет релевантные односторонние
  radio links после valid preflight и при смене владельца.

Отдельно `AIConflictArland/Missions/AICF_Conflict_Arland.conf` предоставляет
плитку `AI Conflict - Arland` и отключает для неё persistence через
`m_eSaveTypes 0`. Это inherited config, а не пятое расширение класса и не копия
мира.

Эти `modded` классы действуют всякий раз, когда загружен Arland addon. Поэтому
его нельзя без review подключать к другой миссии. Особенно важны порядок
`super` и очистка event subscriptions.

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
