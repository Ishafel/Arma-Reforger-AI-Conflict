# Пополнение экипировки и сбор с тел

Issue #6. Owner — `AICF_EquipmentService` в Core. `MatchController` только создаёт
службу, вызывает authority tick и завершает lifecycle. Waypoint принадлежит
`AICF_OrderPlanner`; roster positions, generation и применённые revisions —
`AICF_GroupSlot`.

## Потребность и настройки

Каждая живая AI-позиция сравнивается с binding либо Default её roster prefab.
Target строится в изолированном `AICF_EquipmentDraft` без UI preview cache; этот мир не является
источником трофейных entities. Кэш освобождает неиспользуемые preview worlds.
Перед удалением private model её native storage attachments снимаются, пока
weapon/muzzle существуют; это освобождает ссылки WCS на muzzle. Путь очистки
всегда проверяет, что model не принадлежит миру кампании.

`AICF_EquipmentInventory` суммирует фактические `BaseMagazineComponent.GetAmmoCount`,
боезапас muzzle и патронники. Загруженный magazine не считается второй раз.
Пустые магазины и использованные одноразовые трубы дают ноль. Дополнительные
одинаковые одноразовые трубы учитываются по их фактическим выстрелам.

| Параметр | Default и смысл |
|---|---|
| `aicfEquipmentLowPercent` | 25%, диапазон 1–50: начать при запасе основного оружия/расходников не выше этого процента target |
| `aicfEquipmentSatisfiedPercent` | 75%, диапазон low+1–100: закрыть уже открытый дефицит после достижения этого процента |
| ПТ-порог | Начать при нуле пригодных выстрелов; один подобранный выстрел закрывает ammo-дефицит |
| Rifle target | Полные магазины актуального комплекта, без дополнительного патрона preview в патроннике; настоящий патронник учитывается в остатке |
| Target точки | Полный inventory актуального комплекта |
| Оценка/retry | Оценка раз в 10 секунд, новая попытка после отказа не чаще раза в 60 секунд |
| Combat quiet | 30 секунд непрерывного `SAFE`, threat measure не выше 0.01 у всех живых members |
| Поиск тел | 50 м от нуждающихся живых members; один перенос за tick |
| Подход к телу | До 45 секунд; transfer только до 3 м от конкретного получателя |
| Точка | До 1500 м от живого лидера, issuer radius 15 м, до 180 секунд на выбранную точку |
| Выдача | До 30 секунд; native receipts и фактический readback проверяются отдельно от возврата `Try*` |
| Восстановление | До 60 секунд и трёх проходов; неподтверждённый rollback не становится PASS |

Отсутствующее основное оружие и новая revision создают отдельную потребность.
Изменение другой позиции не переснаряжает весь roster. `MarkMemberLoadoutApplied`
принимает только текущую member identity и текущую revision. Новые deployments
и recruits сохраняют существующий spawn-time путь применения binding.

## Источники

Тело — настоящий мёртвый `ChimeraCharacter` под master authority. Живые,
раненые живые, player-controlled и possessing main entities исключены.
Faction источника не ограничивается: союзник, противник и FIA допустимы.

Переносятся существующие совместимые непустые magazines и нужные consumables
актуального комплекта, включая пригодные штатные гранаты. Launcher разрешён
только позиции, чей комплект предусматривает ПТ-средство. Реальный `EWeaponType`,
доступный profile/catalog, `IsDisposable`, `MagazineWell` и native storage
admission определяют пригодность; поиск слова RPG в имени не используется.
Reloadable launcher можно перенести незаряженным, но потребность остаётся до
появления боезапаса. Одноразовая пустая труба не выбирается.

Перед `TryMoveItemToStorage` повторяются group/member/corpse/item identities,
revision, intent/assignment, combat, distance, source ownership и capacity.
Transfer не использует remove+spawn, не заменяет основное оружие и не меняет
сохранённый preset. После подбора остаток пересчитывается; достаточный подбор
отменяет визит, если нет новой revision или другого дефицита.

Точка — зарегистрированный дружественный `ARMORY` в `ONLINE`, безопасный по
capture state, с настоящим включённым `SCR_ArsenalComponent`. Штатный armory
разделяет одежду и оружие по соседним ящикам: capability объединяет разрешённые
prefabs до 30 м от service entity. Корневой доступный предмет приносит baked
содержимое; явно добавленные вложенные prefabs тоже должны быть доступны.
Казарма или базовые ящики с ограниченным ассортиментом не доказывают возможность
выдать полный комплект.

Выбор использует `AICF_GroupRuntime`, native navmesh endpoint, расстояние и
стабильный `EntityID.ToString().Compare` tie-break. Фактическая непроходимость
заканчивается timeout; источник исключается в текущем визите. Owner, service и
catalog повторно проверяются перед выдачей. Без источника отряд возвращается к
допустимому intent и сохраняет потребность для следующей попытки.

## Стоимость и lifecycle

Пополнение имеющегося живого бойца стоит **0 supplies / 0 tickets**, как текущий
loadout contract без каталожной наценки. Retry и трофейный сбор не создают ledger
transactions, faction supplies или tickets. Найм отсутствующих бойцов остаётся
платным у `AICF_InfantryRecruitmentService`/`AICF_EconomySystem`.
Нулевой supply баланс сам по себе не блокирует эту бесплатную выдачу; фактический
ONLINE/enable state службы и доступность всего комплекта обязательны.

Один visit и один native inventory transaction на slot. Recruitment/muster,
активный transport, route recovery и неполный roster блокируют новый visit.
Inventory transaction блокирует recruitment, vehicle admission и передачу через
squad respawn. Новый ручной infantry intent сохраняет приоритет: завершение
старого visit сверяет assignment/intent и не возвращает старый waypoint.

Отдельный `AICF_EquipmentApplicator` убирает предмет из рук, сохраняет корневые
исходные entities с вложенным содержимым, ожидает native receipts, выдаёт
актуальный snapshot и проверяет readback. Только commit отмечает revision и
удаляет сохранившиеся detached originals. Ошибка возвращает исходные вещи;
на время native transaction блокируется движение carrier, а detached вещи
сохраняются у его фактической позиции. Commit, rollback, identity cancellation
и Stop освобождают только собственную блокировку; AI threat system продолжает работать.
действующий персонаж не уничтожается. Возврат переживает освобождение visit и
не владеет приказом. Identity/player неопределённость закрывает дальнейшие
mutations; сохранённые detached entities остаются в мире с диагностикой,
потребность не помечается выполненной.

Callback `AICF_EquipmentReceipt` меняет только completion flags. Служба не
создаёт `CallLater` или subscriptions; `Stop` освобождает visits, кэши и recovery
records. Фактические проверки — в [TESTING.md](TESTING.md), fixtures — в
[TEST_FIXTURES.md](TEST_FIXTURES.md).
