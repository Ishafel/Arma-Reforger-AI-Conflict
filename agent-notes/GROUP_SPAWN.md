# Диагностика создания managed отряда

Владельцы не изменены: `AICF_GroupSpawner` создаёт пустой faction-correct controller,
`AICF_GroupSlot` хранит generation и roster identity, `AICF_MatchController` связывает
observers до `BeginBoundRosterSpawn()`. Native `SCR_AIWorld` владеет очередью,
`SCR_AIGroup.ExpandOneMember()` и `SpawnGroupMember()` — физическим созданием.
Исходный simulation deadline30s, exact count/faction/alive readiness и экономика
сохраняются; диагностическое наблюдение не продлевает срок и не даёт READY.

## Конкуренция после admission и bounded native batch

Admission не резервирует native очередь атомарно. В ordinary USSR generation2
допуск223+5≤228 был корректен, но дальнейшая конкуренция оставила4/5 к исходному
30s timeout. Controlled A/B с одинаковым competing `RequestSpawn(4)` воспроизвёл
target3/5+competitor3/4 и intended229 при hard228. Это отдельная проблема от
длительного отсутствия capacity; batch не решает последнее.

`AICF_RosterSpawnBatch` работает только с explicit managed slot context после
bind/observers/request marker. Перед каждым stock `ExpandOneMember()` повторяются
controller/state/slot/group/generation/spawn-start/request/expected,
EntityID/componentRpl/master/faction и точный список prefab. Global budget берётся
по `max(active,intended)` для **всего остатка**, повторяются `CanActivateGroup`,
`CanAIBeActivated`, faction admission и finite faction limit. Unknown закрывает
batch. Builders/recruit/logistics сохраняют прежний `BeginRosterSpawn()` и queue.

Оптимизация допустима только при пустых authored static vehicles/static waypoints/
dynamic waypoints: иначе сохраняется полный stock `RequestSpawn()` с его эффектами.
Перед batch очищается только native request exact собственной пустой группы через
`PurgeSpawnRequestsForGroup`; getter очереди в API отсутствует. Каждый expansion
требует actual count+1. Нет прироста/временный veto — один stock request на
фактический остаток, без изменения generation/30s clocks. После смены context в
native callback старый вызов не изменяет группу, не ставит queue и не удаляет
successor через внешний failure path. Nested bound восстанавливает context;
unbound callback сохраняет stock queue.

Stock callbacks только фиксируют наблюдения; READY и economy commit выполняет
прежний следующий poll. Полный batch не вызывает повторный `RequestSpawn()`.
`ROSTER_NATIVE_BATCH_DISPATCHED` содержит created/remaining, purge count, elapsed,
componentRpl и реальные global/faction budgets до/после. Существующие event/fields
сохранены: pending=`max(0,expected-actual)`, owner различает `SCR_AIWORLD_REQUEST`,
`AICF_BOUND_NATIVE_BATCH`, `AICF_BOUND_NATIVE_BATCH_THEN_SCR_AIWORLD_REQUEST`.
Это dispatch owner, а не длина native queue.

Evidence: `.codex-runtime/issue78-finalization-20261029/native-spawn-race/`.
Actual production pair337→339: WB0/0, native0/0; before сохраняет30s timeout и
SCRIPT(E), after native5/5 за39ms, OnAdded5/OnAll1, normal commit250/ticket1
на следующем poll,35s неизменных EntityID/componentRpl, target requests0 и
active/intended228/228. After fatal0/leaks0; strict resource gate FAIL28.
Внешняя нагрузка различалась: before один US server, after он уже завершился;
порядок competing request одинаков, runtime sequential. Typed `GetAIAgents`
census обоих empty controllers: counted groups0, `CountTowardsAIWorldLimit()==false`,
actual/intended неизменны. Controller overhead не причина именно этого stock case;
другие prefab этим не доказаны.

## Общая квота и vehicle AI agents

`GetMaxManagedAgents()` по умолчанию220 ограничивает проектный roster. При старте
`EnsureAIWorldCapacity()` ставит native floor `max(previous, managedLimit)`;
builder service затем обеспечивает `managedLimit + bases.Count()`.
В измеренном Arland это128→220→228. Native stock actors, управляемые бойцы и
vehicle AI agents используют один native hard limit. Существующая проверка
admission не является отдельной гарантированной квотой для каждой подсистемы.

В original Multi-v11 достигнуты устойчивые2/4 машины на сторону; следующая
ступень встретила active233/intended226/hard228 и закончилась исходным transit
deadline. В дополнительной preparation с8фоновыми slots по одному бойцу census
показал complete/reconciled152active =148characters+4vehicles, intended148;
позднее228active =221characters+7vehicles, intended221. В последнем snapshot
52characters принадлежат20managed slots,169characters находятся вне этих
отрядов. Последнее число включает всех остальных персонажей, а не только FIA.
Group controllers в этих снимках сами не расходовали активную квоту.

Эти конкретные снимки доказывают участие vehicle agents в native счётчике,
но не доказывают деактивацию конкретной остановившейся машины: для этого нужны
её собственные `IsAIActivated`, LOD и identity-bound movement readbacks.
Поднятие лимита или изменение ordinary состава ради PASS не выполнялось.
Контракт раздельной квоты stock/AICF либо сохранения228 как строгого ограничения
вынесен пользователю отдельно; пока действует существующая политика.

Evidence: `.codex-runtime/issue78-finalization-20261029/movement/`:
`multi-moving-v11`, `minimum-background-v2` и их полные логи/receipts.
Дополнительная preparation не заменяет original stress с production фоном.

## Что измеряет snapshot

К существующему `GROUP_SPAWN_AUDIT`/`GROUP_SPAWN_TIMEOUT` добавлены значения из
закреплённого API1.8.0.13:

- `ai_active`, `ai_hard_limit`, `ai_intended`, `ai_can_activate`, `group_can_activate` — настоящие
  `AIWorld` budget readbacks. Faction limit и глобальный hard limit различны.
- `group_importance`, `group_intended`, `dormant`, `dormant_alive`, `dormant_dead`,
  `expand_complete`, `lifecycle_policy` — текущее native состояние exact group.
- `expansion_calls`, `expansion_successes`, `expansion_age_ms` — наблюдённые
  вызовы native expansion и её исходный boolean. `member_spawn_calls`,
  `member_spawn_age_ms`, `member_last_index` — входы в физический member boundary.
- `placement_deferred` считает отказы поиска безопасной связной позиции;
  `spawn_boundary` различает отсутствие dispatch, backoff, authority veto,
  placement veto, stock retry и stock success с/без фактического прироста agents.
  `spawn_without_agent` считает последний случай отдельно.

Counters локальны самому controller entity, включены только для явно разрешённого
safe managed spawn на authority; неизвестная группа и edit mode идут через super.
Каждый override вызывает super один раз и сохраняет исходный результат. Возраст
до первого вызова равен-1. Snapshot не ставит новый spawn request и не меняет AI
importance, budget, dormant counts, action, здоровье или identity.

`spawning_pending=expected-actual` остаётся **проектным вычислением**, не длиной
native очереди. `engine_queue_observable=0` сохранён: API1.8.0.13 не предоставляет
read-only queue length или position. Разность двух `expansion_calls` позволяет
отличить отсутствие наблюдённых dispatch от повторяемого placement отказа, но сама
по себе не доказывает причину starvation. `CanActivateGroup=false` — наблюдение
admission в момент snapshot, а не доказательство конкретного решения dispatcher.

Native source отдельно имеет ветку `SpawnGroupMember=true` при null member.
Поэтому `expansion_successes` нельзя выдавать за число созданных бойцов; proof
материализации по-прежнему требует настоящие agents, controlled entities и roster.

## Controlled replacement stress

`tests/fixtures/AICF_GroupReplacementStressProbe.c` копируется только в isolated
stage. По CLI `-aicfReplacementStress 1` после штатного `ROSTER_READY` ждёт90s,
затем в двух последовательных waves убивает текущие originals трёх slots каждой
стороны и просит MOTORIZED_LIGHT/desired5 (INFANTRY штатно создаёт только seed1).
Перед kill обе main базы обязаны иметь ONLINE barracks: fixture создаёт и завершает
по одной штатной composition и указывает этот setup в START. Настоящие death callbacks, economy request/reservation,
replacement generation, group bind, native request и30s readiness выполняются
production владельцами. Fixture пополняет supplies main bases, наблюдает и
завершает запуск. Это `CONTROLLED_REPLACEMENT_STRESS`, не обычная война.

Для каждой из12замен требуется exact five alive faction-correct agents и ровно
следующая generation; успешная26-я попытка не подменяет провал2-й. Spawn timeout
в полном логе всегда FAIL. Общий600s deadline завершает неисполнившийся probe.
`Stop()` и finish снимают recurring callback. Native/file/resource errors
проверяются отдельно по полным остановленным logs; targeted success их не скрывает.

Свежие результаты продолжения хранятся в
`.codex-runtime/issue78-finalization-20261029/spawn/`; это evidence, не старый PASS.
Профильный offline контракт: `Test-GroupSpawnDispatchContracts.ps1`, дополнительно
`Test-InfantrySpawnPlacementStatic.ps1`, `Test-Stage35Static.ps1` и
`Test-UniversalScenarioStatic.ps1`. Terminal Workbench и native runtime обязательны
для Enforce изменений. Ограниченный запуск с `SteamAPI_Init failed` не считается
успешной validation; его полный лог сохраняется до отдельного разрешённого повтора.

Отдельный `-aicfReplacementStressWaitCapacity 1` использует classification
`CONTROLLED_NATIVE_CAPACITY_STRESS` и одну wave6groups. Предварительный threshold
явный: `limit - max(active,intended) < 5` для требуемых5actors. До kill все
selected slots должны быть READY с живыми actors. После настоящей гибели и
обычного replacement ожидания нужен **отдельный** native hold readback:
SPAWNING request≥5s, actual<5, free<5 и `CanAIBeActivated()==false`.
`CanActivateGroup(emptygroup)` может вернуть true при совершенно заполненном
лимите: intended count пустой группы равен нулю. Он остаётся диагностикой,
но не является доказательством допуска одного нового actor.
Одного pre-kill saturation недостаточно; освобождённые actors могли снять
давление, тогда coverage FAIL. Global fixture observation ограничено20min;
production spawn deadline остаётся30s. Через65s после kill fixture освобождает
capacity только настоящей гибелью других собственных managed AI, без изменения
лимита или importance. First attempt failure завершает отрицательный запуск
с FAIL; более поздняя generation его не заменяет. `Test-GroupReplacementStressLog`
вызывается с `-Mode Capacity`, отдельно от старого Standard12cases.

## Допуск replacement до расходования generation

`AICF_SpawnAdmission` читает реальные active/intended/hard counters до
`BeginReplacementSpawn`, создания controller и reservation экономики. Проекция
`max(active,intended)+pending+requested` учитывает только отсутствующих бойцов
других SPAWNING slots и ещё не материализованных recruitment donors; уже живой
donor не учитывается второй раз. Неопределённый снимок закрывает допуск.
При недостатке capacity slot остаётся WAITING с прежней generation. Существующее
расписание replacement повторно проверяет допуск; лимит, importance и30s native
spawn deadline не меняются. WAIT и RELEASED дают отдельные native receipts
`GROUP_SPAWN_NATIVE_CAPACITY_WAIT/RELEASED` с исходной generation и budget math.

Новая fixture ветка `-aicfReplacementStressInjectCapacity 1` — отдельная
`CONTROLLED_NATIVE_CAPACITY_INJECTION`, **не** естественная нагрузка. После kill
она останавливает свой BaseBuilderService, временно ограничивает native budget
до `max(active,intended)+1`, через65s восстанавливает исходный лимит; `Stop`
также обязан восстановить его. Production budget/importance не меняются.
Аудитор `-Mode Injected` требует WAITING≥35s с неизменённой generation и
проверяемым production отказом (`projected>hard_limit` либо global veto),
отсутствие request/bind/debit до release и exact5 первой следующей generation.
Для полного batch5 глобальный допуск одного actor может оставаться true.

Свежий отрицательный `injected-v2` подтвердил timeout30.163s: active/intended84,
hard84, actual0, native expansion/member dispatch0; group_can_activate1.
После исправления `injected-v3` дал functional PASS: шесть slots обеих сторон
WAIT с generation1, hold35.149s, restore228 в65.016s, шесть exact5 generation2,
Workbench0/native0/source drift0. Общий strict verdict остаётся FAIL из-за
native resource errors. Старые24таймаута ordinary US не переименованы в этот
контролируемый reproducer: их причина остаётся недоказанной без старых counters.
