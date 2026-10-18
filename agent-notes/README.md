# Технический контекст для агента

Этот слой хранится в Git и обновляется вместе с реализацией. Правила работы —
в [AGENTS.md](../AGENTS.md), описание для игрока — в [README.md](../README.md),
номерные патчноуты — в [releases/](../releases/). В `docs/` документацию не ведём;
локальные старые отчёты не нужны для начала работы с новым checkout.

| Задача | Что читать |
|---|---|
| Изменить ванильный Arland Co-op СССР, пустой roster и условия поражения | [COOP_SCENARIO.md](COOP_SCENARIO.md), `AICF_MatchRules`, `AICF_FactionState`, `AICF_VictorySystem` |
| Исправить окно `Defend Waypoint not provided to the node!` после смены приказа | [DEFEND_ALLOCATION.md](DEFEND_ALLOCATION.md), `AICF_AINodeLifecycle`, native allocation node в Script Diff 1.8.0.13 |
| Исправить shutdown leaks и проверить lifetime private equipment worlds (issue #63) | [SHUTDOWN_LIFECYCLE.md](SHUTDOWN_LIFECYCLE.md), [EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md), `AICF_EquipmentDraft`, bootstrap OnGameEnd |
| Изменить proximity lifecycle дополнительных сил FIA (issue #59) | [FIA_PROXIMITY.md](FIA_PROXIMITY.md), [FIA_GARRISONS.md](FIA_GARRISONS.md), [AMBIENT_PATROLS.md](AMBIENT_PATROLS.md) |
| Изменить активацию штатной охраны FIA при подходе ИИ (issue #54) | [AMBIENT_PATROLS.md](AMBIENT_PATROLS.md), `Forces/AICF_ManagedObservers.c`, native ambient spawn/lifecycle в API 1.8.0.13 |
| Выполнить критерии issue и привязать PR | [AGENTS.md: работа по issue](../AGENTS.md#работа-по-issue-и-привязка-pr), [TESTING.md](TESTING.md) |
| Найти состав последнего оформленного релиза и ограничения | [0.1.29](../releases/0.1.29.md), [TESTING.md](TESTING.md) |
| Найти домен, состояние и границы побочных эффектов | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Исправить ранние потери при spawn и зависание строительства до готовности roster | [ARCHITECTURE.md](ARCHITECTURE.md), `Forces/AICF_InfantrySpawnPlacement`, `Forces/AICF_InfantryVehicleClearance`, `State/AICF_FactionState`, [матрица запуска](TESTING.md#подготовка-0129-полная-матрица-запуска--2026-10-15) |
| Проверить игровую механику и defaults | [GAMEPLAY.md](GAMEPLAY.md), затем указанные исходники |
| Изменить цены штатного заказа пехоты и техники RHS/WCS | [MENU_PRICES.md](MENU_PRICES.md), [полная таблица](MENU_PRICE_TABLE.csv), RHS/WCS menu price policy и integration |
| Изменить случайную начальную погоду без изменения времени | [GAMEPLAY.md](GAMEPLAY.md#начальная-погода), четыре базовых stock/RHS `MissionHeader`, [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить сложности, случайный состав и усиленную точку FIA | [FIA_GARRISONS.md](FIA_GARRISONS.md), [GAMEPLAY.md](GAMEPLAY.md#сложность-гарнизонов-fia), [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить запрет recovery FIA после урона/потерь (issue #48) | [GAMEPLAY.md](GAMEPLAY.md), `AICF_FIAGarrison`/`AICF_FIAGarrisonIntegrity`, `AICF_FIAGarrisonRecovery`/`AICF_FIAGarrisonCrew`, [TESTING.md](TESTING.md), DamageProbe в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить ПТ-состав и Carl Gustaf US | [GAMEPLAY.md](GAMEPLAY.md#сложность-гарнизонов-fia), `AICF_Difficulty`, Core/RHS content profiles, `AICF_WCSInfantryKit`/`AICF_WCSInfantryEquipment`; probes в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить разнообразие вооружения FIA в RHS/WCS (issue #52) | [FIA_ARMAMENT.md](FIA_ARMAMENT.md), `AICF_RHSPMCArmament`, `AICF_RHSPMCEquipment`, RHS/WCS content profiles |
| Изменить дальность и тактику применения РПГ против бронетехники | [ANTI_TANK_TACTICS.md](ANTI_TANK_TACTICS.md), `Orders/AICF_AntiTankPolicy.c` |
| Оптимизировать каталог и редактор пресетов экипировки | [LOADOUT_PERFORMANCE.md](LOADOUT_PERFORMANCE.md), `Loadouts/AICF_LoadoutCatalog.c`, `UI/AICF_LoadoutEditor.c` |
| Изменить пополнение экипировки, live revisions и сбор с тел (issue #6) | [EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md), `Loadouts/AICF_EquipmentService.c`, `AICF_EquipmentInventory`/`AICF_EquipmentApplicator`, [TESTING.md](TESTING.md) |
| Изменить общий список содержимого разгрузки и навигацию подсумков | [LOADOUT_CONTENTS.md](LOADOUT_CONTENTS.md), `UI/AICF_LoadoutNavigation.c` |
| Исправить пропавшие магазины RHS в исходном WCS kit и проверить preview restore (issue #61) | [LOADOUT_CONTENTS.md](LOADOUT_CONTENTS.md), `AICF_LoadoutContentsInventoryProbe`, `Loadouts/AICF_LoadoutInventory.c` |
| Изменить адаптивные цели, потребность в силах и стратегические gates | [ADAPTIVE_COMMANDER.md](ADAPTIVE_COMMANDER.md), `Command/AICF_AdaptiveCommanderPolicy.c`, `AICF_AdaptiveScore.c` |
| Изменить категории основного/второго оружия | [LOADOUT_WEAPON_CATEGORIES.md](LOADOUT_WEAPON_CATEGORIES.md), `UI/AICF_LoadoutWeaponCategories.c` и content profiles |
| Найти утилиту, её статус и запустить offline набор | [TOOLS.md](TOOLS.md) |
| Выбрать runtime probe и проверить ограничения | [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Проверить identity failed movement и неоднозначный handler | [ARCHITECTURE.md](ARCHITECTURE.md#authority-и-identity), `Forces/AICF_AINodeLifecycle.c`, [TESTING.md](TESTING.md), InfantryMoveHandler probe |
| Подготовить API reference, Workbench, локальный/удалённый runtime и Diag/retail | [DEVELOPMENT.md](DEVELOPMENT.md) |
| Выбрать проверки и сравнить baseline | [TESTING.md](TESTING.md) |
| Выпустить PR и разобрать FAIL, включая baseline | [AGENTS.md: проверка и передача](../AGENTS.md#проверка-и-передача-результата), [TESTING.md: правила выпуска PR](TESTING.md#правила-выпуска-pr) |
| Проверить владельца commander state и стабильный выбор казармы (issue #44) | [TOOLS.md](TOOLS.md#единый-offline-запуск), [RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md), [TESTING.md](TESTING.md) |
| Изменить последовательную очередь фракции, шаг 5 секунд и прогноз пополнения | [RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md) |
| Изменить начальные emotes, салют, общий cooldown игрока и звук | [SOCIAL_BEHAVIOR.md](SOCIAL_BEHAVIOR.md) |
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
