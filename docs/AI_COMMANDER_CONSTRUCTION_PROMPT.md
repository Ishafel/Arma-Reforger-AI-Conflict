# Автономное строительство — оценка и промпт для агента

## Оценка текущей реализации

Проверено 2026-09-05 на `87bbdfe9b995276118bc44888d9ba3dd7e9d0a05`
(`feat: add per-base AI builders with animated construction`). До подготовки
этого документа рабочее дерево было чистым. Production-код не изменён.

Реализация пригодна как исполнительный слой: `AICF_BaseBuilderService`
находит уже размещённые незавершённые stock compositions, проверяет
base/provider/faction identity, создаёт одного рабочего и достраивает проект
через `AddBuildingValue` только после подхода и подтверждения работы инструментом.
Командир пока не выбирает здания, не ищет площадки и не оплачивает размещение.

Предлагаемое расширение имеет среднюю/высокую сложность. Планирование раз в
минуту и ограничение очереди сравнительно просты. Основные риски — автономный
вызов штатного building pipeline без игрока, однократная оплата и размещение
полной composition с доступными входами и выездом техники. Редкий запуск сам
по себе не устраняет нагрузочный пик: поиск мест нужно распределить по ticks.

Подтверждённые особенности закреплённого Script Diff `1.8.0.13`:

- `SCR_CampaignBuildingManagerComponent.GetPlaceablePrefabs()` предоставляет
  building registry. Типы услуг представлены, в частности, labels
  `SERVICE_LIVING_AREA`, `SERVICE_ARMORY`, `SERVICE_VEHICLE_DEPOT_LIGHT`,
  `SERVICE_VEHICLE_DEPOT_HEAVY`. Один `SERVICE_LIVING_AREA` не различает два
  варианта казарм. Точные пять prefab mappings ещё нужно подтвердить каталогом.
- `SCR_CampaignBuildingPlacingEditorComponent` связан с player editor.
  `EntitySpawnedByProvider()` в building manager ищет editor manager и
  player-controlled entity; простой вызов с фиктивным player ID не обеспечивает
  регистрацию AI-заказа в очереди строителей.
- `SCR_CampaignBuildingProviderComponent.IsThereEnoughBudgetToSpawn()` при
  успехе вызывает `AccumulateBudgetChange()`. Это не чистая проверка для
  многократного перебора кандидатов.
- `OnEntityCoreBudgetUpdated()` в building manager участвует в списании и
  возврате supplies через resource consumer/generator и в учёте prop budget.
  Дополнительное безусловное `AddSupplies(-cost)` создаёт риск двойной оплаты.
  Обычный демонтаж возвращает долю стоимости; это не эквивалент полного rollback
  неудавшейся транзакции.
- Штатный obstruction validator зависит от editor preview/area trigger,
  проверяет дочерние части composition, воду и препятствия. Некоторые проверки
  наклона дают `WARNING`, поэтому одного разрешения stock preview недостаточно
  для строгого автоматического размещения.
- Текущая `AICF_BaseBuilderRuntimeProbe.c` размещает тестовые sandbag layouts
  с фиксированным смещением, обходя player placement и оплату. Она не доказывает
  безопасное автономное строительство пяти запрошенных зданий.

Ниже — готовое задание. Порядок приоритетов, резерв supplies и ограничения
нагрузки заданы как предлагаемые начальные настройки, а не существующее
поведение игры. Предполагается, что два ангара — лёгкий и тяжёлый vehicle depot.

## Промпт

Реализуй в этом репозитории автономные приказы AI-командира на строительство
базовых служб. Используй существующих строителей и штатные Conflict buildings.
Доведи работу до реализации, применимых проверок и отчёта; не ограничивайся планом.

### 1. Контекст и границы

Прочитай `AGENTS.md`, `README.md`, `docs/ARCHITECTURE.md`,
`docs/DEVELOPMENT.md`, `docs/TESTING.md` и `docs/BASE_BUILDERS_VALIDATION.md`.
Проверь актуальные HEAD/dirty status и сохрани результаты relevant audits
до изменений. Не считай исторические runtime verdict проверкой нового кода.

Изучи соседние классы:

- `Construction/AICF_BaseBuilderService.c`, `AICF_BaseBuilder.c`,
  `AICF_BaseBuilderSpawner.c`, `AICF_BaseBuilderDanger.c`;
- `Command/AICF_AICommander.c`, `AICF_CommandAuthorityPolicy.c`;
- `Bootstrap/AICF_MatchController.c`, `Orders/AICF_OrderPlanner.c`;
- `Economy/AICF_EconomySystem.c`, `AICF_SupplyDeliverySystem.c`,
  `Config/AICF_Stage4Config.c`;
- building/content integration в RHS и reservations в
  `Vehicles/AICF_VehicleSpawner.c`.

Пути выше относятся к `AIConflictCore/Scripts/Game/AIConflict`, кроме RHS.
Не меняй project GUID, vanilla resources, миры и установленные файлы игры.
Не вводи координаты или названия баз Arland/Everon в Core.
Не добавляй UI, другие виды зданий, собственную систему производства техники
или новые требования к существующему AICF vehicle acquisition.

### 2. Пять типов и выбор базы

Поддержи только малые казармы, большие казармы, арсенал, лёгкий и тяжёлый
vehicle depot. Это функциональные stock compositions: после достройки должны
работать соответствующие услуги и штатный заказ техники/персонала.

Сначала подтверди реальные entries, faction labels, providers, стоимость,
полную геометрию и различия двух казарм через актуальный building registry,
prefab metadata и при необходимости терминальный catalog probe. Запиши
полученную таблицу mappings и источник стоимости. Не выбирай prefab только
по приблизительной подстроке имени, цене или одному общему service label.
Выбирай детерминированно из подтверждённого allowlist/metadata.
RHS mappings оставь в RHS integration/content boundary; неподдержанный entry
даёт явный отказ, без подстановки чужой фракции или stock fallback.

Новые автономные заказы разрешены только authoritative server/master после
`ROSTER_READY`, во время действующего match и только для сторон, которым
`AICF_CommandAuthorityPolicy` разрешает AI commander. При `US`/`USSR` mode
player-commanded сторона не получает самовольных проектов. Существующая
достройка проектов игроков продолжает работать для обеих сторон.

Обходи собственные инициализированные stock bases с допустимым актуальным
provider. Проверяй владение, разрешённые услуги, local supplies, budgets,
захват/боевую небезопасность и возможность размещения. Используй существующие
safety queries, где их контракт подходит; не требуй новых дорогих сканов врагов
для каждой точки. Отсутствие места/ресурсов — нормальное откладывание заказа.

Начальная политика развития: малые казармы → арсенал → лёгкий depot → большие
казармы → тяжёлый depot. Не более одного здания каждого из пяти типов на базу
по инициативе AI. Учитывай готовые, строящиеся, заранее размещённые на карте,
поставленные игроком и уже зарезервированные проекты; не создавай дубликаты
из-за смены owner, повторного tick или неполной регистрации entity.
Если stock service tiers взаимоисключающие или старший вариант полностью
заменяет младший, соблюди этот контракт и документируй coverage типов.

Не начинай новый AI-проект, пока на базе есть валидная незавершённая работа
для строителя. Максимум один pending/active AI construction order на базу.
Не создавай второй worker и не вмешивайся в армейские numeric slots.
При недоступности очередного типа разреши bounded проверку следующего
допустимого кандидата; не зацикливайся на невозможном prefab/месте.

### 3. Частота и нагрузка

Default — решение о новом проекте каждой базы не чаще раза в `60000 ms`.
Разнеси initial due times баз детерминированно внутри минуты и обслуживай их
через существующий server scheduler. Не меняй частоту infantry commander
и работы строителя ради нового планировщика.

Дешёвая проверка ownership/наличия зданий/supplies предшествует геометрии.
Поиск места выполняй порциями: начальный ориентир — не более четырёх
candidate transforms на общий секундный tick, с отдельным конечным лимитом
physics/navmesh queries и общим лимитом попыток на запрос. Ограничь создание
одним новым layout на общий tick. Конкретные budgets вынеси в config,
измерь на Arland/Everon и скорректируй по evidence.

Сохраняй cursor для справедливого обхода баз, кэшируй неизменяемую metadata,
задай deadline поиска и cooldown отказа. Не сканируй весь world/registry
каждый tick и не создавай настоящие gameplay entities для примерки каждой
точки. События capture/removal/completion инвалидируют snapshot, но не
обходят rate limit запуска новых проектов. Stop снимает все subscriptions,
retries и reservations. Lifecycle invalidation уже активного запроса не ждёт
минутного планировщика.

### 4. Архитектура и оплата

Оставь в `AICF_AICommander` стратегическое решение, а в `AICF_MatchController`
только composition/lifecycle orchestration. Выдели компактный construction
domain: policy/planner, bounded site search, stock placement adapter и состояние
заказа. Не делай широкий рефакторинг MatchController и не превращай
`AICF_BaseBuilderService` во второго экономического/стратегического владельца.

До реализации адаптера проследи в Script Diff `1.8.0.13` полный путь:
provider eligibility → prefab/faction labels → budgets → supplies debit →
unfinished layout → registry/provider link → builder progress → service activation.
Используй проверенные API. Если штатный entry point требует player editor,
создай узкий серверный адаптер к необходимым primitives с эквивалентными
проверками. Не имитируй игрока и не считай вызов player RPC с `-1` решением.

Конструкция должна использовать фактическую стоимость и множители текущего
режима. AICF-side budget admission/reservation/rollback координируется через
`AICF_EconomySystem`; stock pipeline сохраняет свои обязанности. Не списывай
tickets за здание. Не добавляй отдельный supply pool.

Предлагаемый начальный резерв после оплаты — стоимость одного replacement
стандартной группы из текущего economy config; для source base не ниже
существующего `GetSourceReserveSupplies()`. Сделай этот резерв настраиваемым.
Учитывай реальные доступные supplies provider/master resource pool,
действующие обязательства и конкуренцию с reinforcement, deliveries и
player placement; не вычитай уже списанный резерв второй раз.

Поиск/оценка должны быть без побочных эффектов. Не вызывай
`IsThereEnoughBudgetToSpawn()` на каждой пробной позиции: успешный вызов
меняет accumulated budgets. Перед единственным commit заново проверь цену,
остаток, лимиты, ownership, provider, дубликаты и выбранную площадку.
Нужна явная граница однократного применения операции и idempotent request token.

Докажи на полном resource/prop budget lifecycle: ровно одно списание при
успешном заказе, никакого списания при отказе до размещения, отсутствие утечки
reservation и корректный rollback при сбое. При частичном сбое учитывай
автоматический stock refund: не совмещай его вслепую с полным ручным возвратом.
Обычный последующий демонтаж принятого здания использует stock refund policy.
Не очищай чужие accumulated budgets и не возвращай старые средства новому
владельцу базы без подтверждённого контракта операции.

Создавай именно unfinished layout. Услуга остаётся offline до реального
завершения существующим строителем; не вызывай мгновенное завершение и не
добавляй progress без инструмента. Если используются глобальные stock spawn
flags/temporary provider, восстанови их на всех путях выхода и не оставляй
их активными между ticks.

Обеспечь обнаружение принятого AI layout службой строителей, включая участок
внутри provider radius за пределами base radius. При необходимости добавь
узкий validated registration hook, учитывая зависимость штатного placement
event от игрока. Проверь stock replication/JIP для layout, provider и готовой
услуги; одноразовый RPC сам по себе не доказывает JIP.

### 5. Размещение без пересечений

Кандидаты вычисляй от текущего provider и геометрии базы: ограниченные кольца
или сетка, конечный набор orientations, устойчивый tie-break. Не используй
случайный поиск без seed/budget или фиксированные map-specific площадки.

Проверяй итоговую composition целиком, с учётом transform и всех существенных
дочерних частей: footprint, высоту, палатку/здание, ограждения и служебные зоны.
Одной точки origin, layout pivot или небольшого свободного радиуса недостаточно.
Используй bounds/obstruction metadata реального prefab; широкую проверку
пересечения дополняй проверенной физической проверкой необходимого объёма.
Проверяй также незавершённые layouts и pending site reservations.

Обязательные ограничения:

- допустимый building radius и world bounds для всей занятой зоны;
- отсутствие пересечений с постройками, стенами, камнями, деревьями с
  блокирующей геометрией, техникой, персонажами и другими проектами;
- отсутствие воды, недопустимого уклона, проваливания или зависания важных
  частей; несколько terrain samples по площади, а не только terrain Y origin;
- свободные входы, подход строителя и связный путь к его рабочему endpoint;
- сохранение доступа к master tent, spawn points, существующим services
  и проездам; не перекрывать дорогу или единственный проход новой постройкой;
- для depot — свободный штатный vehicle spawn envelope и коридор выезда,
  подходящий поддерживаемой технике. Свободного объёма самого ангара мало.

При unloaded navmesh используй bounded loading/retry по проверенному API;
не принимай отсутствие данных за свободную площадку. Согласуй выбранный
рабочий endpoint с существующим способом подхода строителя: доказательство
доступности другой стороны здания не гарантирует его фактический подход.

Не занимай уже зарезервированные AICF vehicle spawn/staging sites. При
необходимости добавь минимальный контракт чтения взаимных spatial reservations,
сохранив владельцев spawn/cleanup в их доменах. Игроки и техника могут появиться
после проверки: резервируй площадку на время запроса и повтори live validation
непосредственно перед созданием layout. Для перехода к окончательной composition
проверь stock obstruction/lifecycle поведение; если оно не защищает от нового
блокера, добавь узкий guard ожидания безопасного completion без телепортации
или удаления мешающих entities.

Если безопасной площадки нет, верни `NO_SAFE_SITE`, освободи свой временный
резерв и повтори после cooldown. Не уменьшай габариты/отступы автоматически
ради успешного размещения. Проверки доказывают отсутствие пересечения в момент
placement/completion; не обещай защиту от последующего движения объектов.

### 6. Identity и отказы

Заказ хранит stable faction, exact base/provider entity identity, prefab/type,
transform, request token и revision/generation собственного состояния.
Запиши graph revision, если решение использует graph. После ожидания заново
проверь актуальную допустимость; не путай display name базы с identity.

Смена владельца, удаление provider/base, потеря authority, дубликат от игрока,
исчезновение цели, timeout и Stop прекращают неподтверждённый старый запрос.
Поздний callback не может создать/оплатить проект новой стороны или завершить
чужую reservation. Уже принятые stock buildings/layouts подчиняются штатному
capture/lifecycle; не сноси их автоматически из-за отмены planner intent.
Недостижимый проект не должен порождать второй такой же или второго рабочего.

### 7. Диагностика и проверка

Сохрани существующие `BUILDER_*` и Stage events. Добавь согласованные
`CONSTRUCTION_*` events для решения, отказа, site selection, reservation,
placement, rollback и завершения/отмены заказа. Логируй base/provider identity,
faction, тип/prefab, token, стоимость, relevant supplies before/after,
reserve, позицию/orientation, reason, число candidates/queries и длительность.
Свяжи accepted layout identity с последующими builder completion events.
Логи отказов ограничь по частоте; повторы ожидания не засоряют каждый tick.

Добавь focused static/log проверки новых контрактов и representative positive
и negative inputs для анализаторов. Сравни до/после `Test-BaseBuildersStatic`,
`Test-AICommanderModeStatic`, Stage 4, Stage 3, Stage 3.5, RecoveryPolicy и RHS
audits; дополнительные проверки выбирай по реально затронутым файлам согласно
`docs/TESTING.md`. Не ослабляй существующие checks ради PASS.

Workbench Validate/Compile выполни терминально для Arland, Everon и RHS.
Server/client запускай только через `tools/Start-AICFRuntime.ps1`, с fresh
profiles и сохранением `AICF_RUNTIME_MANIFEST_JSON`. Не используй GUI automation,
Computer Use, screenshots или ad-hoc launcher commands.

Runtime matrix должна покрывать:

1. Все пять типов до completion и реально активных stock services; обе стороны,
   stock Arland/Everon и применимые RHS mappings.
2. Недостаток ресурсов, граничное значение `cost + reserve`, конкурентную
   трату и сбой размещения: корректный баланс без double debit/refund.
3. Несколько минут повторных ticks, готовое/незавершённое здание игрока,
   несколько баз: отсутствие дубликатов и соблюдение timing/query budgets.
4. Полностью занятая база, склон/вода, узкий проход, заблокированный depot exit,
   недоступный worker endpoint и новый blocker между search и commit/completion:
   безопасный отказ/ожидание с reason, без пересечения.
5. Потеря/повторный захват базы, удаление provider, cancellation, Stop и stale
   retry; смерть/replacement строителя сохраняют существующие gates.
6. `BOTH`, `US`, `USSR`: новые проекты только AI-controlled стороны; достройка
   player layouts обеих сторон сохранена.
7. Client/JIP до и после завершения: реплицируются layout, worker и готовая
   услуга, нет преждевременной активации или повторной постройки.

Новая terminal fixture должна вызывать production planner/placement path.
Допускается детерминированная подготовка supplies и blockers с явной пометкой
test-only. Нельзя подменить end-to-end прогон прямым бесплатным spawn из старой
builder fixture. Все временные production-копии probes удали и повтори финальный
Workbench gate.

Оцени полные остановленные server/client logs и engine/resource ошибки,
а не только выборку AICF events. Отдельно укажи static, compile, server,
client, JIP, performance/soak verdict. Внешний вид, видимость анимации и
визуальное качество размещения требуют ручного verdict пользователя;
до этого отмечай их `NOT RUN`.

Обнови README/architecture/testing по итоговому поведению и documented
ownership нового construction domain. В финальном отчёте перечисли
outcome, файлы, exact commands/exit codes, evidence paths, новые/сохранённые
failures и `NOT RUN`. Не называй работу `ACCEPTED` самостоятельно.

## Baseline при подготовке задания

Каждая команда запускалась из корня репозитория:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-BaseBuildersStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-AICommanderModeStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Stage35RecoveryPolicy.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-RHSIntegrationStatic.ps1
```

| Проверка | Exit | Verdict |
|---|---:|---|
| BaseBuildersStatic | 0 | PASS |
| AICommanderModeStatic | 0 | PASS |
| Stage4Static | 0 | PASS |
| Stage3Static | 1 | FAIL, 3 известных issues |
| Stage35Static | 1 | FAIL, 2 известных issues |
| Stage35RecoveryPolicy | 0 | PASS |
| RHSIntegrationStatic | 0 | PASS |

Сохранены `STAGE3_PROGRESS_EVIDENCE`, `STAGE3_BOUNDED_PROTECTED_CLEARANCE`,
`STAGE3_MARKER_STATE`, `STAGE35_MEANINGFUL_TASK_PROOF`,
`STAGE35_BOUNDED_PROTECTED_CLEARANCE`. Набор совпадает с отчётом строителей.

Workbench, server/client runtime, JIP, soak и visual в этой оценке **NOT RUN**:
подготовлен только Markdown-промпт. Прежние runtime результаты и их ограничения
описаны в `docs/BASE_BUILDERS_VALIDATION.md`; они не повышены до нового PASS.
