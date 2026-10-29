# Пресеты техники — issue #78

Карта → выбранный отряд → «Техника». Клиент редактирует черновик; server
заново определяет controller/player, faction, numeric slot, spawn generation,
vehicle revision и library revision. Чужая faction в рецепте не даёт прав на
отряд. Owner-only RplProp содержит подтверждённый view; поздний клиент запрашивает
актуальный snapshot тем же READ. Данные рецепта не являются разрешением на spawn.

## Рецепт и каталог

`AICF_VehiclePreset` хранит schemaVersion=2, profile, faction, имя, точный prefab,
полную topology мест и numeric position для каждого места; -1 означает свободное.
Дополнительно сохраняются `behavior` (`AICF_EVehicleBehavior.TRANSPORT=0` либо
`COMBAT=1`) и `returnDistance`: целый порог возврата50..1000м, default300м по
решению пользователя. Старый schemaVersion1 читается как TRANSPORT/300; schema2
требует оба поля. Неизвестный enum/версия и порог вне диапазона отклоняются
до изменения binding или библиотеки. Mode/N входят в `SameAssignment`, поэтому
их фактическое изменение создаёт новую конфигурацию, точный повтор остаётся
идемпотентным. Store envelope остаётся schema1; внутри него хранится новый рецепт.
В окне «Техника» есть отдельный выбор поведения и поле порога в метрах.
Save/Apply/SELECT читают целое значение; периодический readback не перетирает
ввод, смена выбранной модели сохраняет mode/N. Load читает сохранённые значения.
Это настройка lifecycle: транспорт высаживает состав при угрозе; боевая единица
оставляет экипаж у оружия, высаживая десант. Runtime gates проверяются отдельно
от schema/UI и описываются по фактическому результату текущего прогона.
Entity/Rpl IDs не сохраняются. Переназначение освобождает прежнее место бойца.
Raw дубликаты member/seat, неизвестная topology и неактивные позиции отклоняются.
Apply требует заполненные Pilot/Turret/attached-turret seats. Если назначена лишь
часть живого состава либо desired size отличается от числа назначенных, server
публикует отдельное подтверждение сокращения. До подтверждения нет transfer,
spawn, binding или debit. Cancel сохраняет состав. Подтверждение оставляет только
назначенных бойцов и устанавливает desired size по их числу; дополнительный crew
не создаётся. После сокращения обычная полная ValidateRoster/ALL_OR_FALLBACK
проверяет уже итоговый состав. Read-only partial validation доступна лишь плану
сокращения, acquisition/boarding её не используют.

`AICF_VehiclePresetCatalog` берёт все enabled ResourceName из native vehicle catalog
своей стороны и `CIV` текущего content graph. Дополняющий factionless каталог
допускается только для entries с явной меткой `FACTION_CIV`; чужие faction catalogs
не обходятся. Дубликат получает собственный faction source, стоящий первым.
До включения проверяются resource и `VehicleWheeledSimulation` в prefab source;
`VehicleTrackedSimulation` и legacy путь `/Tracked/` исключаются. Пользователь
уточнил scope: легковые, грузовики и колёсная бронетехника; авиация/гусеничные/
стационарные установки не входят. Это полный активный whitelist колёсных моделей,
без прежнего ограничения наличием SpawnerData или curated suffixes.

Owner-only JSON catalog содержит параллельные `prefabs`, `names`, `costs`, `sources`,
`categories`. UI отдельно фильтрует «Все / Своя фракция / Гражданские» и
«Все / Легковые / Грузовики / Бронетехника / Прочая техника». Категория берётся
из native labels `VEHICLE_CAR`, `VEHICLE_TRUCK`, `VEHICLE_APC`/`TRAIT_ARMOR`.
Отфильтрованный индекс переводится через `m_aVisibleVehicles`, а не используется
как индекс полного массива. Load раскрывает фильтры, скрывающие модель draft;
смена фильтра не меняет draft, binding или library. Пустой список не шлёт SELECT.
При совпадении native имён UI дописывает короткое имя варианта без GUID/пути.

Стоимость: content profile quote override → native SpawnerData → сумма native
`CAMPAIGN` в `m_EntityBudgetCost` + `m_EntityChildrenBudgetCost` prefab UIInfo.
RHS/WCS override использует существующий `AICF_RHSMenuPricePolicy.Quote`.
Core не знает vendor prices/paths. Чтение budget не вызывает `MergeBudgetCosts`
и не меняет исходные budget objects. Неизвестная/неположительная цена видна как
недоступная; SELECT не выдаёт такую машину бесплатно. Civilian recipe сохраняет
faction получателя (`US`/`USSR`), не `CIV`. Проверки Apply и acquisition заново
строят тот же расширенный whitelist, списание остаётся в EconomySystem.

SELECT создаёт local
модель в private world и читает native accessible compartments, без глобального
ItemPreviewManager cache. Поддерживается наземная машина
с VehicleWheeledSimulation и одним Pilot; модели без Pilot или с несколькими
Pilot отклоняются после live inspection, даже если source прошёл фильтр.

Стабильный seat key: `relative compartment-manager owner ordinal:local slot ID:type`.
Ordinal строится в hierarchy только из entities с compartment manager. Native
`GetCompartmentMgrID()` глобален и меняется между экземплярами/private worlds:
его нельзя использовать в сохранённом рецепте. Перед Apply и после spawn
повторно сравниваются вся topology и roster. На Stock UAZ469_PKM native probe
нашёл **3** доступных места, хотя старые conservative metadata давали 4.
Ручной путь использует фактические compartments.

## Библиотека и применение

- Save пишет `vehicle_<id>_0.json` / `vehicle_<id>_1.json` в
  `$profile:AICF_Loadouts`; namespace не пересекается с `template_` персонажей.
- Последнее валидное поколение и readback защищают от прерванной записи.
  Delete пишет tombstone. Одинаковые profile/faction/name обновляют запись;
  точный повтор не пишет файл и не меняет library revision.
- Load меняет только черновик. Save допускает схему для будущего состава,
  но Apply всегда заново проверяет живой roster. Delete не меняет binding.
- Apply без сокращения меняет только отдельный binding stable slot. Совпадающая модель и
  рассадка не увеличивают revision и не создают новую заявку/списание.
- «Автоматика» снимает binding, сохраняя открытый черновик и несохранённую рассадку;
  после cleanup действует обычный unit profile.
  Ручной binding имеет приоритет над профилем motorization и сохраняется при
  переименовании роли и замене группы. Новая incarnation проверяется заново.

Persistence использует тот же profile и существующий persistent library mount.
Launcher import `-LoadoutLibraryPath` пока импортирует только старые character
files; перенос `vehicle_` в новый изолированный profile этим flag не реализован.
Это ограничение переноса, а не основание считать новые presets сохранёнными
между произвольными свежими profiles без persistent mount.

## Превью, состав и возврат

`AICF_VehiclePreview` владеет собственной local моделью, камерой и светом в private
world; native RenderTargetWidget не создаёт машины/crew в кампании. Close очищает
модель и world. Footer явно разрешает AICF localization перед SetText; native
EditBox label/placeholder заменены названием пресета. Hover/pressed кнопок формы
и commander сохраняется при периодической перерисовке. Commander использует
native SCR_SliderComponent 1..10: число меняется при движении, configuration
request отправляется на ChangedFinal. Refresh не сбрасывает текущий drag.
Это прежний desired-size контракт; ограничения автоматического armed profile
2..4 сохраняются и проверяются сервером.

`AICF_VehicleCrewPlan` — одноразовый owner-bound challenge на 60s: exact encoded
draft, group ID, spawn generation, assignment/loadout/vehicle revisions, desired
size и IDs всех живых numeric positions. Любая смена roster/config или новая
операция отменяет challenge. CONFIRM_CREW/CANCEL_CREW добавлены после прежних
значений enum; сервер заново проверяет права, каталог, обязательный crew, cap,
supplies, отсутствие recruitment/equipment/transport операции и занятости людьми.

`AICF_VehicleCrewReturn` владеет auxiliary roster и return polling, OrderPlanner —
его waypoint. Только после подтверждения подготовляются пустые faction-correct
controllers без SpawnUnits, по одному на возвращаемого AI. Сначала готовятся все
маршруты; failure подготовки очищает только пустые controllers. Transfer проверяет
родительскую группу, при failure выполняется rollback, непустая группа не удаляется.
Возвращаемые учитываются в MatchController.CountManagedAgents до окончания возврата.
Ближайшая безопасная owned base выбирается из runtime graph отдельно для каждого
бойца; tie сохраняет graph order. Смена владельца/угроза отменяет старый маршрут.
Удаление требует physical distance <=15m от active friendly spawn point, SAFE
threat, exact group/member identity, authority, отсутствие player possession и
compartment transitions. Нет timeout deletion или teleport. При блокированном пути
AI остаётся живым. Stop снимает owned waypoints и LOD pin; не удаляет AI в поле.

GroupSlot компактно перенумеровывает только оставшихся, remaps seats и loadout
bindings вместе с applied revisions; текущая equipment source role сохраняется
для того же живого бойца, включая Inspector. Новый recruit получает обычную роль
своей новой позиции; deployment очищает временную карту ролей. Inventory не пересоздаётся. Spawn generation и assignment
revision инвалидируют старые callbacks. Stable faction/numeric slot не меняется,
сохранённые library recipes не переписываются.

Manual staging также допускает полный состав из одного сохранённого Pilot:
`captured > 0` при точном равенстве assigned, desired и actual native agents.
Пустой, неполный или чужой roster по-прежнему отклоняется с прежними actor/Rpl/
faction/authority/foot guards. Регрессия `captured > 1` после сокращения10→1
давала `SPAWN_STAGING_ROSTER_CAPTURE_REJECTED` до покупки. Fresh
`crew-final-current-v2` подтвердил WB0, client15/server8, native0/0 и single UAZ
Pilot после девяти distinct ARRIVED; full resource errors30/158 остаются FAIL.
Evidence: `.codex-runtime/issue78-finalization-20261029/staging-single-crew/`.

## Lifecycle и экономика

VehicleCoordinator допускает ручной запрос и для INFANTRY с одним живым бойцом,
сохраняя admission cap, meaningful order и infantry ownership fences. Trip
захватывает slot/revision/group/generation/member IDs. Смена настройки отменяет
старый trip через typed fallback в TransportTripController; flows сами не меняют
phase и не вызывают соседних владельцев.

Acquisition использует единственный выбранный prefab: без замены на truck или
другую модель. Site reservation/staging, fleet lease и live revalidation остаются
штатными. EconomySystem проверяет native catalog cost, безопасную faction base,
supplies и logistics obligations перед spawn; после live validation списывает
стоимость перед bind. Неуспешный bind возвращает платёж и удаляет только ещё
непринятый candidate. Повтор accepted asset не проходит purchase заново.

`AICF_VehicleBoardingSiteQuery` добавляет консервативный gate пешего покрытия
площадки: center и восемь точек сетки с offsets ±4m. Для каждой нужны загруженный
tile и native foot-nav projection со сдвигом <=0.5m по XZ и <=1m по высоте.
Selector отклоняет такой candidate до SELECTED; fresh spawn commit повторяет все
девять запросов перед reservation lease/платежом. Group/entity/Rpl/faction/world/
pathfinding/navmesh identity проверяются повторно. Дедупликация строк commit
не кеширует eligibility. Незагруженная navmesh даёт явный отказ без новых load
requests, дополнительных попыток или продления deadlines.

События `VEHICLE_FOOT_PAD_PROBE/POINT` содержат phase, query, tile loaded/requested,
projection и deltas. Непосредственно после spawn, до live validation и debit,
read-only `VEHICLE_ENTRY_SITE_PROBE` читает до16 уникальных реальных AI entry doors
с `sample_phase=POST_SPAWN_IMMEDIATE`. Этот ранний sample может предшествовать
asynchronous dynamic carving: поля `reachability_proven=0 async_carving_proven=0`
сохраняют эту границу. Поздние ENTRY_PATH snapshots позволяют сопоставить
покрытие до создания машины, после spawn и при stalled entry. Данные projection
не доказывают связь с leader, исправность BT или точную причину препятствия.
Gate не перемещает машину/бойцов и не меняет обычный 125s boarding budget.
Контракты: `Test-VehicleBoardingSiteContracts.ps1`.

Manual preset дополнительно требует физический сбор всех бойцов в существующем
rally radius25m до spawn и запуска boarding clock. Одного pad envelope10..90m
недостаточно для последовательной exact-seat очереди: дальний actor может
потратить оставшийся срок на подход. Immutable SpawnPlan захватывает manual flag;
Measure, fresh commit и individual approach используют один geometry predicate.
Смена режима плана отклоняется; manual staging physical relocation запрещена.
Automatic сохраняет прежний pad envelope: это новая узкая manual policy по
native causal evidence, а не восстановление случайно удалённого if. Сроки,
радиусы, retry и dismount budgets не увеличены.

Preset boarding внутри существующего BoardingFlow выдаёт native SCR_AIGetInVehicle
на конкретные seats и tracks прежние identity-safe tokens. Нужны два полных
readback polls: exact occupant, compartment, settled и отсутствие transitions.
Только затем controller получает START_MOVEMENT. Deadline/отказ возвращает
ALL_OR_FALLBACK. Никакого teleport в production preset path нет. Cleanup остаётся
у VehicleCleanupManager с прежними human occupancy/clearance guards.

Если после посадки текущая цель уже в dismount zone, до первого staged route
ручной preset остаётся в TRANSIT без vehicle waypoint. TransitFlow возвращает
`PRESET_WAITING_FOR_DESTINATION`, записывая assignment revision/target. Controller
через `CommitPresetDestinationWait` включает ожидание только для текущего preset,
route generation0 и отсутствующих current/superseded waypoints. Crew/health/identity
проверяются каждый обычный tick. Время ожидания не расходует trip/movement budget;
controller возобновляет оба дедлайна при следующем ином outcome и на ExitTransit.
Первая удалённая цель создаёт штатный маршрут в той же машине. После его начала
близкая цель снова означает обычную высадку; автоматические trips не ждут.
Editor показывает локализованный статус с просьбой задать удалённую цель.

Normal dismount сохраняет физический oracle: два polls без occupants,
transitions и попадания в расширенные bounds. Если normal deadline уже истёк,
но все вышли без transitions, bounded guidance/clearance продолжается до
существующего terminal deadline (при штатном timeout20s — максимум40s от Begin).
Этот запас не продлевается повторным tick; оставшиеся occupants/transitions
сохраняют прежний normal timeout. Ни cleanup proof, ни human/LOS fences не ослаблены.

## Проверки и текущая граница доказательств

Exact boarding допускает максимум два одновременных native входа, когда
`AICF_PresetEntrySeparation` доказал независимость всех доступных дверей пары:
непересекающиеся root-relative owner/live-manager/door keys и расстояние не менее
2.5m по XZ между каждой парой возможных entry points. Ближайшая дверь не угадывается.
Общие двери, неизвестные metadata и Turret остаются последовательными; чужие
occupants/reservations, human actor и stale token отклоняют параллельный допуск.
Busy union включает owned actions и все физические transitions, поэтому третий
вход не запускается. Общий срок `25s * max(3, assigned)` (125s для пяти),
две settled polls и максимум два повтора не меняются. Событие
`VEHICLE_PRESET_PARALLEL_ENTRY` отмечает допуск, а
`VEHICLE_PRESET_BOARDING_DEADLINE_SNAPSHOT` перед отказом сохраняет required/settled,
число settled polls, busy entries и исходный deadline. Контракты queue/parallel entry
проверяют third-entry veto, complete metadata, shared/unknown/turret/human/stale
и все cross points; native прогон остаётся отдельным gate.

После `COMPLETE` или `FALLBACK` с восстановленным пешим приказом scheduler
сохраняет существующий trip как подтверждение завершённой попытки перевозки
для exact group/generation, strategic intent, destination и
preset revision. Восстановление пешего waypoint меняет assignment revision, но не
заказывает ещё одну машину по тому же приказу; base revision также не переармляет
поездку. Новый intent/target/preset, замена группы/типа или явная admission-заявка
проверяются прежними cleanup и admission gates. Это не временный cooldown и не
обход failed-closed политики. Одинаковый UI Apply остаётся идемпотентным и сам
по себе не создаёт explicit admission. `Test-CompletedTransportIntentContracts.ps1`
проверяет actual production predicates и повтор исходного handoff assignment6→7
при неизменном intent3, включая новые приказы и explicit request.

Штатный group-wide `SCR_AIGetOutActivity` после failed vehicle movement распознаётся
отдельно от actor-level OpenGate. Stateless `AICF_TransportNativeAbandon` требует
текущий исполняемый high-priority PLAYER GetOut точной зарегистрированной машины,
пустой related waypoint/boarding parameters, полный актуальный AI preset binding
и неизменный route context. Только этот proof возвращает typed fallback
`NATIVE_GROUP_ABANDONED_VEHICLE` до попытки восстановить водителя. Native actions,
physics и progress clocks observer не меняет; phase, пеший handoff и clearance
остаются у прежнего controller/cleanup. Existing crew/gate owners сохраняют приоритет.

`Test-VehiclePresetContracts.ps1` проверяет 78 структурных контрактов;
`Test-VehiclePresetLogContracts.ps1` — 24 positive/negative controls анализатора.
`AICF_VehiclePresetTransitProbe.c` с `-aicfVehiclePresetTransitProbe 1` проверяет
отдельный native roster5 и пятиместный UAZ:30s точного crew внутри у исходной цели,
пауза idle budget, новая удалённая цель из runtime graph, физическое движение
той же машины >=25m с >=5 velocity polls и без mobility recovery, затем штатная
высадка всех5 после retarget к текущему положению. Это первый участок маршрута,
а не доказательство движения до любой удалённой базы. `Test-VehiclePresetTransitLog.ps1`
требует16 cases, stopped/full logs, manifest/CLI, native0,5 exact occupants,
один purchase125, штатный HANDOFF→COMPLETE и порядок движения перед высадкой;
fallback/failed-closed/latch отклоняются. Его contracts —32 controls.
Полная поездка до неизменной цели, пешее прибытие всех бойцов и повторная
посадка проверяются отдельно: [VEHICLE_SCENARIOS.md](VEHICLE_SCENARIOS.md).
Там же описаны новые read-only GROUP_TRACE/MEMBER_TRACE и bounded exact-seat retry.
`Test-VehiclePresetLog.ps1` требует manifest/CLI, native exit, ROSTER_READY,
полные stopped logs, обязательные cases и actual occupants до movement.
Он отклоняет неверного occupant, intent без посадки и повторный debit.
`-LibraryReload` проверяет отдельный новый процесс, не объявляя физическую
посадку проверенной в этом режиме. `Test-VehiclePresetClientLog.ps1` требует
20 client cases, 3 JIP cases и оба server readbacks, точные manifests/CLI,
native0 и полные stopped logs; его 12 controls включают отказ на serializer leak.
`-CatalogFilters` требует 28 client +5 JIP checks и фактический CLI flag на peers;
`-Civilian` в physical auditor требует whitelist case и CLI civilian mode.
`AICF_VehicleCatalogProbe.c` сравнивает catalog с полными native own/CIV источниками,
categories, независимыми budget snapshots и чужими prefabs; заканчивается21 cases.
`Test-VehicleCatalogLog.ps1` требует полный stopped log, exact CLI,21 cases и оба
native totals. Его contracts проверяют13 controls, включая missing civilian,
изменённый budget, enemy leak, side effects и fatal/unfinished logs.
`-Lifecycle` дополнительно требует смену модели/роли, реальную новую группу,
revalidation и cap/funds/library negatives; `-HumanProtection` требует takeover,
15s retention и сохранённого occupant. Количество уникальных подтверждённых
мест обязано совпадать с составом fixture (обычный 4, armed 3, driver 1).
`-Policy` требует native admission readback до и после снятия binding.

Runtime fixtures: `AICF_VehiclePresetProbe.c` (Core/Vehicles, isolated
`AICF_ZVehiclePresetProbe.c`, `-aicfVehiclePresetProbe 1`) и
`AICF_VehiclePresetClientProbe.c` (Core/UI, isolated
`AICF_ZZVehiclePresetClientProbe.c`, `-aicfVehiclePresetProbe client
-aicfVehiclePresetClient 1`). Дополнительный `-aicfVehiclePresetCrew armed`
готовит отдельный managed состав из 3 бойцов; другое значение — из 4.
Значение `driver` создаёт совместимый состав из одного водителя для human probe.
`-aicfVehiclePresetCivilian 1` выбирает civilian UAZ; fixture-only
`-aicfVehiclePresetModel S105_beige.et` позволяет проверить другую конкретную
модель из того же разрешённого каталога. Production CLI эти flags не читает.
`-aicfVehiclePresetJIP 1` запускает отдельный late owner readback. Менять stage
между запуском server и подключением клиентов нельзя: script CRC обязан совпадать.
`-aicfVehiclePresetReload 1` читает сохранённые id1/tombstone id0 и старый member
preset из того же persistent mount, без нового Apply/spawn. `-aicfVehiclePresetHuman 1`
требует direct-connect client и проверяет передачу уже сидящего водителя игроку,
сброс binding и сохранение машины/occupant после cleanup ticks. Это fixture-only
передача существующего бойца; production не создаёт дополнительного crew.

`AICF_VehicleNativeControl.c` — диагностический контроль без editor RPC. На
native-only graph ROSTER_READY обозначает нулевой managed roster; на graph main
нужен `-aicfControlManaged 1`, чтобы оставить настоящую readiness. Дополнительные
`aicfControlFaction`, `aicfControlJIP`, `aicfControlPreview` и `aicfControlNoSave`
изолируют выбор faction, повторное подключение, private model world и save policy.
Это не substitute production runtime gate.

Свежие результаты и остающиеся критерии фиксируются в [TESTING.md](TESTING.md).

Высадка разделяет два бюджета: до3 batches принудительного native logical exit
и до3 batches физического освобождения hull после доказанного logical_occupants=0
и transitions=0. Это увеличение общего предела до6 batches на фазу, максимум12
за normal dismount и уже существующую terminal fallback phase вместе. Повторные
polls бюджеты не сбрасывают; normal/terminal/absolute deadlines, три animated
retry, три guidance attempts, exact identity и player/LOS/navmesh fences прежние.
Непустой безопасный probe и свежая проверка полного состава предшествуют charge
physical budget. Terminal escalation учитывает операцию, которая ещё нужна;
исчерпанный logical exit не отменяет последующее освобождение hull.
`force_attempts` остаётся суммой, новые `force_exit_attempts` и
`physical_relocation_attempts` показывают составляющие; максимумы3/3/6 явно
пишутся в clearance events и проверяются scenario auditor. Контракт
`Test-DismountOperationBudgetContracts.ps1` исполняет production state methods,
проверяет последовательность logical3→physical3, остановку следующей попытки,
запрет physical recovery при logical/transition occupancy и отсутствие rearm.
Изолированная causal fixture и отдельный обычный `ural5` описаны в
[VEHICLE_SCENARIOS.md](VEHICLE_SCENARIOS.md); static PASS не подтверждает native
выполнение новой ветки.
Визуальное качество, физический ввод и удобство остаются NOT RUN: Computer Use,
screenshots и GUI automation запрещены; hidden handlers этого не доказывают.
