# Архитектура

## Общая модель

Проект расширяет штатный Conflict только скриптами:

```text
Arma Reforger / stock Conflict
             |
             v
       AIConflictCore
             |
             v
       AIConflictArland
```

`AIConflictCore` не должен знать о конкретных именах или координатах Arland.
Он получает базы, фракции, spawn points, radio reachability и prefab-каталоги
через stock API. `AIConflictArland` подключает Core к конкретной stock-миссии и
содержит неизбежные map-specific overrides.

В production path нет собственного мира. Рабочий проект для Workbench и Diag —
`AIConflictArland/addon.gproj`; зависимость подтягивает Core.

## Bootstrap

Точка входа — modded `SCR_GameModeCampaign` в
`AIConflictArland/.../AICF_ArlandCampaignBootstrap.c`.

Последовательность запуска:

```text
SCR_GameModeCampaign.OnGameStart()
  -> super.OnGameStart()
  -> AICF_StrategicUIController.Start() на игровых peers
  -> server/master guard
  -> ожидание campaign.HasStarted()
  -> ожидание baseManager.IsBasesInitDone()
  -> AICF_ArlandRadioBridgeNormalizer.Start()
  -> next-frame AICF_MatchController.Start()
```

`AICF_MatchController.Start()`:

1. Проверяет play mode и server/master authority.
2. Создаёт Stage 1–4 configs и диагностические каналы.
3. Собирает stock bases и строит `AICF_ObjectiveGraph`.
4. Разрешает только фракции `US` и `USSR`.
5. Создаёт faction state, planner, spawner, reliability, victory, markers,
   economy и vehicle subsystem.
6. Подписывается на stock events и создаёт начальные roster.
7. Запускает периодические server loops.

`AICF_Stage0Controller` и классы `AICF_Test*` являются исследовательскими
утилитами и не вызываются из текущего production bootstrap.

## Периодические циклы

| Цикл | Частота | Ответственность |
|---|---:|---|
| `Update()` | 1 секунда | group lifecycle, reinforcement/economy, vehicle tick, task audit, replicated UI state, victory |
| `CommanderTick()` | `aicfCommanderIntervalMs`, default 15 секунд | детерминированный выбор ролей/целей и стратегическое перепланирование |
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
  -> AICF_TargetSelector
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
- `AICF_OrderPlanner` владеет AICF infantry waypoints: создаёт, связывает,
  заменяет, восстанавливает и удаляет их. Vehicle flows не должны обходить эту
  границу для стратегического infantry order.

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
- карта/снабжение: `graph revision`;
- асинхронная операция: request/trip token и immutable entity identity.

Slot переживает уничтожение и замену группы. Его основной lifecycle:

```text
EMPTY -> SPAWNING -> READY -> DESTROYED -> WAITING -> SPAWNING
```

`AICF_GroupSpawner` создаёт controller entity и настраивает roster. Spawn queue
Reforger 1.8 асинхронна: готовность доказывается фактическим составом,
faction/replication checks и callback/generation fencing, а не самим вызовом
`RequestSpawn()`.

Для физического положения группы используется `AICF_GroupRuntime`: живой
leader, а пока promotion не завершён — живой участник. Origin
`SCR_AIGroup` не считается положением бойцов.

## Vehicle domain

Vehicle subsystem всегда включён. Его задача — временно ускорить существующий
infantry assignment, а не стать вторым стратегическим командиром.

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

- tickets;
- Stage 4 supply/tier/request/shipment totals;
- strategic objective и допустимые order targets;
- summaries десяти групп на каждую сторону.

Сводки UI сериализуются компактными строками с разделителями `|`, `~` и `;`.
Это внутренний wire contract между server state и программно построенным UI;
изменение формата требует одновременной правки producer и parser.

`AICF_StrategicUIController` только отображает состояние и вызывает методы
player-owned `SCR_PlayerController`. Reliable RPC передаёт slot/selection, но
server заново получает `GetPlayerId()`, faction и authoritative slot и проверяет
role, target, size, rate limit и текущий lifecycle.

Map markers получают faction streaming и показывают союзные группы/цели.
Маркер следует за живым leader и перепривязывается после замены группы.

## Arland integration

`AIConflictArland` содержит четыре чувствительных расширения stock классов:

- bootstrap ждёт готовности Conflict и bases;
- `AICF_ArlandSeizingPolicy` разрешает AI-only capture;
- `AICF_ArlandVictoryPolicy` отключает stock territorial winner, чтобы
  `AICF_VictorySystem` завершал матч по ticket contract;
- `AICF_ArlandRadioBridgeNormalizer` исправляет релевантные односторонние
  radio links после bootstrap и смены владельца.

Эти `modded` классы действуют всякий раз, когда загружен Arland addon. Поэтому
его нельзя без review подключать к другой миссии. Особенно важны порядок
`super` и очистка event subscriptions.

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
