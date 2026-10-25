# Пресеты техники — issue #78

Карта → выбранный отряд → «Техника». Клиент редактирует черновик; server
заново определяет controller/player, faction, numeric slot, spawn generation,
vehicle revision и library revision. Чужая faction в рецепте не даёт прав на
отряд. Owner-only RplProp содержит подтверждённый view; поздний клиент запрашивает
актуальный snapshot тем же READ. Данные рецепта не являются разрешением на spawn.

## Рецепт и каталог

`AICF_VehiclePreset` хранит schemaVersion=1, profile, faction, имя, точный prefab,
полную topology мест и numeric position для каждого места; -1 означает свободное.
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
Визуальное качество, физический ввод и удобство остаются NOT RUN: Computer Use,
screenshots и GUI automation запрещены; hidden handlers этого не доказывают.
