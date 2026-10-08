# Runtime fixture: каталог и применение

Все отслеживаемые probes перенесены в `tests/fixtures/` без изменения содержимого.
Статус при реорганизации: **DIAGNOSTIC / runtime NOT RUN**. Таблица описывает
назначение, но не обещает актуальный compile или прохождение сценария. Старое
evidence смотри в профильных заметках; новый запуск требует собственных логов.

Перед применением прочитай весь файл, его `modded class`, CLI guards и связанные
probes. Не копируй каталог целиком: модификации могут конфликтовать, использовать
одинаковые CLI flags или завершать игру. Отсутствие CLI флага в таблице не означает,
что fixture безопасна без него: некоторые hooks действуют с момента загрузки.

Порядок: отдельная копия нужного addon graph в `.codex-runtime/<задача>/source`,
только нужные probes в соответствующем `Scripts/Game`, terminal Workbench compile
для stage, затем canonical `tools/Start-AICFRuntime.ps1 -RepositoryRoot <stage>`.
Server и client используют согласованный stage. Для RHS/WCS hooks нужен именно
их integration addon. `PresetRespawnProbe` требует добавления в указанный файл,
а не произвольного копирования отдельного `.c`. Generated databases и evidence
не коммитятся. После остановки сохрани полные логи и убери свою source copy.

Параметры ниже извлечены из `GetCLIParam`: это имена, **не готовый argv**.
Значения/зависимости определяются исходником: не всем параметрам подходит `1`,
а динамические суффиксы (`Position`, `Angles`, `Work`) требуют номера case.
Для recruitment есть [точные сценарии и границы](RECRUITMENT_SUPPLY_PLANNING.md),
для WCS — [integration guide](../AIConflictArlandWCSRHS/README.md).

| Fixture | Назначение и ограничения | CLI параметры в исходнике |
|---|---|---|
| [AICF_AICombatInputProbe.c](../tests/fixtures/AICF_AICombatInputProbe.c) | Пустые weapon inputs; временно снимает selected-weapon reference | `aicfCombatInputProbe` |
| [AICF_ApproachRecoveryProbe.c](../tests/fixtures/AICF_ApproachRecoveryProbe.c) | Полные отряды, approach recovery и bounded shutdown | `aicfRecoveryProbe` |
| [AICF_BarracksCombatProbe.c](../tests/fixtures/AICF_BarracksCombatProbe.c) | Подготовка казарм/supplies и размещение лидеров; не доказывает подход | `aicfBarracksCombatProbe` |
| [AICF_BaseBuilderCompletionProbe.c](../tests/fixtures/AICF_BaseBuilderCompletionProbe.c) | Отказ completion на 45 секунд с последующей native clearance | `aicfBuilderCompletionProbe` |
| [AICF_BaseBuilderRuntimeProbe.c](../tests/fixtures/AICF_BaseBuilderRuntimeProbe.c) | Worker lifecycle; подготовка layouts обходит оплату/player placement | `aicfBuilderProbe` |
| [AICF_BuilderMovementProbe.c](../tests/fixtures/AICF_BuilderMovementProbe.c) | Инъекция BT result в группу строителя; проверка fencing и возврата | `aicfBuilderMovementProbe` |
| [AICF_ConstructionAllBasesProbe.c](../tests/fixtures/AICF_ConstructionAllBasesProbe.c) | Все точки одной стороне и полный supply pool; production стройка | `aicfAllBasesProbe`, `aicfAllBasesProbeMs` |
| [AICF_ConstructionBudgetProbe.c](../tests/fixtures/AICF_ConstructionBudgetProbe.c) | Общая квота без geometry calls; дополнительные параметры баз/radius | `aicfConstructionBudgetProbe`, `aicfConstructionProbeRadius`, `aicfConstructionProbeUSBase`, `aicfConstructionProbeUSSRBase` |
| [AICF_ConstructionCatalogProbe.c](../tests/fixtures/AICF_ConstructionCatalogProbe.c) | Registry, labels и budgets; не полная геометрия | `aicfConstructionCatalog` |
| [AICF_ConstructionManualProbe.c](../tests/fixtures/AICF_ConstructionManualProbe.c) | Диагностика ручного проекта без отдельного CLI guard | Нет собственного GetCLIParam; читай hooks |
| [AICF_ConstructionReplayProbe.c](../tests/fixtures/AICF_ConstructionReplayProbe.c) | Повтор geometry guards на переданных transforms, без gameplay spawn | `aicfConstructionReplay`, `aicfConstructionReplayAngles`, `aicfConstructionReplayPosition`, `aicfConstructionReplayProvider`, `aicfConstructionReplayValidOnly`, `aicfConstructionReplayWork` |
| [AICF_ConstructionRuntimeProbe.c](../tests/fixtures/AICF_ConstructionRuntimeProbe.c) | Подготовка supplies/матрицы; production поиск, оплата и завершение | `aicfConstructionClientProbe`, `aicfConstructionProbe`, `aicfConstructionProbeFault`, `aicfConstructionProbeMatrix`, `aicfConstructionProbeMatrixPhaseMs`, `aicfConstructionProbeMatrixType`, `aicfConstructionProbeMs`, `aicfConstructionProbeRefill`, `aicfConstructionProbeRepeatType`, `aicfConstructionProbeSupplies`, `aicfConstructionProbeTrace`, `aicfConstructionProbeType` |
| [AICF_ConstructionSearchProbe.c](../tests/fixtures/AICF_ConstructionSearchProbe.c) | Trace и fault injection поиска; применяется с runtime preparation | `aicfConstructionProbeMoveArmories`, `aicfConstructionProbeRemoveArmories`, `aicfConstructionSearchFault`, `aicfConstructionSearchTrace` |
| [AICF_DefendArrivalProbe.c](../tests/fixtures/AICF_DefendArrivalProbe.c) | Late move result при US system hold; commander USSR | `aicfDefendArrivalProbe` |
| [AICF_DefendWaypointInputProbe.c](../tests/fixtures/AICF_DefendWaypointInputProbe.c) | Реальные group/waypoint, смена приказа через planner | `aicfDefendInputProbe` |
| [AICF_EndgameProbe.c](../tests/fixtures/AICF_EndgameProbe.c) | Контролируемые territory/presence/hold time; настоящее завершение матча | `aicfEndgameProbe` |
| [AICF_EveronNorthProbe.c](../tests/fixtures/AICF_EveronNorthProbe.c) | Северный Everon: 7 активных баз, 2 HQ, 5 механизмов захвата, связность radio graph; Кермован исключён | `aicfNorthProbe` |
| [AICF_ForcedSmallBarracksProbe.c](../tests/fixtures/AICF_ForcedSmallBarracksProbe.c) | Отказ обычного коридора поиска, forced-small fallback | `aicfForcedSmallProbe` |
| [AICF_GroupCallsignProbe.c](../tests/fixtures/AICF_GroupCallsignProbe.c) | Три поколения групп в двух factions; group spawn и callsign uniqueness | `aicfCallsignProbe` |
| [AICF_InfantryAdvanceProbe.c](../tests/fixtures/AICF_InfantryAdvanceProbe.c) | Production infantry advance и подтверждение перемещения | `aicfInfantryAdvanceProbe` |
| [AICF_InfantryApproachProbe.c](../tests/fixtures/AICF_InfantryApproachProbe.c) | Approach navigation и bounded recovery | `aicfApproachProbe` |
| [AICF_InfantryRecruitmentRuntimeProbe.c](../tests/fixtures/AICF_InfantryRecruitmentRuntimeProbe.c) | Recruitment подготовка, fault/muster режимы; основа связанных probes | `aicfRecoveryEpisodeProbe`, `aicfRecruitHiddenContinue`, `aicfRecruitProbe`, `aicfRecruitProbeFaults`, `aicfRecruitProbeMuster` |
| [AICF_InfantrySpawnPlacementProbe.c](../tests/fixtures/AICF_InfantrySpawnPlacementProbe.c) | Native spawn placement и границы inputs | `aicfSpawnPlacementProbe` |
| [AICF_LoadoutClientProbe.c](../tests/fixtures/AICF_LoadoutClientProbe.c) | Клиентские loadout preview/validation; визуальный verdict требует пользователя | `aicfLoadoutClientProbe`, `aicfLoadoutClientProbeKeepOpen`, `aicfLoadoutPreviewOnly` |
| [AICF_LoadoutClothingProbe.c](../tests/fixtures/AICF_LoadoutClothingProbe.c) | Клиентские/серверные проверки одежды и inventory; stage обоих peers | `aicfClothingProbe` |
| [AICF_LoadoutMagazineProbe.c](../tests/fixtures/AICF_LoadoutMagazineProbe.c) | Magazines, ammo и runtime inventory | `aicfMagazineProbe` |
| [AICF_LoadoutNavigationProbe.c](../tests/fixtures/AICF_LoadoutNavigationProbe.c) | Loadout navigation regression; одноимённый CLI есть у другого probe | `aicfNavigationProbe` |
| [AICF_LoadoutRuntimeProbe.c](../tests/fixtures/AICF_LoadoutRuntimeProbe.c) | Runtime выдача/проверка loadout; optional recruit/hold | `aicfLoadoutProbe`, `aicfLoadoutProbeHoldSeconds`, `aicfLoadoutRecruitProbe` |
| [AICF_LocalizationProbe.c](../tests/fixtures/AICF_LocalizationProbe.c) | Runtime локализация и форматирование строк | `aicfLocalizationProbe` |
| [AICF_LogisticsFallbackProbe.c](../tests/fixtures/AICF_LogisticsFallbackProbe.c) | Инъекция logistics fallback; используется с logistics runtime setup | `aicfLogisticsProbeFallback` |
| [AICF_LogisticsMapMarkerProbe.c](../tests/fixtures/AICF_LogisticsMapMarkerProbe.c) | Отображаемое состояние logistics markers без ручного visual PASS | `aicfLogisticsMarkerProbe` |
| [AICF_LogisticsRecoveryProbe.c](../tests/fixtures/AICF_LogisticsRecoveryProbe.c) | Runtime recovery logistics; совместно с LogisticsRuntimeProbe | `aicfLogisticsRecoveryProbe` |
| [AICF_LogisticsRuntimeProbe.c](../tests/fixtures/AICF_LogisticsRuntimeProbe.c) | Маршрут/спрос/supplies, доставка и client evidence; подготовка меняет мир | `aicfLogisticsClientProbe`, `aicfLogisticsClientSpawnFaction`, `aicfLogisticsProbe`, `aicfLogisticsProbeDepotAtSource`, `aicfLogisticsProbeDurationMs`, `aicfLogisticsProbeMaintainDemand`, `aicfLogisticsProbeMinSourceCapacity`, `aicfLogisticsProbePeace`, `aicfLogisticsProbePrepare`, `aicfLogisticsProbeRepeatSource`, `aicfLogisticsProbeReturn`, `aicfLogisticsProbeRouteMatrix`, `aicfLogisticsProbeStopAfterUralDelivery`, `aicfLogisticsRecoveryProbe` |
| [AICF_LogisticsSpawnClearanceProbe.c](../tests/fixtures/AICF_LogisticsSpawnClearanceProbe.c) | Clearance, slot selection/reservations вместе с LogisticsRuntimeProbe | Нет собственного GetCLIParam; читай hooks |
| [AICF_LogisticsSpawnParityProbe.c](../tests/fixtures/AICF_LogisticsSpawnParityProbe.c) | Depot parity вместе с LogisticsRuntimeProbe; direct setup обходит часть construction guards | `aicfParityDirectDepotSetup` |
| [AICF_LogisticsVehicleLimitProbe.c](../tests/fixtures/AICF_LogisticsVehicleLimitProbe.c) | Vehicle cap и leases | `aicfVehicleLimitProbe` |
| [AICF_ManualSupplyClientProbe.c](../tests/fixtures/AICF_ManualSupplyClientProbe.c) | Manual supply RPC/конкурентные intents на клиенте | `aicfLogisticsClientSpawnFaction`, `aicfManualSupplyProbe`, `aicfManualSupplyProbeConcurrent`, `aicfManualSupplyProbeStartStep` |
| [AICF_ManualSupplyProbeContracts.c](../tests/fixtures/AICF_ManualSupplyProbeContracts.c) | Enforce контракты manual supply helper, не самостоятельный PowerShell test | `aicfManualSupplyProbeContracts` |
| [AICF_MovementRecoveryProbe.c](../tests/fixtures/AICF_MovementRecoveryProbe.c) | Movement recovery и identity fencing | `aicfMovementRecoveryProbe` |
| [AICF_MusterTurretRuntimeProbe.c](../tests/fixtures/AICF_MusterTurretRuntimeProbe.c) | Muster при занятой turret; runtime подготовка | `aicfMusterTurretProbe` |
| [AICF_NativeMoveFailureProbe.c](../tests/fixtures/AICF_NativeMoveFailureProbe.c) | Сбор native movement failure; перемещение бойцов — подготовка | `aicfNativeMoveProbe` |
| [AICF_NavigationRecoveryProbe.c](../tests/fixtures/AICF_NavigationRecoveryProbe.c) | Координатный snapshot 2026-09-19; не универсальный тест карты | `aicfNavigationProbe` |
| [AICF_NorthFailureProbe.c](../tests/fixtures/AICF_NorthFailureProbe.c) | Прямая подача failed movement inputs; wiring портов проверяется отдельно статикой | `aicfNorthFailureProbe` |
| [AICF_PersonalLoadoutProbe.c](../tests/fixtures/AICF_PersonalLoadoutProbe.c) | Personal catalog/recipe validation; не имитирует UI/player/JIP | `aicfPersonalLoadoutProbe` |
| [AICF_PersonalNetworkProbe.c](../tests/fixtures/AICF_PersonalNetworkProbe.c) | WCS native owner RPC без управления UI | `aicfPersonalNetworkProbe`, `aicfProbeFaction`, `aicfProbePersonal` |
| [AICF_PresetRespawnProbe.c](../tests/fixtures/AICF_PresetRespawnProbe.c) | Добавляется в staging AICF_SquadRespawnUI.c; создаёт recruit и убивает test characters | `aicfPresetProbeFaction`, `aicfPresetRespawnProbe` |
| [AICF_RecoveryEpisodeProbe.c](../tests/fixtures/AICF_RecoveryEpisodeProbe.c) | Episode, hidden continuation и player release; требует чтения связанных recruitment fixtures | `aicfPlayerReleaseProbeOnly`, `aicfRecoveryEpisodeProbe`, `aicfRecruitHiddenContinue`, `aicfRecruitHiddenProbe`, `aicfRecruitMultiMemberProbe` |
| [AICF_RecoverySoakProbe.c](../tests/fixtures/AICF_RecoverySoakProbe.c) | Production gameplay с ограничением длительности и проверкой terminal hold | `aicfRecoverySoakMs` |
| [AICF_RecruitmentLifecycleProbe.c](../tests/fixtures/AICF_RecruitmentLifecycleProbe.c) | Native ownership/death/replacement; перемещает seed к казармам, не проверяет навигацию | `aicfRecruitmentLifecycleProbe` |
| [AICF_RecruitmentMapProbe.c](../tests/fixtures/AICF_RecruitmentMapProbe.c) | Synthetic slot с настоящими faction/base identities | `aicfRecruitmentMapProbe` |
| [AICF_RecruitmentStabilityProbe.c](../tests/fixtures/AICF_RecruitmentStabilityProbe.c) | Вместе с LifecycleProbe; управляемый income snapshot и стабильность replan | `aicfRecruitmentStabilityProbe` |
| [AICF_RecruitmentSupplyProbe.c](../tests/fixtures/AICF_RecruitmentSupplyProbe.c) | Шесть конкурирующих отрядов; stock казармы/transactions, test supplies | `aicfRecruitmentSupplyProbe` |
| [AICF_RemainingErrorsProbe.c](../tests/fixtures/AICF_RemainingErrorsProbe.c) | Диагностика identity/parent/physics; отсутствие CLI не гарантирует бездействие | `aicfRemainingProbeSeconds` |
| [AICF_RHSPatchesProbe.c](../tests/fixtures/AICF_RHSPatchesProbe.c) | RHS роли, inventory, повторная выдача и разнообразие patches | `aicfPatchProbe` |
| [AICF_RHSPMCScenarioProbe.c](../tests/fixtures/AICF_RHSPMCScenarioProbe.c) | RHS AC охрана без исходного primary и FIA стрелок; без отдельного CLI guard | Нет собственного GetCLIParam; читай hooks |
| [AICF_RHSWardrobeShowcase.c](../tests/fixtures/AICF_RHSWardrobeShowcase.c) | RHS экипировка: режим 1 — ручной осмотр, 2 — smoke с остановкой | `aicfWardrobeShowcase` |
| [AICF_SquadCommandsProbe.c](../tests/fixtures/AICF_SquadCommandsProbe.c) | Barracks/движение/покупка/planner; не доказывает client RPC/UI | `aicfSquadProbe` |
| [AICF_SquadRespawnProbe.c](../tests/fixtures/AICF_SquadRespawnProbe.c) | Server/client possession pipeline; подготовка AI и смерть игрока | `aicfSquadRespawnProbe` |
| [AICF_StuckRouteWaitProbe.c](../tests/fixtures/AICF_StuckRouteWaitProbe.c) | Зависшее ожидание stock ALL; planner/watchdog/timer остаются production | `aicfStuckWaitProbe` |
| [AICF_VictoryRespawnProbe.c](../tests/fixtures/AICF_VictoryRespawnProbe.c) | Казарма/смерть/баланс задаются fixture; production replacement/victory | `aicfVictoryRespawnProbe` |
| [AICF_WCSAccessProbe.c](../tests/fixtures/AICF_WCSAccessProbe.c) | Native filters арсенала/heavy provider; общий trigger с WCSInfantry | `aicfWCSInfantryProbe` |
| [AICF_WCSCatalogProbe.c](../tests/fixtures/AICF_WCSCatalogProbe.c) | Merged catalogs WCS без клиента | `aicfWCSInfantryProbe`, `aicfWCSProbe` |
| [AICF_WCSInfantryProbe.c](../tests/fixtures/AICF_WCSInfantryProbe.c) | Два полных roster через production async spawn | `aicfWCSInfantryProbe` |
| [AICF_WCSLoadoutCatalogProbe.c](../tests/fixtures/AICF_WCSLoadoutCatalogProbe.c) | Каталог и источники WCS, не visual UI | `aicfWCSLoadoutCatalogProbe` |
| [AICF_WCSPlayerDeployProbe.c](../tests/fixtures/AICF_WCSPlayerDeployProbe.c) | Fault injection Restore на synchronous case, WCS deploy/rollback | `aicfWCSPlayerDeployProbe` |

## Synthetic logs

[physical-delivery.fixture](../tests/fixtures/logistics/physical-delivery.fixture) и
[client-loaded.fixture](../tests/fixtures/logistics/client-loaded.fixture) — входы
[Test-LogisticsContracts.ps1](../tests/contracts/Test-LogisticsContracts.ps1).
Это искусственные server/client events для положительных и отрицательных
проверок анализатора; они не являются evidence реального runtime.
