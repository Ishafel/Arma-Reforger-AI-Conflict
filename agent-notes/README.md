# Технический контекст для агента

Этот слой хранится в Git и обновляется вместе с реализацией. Правила работы —
в [AGENTS.md](../AGENTS.md), описание для игрока — в [README.md](../README.md),
номерные патчноуты — в [releases/](../releases/). В `docs/` документацию не ведём;
локальные старые отчёты не нужны для начала работы с новым checkout.

| Задача | Что читать |
|---|---|
| Найти состав последнего оформленного релиза и ограничения | [0.1.26](../releases/0.1.26.md), [TESTING.md](TESTING.md) |
| Найти домен, состояние и границы побочных эффектов | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Проверить игровую механику и defaults | [GAMEPLAY.md](GAMEPLAY.md), затем указанные исходники |
| Изменить сложности, случайный состав и усиленную точку FIA | [FIA_GARRISONS.md](FIA_GARRISONS.md), [GAMEPLAY.md](GAMEPLAY.md#сложность-гарнизонов-fia), [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить ПТ-состав отрядов по сложности | [GAMEPLAY.md](GAMEPLAY.md#сложность-гарнизонов-fia), `AICF_Difficulty`, Core/RHS content profiles; probes в [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Изменить дальность и тактику применения РПГ против бронетехники | [ANTI_TANK_TACTICS.md](ANTI_TANK_TACTICS.md), `Orders/AICF_AntiTankPolicy.c` |
| Оптимизировать каталог и редактор пресетов экипировки | [LOADOUT_PERFORMANCE.md](LOADOUT_PERFORMANCE.md), `Loadouts/AICF_LoadoutCatalog.c`, `UI/AICF_LoadoutEditor.c` |
| Изменить общий список содержимого разгрузки и навигацию подсумков | [LOADOUT_CONTENTS.md](LOADOUT_CONTENTS.md), `UI/AICF_LoadoutNavigation.c` |
| Изменить категории основного/второго оружия | [LOADOUT_WEAPON_CATEGORIES.md](LOADOUT_WEAPON_CATEGORIES.md), `UI/AICF_LoadoutWeaponCategories.c` и content profiles |
| Найти утилиту, её статус и запустить offline набор | [TOOLS.md](TOOLS.md) |
| Выбрать runtime probe и проверить ограничения | [TEST_FIXTURES.md](TEST_FIXTURES.md) |
| Подготовить API reference, Workbench, локальный/удалённый runtime и Diag/retail | [DEVELOPMENT.md](DEVELOPMENT.md) |
| Выбрать проверки и сравнить baseline | [TESTING.md](TESTING.md) |
| Изменить прогноз снабжения и конкуренцию за казармы | [RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md) |
| Изменить WCS content profile или зависимости | [Arland WCS + RHS](../AIConflictArlandWCSRHS/README.md) |
| Изменить полный или северный WCS Everon | [Everon WCS + RHS](../AIConflictEveronWCSRHS/README.md) |

Перед правкой прочитай соседние классы выбранного домена и сними новый baseline.
Заметки помогают найти контракт; точные сигнатуры и defaults проверяй в коде и
закреплённом Script Diff. Датированные результаты относятся только к указанным
исходникам и конфигурации, а не автоматически к текущей ветке.

После правки обновляй существующую тему, команды и ограничения проверок в том же
коммите. Добавляй новый файл только для самостоятельного домена и включай его
в этот указатель. Не превращай заметки в журнал каждой попытки: полные логи,
manifests и временные stage хранятся вне Git в каталоге evidence своей задачи.
