# Технический контекст для агента

Этот слой хранится в Git и обновляется вместе с реализацией. Правила работы —
в [AGENTS.md](../AGENTS.md), описание для игрока — в [README.md](../README.md),
номерные патчноуты — в [releases/](../releases/). В `docs/` документацию не ведём;
локальные старые отчёты не нужны для начала работы с новым checkout.

| Задача | Что читать |
|---|---|
| Изменить окно пресетов отряда, прокрутку мест, авторассадку и вращение техники | [VEHICLE_PRESETS.md](VEHICLE_PRESETS.md), проверки нового UI в [TESTING.md](TESTING.md) |
| Продолжить #78 после передачи draft PR и выключения компьютера | [ISSUE78_STATUS.md](ISSUE78_STATUS.md), итоговый комментарий в issue и точные manifests |
| Разделить native stock и AICF quotas, проверить full basket/catalog и позднюю активацию техники | [AI_QUOTAS.md](AI_QUOTAS.md), текущая реализация/admission и отдельные обязательные native pressure gates |
| Воспроизвести human Catalog/FIFO, full-basket отказ и покупку пустой машины при полной character квоте | [AI_QUOTA_CATALOG.md](AI_QUOTA_CATALOG.md), public separate-role runner, native proof и строгие полные log audits |
| Воспроизвести заполнение stock-квоты и обычный оплаченный набор AICF | [AI_QUOTA_STOCK_PRESSURE.md](AI_QUOTA_STOCK_PRESSURE.md), public Prepare/Run, scope и исходные failures |
| Отличить нехватку средств до покупки машины от нарушения lifecycle | [VEHICLE_ECONOMY_DECLINE.md](VEHICLE_ECONOMY_DECLINE.md), typed pre-asset quote/receipt, own empty lease и terminal foot fallback |
| Разобрать taskless ожидание квоты перед созданием машины | [VEHICLE_ACQUISITION_HOLD.md](VEHICLE_ACQUISITION_HOLD.md), typed staged receipt, реальный owned Defend и cleanup до продолжения acquisition |
| Освободить исходного DEAD водителя, которого native исключил из сохранившейся группы | [CAPTURED_CASUALTY_CLEANUP.md](CAPTURED_CASUALTY_CLEANUP.md), admission capability, прежние physical guards и opt-in actual-decision observer без повторного принятия решения |
| Разобрать неполный native spawn и очередь замены отрядов | [GROUP_SPAWN.md](GROUP_SPAWN.md), GroupRuntime/InfantrySpawnPlacement и controlled replacement stress |
| Разобрать отказ короткой коррекции при native STUCK на подходе к казарме | [RECRUITMENT_DECISIONS.md](RECRUITMENT_DECISIONS.md), isolated decision observer без изменения recovery |
| Найти источник долгого Census UNKNOWN при посадке | [AI_QUOTAS.md](AI_QUOTAS.md#разбор-census-unknown-без-изменения-допуска), отдельный opt-in decision observer и строгие старые gates |
| Разобрать Census UNKNOWN после посадки экипажа с inventory | [VEHICLE_CENSUS_TOPOLOGY.md](VEHICLE_CENSUS_TOPOLOGY.md), structural compartment topology, character subtree boundary и неизменные occupant guards |
| Разобрать конфликт приказа внутри MOB с требованием выхода с базы | [PLAYER_MOB_TASK.md](PLAYER_MOB_TASK.md), отдельные egress/arrival обязанности; [MOB_DECISIONS.md](MOB_DECISIONS.md), opt-in native decision observer |
| Исправить persistence leaks при direct-connect без backend identity | [PERSISTENCE_IDENTITY.md](PERSISTENCE_IDENTITY.md), native SpawnLogic identity boundary и separate client/JIP gate |
| Разобрать native out-of-world-bounds у оружия и отделить main-world drop от private Equipment model | [TEST_FIXTURES.md](TEST_FIXTURES.md#read-only-loose-weapon--private-equipment-model-observer), fixture-only LooseWeaponObserver и отдельный controlled native-drop baseline |
| Защитить известное выпавшее оружие AI от runaway fall после пересечения terrain | [DROPPED_WEAPON_FALL_SAFETY.md](DROPPED_WEAPON_FALL_SAFETY.md), exact inventory-removal permission, human/subtree fences и отдельное подтверждение удаления |
| Изменить поведение TRANSPORT/COMBAT, N возврата, замену потерь и разрешённую короткую принудительную посадку | [VEHICLE_BEHAVIOR.md](VEHICLE_BEHAVIOR.md), BehaviorFlow/Policy/ShortRecovery и retained TACTICAL_DEPLOYMENT |
| Разобрать ожидание входа в машину и ранний пеший подход без расширения door cap/deadline | [VEHICLE_BOARDING_QUEUE.md](VEHICLE_BOARDING_QUEUE.md), PresetQueuedApproach/BoardingState и callback-safe QueuePresetSeat |
| Разобрать native UNKNOWN во время подхода к площадке техники | [VEHICLE_APPROACH_FAILURE.md](VEHICLE_APPROACH_FAILURE.md); exact phase-owned failure receipt и отдельный controlled native probe |
| Проверить bounded route repair после настоящего OpenGate/возврата экипажа | [TEST_FIXTURES.md](TEST_FIXTURES.md#controlled-native-opengate-expiry), NativeGateExpiryProbe с единственной controlled expiry и отдельным native motion proof |
| Передать игроку визуальную проверку окна техники и подтверждения состава | [VEHICLE_VISUAL_CHECK.md](VEHICLE_VISUAL_CHECK.md); terminal probes не заменяют этот NOT RUN |
| Проверить2→4→6 машин на сторону и native combat hold/release/resume | [VEHICLE_MULTISQUAD.md](VEHICLE_MULTISQUAD.md), MultiSquad/TransportCombatResume fixtures и их terminal runners/log auditors |
| Разобрать повреждённую ходовую, bounded fallback и повторную покупку на прежнем intent | [VEHICLE_SCENARIOS.md](VEHICLE_SCENARIOS.md), `Invoke-AICFTransportDamageFallback.ps1`, movement damage/finished intent contracts |
| Проверить штатный найм5 бойцов в первом отряде, UAZ/Humvee и30 минут реального боя | [VEHICLE_SOAK.md](VEHICLE_SOAK.md), VehicleSoakProbe, Invoke/Watch-AICFVehicleSoak и SoakLog |
| Прогнать полные поездки/повторную посадку, разобрать действия и координаты бойцов | [VEHICLE_SCENARIOS.md](VEHICLE_SCENARIOS.md), GroupTrace, VehicleScenarioProbe и `Invoke-AICFVehicleScenarios.ps1` |
| Изменить категории своего/CIV колёсного каталога, preview, numeric рассадку, подтверждение сокращения состава и библиотеку техники (issue #78) | [VEHICLE_PRESETS.md](VEHICLE_PRESETS.md), `AICF_VehiclePreset`/`AICF_VehiclePresetCatalog`, `AICF_VehiclePresetService`/`AICF_VehicleCrewReturn`/`AICF_VehiclePresetStore`, content price override, acquisition/boarding/trip owners |
| Изменить «Инспектор», прокрутку, ввод окон, названия назначений, постоянную библиотеку, Save/Apply/Delete, перенос комплекта между ролями, массовое назначение или настройку противника (issue #66) | [LOADOUT_PRESETS.md](LOADOUT_PRESETS.md), `AICF_LoadoutService`/`AICF_LoadoutStore`/`AICF_LoadoutTransfer`, `AICF_LoadoutPreset`/`AICF_LoadoutBatch`, `AICF_LoadoutEditor`/`AICF_LoadoutRpc`, `AICF_LoadoutLibraryView`/`AICF_LoadoutSummary`, `tools/AICF_LoadoutLibrary.ps1` |
| Запустить локальные Play/Host через обычный клиент или разобрать native shutdown crash | [DEVELOPMENT.md](DEVELOPMENT.md#локальные-playhost), [TESTING.md](TESTING.md), `tools/Start-AICFRuntime.ps1 -Role Frontend`; успешный offline/compile не заменяет stopped-log runtime gate |
| Изменить внутриигровой брифинг на карте | [MISSION_BRIEFING.md](MISSION_BRIEFING.md), `UI/AICF_MissionBriefing.c`, MissionBriefingProbe в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Разобрать resource/navmesh/Hierarchy ошибки и границу внешнего baseline | [EXTERNAL_RESOURCE_BASELINE.md](EXTERNAL_RESOURCE_BASELINE.md), [SHUTDOWN_LIFECYCLE.md](SHUTDOWN_LIFECYCLE.md), [TESTING.md](TESTING.md) |
| Разобрать native падение проверки занятого места при отсутствующем Physics | [NATIVE_SLOT_PHYSICS_GUARD.md](NATIVE_SLOT_PHYSICS_GUARD.md), `AICF_NativeSlotPhysicsGuard`, fail-closed occupancy и отдельный Catalog native gate |
| Исправить damage RPC при смене обуви у private model или при shutdown | [SHUTDOWN_LIFECYCLE.md](SHUTDOWN_LIFECYCLE.md), `Integration/AICF_FeetDamageLifecycle.c`, FeetDamageProbe в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить универсальное меню карт/контента, первый вход Workshop, подсказку «Как настроить», время/погоду, зависимости и native launch, видимость сценариев или ошибки SCR_FilterCategory | [SCENARIO_SETUP.md](SCENARIO_SETUP.md), `AICF_ScenarioBootstrap`/`AICF_ScenarioHostTransition`, `AICF_ScenarioEnvironment`/`AICF_ScenarioEnvironmentHandler`, `AICF_ScenarioRegistry`, `AICF_ScenarioLaunch`, `AICF_ScenarioSetupSelection`, `AICF_ScenarioSetupScreen`, `AICF_ScenarioSetupEntry`, `AICF_ArlandScenarioCatalog`, `AICF_WorkshopTagMetadata` |
| Исправить классификацию смерти INSTIGATOR_OTHER | [INSTIGATOR_CONTEXT.md](INSTIGATOR_CONTEXT.md), `AICF_InstigatorContext`, pinned `SCR_InstigatorContextData` и native Instigator API |
| Добавить карту/мод или сторону Co-op; исправить лимит имён Workshop, roster, выбор фракции, группы игроков, строительство и поражение | [COOP_SCENARIO.md](COOP_SCENARIO.md), `tools/config/AICF_Scenarios.json`, `AICF_MatchRules`, `AICF_CoopFactionAccess`, `AICF_CoopFactionMenu`, `AICF_CoopPlayerGroups`, `AICF_ConstructionPlanner` |
| Изменить найм купленных в казарме бойцов во всех сценариях | [GAMEPLAY.md](GAMEPLAY.md#найм-купленных-в-казарме-бойцов), `AICF_BarracksRecruitment`, barracks fixture в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Исправить смещение визуальной зоны ремонтной станции | [SUPPORT_STATION_AREA.md](SUPPORT_STATION_AREA.md), `AICF_SupportStationArea`, support area fixture в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Исправить WCS M2CG, повторные EquipmentPlan и удаление private models (issue #67) | [EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md), [SHUTDOWN_LIFECYCLE.md](SHUTDOWN_LIFECYCLE.md), [GAMEPLAY.md](GAMEPLAY.md), EquipmentFailureProbe в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Исправить окно `Defend Waypoint not provided to the node!` после смены приказа | [DEFEND_ALLOCATION.md](DEFEND_ALLOCATION.md), `AICF_AINodeLifecycle`, native allocation node в Script Diff 1.8.0.13 |
| Исправить shutdown leaks и проверить lifetime private equipment worlds (issue #63) | [SHUTDOWN_LIFECYCLE.md](SHUTDOWN_LIFECYCLE.md), [EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md), `AICF_EquipmentDraft`, bootstrap OnGameEnd |
| Изменить proximity lifecycle дополнительных сил FIA (issue #59) | [FIA_PROXIMITY.md](FIA_PROXIMITY.md), [FIA_GARRISONS.md](FIA_GARRISONS.md), [AMBIENT_PATROLS.md](AMBIENT_PATROLS.md) |
| Изменить активацию штатной охраны FIA при подходе ИИ (issue #54) | [AMBIENT_PATROLS.md](AMBIENT_PATROLS.md), `Forces/AICF_ManagedObservers.c`, native ambient spawn/lifecycle в API 1.8.0.13 |
| Выполнить критерии issue и привязать PR | [AGENTS.md: работа по issue](../AGENTS.md#работа-по-issue-и-привязка-pr), [TESTING.md](TESTING.md) |
| Найти состав последнего оформленного релиза и ограничения | [0.2.2](../releases/0.2.2.md), [TESTING.md](TESTING.md) |
| Удалить старые сценарии, изменить runtime policy/миграцию | [SCENARIO_SETUP.md](SCENARIO_SETUP.md), `AICF_RuntimeScenario`, [TESTING.md](TESTING.md) |
| Подготовить и проверить каталог семи Workshop пакетов без загрузки на backend | [SCENARIO_SETUP.md](SCENARIO_SETUP.md#метаданные-публикации-workshop), [Prepare-AICFWorkshopCatalog.ps1](../tools/Prepare-AICFWorkshopCatalog.ps1), [Prepare-AICFWorkshopBundle.ps1](../tools/Prepare-AICFWorkshopBundle.ps1), [DEVELOPMENT.md](DEVELOPMENT.md#подготовка-окончательного-списка-workshop) |
| Найти домен, состояние и границы побочных эффектов | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Исправить ранние потери при spawn и зависание строительства до готовности roster | [ARCHITECTURE.md](ARCHITECTURE.md), `Forces/AICF_InfantrySpawnPlacement`, `Forces/AICF_InfantryVehicleClearance`, `State/AICF_FactionState`, [матрица запуска](TESTING.md#подготовка-0129-полная-матрица-запуска--2026-10-15) |
| Проверить игровую механику и defaults | [GAMEPLAY.md](GAMEPLAY.md), затем указанные исходники |
| Изменить цены штатного заказа пехоты и техники RHS/WCS | [MENU_PRICES.md](MENU_PRICES.md), [полная таблица](MENU_PRICE_TABLE.csv), RHS/WCS menu price policy и integration |
| Изменить случайную начальную погоду без изменения времени | [GAMEPLAY.md](GAMEPLAY.md#начальная-погода), `AICF_RuntimeScenario` и native time handler, [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить сложности, случайный состав и усиленную точку FIA | [FIA_GARRISONS.md](FIA_GARRISONS.md), [GAMEPLAY.md](GAMEPLAY.md#сложность-гарнизонов-fia), [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить запрет recovery FIA после урона/потерь (issue #48) | [GAMEPLAY.md](GAMEPLAY.md), `AICF_FIAGarrison`/`AICF_FIAGarrisonIntegrity`, `AICF_FIAGarrisonRecovery`/`AICF_FIAGarrisonCrew`, [TESTING.md](TESTING.md), DamageProbe в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить ПТ-состав и Carl Gustaf US | [GAMEPLAY.md](GAMEPLAY.md#сложность-гарнизонов-fia), `AICF_Difficulty`, Core/RHS content profiles, `AICF_WCSInfantryKit`/`AICF_WCSInfantryEquipment`; probes в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить разнообразие вооружения FIA в RHS/WCS (issue #52) | [FIA_ARMAMENT.md](FIA_ARMAMENT.md), `AICF_RHSPMCArmament`, `AICF_RHSPMCEquipment`, RHS/WCS content profiles |
| Изменить дальность и тактику применения РПГ против бронетехники | [ANTI_TANK_TACTICS.md](ANTI_TANK_TACTICS.md), `Orders/AICF_AntiTankPolicy.c` |
| Изменить личную библиотеку и общий Inspector или оптимизировать каталог экипировки | [LOADOUT_PRESETS.md](LOADOUT_PRESETS.md), [LOADOUT_PERFORMANCE.md](LOADOUT_PERFORMANCE.md), `AICF_PersonalPresetLibrary`/`AICF_PersonalLoadout`, `Loadouts/AICF_LoadoutCatalog.c`, `UI/AICF_LoadoutEditor.c` |
| Изменить пополнение экипировки, live revisions и сбор с тел (issue #6) | [EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md), `Loadouts/AICF_EquipmentService.c`, `AICF_EquipmentInventory`/`AICF_EquipmentApplicator`, [TESTING.md](TESTING.md) |
| Изменить общий список содержимого разгрузки и навигацию подсумков | [LOADOUT_CONTENTS.md](LOADOUT_CONTENTS.md), `UI/AICF_LoadoutNavigation.c` |
| Исправить пропавшие магазины RHS в исходном WCS kit и проверить preview restore (issue #61) | [LOADOUT_CONTENTS.md](LOADOUT_CONTENTS.md), `AICF_LoadoutContentsInventoryProbe`, `Loadouts/AICF_LoadoutInventory.c` |
| Изменить адаптивные цели, потребность в силах и стратегические gates | [ADAPTIVE_COMMANDER.md](ADAPTIVE_COMMANDER.md), `Command/AICF_AdaptiveCommanderPolicy.c`, `AICF_AdaptiveScore.c` |
| Изменить категории основного/второго оружия или пропущенный быстрый слот второй винтовки | [LOADOUT_WEAPON_CATEGORIES.md](LOADOUT_WEAPON_CATEGORIES.md), `UI/AICF_LoadoutWeaponCategories.c`, `Integration/AICF_WeaponQuickslots.c` и content profiles |
| Найти утилиту, её статус и запустить offline набор | [TOOLS.md](TOOLS.md) |
| Исправить orphan trip после native гибели группы, проверить DEAD cleanup и типизированную retirement projection | [VEHICLE_RETIREMENT.md](VEHICLE_RETIREMENT.md), [VEHICLE_SOAK.md](VEHICLE_SOAK.md) |
| Выбрать runtime probe и проверить ограничения | [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Проверить identity failed movement и неоднозначный handler | [ARCHITECTURE.md](ARCHITECTURE.md#authority-и-identity), `Forces/AICF_AINodeLifecycle.c`, [TESTING.md](TESTING.md), InfantryMoveHandler probe |
| Подготовить API reference, Workbench, локальный/удалённый runtime и Diag/retail | [DEVELOPMENT.md](DEVELOPMENT.md) |
| Выбрать проверки и сравнить baseline | [TESTING.md](TESTING.md) |
| Выпустить PR и разобрать FAIL, включая baseline | [AGENTS.md: проверка и передача](../AGENTS.md#проверка-и-передача-результата), [TESTING.md: правила выпуска PR](TESTING.md#правила-выпуска-pr) |
| Проверить владельца commander state и стабильный выбор казармы (issue #44) | [TOOLS.md](TOOLS.md#единый-offline-запуск), [RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md), [TESTING.md](TESTING.md) |
| Изменить последовательную очередь фракции, шаг 5 секунд и прогноз пополнения | [RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md) |
| Изменить начальные emotes, салют, cooldown, звук и безопасное обновление дыма сигареты | [SOCIAL_BEHAVIOR.md](SOCIAL_BEHAVIOR.md) |
| Изменить WCS content profile или проверить зависимости и повреждение обновлений | [Arland WCS + RHS](../AIConflictArlandWCSRHS/README.md#зависимости), [проверка 15.10.2026](TESTING.md#зависимости-и-вылеты-после-обновления-wcs--2026-10-15) |
| Изменить полный или северный WCS Everon | [Everon WCS + RHS](../AIConflictEveronWCSRHS/README.md) |

Перед правкой прочитай соседние классы выбранного домена и сними новый baseline.
Заметки помогают найти контракт; точные сигнатуры и defaults проверяй в коде и
закреплённом Script Diff. Датированные результаты относятся только к указанным
исходникам и конфигурации, а не автоматически к текущей ветке.

После правки обновляй существующую тему, команды и ограничения проверок в том же
коммите. Добавляй новый файл только для самостоятельного домена и включай его
в этот указатель. Не превращай заметки в журнал каждой попытки: полные логи,
manifests и временные stage хранятся вне Git в каталоге evidence своей задачи.
