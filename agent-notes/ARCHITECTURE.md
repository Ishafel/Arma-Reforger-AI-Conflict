# Архитектура и навигация по исходникам

`AICF_PlayerMobHoldPolicy.HasCurrentInternalTask` отделяет текущую пешую задачу
игрока внутри MOB от требования выхода за границу MOB. Audit root учитывает
только read-only proof exact intent/waypoint/native task/пешего состава; правило
не объявляет прибытие или разрешённый idle. Исходные all-member arrival,
meaningful-task и movement recovery остаются самостоятельными обязанностями.
Границы и known leader-only ограничение — [PLAYER_MOB_TASK.md](PLAYER_MOB_TASK.md).

Ручной preset теперь имеет retained `TACTICAL_DEPLOYMENT` для TRANSPORT/COMBAT:
он сохраняет ту же оплачиваемую машину между пешими эпизодами. BehaviorFlow
владеет физическими операциями и возвращает typed outcomes; Controller остаётся
владельцем переходов и Handoff/OrderPlanner остаётся владельцем waypoint.
Отдельная weak native policy сдерживает конкурирующую auto посадку/замену crew.
Новые разрешения короткой видимой коррекции и сроки — [VEHICLE_BEHAVIOR.md](VEHICLE_BEHAVIOR.md).
Описания одноразового COMPLETE ниже сохраняются для automatic trips и старого evidence.

`AICF_TransportCombatHold` — read-only proof текущего собственного
`SCR_AIVehicleCombatActivity` для ручного vehicle preset в TRANSIT. Проверяются
current/owned native action, vehicle manager, lease/entity/Rpl/generations,
assignment и исходный живой AI roster без чужих occupants. В API1.8.0.13
точный выбранный combat activity может остаться EVALUATED: его native
`OnActionSelected` не вызывает super. Просто queued EVALUATED не подходит.
Transit возвращает typed WAIT до crew/mobility recovery, но после health и
absolute deadline gates. Уже начатые CrewRecovery/NativeGate не скрываются.
Movement progress timestamps, deadlines, actions и recovery counters не меняются.
Registry экспортирует отдельный `HasNativeCombatTask()` при актуальной graph
revision; MatchController учитывает его как meaningful task/bounded lifecycle.
События `TRANSPORT_NATIVE_COMBAT_STARTED/WAIT/ENDED` сохраняют реальные clocks.

`AICF_VehicleCrewRoleOwnership` не меняет экипаж: это узкий current preset/actor/
role/lease proof для прежнего3s ожидания собственного физического перехода.
Transit проверяет срок/owner/FAILED уже существующего crew token до такого WAIT,
не расходует новые попытки и не освобождает token при passive Pilot transition.
Чужой или неидентифицированный occupant по-прежнему не вытесняется.

Native повторная посадка ручного preset проходит через scoped actor reaction
`AICF_VehiclePresetNativeSeatPolicy`: точный seat передаётся существующему native
GetIn, без подмены action/приоритетов/таймеров. Weak bindings регистрирует Transit
после ValidateContext и снимает в AbortOwnedActions. Helpers не владеют trip phase,
экономикой или cleanup автомобиля; reservation далее обслуживает native action.
Все current/stale/foreign/actor/lease guards повторяются перед dispatch/rollback.
Raw path trace — только наблюдение, не новое основание meaningful progress.

`AICF_TransportNativeCrewEntry` отдельно наблюдает собственный текущий native
GetIn в пустое exact reserved Pilot/Turret место. Reservation сама по себе не
достаточна: требуется actual current/executed RUNNING action, binding и повторный
actor/seat/lease/assignment proof. Первый срок ограничен прежним boarding timeout
и trip absolute deadline; повторное наблюдение его не продлевает. Оба role records
проверяются до passive WAIT. Observer не отменяет action, не расходует recovery
attempt и не пишет progress clocks; timeout/stale context остаются typed fallback.

Acquisition перед собственным staging waypoint приостанавливает infantry order
через `AICF_VehicleTaskHandoff`. Coordinator сохраняет suspension от SITE_PLANNED
до SPAWN_COMMIT, чтобы root не восстановил PLAYER hold поверх approach.
Spawn/readiness и прежние сроки не обходятся; terminal restoration остаётся у Handoff.

`AICF_VehicleNativeRouteProgress` отделён от raw path trace. Он дважды читает полный
текущий путь exact settled AI Pilot/AICar, проверяет endpoint и единственную
близкую XZ-проекцию с высотой. Только физически правдоподобное продвижение вдоль
того же пути может дать прежний порог progress. Смена пути даёт baseline;
bounded история сохраняет frontier при A→B→A. Transit единолично пишет progress
clock и pending recovery evidence после положительного proof. Absolute deadline,
direct-distance и motion checks сохранены. Причина отказа видна на прежней cadence
`VEHICLE_MOTION`; отсутствие credit не объявляется доказательством остановки машины.

`AICF_VehicleHealthTrace.Capture` читает native usability, movement damage,
engine/gearbox/wheels efficiency, fire, root damage flag, bounded hitzone hierarchy
и pose/velocity/terrain. GroupTrace вызывает его на прежней cadence, Transit —
в failure snapshot; helper не меняет actions, physics или clocks. Повторный
entity/component Rpl/manager/default hitzone/physics fence отбрасывает сменившийся
контекст. Поля `health_*` — наблюдения с `health_cause_proven=0`, не attribution.
Отключённый root damage flag не означает защиту всех дочерних managers.

Диагностика групп: `AICF_GroupTrace` у VehicleCoordinator читает authority state
и реальные позиции живых бойцов, без удержания engine entities в cursor.
`AICF_NativeVehicleTrace` наблюдает штатные callbacks с однократным super,
не меняет actions/приказы. Полные транспортные fixtures и их изоляция от боя —
[VEHICLE_SCENARIOS.md](VEHICLE_SCENARIOS.md).
`AICF_TransportNativeGate` — read-only observation у transit flow для штатного
OpenGate водителя/стрелка ручного preset. MovementState владеет observation,
flow возвращает typed WAIT/fallback, controller по-прежнему владеет переходами.
Наблюдение не продлевает trip и не выдаёт движение за реальный progress.

Gate observer проверяет все назначенные места, включая Cargo. Требуются
единственная current native OpenGate chain, exact seat return, наблюдавшийся
физический выход, завершённый native return и последующее реальное движение.
Одна и та же interaction сохраняет первый60s срок в bounded value-only history
MovementState; смена бойца/повторное наблюдение его не продлевает. Общий trip
deadline остаётся верхней границей. Неоднозначные chains отклоняются.
Native return завершение сохраняется exact action callback `m_OnActionCompleted`:
штатный `OnActionDeselected` может сразу заменить COMPLETED на EVALUATED.
Receipt не заменяет физический выход, exact settled seat и движение. Gate.Stop
снимает listener; replacement/Reset/Abort используют этот boundary, включая
локальный cleanup до invalid-trip return в Controller.AbortForStop.

`AICF_TransportNativeAbandon` отдельно распознаёт текущий штатный group GetOut
для exact trip vehicle после отказа от машины. Это stateless read-only proof,
а не разрешение возвращать водителя в покидаемый транспорт. Transit возвращает
typed fallback до crew recovery; существующий controller владеет высадкой,
восстановлением пешего приказа и cleanup. Конкурирующие async owners сохраняются.

До stock failed-movement abandonment `AICF_TransitMoveFailure` может принять
только exact current vehicle handler ручного preset и здоровый полностью
settled original AI roster с оставшимся mobility budget. Native callback доставляется
ровно один раз; этот receipt не меняет actions/waypoints/physics. Обычный Transit
Tick повторно проверяет immutable identity/revisions/health, расходует прежнюю
попытку и запускает существующий bounded recovery. STUCK допускает прежнюю
короткую коррекцию4..15м при captured/current speed<=1м/с, иначе route rebuild;
UNKNOWN/UNREACHABLE — route rebuild. Исчерпанный бюджет,
чужой handler/occupant, повреждённая машина и stale context не подавляют stock.
Абсолютные сроки и clocks не продлеваются; Reset/ExitTransit снимают receipt.

`AICF_OverturnRecovery` хранит только bounded3s observation lease здорового
перевёрнутого manual vehicle с исходным settled AI roster. Captured assignment,
lease и route не меняются на retry; eligibility повторяет native subgroup/Move,
health/authority/roster fences. Read-only asset inspection может отложить только
такой overturn, а фактическая коррекция остаётся в Transit Tick и старом mobility
budget. Перед transform повторяются scope/window/скорость≤1 и прежние physical
fences; state/Stop очищает observation. Helper не владеет phase, health или crew.

Native vehicle record сохраняется при `TACTICAL_DEPLOYMENT → TRANSIT`: уже
сидящие бойцы не создадут повторный `CompartmentEntered` после удаления record.
`AICF_VehicleNativeSubgroup` восстанавливает только bookkeeping exact leased
vehicle через stock callback при unassigned negative record; positive handler
должен совпадать с actual Pilot/car handler. Перед каждым вызовом повторяются
current assignment/generations/component Rpl, original authoritative AI/settled
seat и отсутствие другого native movement owner. Default/positive mismatch,
human/foreign actor и неопределённая identity дают отказ. Физические recovery и
решения о замене экипажа остаются у существующих flows.

Во время manual acquisition stock может снять завершённый групповой staging
waypoint, пока отдельные бойцы ещё исполняют собственный подход.
`AICF_VehicleStagingTaskProof` передаёт в read-only slot view только actual
selected/executed `RUNNING` staging actor task: token хранит immutable assignment
snapshot, goal, group/trip/request/entity identity; повторяются slot revisions,
base revision, reservation и исходные deadlines. Unowned, suspended, cancelled,
terminal action и stale context не считаются meaningful task. Это проверка
выполняемой работы; phase/timeout exceptions или продления сроков она не вводит.

Завершённый `COMPLETE` или `FALLBACK` transport intent с восстановленным пешим
приказом хранится в retained terminal trip.
Coordinator не принимает handoff assignment/waypoint rebuild за новый заказ
машины: сравниваются group/generation, intent, destination и preset revision.
Cleanup gates остаются первыми; explicit admission, новый приказ, preset или
группа проходят обычную проверку заново.

Transit проверяет native terminal movement damage до crew/combat/gate observers:
`GetMovementDamage() >= 1`, native `VehicleCanMove == false` и повторный exact
damage manager. Это сохраняет штатный actor-level выход из повреждённой машины
даже при текущем group Move. Временная неготовность здоровой машины не получает
этот veto. Fallback, handoff и cleanup остаются у прежних владельцев; budgets
и deadlines не меняются.

DismountState раздельно ограничивает logical force-exit и physical-only relocation:
до3 batches на каждую операцию, суммарно6 на phase. Existing normal→terminal
fallback может дать второй такой phase, то есть общий ceiling12. Сроки и прежние
human/foreign/identity/navmesh/position permissions не расширены. Physical budget
доступен только после свежего полного logical egress и при подтверждённом hull
blocker; aggregate `force_attempts` остаётся telemetry, не общим лимитом3.
Реальный найм и длительное наблюдение первого managed slot —
[VEHICLE_SOAK.md](VEHICLE_SOAK.md). `AICF_VehicleCleanupTrace` хранит исходные
actor identities только как подписи диагностики retained job; история не даёт
разрешений на recovery/delete и не участвует в occupancy/player gates.

Проект расширяет stock Conflict скриптами Enforce. Единственный gameplay resource —
тонкий inherited `MissionHeader` входа AI Conflict; runtime selection выбирает
штатную миссию и применяет её настройки.
Собственных world/base/layout ресурсов нет. North ограничивает штатные базы
whitelist, оставляя ландшафт целым. UI редактора экипировки использует временный
in-memory world для preview; он не является миром кампании.

Личный и AI editor используют одну разметку Inspector; side effects остаются
раздельными: personal active принадлежит AICF_PersonalLoadoutStore, private
named variants — AICF_PersonalPresetLibrary с AICF_LoadoutStore и отдельным
identity prefix файлов. Library Load меняет только owner draft view; spawn по-прежнему
читает active с повторной server validation. Контракт: [LOADOUT_PRESETS.md](LOADOUT_PRESETS.md).

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
| `AIConflictEveron` | `A4B2E62595F645A4` | Runtime whitelist North и policy выхода из изолированного radio-компонента |
| `AIConflictArlandRHS` | `9F88011DA22B471C` | RHS content profile и compatibility adapters |
| `AIConflictEveronRHS` | `FA9FDCCA428A43BA` | Объединение Everon policy и RHS profile, callsign pool adapter |
| `AIConflictArlandWCSRHS` | `A1CF260928100001` | WCS экипировка, арсенал и техника поверх RHS |
| `AIConflictEveronWCSRHS` | `A1CF261006100001` | Полный и северный Everon с общим WCS + RHS profile |

Vanilla dependency — `58D0FB3206B6F859`. Точный граф зависимостей принадлежит
`addon.gproj` каждого addon и `tools/config/AICF_Scenarios.json`; launcher читает
этот каталог через `AICF_Scenarios.ps1`. GUID не меняются.
Core и stock integrations не должны получать обязательные RHS/WCS dependencies.

Co-op за US/USSR выбирается runtime selection для каждого из девяти
сочетаний карты и контента. Генератор задаёт native target и dependency graph;
mode/difficulty передаются отдельно, North whitelist задаёт Everon integration. Общая immutable
`AICF_MatchRules` задаёт 0 боевых slots у людей и 10 у противника, ticket defeat по стороне;
faction state и helpers сохраняются. Владение roster остаётся у FactionState,
условиями победы — у VictorySystem. Контракт: [COOP_SCENARIO.md](COOP_SCENARIO.md).
Co-op фильтрует меню и authority faction requests, сохраняя native IsPlayable
для HQ/opponent setup. Auto construction policy применяется в ConstructionPlanner;
служба рабочих остаётся отдельным владельцем достройки player layouts.
`AICF_CoopPlayerGroups` сохраняет штатного владельца player groups: включает
их меню и выдвижение в лидеры, а после faction callbacks выполняет native
join/create для игрока без группы. Roster AICF и права других лидеров не меняет.
`AICF_BarracksRecruitment` после native `OnEntityRequested` разрешает найм членов
оплаченной player-side группы из казармы, включая Guard prefabs. Фракция группы
берётся через `SCR_AIGroup.GetFaction`, а не через отсутствующий у неё affiliation
component. Authority повторно сверяет текущего игрока и его faction; разрешение
применяется к существующим и асинхронно добавленным members той же группы.
Native лидерство, лимит и ownership RPC сохраняют свои проверки.
Это общая Core механика для STANDARD и Co-op; ограничения player faction
по-прежнему задаёт MatchRules. Контракт: [GAMEPLAY.md](GAMEPLAY.md#найм-купленных-в-казарме-бойцов).

`AICF_SupportStationArea` восстанавливает local position визуального child
по offset непосредственного parent support station до native Init/GenerateAreaMesh.
Сама станция, дальность и её gameplay state сохраняются. Вложенные и ошибочные
иерархии остаются под native validation. Причина и проверка:
[SUPPORT_STATION_AREA.md](SUPPORT_STATION_AREA.md).

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

`AICF_ScenarioRegistry`/`AICF_ScenarioSetup` владеют локальным frontend выбором.
Concrete targets и dependency graphs генерируются в Arland integration из общего
JSON каталога. `AICF_ScenarioLaunch` проверяет native permissions входного item,
enabled state внешнего content graph, зависимости, download/platform/forced-CLI
gates и передаёт точный граф native
Play/Host transition. Атомарный выбор передаётся через GameSessionStorage в native target.
AICF_RuntimeScenario применяет mode/difficulty/rank/save/environment перед
native weather/base initialization и восстанавливает cached header при cleanup.
North whitelist принадлежит Everon integration. Server публикует variant,
mode/difficulty/ready через существующий game mode RplProp; JIP зеркалит только
локальную presentation и rank caches. Старые 81 ресурса и GUID удалены,
обратной совместимости с ними и StockCoopUSSR alias нет.
При смене scripts graph в preparation Listen HostTransition владеет переходом
через menu, повторным native Host с engine config и native mission; проверяет
итоговый полный selection, потому что разные режимы/North используют общий ID.
Arland регистрирует единственный собственный setup; stock записи сохраняются.
Подробности — [SCENARIO_SETUP.md](SCENARIO_SETUP.md).
Core разрешает subtype чтением ресурса по ID; nullable catalog `GetHeader()` не
определяет доступность входа. `AICF_WorkshopTagMetadata` — узкий адаптер чтения
штатных UI tag maps, избегающий standalone protected constructor SCR_FilterCategory.

Spawn deadline managed slot измеряется WorldTimestamp текущего мира с момента
BeginInitialSpawn/BeginReplacementSpawn, через GetSpawnSimulationAgeMs. Это
сохраняет 30 секунд активной симуляции при Welcome/SP pause; существующие
System.GetTickCount getters остаются wall-clock диагностикой. Смена generation
начинает новый timestamp, readiness/native spawn queue не обходятся.

`AICF_InstigatorContext` — узкий compatibility boundary transient death context.
OTHER с известным character передаётся штатной AI/PLAYER классификации;
источник без управляющего получает OTHER_DEATH с сохранённым Instigator и
исходными entities. Damage/score/ownership остаются у native владельцев.
Контракт: [INSTIGATOR_CONTEXT.md](INSTIGATOR_CONTEXT.md).

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

Ручная настройка техники принадлежит stable `AICF_GroupSlot`: отдельная
vehicle revision и сериализованный рецепт не меняют loadout bindings, роль или
размер состава. Trip захватывает независимую копию, revision, generation и
immutable identity каждого назначенного member. Подбор exact prefab остаётся
в acquisition, точная native посадка — в boarding, списание — в EconomySystem.
Каталог читает compartments в private world; глобальный native manager ID не
сохраняется. Контракт и ограничения проверок: [VEHICLE_PRESETS.md](VEHICLE_PRESETS.md).

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

Для recruitment `STUCK`/`UNREACHABLE` с exact `DEFAULT_HANDLER_ID` и единственным
AI участником `AICF_RecruitmentMoveFailure` сохраняет immutable receipt до
native callback. Service проверяет живую регистрацию exact order и graph,
slot — свой exact order pointer; только прежняя repair attempt может списать
этот receipt и назначить безопасный перенос до8m. Node не ремонтирует waypoint,
не покупает бойцов и не переносит actor. Все cross-tick identity/authority/age,
физические guards и consume-before-Teleport находятся в профильном helper.
Подробности и ограничения: [RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md).

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
Native `UNKNOWN` на exact current acquisition approach обрабатывается через
identity-bound receipt: node → resolver → coordinator/controller → acquisition.
Только обычный Tick flow возвращает outcome, controller выполняет переход и
cleanup; чужие/stale/неоднозначные callbacks остаются fail-closed. Подробности и
отдельный fault-injection gate — [VEHICLE_APPROACH_FAILURE.md](VEHICLE_APPROACH_FAILURE.md).
Перед vehicle delete повторяются identity, occupancy и clearance checks;
неопределённость означает fail-closed.

Ручной vehicle preset до первого маршрута может ждать удалённую цель в TRANSIT.
TransitFlow возвращает WAIT `PRESET_WAITING_FOR_DESTINATION`; только controller
через TransportTrip приостанавливает/возобновляет trip и movement deadlines.
Во время ожидания сохраняются identity, health и exact crew guards. После
первого staged route действуют обычные arrival/dismount и ограниченный бюджет.
`AICF_VehiclePresetWaitPolicy` проецирует только доказанное текущее ожидание
в `AICF_VehicleSlotView.IsPresetDestinationWait()`: current assignment/intent/base
revisions, lease/entity/RplId и exact settled assigned occupants. MatchController
читает эту проекцию как meaningful task и bounded idle reason; сам не создаёт
маршрут и не меняет transport deadlines. BASE сравнивается по identity базы,
POSITION — также по точным координатам; resolved snapshot не сравнивается с
raw owner origin базы. `AICF_PlayerMobHoldPolicy` отдельно распознаёт физически
достигнутое пешее PLAYER POSITION удержание у MOB с текущим bound DefendWaypoint.

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

Ручной vehicle catalog объединяет enabled колёсные entries своей faction и native
CIV, а factionless entries требует маркировать `FACTION_CIV`. Категории принадлежат
native metadata; UI фильтрует presentation, server заново разрешает prefab из того
же whitelist. `AICF_ContentProfile.GetVehiclePresetCostOverride` — vendor boundary:
RHS/WCS возвращает прежний quote, Core использует native SpawnerData/CAMPAIGN
fallback без мутации budget объектов. Faction в recipe — получатель, даже для
civilian prefab. Выдача, purchase и boarding остаются у прежних доменных owners.
Контракт: [VEHICLE_PRESETS.md](VEHICLE_PRESETS.md).

Явно подтверждённое сокращение vehicle crew принадлежит `AICF_VehicleCrewReturn`:
одноразовый `AICF_VehicleCrewPlan` проверяет identities/revisions, GroupSlot
компактно remaps живой roster и kits, auxiliary groups возвращают неназначенных.
Их waypoint остаётся у OrderPlanner; физическое прибытие и повторная проверка
authority/possession/compartments обязательны перед despawn. Возвращаемые остаются
в managed agent cap. До product confirmation нет side effects. Boarding сохраняет
полный ALL_OR_FALLBACK для итогового состава, а не выполняет частичную перевозку.

Loadout templates назначаются slot/memberIndex: новые бойцы получают binding
при создании, живые — при допустимом посещении действующего арсенала.
`AICF_EquipmentService` владеет потребностью, поиском источников и ограниченным
визитом; `AICF_OrderPlanner` остаётся единственным владельцем waypoint.
Slot хранит применённую revision каждой позиции и weak references visit/transaction.
Трофейный перенос использует существующие entities; экономика не меняется.
Async native receipts не меняют roster/приказы; identity-safe восстановление
инвентаря может продолжаться после отмены визита. Контракт и настройки:
[EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md).
Library Save/Delete независимы от назначения: LoadoutStore хранит два поколения
и tombstones; LoadoutService готовит member/squad plans, LoadoutBatch проверяет
все revisions до commit, GroupSlot копирует binding каждой позиции.
AI member preset доступен любой роли той же faction/profile. Character в recipe
остаётся исходным prefab сборки, а не ограничением получателя. LoadoutService
сверяет его с authoritative recruit sources; AICF_LoadoutTransfer восстанавливает
серверный inventory snapshot на private draft каждого другого recipient source
и требует полного signature readback. Назначение одного комплекта всем десяти
позициям выбранного slot использует тот же atomic batch; roster не меняется.
Admin targetFaction разрешается повторно на server, UI не получает authority.
Протокол и legacy migration — [LOADOUT_PRESETS.md](LOADOUT_PRESETS.md).
Personal recipes хранятся отдельно по backend identity,
faction и content profile. Server validation повторяется перед выдачей;
ошибки требуют проверяемого rollback. Подробнее о поведении: [GAMEPLAY.md](GAMEPLAY.md).

Не переименовывай events или `key=value` поля без синхронного обновления
анализаторов и этих заметок. Static, compile и runtime — отдельные gates:
[TESTING.md](TESTING.md).

Native garbage не владеет удалением cap-held машины AICF. Fleet после binding устанавливает lease-owned `AICF_VehicleGarbagePermission`; weak registry позволяет callbacks проверить exact physical identity и owner. Pending/failed cleanup сохраняет protection до safety-approved transfer. `RelinquishReleasedVehicleReference` снимает registry до очистки reference; после собственного veto native tracking восстанавливается через обычный Insert. При owner destructor проверяется captured entity/Rpl/master без уничтоженных weak owners; другой current owner запрещает stale restore. Детали: [VEHICLE_BEHAVIOR.md](VEHICLE_BEHAVIOR.md#native-garbage-и-удерживаемая-машина).
