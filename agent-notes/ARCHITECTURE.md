# Архитектура и навигация по исходникам

Проект расширяет stock Conflict скриптами Enforce. Gameplay resources — тонкие
inherited `MissionHeader`; они выбирают штатный сценарий и его настройки.
Собственных world/base/layout ресурсов нет. North ограничивает штатные базы
whitelist, оставляя ландшафт целым. UI редактора экипировки использует временный
in-memory world для preview; он не является миром кампании.

## Граф addon

Вооружение FIA в RHS/WCS принадлежит integration: profiles владеют пулами
и счётчиком каждой concrete роли одного матча, character — сохранённым выбором,
identity и одноразовым флагом commit. `AICF_RHSPMCEquipment` готовит local draft
и выполняет native inventory transaction с rollback; успешный повтор не
перезаписывает изменения игрока. Оба FIA boarding owner читают pending gate,
который снимается только после успешной подготовки. Core и stock FIA этот
контракт не расширяют. Подробности: [FIA_ARMAMENT.md](FIA_ARMAMENT.md).

| Addon | GUID | Ответственность |
|---|---|---|
| `AIConflictCore` | `9178E5822AFE48EA` | Независимые от карты механики, UI, диагностика |
| `AIConflictArland` | `B52C5F6AEDBF423E` | Общий bootstrap stock Conflict, radio normalization, Arland header |
| `AIConflictEveron` | `A4B2E62595F645A4` | Everon headers и policy выхода из изолированного radio-компонента |
| `AIConflictArlandRHS` | `9F88011DA22B471C` | RHS content profile и compatibility adapters |
| `AIConflictEveronRHS` | `FA9FDCCA428A43BA` | Объединение Everon policy и RHS profile, callsign pool adapter |
| `AIConflictArlandWCSRHS` | `A1CF260928100001` | WCS экипировка, арсенал и техника поверх RHS |
| `AIConflictEveronWCSRHS` | `A1CF261006100001` | Полный и северный Everon с общим WCS + RHS profile |

Vanilla dependency — `58D0FB3206B6F859`. Точный граф зависимостей принадлежит
`addon.gproj` каждого addon и `tools/Start-AICFRuntime.ps1`; GUID не меняются.
Core и stock integrations не должны получать обязательные RHS/WCS dependencies.

Цены штатных RHS/WCS меню принадлежат integration policy/UIInfo adapter,
см. [MENU_PRICES.md](MENU_PRICES.md). Adapter возвращает CAMPAIGN quote;
сами списания/возвраты и проверки прав остаются в native building/spawner.
Он не является частью AICF economy и не меняет стоимость auto reinforcement.

Общий вход — `AIConflictArland/Scripts/Game/AIConflictArland/Integration/`
`AICF_ArlandCampaignBootstrap.c`. Он допускает штатные Conflict worlds Arland
и Everon. Integration addons подменяют нужные factory boundaries, сохраняя
один controller и один lifecycle. Карта передаёт Core stock базы и radio graph;
имена и координаты конкретных баз в Core запрещены.

## Домены Core

Корень: `AIConflictCore/Scripts/Game/AIConflict/`. Начинай с владельца домена,
затем читай его конфигурацию и соседние классы.

| Каталог | Ответственность и основные владельцы |
|---|---|
| `Bootstrap/`, `Config/` | Composition root `AICF_MatchController`, запуск и настройки |
| `Command/` | `AICF_CommandAuthorityPolicy`, `AICF_AICommander`: разрешения и orchestration; `AICF_AdaptiveCommanderPolicy` собирает faction snapshot, `AICF_AdaptiveScore` оценивает кандидатов |
| `Objectives/`, `Orders/` | Граф целей, `AICF_OrderPlanner`: infantry intent, target, waypoint, recovery |
| `State/`, `Forces/` | `AICF_FactionState`, `AICF_GroupSlot`: stable slots, async roster, replacement; `AICF_GroupRuntime`: позиция живого бойца |
| `Forces/` | `AICF_InfantryRecruitmentService`, `AICF_InfantryRecruitSpawner`: посещение казармы и покупка пополнения |
| `Forces/`, `State/`, `Integration/` | `AICF_SocialService`/`AICF_SocialActor`: начальные emotes и салют; `AICF_GreetingGate`: общий cooldown identity игрока; `AICF_GreetingAudio`: одноразовое клиентское аудио |
| `Vehicles/`, `State/Vehicles/` | Transport trips, fleet, task handoff и cleanup; `AICF_FIAGarrisonService` — initial гарнизон FIA и его AIWorld квота по сложности header; `AICF_VehicleSpawner` выбирает дорожную или стационарную площадку; `AICF_FIAGarrisonPatrol` — локальные маршруты и запрет движения стационарной обороны; `AICF_FIAGarrisonRecovery` — перенос существующей техники с player/identity/geometry fences; `AICF_FIAGarrisonCrew` — выбранные места, exact-seat возврат и разрешённый header десант |
| `Economy/` | `AICF_EconomySystem`: ticket/supply transaction; `AICF_RecruitmentSupplyForecast`: read-only прогноз |
| `Construction/` | Планирование платных layouts и отдельный worker lifecycle каждой базы |
| `Content/`, `Integration/` | Content profiles и узкие stock API adapters |
| `Loadouts/` | Catalog/draft/recipe/binding; `AICF_EquipmentService` — живое пополнение, `Inventory` — native перенос, `Applicator` — выдача/rollback |
| `Respawn/` | `AICF_SquadRespawnPolicy`: передача живого бойца игроку |
| `UI/` | Карта, командование, снабжение, экипировка и клиентские intents |
| `Victory/` | `AICF_VictorySystem`, `AICF_VictoryDiagnostics`: завершение матча и причины отказа |
| `Diagnostics/` | События `[AICF][STAGE...]`, читаемые анализаторами |

`AICF_MatchController` остаётся composition root. Новое поведение помещай в
профильный класс; контроллер связывает его с lifecycle, не забирая доменную логику.

Адаптивный commander читает stock radio graph, native capture/threat flags и
живых бойцов через GroupRuntime. Учёт сил — действующие назначения собственной
фракции, включая player intent и suspended vehicle intent; успешные назначения
немедленно видны следующему slot. Policy не владеет waypoint или transport.
Planner повторно сверяет slot/group entity, generation, graph/assignment revision,
authority и допустимость цели. Веса, fallback и исключения gates:
[ADAPTIVE_COMMANDER.md](ADAPTIVE_COMMANDER.md).

## Authority и identity

Стратегия, spawn/delete, деньги, тикеты, ownership и match result принадлежат
authoritative server/master. Клиент передаёт намерение; RPC заново проверяет
игрока, faction, numeric slot, availability, лимиты и цель. Постоянные данные
JIP передаются через `RplProp`; `Replication.BumpMe()` вызывается только при
фактическом изменении authority state.

Идентичность силы — `faction + numeric slot`. Role-local имя и incarnation
группы могут измениться. Callback/retry обязан сохранять и сверять применимые
group/spawn/trip generation, assignment/intent/graph revision, request token и
immutable entity identity. Не обходи readiness после `RequestSpawn()`.
Origin `SCR_AIGroup` не означает положение бойцов: используй `AICF_GroupRuntime`.
Каждому subscription и повторному `CallLater` соответствует явный cleanup.

`SCR_AIProcessFailedMovementResult` в `AICF_AINodeLifecycle` передаёт ошибку
пешего движения владельцу slot/recruitment, сохраняя waypoint, generation и
assignment fences. `handler=-1` неоднозначен: registry может содержать свободную
машину с таким же ещё не назначенным subgroup. Native result не передаёт
identity/generation исходной операции: текущий Move и уже пеший roster
не доказывают, что результат относится к новому приказу. При зарегистрированном
sentinel classifier безусловно отказывает до чтения waypoint текущего action,
сообщения failure в slot/recruitment и `OnMoveFailed`. Этот случай остаётся
на native границе; разрешённой привязки к новой infantry operation нет.
Для `-1` без registry entry каждый живой member должен принадлежать этой группе,
иметь `CompartmentAccessComponent`, находиться снаружи и не входить/выходить.
Пустой roster, неизвестный handler и физическая неопределённость сохраняют отказ.
Обработка не меняет transport trip или fleet.
После `OnMoveFailed` повторно сверяются failure identity и executed action;
синхронно заменённый action не завершается.

Native defend allocation может получить удалённый вход после смены приказа.
`AICF_AINodeLifecycle` возвращает FAIL только для invalid input управляемой
server infantry, сохраняя native delegate валидных/чужих nodes. Он не подменяет
input текущим waypoint и не меняет новый приказ. Контракт и native BT oracle:
[DEFEND_ALLOCATION.md](DEFEND_ALLOCATION.md).

Дополнительные FIA crew/desant controllers передают материализацию native
`ProximityDriven` через `AICF_FIAProximityLifecycle`. Logical seats и потери
переживают смену member entity; vehicle/lease не заменяются. Safety gate native
despawn остаётся у CleanupManager, initial content readiness — у profile.
Подробности: [FIA_PROXIMITY.md](FIA_PROXIMITY.md).

## Границы vehicle domain

| Владелец | Обязанность |
|---|---|
| `AICF_VehicleCoordinator` | Facade и scheduling |
| `AICF_TransportTripController` | Переходы trip и cross-domain effects |
| Acquisition/boarding/transit/dismount flow | Выполнение фазы и возврат `AICF_TripOutcome` |
| `AICF_VehicleSpawner` | Site/reservation и entity spawn, только из acquisition в его фазе |
| `AICF_FactionFleet` | Lease, generation и faction cap |
| `AICF_VehicleTaskHandoff` | Vehicle waypoint boundary |
| `AICF_VehicleCleanupManager` | Clearance, release, delete; может жить после окончания trip |

Flow не меняет фазу и не вызывает соседний flow, controller или cleanup.
Waypoint сначала снимается с группы, затем удаляется как replicated entity.
Перед vehicle delete повторяются identity, occupancy и clearance checks;
неопределённость означает fail-closed.

## Соседние контракты

`AICF_ManagedObservers` регистрирует серверные native observers для живых READY
US/USSR slots. Существующий scheduler раз в секунду читает текущую incarnation
и живого бойца через GroupRuntime; потеря допустимости и Stop удаляют собственные
ключи. Native ambient system, group lifecycle и spawn queue сохраняют владение
spawn/despawn, budget и eliminated/respawn правилами. Сервис не меняет дополнительные
FIA гарнизоны. Контракт и fixture: [AMBIENT_PATROLS.md](AMBIENT_PATROLS.md).

`AICF_FIAGarrison` владеет постоянным ban на recovery конкретной vehicle
identity/generation. `AICF_FIAGarrisonIntegrity` — его наблюдатель здоровья
исходной машины и исходных members: весь hit-zone hierarchy, immutable entity
и generation, health/damage invokers с явным Remove при Retire/Stop.
Плановый desant transfer сохраняет исходную identity. Ранение essential crew,
гибель любого исходного member, повреждение машины и неизвестное состояние
закрывают recovery на весь lifecycle. Ordinary patrol/CanDrive независимы от ban.
Recovery и forced exact-seat return читают один допуск, повторяя его после
синхронных waypoint/queue callbacks перед физической мутацией; MatchController
не получает новой ответственности. Подробности — [GAMEPLAY.md](GAMEPLAY.md).

Начальный FIA состав принадлежит `AICF_FIAGarrisonPlanner`, допустимость
усиленной точки и готовность полного графа — `AICF_FIAStrongpointPolicy`, доступность
prefab — catalog/profile. Единая проверка связности и HQ exits используется во
всех успешных путях radio setup и повторно в Hard planner.
Integration `AICF_FIAStrongpointRadioSetup` готовит реальный radio graph до
MatchController. Immutable snapshot реплицируется на stock base. Подробности:
[FIA_GARRISONS.md](FIA_GARRISONS.md).

Узкий stock adapter `AICF_AIMagazineSwitchGuard` отклоняет magazine-switch task
на compartment без weapon manager до вызова vanilla/WCS цепочки. RHS integration
владеет PMC readiness перед обоими входами посадки (`Board` и `BoardGarrison`);
перенос экипировки и vehicle lifecycle остаются в своих доменах.

`AICF_AntiTankPolicy` меняет дальность выбранного РПГ, допуск наведения и
локальные combat move requests только у пешего AI против бронетехники.
Владение infantry waypoint остаётся у OrderPlanner, movement — у штатного
combat move state. Параметры и gates: [ANTI_TANK_TACTICS.md](ANTI_TANK_TACTICS.md).

RecruitmentService закрепляет одну автоматическую заявку на фракцию до завершения
или потери допустимости, независимо от базы; ручной приоритет остаётся исключением.
Следующая заявка выбирается по ATTACK/DEFEND/RESERVE и FIFO внутри ранга.
Шаг и первая пауза — 5000 ms. Прогноз моделирует общую временную шкалу и отдельные
склады баз без изменения supplies. Ожидающая плана заявка
не владеет waypoint. Supplies не резервируются, остаток после спроса более ранних
заявок и оплата проверяются перед каждым transfer:
[точная модель и evidence](RECRUITMENT_SUPPLY_PLANNING.md).

Перед созданием бойца `AICF_InfantrySpawnPlacement` проверяет navmesh,
terrain, свободный объём и локальную связность. `AICF_InfantryVehicleClearance`
дополняет trace свежими world bounds ближайшей техники: query radius 16 м,
горизонтальный отступ 2 м. Высота не исключает опасность падающей машины.
Прерванный query или некорректные bounds отклоняют candidate; child и сама
vehicle проверяются через одну identity. Проверка только читает geometry;
техника и уже существующие бойцы не перемещаются.

Construction planner оплачивает unfinished layout один раз; отдельный worker
базы подходит к проекту и выполняет stock progress. Recovery не создаёт второй
worker и не повторяет оплату. Копии runtime fixture в production не остаются.
Construction и builders могут работать после server/master barrier
`INITIAL_SPAWN_QUEUE_SETTLED`: обе стороны имеют ровно десять slots, каждый
в `READY`, `WAITING` либо `SPAWNING` платного replacement. `EMPTY`, `DESTROYED`,
null, неверный numeric slot и незаконченный initial `SPAWNING` запрещены.
Это завершение initial materialization queue, а не готовность живой армии:
ранняя потеря остаётся обычным replacement с barracks, supply и ticket gates.
Иначе потерянный slot ждёт barracks, а строительство ждёт потерянный slot.
Controller публикует barrier один раз со всеми двадцатью состояниями в
`us_states`/`ussr_states`; запись `slot:state:generation:replacement` сохраняет
numeric identity. `ROSTER_READY` по-прежнему требует двадцать одновременно
`READY` slots; остальные subsystem gates не используют новый barrier.

Social service не владеет waypoint или recruitment: он читает initial muster/
ожидание приказа и подписывается на `GetOnPresentationInvalidated` slot.
Переход приказа немедленно освобождает анимацию; Stop снимает подписки.
Backend identity ledger не зависит от controlled entity, audio не входит в JIP
snapshot. Ресурсы, допуск и ограничения: [SOCIAL_BEHAVIOR.md](SOCIAL_BEHAVIOR.md).

Loadout templates назначаются slot/memberIndex: новые бойцы получают binding
при создании, живые — при допустимом посещении действующего арсенала.
`AICF_EquipmentService` владеет потребностью, поиском источников и ограниченным
визитом; `AICF_OrderPlanner` остаётся единственным владельцем waypoint.
Slot хранит применённую revision каждой позиции и weak references visit/transaction.
Трофейный перенос использует существующие entities; экономика не меняется.
Async native receipts не меняют roster/приказы; identity-safe восстановление
инвентаря может продолжаться после отмены визита. Контракт и настройки:
[EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md).
Personal recipes хранятся отдельно по backend identity,
faction и content profile. Server validation повторяется перед выдачей;
ошибки требуют проверяемого rollback. Подробнее о поведении: [GAMEPLAY.md](GAMEPLAY.md).

Не переименовывай events или `key=value` поля без синхронного обновления
анализаторов и этих заметок. Static, compile и runtime — отдельные gates:
[TESTING.md](TESTING.md).
