# Защита известного выпавшего оружия от провала через terrain

## Исходный дефект и граница доказательства

Controlled r32 `controlled-r32-USSR-900-v1` завершился engine assertion:
`GenericEntity / VZ58_body.xob / Entity out of world bounds`, y=-34011.066406.
Полные console/script/error/crash сохранены. Точный weapon EntityID126B/Rpl
-2147471511 появился в episode323 `INVENTORY_REMOVED` в06:38:08.619 у FIA SL1264
с current source Rpl-2147471369. Начальный origin был на6см выше terrain,
parent/slot/storage/held/weapon-slot/Contains отсутствовали. Через45мс rigid body
двигался вниз; первый следующий5s sample уже был на105м ниже terrain. Через96с
получен assertion. Это actual trajectory, а не доказательство причины collider
failure. `MAIN_UNMANAGED` обозначает ownership scope, не stock-only root cause.
Фактический вызывающий drop-код неизвестен: есть native inventory removal,
но для этого item нет `SCR_AIDropWeapon` completion receipts.

Первичное evidence и read-only proposal:
`.codex-runtime/issue78-finalization-20261030/vz58-r32-readonly/`.
Ранее аналогичный VZ58 assertion описан в `TESTING.md`; прежние FAIL сохраняются.
Новый safety owner не превращает их в PASS и не объясняет неизвестный native
collision defect. Fixture-free MRE для stock-only attribution пока **NOT RUN**.

## Владелец и события

`Loadouts/AICF_DroppedWeaponFallSafety.c` — отдельный authoritative owner.
MatchController только создаёт/запускает его и останавливает до остальных
сервисов. Общий `UPDATE_INTERVAL_MS=1000` не менялся: safety имеет собственный
`CallLater(Tick,100,true)` и exact `Remove(Tick)` в идемпотентном Stop.

Core modded `ScriptedInventoryStorageManagerComponent.OnItemRemoved/OnItemAdded`
всегда делегирует исходный callback ровно один раз с прежними аргументами.
Перед super устанавливается уникальный per-item frame и отзывается старый
candidate; после super только всё ещё текущий frame может принять новый
removal. Nested Add/Remove и Stop не позволяют старому callback записать
successor state. Event-owner inventory changes также отзывают candidate того
же root weapon или известного subtree node. Несвязанный ammo event у прежнего
source не отзывает candidate другого свободного weapon. Private worlds не допускаются.
Для Added при пустых candidates и frames используется pure empty fastpath:
нет allocation/GetOwner до исходного super. Nested Remove внутри такого super
создаёт собственный frame; пустой outer End не отменяет его successor.
`FrameOwned` проверяет только owner/epoch/token membership, а `FrameCurrent`
дополнительно требует живые native manager/item references. Поэтому End может
снять свой frame после удаления native component внутри super, не разыменовывая
его и не оставляя до64 мёртвых entries. Stale outer не снимает nested successor.

Permission рождается только из реального удаления primary/launcher weapon
из inventory известного AI character. Сохраняются exact item/source
EntityID/Rpl/world/component identities и действующий `GetControlAIAgent`
при admission. Последующее исчезновение agent допустимо только у того же
DEAD source с неизменными identities и всеми current human veto. Отсутствие
исходного AI witness означает UNKNOWN/fail-closed. ALIVE не является обязательным
условием для известного погибшего AI source. Actor, здоровье, resurrection,
vehicle, quota и inventory contents не изменяются.

Observation и destructive permission разделены. Actual removal может ещё
находиться внутри native changing-item/weapon-slot/lock перехода. Candidate
с известными identities сохраняется в пределах исходного TTL, но не получает
delete до полного `ItemFree/SourceFree`. Новый Add/Remove того же item,
identity/world/Rpl loss или human ownership отменяют candidate; timeout не
продлевается. Временный changing/held/slot сам по себе не объявляется UNKNOWN AI.

## Когда разрешён единственный delete

Candidate должен наблюдаться над local terrain и затем двигаться вниз минимум
на2м ниже terrain. `GetSurfaceY` — предварительный фильтр, а permission требует
actual synchronous `TraceMove` с `TraceFlags.WORLD`: путь между сохранённой
above-point и текущим item пересёк terrain, hit fraction строго между0 и1,
normal направлена вверх. Просто отрицательная Y, prefab VZ58 или имя модели
не являются разрешением. Отсутствие terrain hit, NaN/Infinity, private world,
нединамический/неизвестный physics либо обычный лежащий предмет дают отказ.
Перед `GetPhysics`, `IsDynamic` и `GetVelocity` и после возврата повторяется
`PhysicalContext`: текущий owner/epoch, живые weak references, actual main-world
и authoritative campaign. Ненулевой wrapper после потери item/source/world не
разрешает следующий Physics getter; private preview physics не исследуется.

До delete повторяются current master/Rpl/EntityID→same object, InventoryItem и
BaseWeapon identity, отсутствие parent/ParentSlot/lock, current source manager
Contains/weapon slots/hands/changing-item и полный bounded human/possession
scan. Source/human/world guards идут после subtree/physical reads; последним
идёт pure owned epoch/record fence. Новый inventory callback отзывает permission.
Unknown или foreign ownership не превращается в отсутствие владельца.
После последнего subtree scan повторяется `ItemFree`: child native callback
может изменить root lock/slot после того, как root уже был проверен в traversal.

Удаление охватывает children: pinned `SCR_EntityHelper.DeleteEntityAndChildren`
явно является wrapper над `RplComponent.DeleteRplEntity(entity,false)`.
Поэтому subtree ограничен32 nodes, циклы/duplicates/foreign world/parent/storage,
неизвестные не-inventory children, character/vehicle, locked или proxy children
дают veto. Дочерние inventory slots должны принадлежать storage внутри того же
weapon tree. Никакого удаления тела, персонажа, автомобиля или чужого контейнера.

`DeleteRplEntity(item,false)` — единственный destructive effect. Телепортация,
изменение physics/CCD/gravity/velocity, лечение и воскрешение не используются.
Исходный observer `AICF_LooseWeaponObserverProbe.c` остаётся read-only,
`cleanup_enabled=0`; его ledger никогда не выдаёт cleanup permission.

## Ограничения, бюджет и подтверждение

Это mitigation начального провала после AI drop, а не универсальный ремонт
физики всех мировых предметов. Не более64 candidates, TTL10s от actual removal,
8 samples и2 terrain traces на100ms Tick; overflow явно сообщает
`DROPPED_WEAPON_FALL_SAFETY_OVERFLOW coverage=INCOMPLETE`. TTL не продлевается
при ожидании и не меняет игровые order/retry/deadline clocks. Source UNKNOWN,
неполная topology или ownership proof приводят к fail-closed пропуску.

Void delete call даёт только `DROPPED_WEAPON_FALL_SAFETY_DELETE_REQUESTED`
с `confirmed_absent=0`. На следующем Tick отдельный read-only receipt проверяет
отсутствие exact EntityID и Rpl в том же world; лишь тогда выводится
`DELETE_CONFIRMED confirmed_absent=1`. Проверка не повторяет delete.
`DELETE_UNCONFIRMED retry=0` и отсутствие confirmation не являются успехом.
Stop очищает candidates, frames и acknowledgement receipts без native effects.

## Проверки

До изменения выполнены пять checks: EquipmentContracts,
EquipmentFailureInteropContracts, LooseWeaponObserverContracts, Stage3Static,
Stage35Static — **5/5 PASS**, actual session83951 closed0. Evidence:
`.codex-runtime/issue78-finalization-20261030/dropped-weapon-fall-safety/baseline/`.

Финальный `Test-DroppedWeaponFallSafetyContracts.ps1` исполняет actual production
source: **94 actual cases /9 executed unsafe PASS** в PowerShell7 и Windows
PowerShell5.1. Core SHA256:
`0001A4671C6812E0149EB88D292D8170DE261B5285A393363DB94877F0949C9E`;
contract SHA256:
`6034954C76A7BBCE68108F64783121A45FE5F01BCAB0CCEE6F1BC13E296D9F55`.
Проверены 65 dead-frame cleanup/reclaim, nested successor/Stop, stale Physics
wrapper после потери item/source/world, известный DEAD source без прежнего
agent против неизвестного source, bounded transient WAIT, unrelated ammo и
subtree events, лимиты64/8/2, fresh TTL и независимое world/Rpl confirmation.
Native getters и terrain/player/inventory callbacks — deterministic boundaries,
не native physics proof. Receipt и полные результаты:
`.codex-runtime/issue78-finalization-20261030/dropped-weapon-fall-safety/final-actual-dual-v2/SOURCE_READY.json`.

Исходные actual failures не скрыты: `lateChildLock` требовал повторного ItemFree
после последнего child getter; F69 пропускал elapsed TTL после native callback
и принимал +Infinity TraceNorm. Финальный код отвергает все три случая, не
продлевает TTL и не исправляет тест под желаемый PASS. Исторические logs/source
остаются в том же evidence, включая exact F002/0AB/F69 snapshots. После начальной
правки пять соседних checks также дали5/5 PASS (scope F002, session78986 closed0);
новый полный offline набор является отдельным gate.

Ранний F429 Workbench3graphs PASS относится только к тому прежнему snapshot.
Финальные Workbench3graphs для0001, controlled native drop и повтор исходного
полного900/1800 окна пока **NOT RUN**. Старый crash остаётся blocker до доказанного
нового runtime результата; player cut не является доказательством прохождения
этих native gates.
