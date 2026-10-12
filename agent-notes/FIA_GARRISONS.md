# Случайные гарнизоны FIA

Issue #25. `AICF_FIAGarrisonPlanner` один раз на authoritative server выбирает
состав стартовых objective-баз FIA, исключая HQ. Scheduler, fleet, spawn geometry,
boarding, patrol, recovery и retain cleanup сохраняют владельцев.

| Сложность | Обычная база | Усиленная база |
|---|---|---|
| Easy | Только штатная пехота; машины и дорожные патрули отключены | Нет |
| Medium | Один БТР; лимит десанта 2 или 4 с вероятностью 50/50 | Нет |
| Hard | 2 танка / 2 БТР / танк + БТР, по 1/3 | Одна: 2 БТР + 1 или 2 танка, по 1/2 |

При отсутствии танка FIA каждый выбранный танк заменяется БТР — согласованное
ограничение vanilla/RHS. Усиленная база получает 3/4 БТР. Диагностика:
`FIA_GARRISON_TANK_FALLBACK`. Easy проверяется до catalog/strongpoint/reserve.
Legacy поля header `m_bAICFFIATank`/`m_bAICFFIAPassengers` сохранены для совместимости
ресурсов, но planner использует только difficulty и доступный контент.

## Контент и посадка

`GetFIAGarrisonCandidates` объединяет profile assets и активный FIA VEHICLE catalog.
Core допускает семейство БТР; WCS добавляет классификацию БМП/Т-72. Метка
`VEHICLE_APC` сама по себе недостаточна: разведывательный BRDM исключён.
`AICF_FIAGarrisonCatalog` проверяет точный ResourceName, класс и FIA affiliation,
удаляет дубликаты, сортирует пул. При единственном подходящем образце выбирается он.

Для каждого БТР независимо выбирается существующий FIA GROUP template. Каждый
персонаж из `m_aUnitPrefabSlots` должен иметь доступный ресурс и FIA affiliation.
RHS/WCS сохраняют действующий PMC equipment hook для этих персонажей; чужие
ION-группы не перекрашиваются. Новые prefab-ресурсы не создаются. Готовые списки
из `_NotSpawned` templates допустимы: их controllers не создаются.

Essential seats заполняются экипажем; пассажиры занимают оставшиеся места,
не более лимита и длины шаблона. Длинный шаблон обрезается в исходном порядке,
короткий не размножается. Поэтому Medium означает **до** 2/4, Hard — до вместимости
и длины шаблона. БТР без места для десанта отклоняется. После `CreateGarrison`
действуют прежние async `RequestSpawn`, exact faction roster и boarding gates.
`FIA_GARRISON_ROSTER` сообщает фактическое число пассажиров и экипажа.

RHS/PMC барьер применяется к обоим входам посадки: `Board` у дорожного патруля
и отдельному `BoardGarrison` у гарнизона. Пока хотя бы один боец ожидает
`AICF_ApplyPMCEquipment`, группа не начинает посадку. Иначе native inventory
может отказать в замене оружия уже сидящего бойца. Pending снимается прежним
identity-safe callback; дополнительных таймеров и повторной выдачи нет.

`AICF_AIMagazineSwitchGuard` проверяет владельца текущего compartment перед
штатным `SCR_AISwitchMagazine`. Если у места нет `BaseWeaponManagerComponent`,
задача возвращает FAIL: stock 1.8.0.13 разыменовывает этот manager без проверки.
Пехота вне машины и места с manager проходят через исходную реализацию,
включая цепочку WCS. Guard не меняет оружие, магазины и AI ownership.

## Усиленная точка и настройка графа

`AICF_FIAStrongpointPolicy`: два HQ, минимум два разных соседа, связность с обоими
HQ и отсутствие ребра к любому HQ **в обоих направлениях**. Связность — слабая
компонента направленного radio graph, поскольку stock relay бывает односторонним.
Кандидаты упорядочены по node ID; один выбирает PRNG. Его машины получают первые
spawn slots. Названий и координат карт в Core нет.

На North исходные 2000 м делают все пять целей соседями HQ.
`AICF_FIAStrongpointRadioSetup` вызывается общим integration bootstrap после
radio normalization, до MatchController. Только Hard без допустимого кандидата
сначала соединяет раздельные компоненты ближайшими парами радиостанций с запасом
50 м; прямой bridge HQ–HQ запрещён. Не более N−1 попыток, каждая обязана
увеличить связную компоненту по фактическому графу. Затем при необходимости
пробует сократить дальность одной FIA objective и HQ до расстояния пары минус
50 м. На этом этапе увеличение исключено. Меняются реальные transceiver/default ranges,
вызывается `SCR_RadioCoverageSystem.UpdateAll(true)`, заново строится граф.
Расстояние предлагает настройки, **решение принимает фактический граф**.

Настройка принимается только при наличии допустимого узла, связности всех
узлов и исходящем ребре каждого HQ к objective. При отказе range/default
восстанавливаются и coverage пересчитывается; затем проверяется следующий
кандидат. Если весь подбор неудачен, откатываются также начальные bridges.
Whitelist, HQ ownership и world resources не меняются. Дальнейшее
строительство/захват могут менять radio graph; план при этом не перебрасывается.

Если исправление невозможно, `FIA_STRONGPOINT_RADIO_REJECTED` и
`FIA_STRONGPOINT_UNAVAILABLE` требуют перенастроить HQ/активный whitelist:
нужна связная цепочка HQ — цель — промежуточная FIA — цель — HQ.
`FIA_GARRISON_PLAN_FAILED` останавливает AICF до managed warfare, не выдавая
некорректную конфигурацию Hard за матч с гарантированной точкой.

## PRNG, identity и сеть

Локальный Park–Miller/Schrage получает server seed 1..999999 из native random
sample и времени запуска. Modulo перед AbsInt безопасно нормализует отрицательные
native samples, не схлопывая их все в seed=1. Одинаковые seed и
отсортированные входы дают одинаковые решения; новые матчи могут отличаться.
Глобальный RNG не переинициализируется. `Start` защищён `m_Planner`/stopped guard;
retry, graph rebuild, capture и JIP не выбирают состав повторно.

На каждой stock base `AICF_FIAGarrisonState` хранит immutable `RplProp` string:
версия, seed, difficulty, slot, node, strongpoint, тип/ресурс машины, шаблон и
лимит десанта. Setter только authority, запись однократная, `BumpMe` только при
изменении. Машины/персонажи используют штатную репликацию. Client/JIP runtime
требует отдельного evidence; наличие `RplProp` не является сетевым PASS.

Fleet cap равен полному плану, включая усиленную базу. AIWorld резервирует
верхнюю границу 16 мест на машину и запас builders. После потерь нет replacement.
Невозможная площадка повторяется по прежнему правилу; потеря базы до spawn
закрывает slot. Гарантия относится к initial generation, не к восстановлению
бронетехники после боевых потерь.

Новые events: `FIA_GARRISON_CATALOG/COMPOSITION/ASSET_PLAN/ROSTER`,
`FIA_STRONGPOINT_SELECTED`, `FIA_STRONGPOINT_COMPONENT_BRIDGE`,
`FIA_STRONGPOINT_RADIO_SETUP/CONFIGURED/REJECTED`.
Старые PLAN/READY и поля сохранены: `per_base` — обычный размер, усиленная база
включена в общее `vehicles`. Проверки и verdict — в [TESTING.md](TESTING.md).
