# Склад снабжения для дорогих пресетов

`SUPPLY_DEPOT` добавлен после прежних пяти значений `AICF_EConstructionType`;
их числовые значения сохранены. Три массива `AICF_ConstructionBaseState`
выделяются по `COUNT`, включая reset и cleanup нового индекса 5.

AI commander только читает собственные stable slots и вычисляет максимальную
цену одного назначенного vehicle preset. Текущая готовность roster не нужна.
Если назначений нет или `base.GetSuppliesMax()` уже покрывает эту цену,
строительство склада не требуется. Завершённый склад при всё ещё недостаточной
ёмкости не закрывает потребность; незавершённый удерживает повторный заказ.
Planner по-прежнему соблюдает Co-op veto автоматического строительства для
человеческой стороны. Геометрия, provider identity, affiliation, native layout,
builder, резерв и списание принадлежат прежним владельцам.

Ресурс ищется через faction building field, затем зарегистрированный
`SCR_CampaignBuildingManagerComponent.GetPlaceablePrefabs()`. Кандидаты
сортируются по ResourceName. Сначала проверяются имена с `supply`/`cache`/`storage` (без учёта регистра), затем прочие зарегистрированные ресурсы, если первый проход не дал результата. Эти имена — приоритет загрузки, а не разрешение: требуются `TRAIT_SERVICE`, `SERVICE_SUPPLY_STORAGE`,
правильная фракция и её label. Positive cache привязан к точным world, manager
и faction и повторно проверяет регистрацию/metadata. Пустой результат не
кешируется до готовности manager. `[CONSTRUCTION_SUPPLY_PREFAB]` содержит число
проверенных ресурсов и время поиска.

В RHS native building field пуст; зарегистрированные supply caches используют
штатные `US`/`USSR`, хотя runtime factions — RHS. Узкое разрешение в RHS profile
действует только для `SUPPLY_DEPOT`, соответствующей stable faction, её stock
faction label, service/trait labels и точно выбранного зарегистрированного
ресурса. Resolver и `ConstructionMetadata.Load` используют одну policy.
Остальные пять типов сохраняют прежнюю строгую проверку. После создания
`PrepareConstructionFaction` по-прежнему обязан привязать composition к
ожидаемой runtime faction.

Склад расширяет физическую ёмкость, но не выдаёт припасы бесплатно. Нехватка
самих припасов, building budget, свободной площадки или builder по-прежнему
может задержать строительство и покупку. Цены и resource container maximum
напрямую не изменяются.

Проверки: `Test-SupplyDepotDemandContracts.ps1` исполняет извлечённые production
predicates, resolver, RHS alias policy, constructor/reset/Cancel: 54 проверки
и 8 отвергаемых unsafe variants. Baseline и текущий набор construction/static
сохранены в `.codex-runtime/coop-fleet-recovery-20261101/supply-depot/`.
Отдельный opt-in `AICF_SupplyDepotConstructionProbe` приоритизирует только новый
тип, затем наблюдает обычные metadata/site/payment/worker/service gates;
результат native запуска следует оценивать по полным остановленным логам,
не заменяя его controlled spawn из physical tank probe.

Native Stock registry-only: обе стороны дали registered storage prefab, Metadata.Load valid=1, composition IDs77/78, штатный exit0/Game destroyed; full sibling30 resource E остаются строгим FAIL. Native Everon до hint-first: обычный layout175supplies, builder50work, ONLINE, capacity1000→4000, quote3750, humanveto1; full stopped native0. Общий resource gate468E FAIL. Из них две новые строки относятся к ненужной загрузке stock Helipad_Lights_US_01.et через общий registry scan; hint-first исправляет этот project-triggered путь без подавления ошибок. Повторный native gate после изменения хранится отдельно; прежний результат не является проверкой новых байтов.

Последний native результат: hint-first Everon4ресурса/9ms, metadata valid1, full stopped native0, obsolete helipad error0 и raw466 (до исправления468). Новая случайная MOB не дала площадки в256кандидатах: платный цикл новых байтов NOT EXERCISED; после одного chunk выполнен штатный stopflag→RequestClose. Полный предыдущий paid175→builder50→ONLINE/cap4000 хранится отдельно, не подменяет этот verdict. Fresh Stock на новых байтах:US/USSR registered IDs77/78, metadata valid1, native0/Game destroyed, raw28 (до исправления30), fatal/OOM0. Общий strict resource gate остаётсяFAIL.
