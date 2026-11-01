# Native human Catalog / FIFO — терминальная проверка

`tools/Invoke-AICFQuotaCatalogProbe.ps1` воспроизводит запрос настоящего игрока
через штатный `SCR_CatalogEntitySpawnerComponent`. Это отдельный gate от
managed GroupSpawner pressure и от позднего появления экипажа внутри машины.
Offline replay и Workbench не подтверждают native покупку.

## Public runner

Запускай фазы из корня репозитория. `Prepare` требует новый evidence-каталог:
он фиксирует все raw файлы семи addons, добавляет только Catalog fixture,
сохраняет аудит и launcher, выполняет терминальный Workbench и связывает
compile receipt с source manifest. При необходимости `-SourceRoot` указывает
на заранее проверенный fixture-free frozen payload. Встроенные stages других
тестов источником production payload не являются.

```powershell
$catalogEvidence = '.codex-runtime/catalog-current'
pwsh -NoProfile -File tools/Invoke-AICFQuotaCatalogProbe.ps1 `
  -Phase Prepare -EvidenceRoot $catalogEvidence -Case group -Variant Stock
```

`Server` и `Client` запускаются в **двух разных терминальных сессиях**.
Оба используют только canonical `Start-AICFRuntime.ps1`. Перед Client launcher
проверяет точный CLI живого Server и `[ROSTER_READY]`. Для server/client pair
нужны оба выделенных native слота и свежая проверка свободной памяти >8GiB;
согласуй запуск с владельцами других native проверок.

```powershell
pwsh -NoProfile -File tools/Invoke-AICFQuotaCatalogProbe.ps1 `
  -Phase Server -EvidenceRoot .codex-runtime/catalog-current -Case group -ServerPort 2670

pwsh -NoProfile -File tools/Invoke-AICFQuotaCatalogProbe.ps1 `
  -Phase Client -EvidenceRoot .codex-runtime/catalog-current -Case group -ServerPort 2670
```

Client использует публичную последовательность `RequestFaction` →
`RequestLoadout` → валидный native spawnpoint → `RequestSpawn`, затем передаёт
owner RPC готовности. Сервер заново определяет player/controller/body/faction.
CLI autodeploy, поддельный UUID и ручная установка avatar/readiness отсутствуют.

После завершения **обоих** процессов:

```powershell
pwsh -NoProfile -File tools/Invoke-AICFQuotaCatalogProbe.ps1 `
  -Phase Audit -EvidenceRoot .codex-runtime/catalog-current -Case group -ServerPort 2670
```

`-Case character` и `-Case vehicle` используют отдельные свежие profiles в том
же подготовленном stage; запускай их последовательно. Не меняй frozen stage,
auditor или launcher между Prepare и Audit.

## Что должен подтвердить runtime

- `group`: native отказ целой authored корзины при N−1 свободных STOCK units,
  без header/body/callback/debit; после освобождения собственной pressure
  reservation — native header, ровно N физических тел, BOUND/closed receipt
  до native activation callback и одно фактическое списание.
- `character`: одно физическое тело и одно фактическое списание.
- `vehicle`: заполненная character STOCK квота не блокирует покупку пустой
  человеческой машины; native compartment readback доказывает отсутствие
  occupants/reservations и counted vehicle agent.

Positive требует 25s неизменного physical/payment tail. Server ограничен
исходными 300s; timeout с нулём созданных тел остаётся FAIL. Fixture выбирает
только уже доступные native entries и не добавляет/включает каталог ради PASS.
Для известного отсутствия supplies разрешён один явно отмеченный controlled
funding shortfall через native `AddSpawnerSupplies`/generator, с точным
consumer readback до pressure. Цена и AICF economy не изменяются. Оплата
проверяется по `GetSpawnerResourceValue`, а не устаревшему custom balance
`GetSpawnerSupplies`; отсутствие фактического consumer debit остаётся FAIL.

Read-only `CATALOG_ENUMERATION`/`CATALOG_METADATA` описывает реальные faction,
`m_aCatalogTypes` и уже отфильтрованный `m_aAssetList`: тип, enabled spawner
data, authored count и цену. Обход ≤256 и `scan_complete` явно отличают
полное наблюдение от непроверенного suffix. Наблюдатель не вызывает admission,
GetFreeSlot, создание или оплату и не меняет условия selector.

## Verdict и сохранённые ограничения

Audit отдельно сохраняет native exit, functional proof, full sibling fatal,
resources и все прочие `(E/F)`. Любой из этих FAIL оставляет общий strict FAIL.
Client проверяет реальный bootstrap; server proof не заменяется client proof.
`Game destroyed` должен следовать за proof. Полные sibling logs, canonical
manifests, native exits и SHA сохраняются рядом с case summary.

На 30.10.2026 public v11 на human383 завершён native0/0: actual human binding
retirement и восстановление census подтверждены, но GROUP покупка не началась
за300165ms (factory/callback/debit0) — functional FAIL. Server fatal0/resource6/
other24; Client bootstrap PASS/fatal0/resource118/other24.

Metadata v12 доказал три штатных producer USSR с `catalog_types=30` (VEHICLE),
по13/13 полностью просмотренных записей: GROUP40 отсутствует. После трёх
одинаковых снимков выполнен адресный stop/native−1; GROUP gate остаётся FAIL,
причина setup доказана, production selector не ослаблен. Отдельный vehicle case
получил штатный `NOT_ENOUGH_SUPPLIES` для всех записей и тоже сохранён как FAIL
до создания/оплаты. Controlled funding/consumer oracle проверяются новым stage;
native покупка ещё не подтверждена. Полноценный GROUP/CHARACTER gate требует
реального producer с совместимыми authored GROUP slots, а не подстановки
vehicle slot или обхода GetFreeSlot.

Текущий v17 (`catalog-native-v17-service-response-public`) на тех же383 raw
production завершился **native0/0**, full sibling fatal0 у обеих ролей, но
server functionalFAIL: `CONTROLLED_NATIVE_FUNDING_NOT_CONFIRMED`,34318ms,
factory/callback/debit0. Server resources14/other24; Client bootstrap PASS,
resources118/other24. Оба strict audits FAIL; адресное завершение не применялось.
Final escalated CIM22:05:52: все native процессы завершены.

Actual единственный `RequestGeneration` вернул `SUFFICIENT`, available space1200,
requested200, multiplier0.333, storage range135, actions Store/DropContainers.
До него generator/consumer queues0/0; после generator11, consumer0,
exact-pointer intersection0, все просмотренные generator containers имели
`linked_consumer=0`. `GetBases` выбранного service point вернул0. Consumer
readback остался0, поэтому funding не подтверждён. Post-container value200
**не доказывает** delta+200: его физическое значение до grid refresh не снято.
Lazy queue update и отсутствие service/base linkage ещё различаются source
review; `SUFFICIENT` не является доказательством оплаты или готовности consumer.

Из четырёх собственных profile exports подтверждены штатные
`RequestTable_US_Barracks.et`/`RequestTable_USSR_Barracks.et` с catalog types
CHARACTER/GROUP и max slot distance7; оба character-slot ресурса имеют authored
GROUP_SMALL. Эти resources ещё не были созданы и связаны в новом controlled
native setup. Native положительная покупка CHARACTER/GROUP, full-basket
negative→positive и empty VEHICLE при полной character квоте — **NOT RUN /
не доказаны**; существующие setup FAIL сохраняются отдельно.

v17: fixture `B6F40173…ED139`, auditor `ACEA76CA…CEB52`, contract
`A3BB2B69…BF7821`; source manifest `F16CF5CB…A21AB2`, Workbench0,
post-source SHA drift0. Независимые актуальные проверки:14funding/3unsafe,
6payment/1unsafe,9factory callback,7single-super generation и8queue-link cases,
17funding/10vehicle log controls; прежние actual callback/CLI cases сохранены.
Public runner7helpers/11separate-process CLI PASS. Это targeted scope, а не новый
полный All. v13 duplicate fixture override compileFAIL, v16 base-generator
serialization compileFAIL и прежний NaN-before→1write counterexample сохранены;
текущий finite guard отвергает его с0writes. Observer ограничен конкретным
ServicePoint subclass и не копирует static native serialization.

Evidence: `.codex-runtime/issue78-finalization-20261029/quota/`
`catalog-native-v11-human-bridge-public/REPORT.md`; frozen source/compile receipts,
`group/full-stopped-log-sha.json` и отдельные role audits. Предыдущий native AV
в v3 остаётся UNKNOWN; guarded loadout index−1 не является доказанной причиной.
Текущие полные stopped logs/CLI/exits/audits и source manifest — в каталоге v17;
metadata exports — `vehicle/Server-profile/profile/AICF_CatalogMetadata_0.et`
…`_3.et`. Отдельные independent peer receipts —
`.codex-runtime/issue78-finalization-20261030/catalog-generation-peer/` и
`catalog-funding-peer/`; полные SHA и outcomes связывает v17 REPORT/manifest.

Релевантные offline проверки: `Test-AIQuotaCatalogContracts.ps1`,
`Test-AIQuotaCatalogBootstrapContracts.ps1`, `Test-AIQuotaCatalogProbeContracts.ps1`
и `Test-AIQuotaCatalogRunnerContracts.ps1`. Последние исполняют actual callback/
observer replay и реальные отдельные WinPS5.1 CLI positive/negative случаи.

## Pending consumer readback после штатных подписок — v19

Fixture устраняет пропуск normal resource lifecycle перед прямым Catalog
request. Authoritative server закрепляет первый producer, entry, quote,
consumer и generator; owner client получает их реальные Rpl IDs и запрашивает
два `RequestSubscriptionListenerHandleGraceful` через собственный настоящий
`SCR_ResourcePlayerControllerInventoryComponent`. Это явная setup-операция,
аналогичная native Catalog user action, а не read-only observer. ACK
`RESOURCE_HANDLES_REQUESTED` доказывает запрос точных handles; регистрацию
listener на сервере он **не доказывает** (`server_registration=UNOBSERVED`).

После ACK сохраняются фактические `before`/shortfall и до вызова native
generator устанавливается once-latch. Если cached consumer ещё не обновился,
fixture читает тот же consumer в пределах исходного общего300s, без повторной
генерации, `RequestAvailability`, `UpdateInteractor`, private balance/queue
writes или изменения цены. Funding подтверждается только finite readback
не ниже цены и точной delta. Настоящие `GetRequestState`, `GetFreeSlot`,
factory, payment и25s physical tail остаются обязательными. Смена identity,
serial, quote или player прекращает setup; другой producer/entry во время
pending не подбирается. `GetBases=0` не превращается в доказанную normal
GROUP/Barracks покупку: этот отдельный setup пока отсутствует.

Client проверяет captured handles/serial после первого `Poke`, после `Tick`
и после ACK перед повторным callback. `Stop` удаляет свой timer, освобождает
handles и при сохранённом точном контексте запрашивает unsubscription.
При утраченном контексте новых запросов нет; graceful manager retention
истекает штатно, дальнейший `Poke` прекращается. Server readback повторяет
captured identity до и после getter/guard boundaries; финальное подтверждение
выполняется только после чистого сравнения того же состояния.

Проверки v19: actual funding replay22cases/5unsafe и actual client
handle/`Tick`/`Stop` replay13cases/3unsafe — PASS. Сохранены предыдущие
callback, payment, enumeration, native-super, full-log и separate-process CLI
controls. Terminal Stock Workbench: exit0, validation successful, fatal0 во
всех трёх полных sibling logs, raw384+fixture=385 и source drift0. Это compile
и targeted replay, **native v19 NOT RUN**, полного offline набора после
этого delta ещё нет. Прежний native v17 funding FAIL остаётся FAIL.

Evidence: `.codex-runtime/issue78-finalization-20261030/`
`catalog-native-v19-readback-owner-fences/final-compile-source-verdict.json`;
actual replay — `issue78-finalization-20261029/quota/catalog-adapters/`
`catalog-pending-after-v4.txt`. Fixture SHA `76CDB491…0437A9`, source manifest
`D2AEA774…98B240`; точные hashes находятся в verdict.

## Native v20 и точные resource checkpoints v21

V20 использовал production384 frozenV25 с исправлением MOB и прежний Catalog
fixture `76CDB491…0437A9`. Отдельные public Server/Client завершились native0/0,
но server получил исходный `BOUNDED_TIMEOUT`,300798ms: выбранных entry,
factory/callback/debit/header0. Client выполнил настоящий bootstrap игрока USSR.
Server fatal0/resources14/other24; Client fatal0/resources118/other24; оба strict
FAIL. Ни ACK, ни подтверждённого funding нет. V20 не записывал owner-client
resource Start checkpoints, поэтому конкретный первый false guard **UNKNOWN**;
отсутствие ACK не устанавливает причину. Полные остановленные логи и source
receipts: `.codex-runtime/issue78-finalization-20261030/catalog-native-v20-mob-source/`.

Первый WinPS5.1 wrapper завершился1 на native stderr warning до scripts, native
exit UNKNOWN; его профиль сохранён отдельно. Повтор использовал документированный
`pwsh`7 для обеих ролей; source/launcher/сроки не менялись. Это подтверждает
требование shell из DEVELOPMENT, а не исправление Catalog.

V21 добавляет только bounded diagnostics: authoritative dispatch с фактическими
producer/resource Rpl IDs, один pending serial receipt, получение owner RPC,
Start checkpoints, actual первый false guard `Current`/`ExactHandle`, результаты
получения native handles и cached ID/type comparisons. Каждый interactor getter
вызывается ровно в прежнем месте short circuit; для логирования используется
полученное значение, дополнительные gameplay getters/CanSpawn/Current и refresh
не вызываются. До64distinct notes на serial/stage; overflow означает неполное
наблюдение. Stop очищает diagnostic notes при game end. Однократный generator,
native funding/price/rank/slot/quota gates и исходный300s остаются прежними.

Свежие targeted controls:22funding/5unsafe,13client handle lifecycle/3unsafe,
22actual Current guard cases/3unsafe — PASS. Последние сравнивают результат и
точный префикс/порядок16public native-boundary getters с прежним compound guard;
для handle comparison отдельно проверяется short circuit всех6getters.
Прежние callback/payment/log/native-super/separate-process CLI controls сохранены.
V21 native **NOT RUN**; его отдельный Workbench выполнен ниже. Полный261PASS
относится к frozenV25 до этого diagnostic delta. Старый v20 не переоценивается.

## Client identity v22: исправление fixture

Source review выявил детерминированный дефект owner-client setup: production
`AICF_AIQuotaCatalogIdentity.Capture` и `IsCurrent` требуют server realm даже
при `requireMaster=false`. Применение этого helper на client гарантированно
возвращает null, если выполнение дошло до capture; последующий `Current`
не позволяет отправить ACK. V20 не записывал получение RPC и Start, поэтому
фактический первый runtime guard того запуска остаётся UNKNOWN. Source proof
не доказывает прохождение более ранних native/RPC gates.

V22 вводит только в opt-in fixture `AICF_QuotaCatalogClientIdentity` для двух
полей ResourceClient: user и producer. Он требует client realm, ту же entity,
main world, EntityID и world lookup, тот же присутствующий Rpl component и
валидный неизменный RplId. Capture завершается повторным `IsCurrent`.
Client master/proxy роли допустимы: authority не выводится из роли Rpl.
Проверки local controller, PlayerManager и controlled body остаются прежними.
Production server helper, server authority, quota/factory/economy gates,
funding и исходные300s/25s не меняются.

Прежние13lifecycle cases проверяли handles/`Tick`/`Stop` с подставленным
контекстом и не исполняли identity capture: их PASS не доказывал client setup.
Новый replay исполняет actual тела нового helper и production server helper:
старый Capture отвергает client, новый принимает обе client Rpl роли,
отвергает server и stale world/entity/lookup/Rpl/ID/teardown.30cases и7executed
unsafe controls — PASS. Все прежние source/replay/CLI controls сохраняются;
diagnostic wrapper extraction теперь ограничен точными modded классами,
чтобы новый одноимённый Capture не подменял тестируемый wrapper.

V21 immutable stage с fixture `B084D9A6…D1165F` скомпилирован: exit0,
validation successful, fatal0 во всех3полных sibling logs, raw384+fixture385,
source drift0. Native v21 **NOT RUN**; stage не обновлялся после v22 delta.
V22 fixture `9848FDB7…AD5B4` прошёл свежий Stock WB в stage-v23:
exit0/validation/full3sibling fatal0, raw384+1=385/sourceSHA0; frozen V27
содержит topology fix `83E62641`. Compile session85753 closed0. Первый stage-v22
запуск без необходимых прав завершился platform initialization FAIL/nativeWB-1,
без validation success; его полные logs сохранены, stage не изменён.
Исправный повтор выполнен явно elevated через `pwsh`. Native нового helper
пока **NOT RUN**; эти compile/targeted результаты не переоценивают v20 timeout.
Новый полный V27 завершён262/262PASS, collector0/exact union/Logistics1/
1021SHA0/6actual sessions closed0; он включает новый client helper и diagnostics.
Позднее doc-only уточнение результатов не изменяет frozen source.
Evidence v21 compile и v22 controls:
`.codex-runtime/issue78-finalization-20261030/catalog-native-v21-guard-diagnostics/`.

## Native v23: достаточный баланс после subscription

V22 source с отдельной client identity прошёл fresh elevated Workbench в stage
v23 на frozenV27: exit0, validation successful, fatal0 во всех3полных sibling
logs, raw384+fixture385, source drift0. Первый v22 запуск с неверными правами
завершился native-1 на Steam platform init до validation; его логи сохранены.

V23 public separate Server/Client дали native0/0. Actual client RPC42 получен,
user/producer capture успешны, оба native subscription handle точны, ACK42
доставлен серверу. Это подтверждает устранение client-realm дефекта fixture;
ACK сам по себе не доказывает server resource registration. Следующий actual
guard01:30:46.947 — `FUND_BEFORE_REJECT`: native consumer15600 при quote200,
shortfall-15400. Pending setup продолжал требовать положительную недостачу и
не возвращал управление к native selection. DONE01:35:02.694 — исходный
`BOUNDED_TIMEOUT`,300249ms, selected/factory/header/callback/debit0.
Server functional FAIL, fatal0/resources14/other24; client bootstrap functional
PASS, fatal0/resources118/other24; strict оба FAIL. Полные остановленные логи,
native exits, actual отдельные сессии и escalated process closure сохранены в
`.codex-runtime/issue78-finalization-20261030/catalog-native-v23-client-identity-elevated/`.
Причина перехода native balance к15600 отдельно не установлена; readback не
превращается в утверждение о конкретном grid callback.

Следующий fixture delta подтверждает уже достаточный native consumer только
для собственного pending setup после matching ACK, при неизменных captured
player/user/producer/resource/consumer/generator/entry/quote/serial. Повтор
контекста выполняется после native getters и финального funding guard.
Finite readback>=quote устанавливает только FundingConfirmed: generation не
вызывается, FundingAttempted не подделывается. Отдельный receipt
`NATIVE_ALREADY_FUNDED` содержит amount0/generation_attempted0. Положительная
shortfall ветвь сохраняет единственный native generator и точный delta proof;
начатый generation pending не может переключиться на обход delta.

Штатные GetRequestState, slot, quota, paid native request, ровно один actual
consumer debit и25s physical tail сохраняются. Auditor не меняется: generator
receipt уже optional, настоящий payment обязателен. Actual replay старого Fund
при15600/200 возвращает false, новый подтверждает только matching ACK без
генерации;49cases/7unsafe — PASS, прежние22funding/5unsafe — PASS. Это source
и replay; весь Catalog contract и3соседних контракта — PASS.

Свежий v24 terminal elevated Workbench: exit0, validation successful,
fatal0 во всех3полных sibling logs, source drift0. Это явно hybrid stage:
production384 frozenV27 + reviewed checkout fixture `C1FF0C13…D9FEE7`,385files;
public runner frozenV27 использовал RepositoryRoot=live и SourceRoot=frozenV27,
runner/launcher/auditor SHA совпадают. Manifest `91E7EAF1…F498953`, evidence:
`.codex-runtime/issue78-finalization-20261030/catalog-native-v24-already-funded/`.
Оба набора source receipts и полный compile verdict сохранены. Full262PASS
относится к frozenV27 и **не покрывает** этот поздний fixture delta.
Native v24 выполнен: **strict FAIL**, результаты и следующий vehicle-only delta описаны ниже.
Свежий compile: `catalog-native-v23-client-identity-elevated/compile-source-verdict.json`.

## Native v24: supplies готовы, выбор слота не завершён

Обе отдельные public роли v24 завершились штатно, native0/0; полные
остановленные sibling logs и role/session closure сохранены. Server functional
FAIL: DONE02:02:04.383, `BOUNDED_TIMEOUT`,300616ms, factories/headers/callbacks/
debits0. Client bootstrap functional PASS, оба strict audits FAIL: server
fatal0/resources14/other24, client fatal0/resources118/other24. Это **не**
успешная покупка.

`01:58:45.617 NATIVE_ALREADY_FUNDED` действительно подтвердил native consumer
15600 при quote200, без нового generation. Затем183 `ENTRY` относятся только к
Ural4320_transport, native_state8 (`CAN_SPAWN`). Четыре same-faction producer
0262/03BA/03BB/23C1 перечислены, но ранний FundingSpawner/Entry binding исключил
альтернативы. Единственная следующая ветка опубликованного selector —
`GetFreeSlot(data)`; его null — вывод из control flow, не сохранённый return
receipt. Реальные типы, занятость, cache state и расстояния слотов в v24
**не наблюдались**. Нельзя утверждать, что перестановка выбора сама по себе
гарантирует native покупку.

Полные логи/выходы/audits:
`.codex-runtime/issue78-finalization-20261030/catalog-native-v24-already-funded/`.
Проверенная C1FF-копия, hashes, факты и план:
`.codex-runtime/issue78-finalization-20261030/catalog-vehicle-slot-selection-readonly/`.

## Vehicle-only preflight перед funding binding

Для `vehicle` selector теперь ищет настоящий совместимый native free slot
**до** первого binding/subscription/funding. Null-slot кандидат пропускается;
проверяются остальные native entries и producers. Режимы `group`/`character`
сохраняют прежний selector. Новая ветка не добавляет каталог, не меняет slot
type/registry, native cache, цену, rank, cooldown, квоты или factory.

Штатный `NOT_ENOUGH_SUPPLIES` возвращается раньше faction/rank/cooldown checks,
поэтому до funding выполняются реальные `GetRequesterFaction`, `RankCheck` и
`CooldownCheck`. После ACK и непосредственно перед запросом сохраняются
штатные `GetRequestState`, `InitiateSpawn` и `PerformSpawn`. Последний заново
проверяет реальную совместимость и занятость preferred slot; native veto не
обходится. Исходные300s, один native generator при shortfall, уже достаточный
consumer без генерации, фактический debit и25s physical tail не изменены.

Pending candidate связывает точные producer/user/entry/data/quote и native slot
component/owner entity/world/Rpl identities. Slot owner требует authoritative
валидный Rpl; наличие его у всех штатных slots **не предполагается**. Если
capture отказал, `NATIVE_SLOT_CANDIDATE_REJECT` отражает доступные cached facts,
а identity не ослабляется ради PASS. После binding смена candidate/slot/context
завершает тест до factory/debit; другого producer/entry и второго funding в
этом scope нет. Frame/request callback, остановивший owner, не запускает native
request. `Stop` очищает observer и candidate.

`NATIVE_SLOT_OBSERVATION` фиксирует actual native type, compatibility и
occupancy calls, free-cache hit, registered Child/Near arrays, occupied-cache
membership snapshot, effective radius и позиции. Все overrides вызывают
`super` ровно один раз и возвращают его результат без изменений. Для логов
нет дополнительных `GetFreeSlot`, `IsOccupied` или physics traces. Это важно:
штатный `IsOccupied.TraceCallback` сам может удалить destroyed vehicle wreck;
его нельзя считать пассивным getter и нельзя дублировать ради диагностики.

Native cache interval из pinned1.8.0.13 —6000ms. Cached free не выдаётся за
fresh empty proof; occupied-cache snapshot отдельно от actual occupancy call.
Near-slot правило использует strict XZ radius и parent service; Child slots
регистрируются отдельно. Metadata/позиции помечаются как snapshots, без
утверждения об atomic или историческом registration. Записи ограничены512
различными ключами, overflow явный. При потере frame печатается отдельный
`SLOT_OBSERVER_SCOPE_LOST`, без изменения successor/shared keys.

Offline gate на точных fixture `77429416…E1CD` и объединённом contract
`FAC68763…D4C0`: оба Windows PowerShell5.1 и pwsh7 — PASS всего прежнего Catalog
контракта и portable namespaced addendum. Addendum:39 cases (включая2 observer),
2 executed old-selector pin replays и6 actual request-wrapper cases. Он
показывает старое binding на неподходящем Ural, выбор альтернативы новой веткой,
stale identity/ACK/context veto, callback/overflow, native-call single-super
counts и request space veto на явно моделируемых native boundaries. Два unsafe
mutants (`producer-component`, `observer-scope`) дают ожидаемый FAIL в обоих
shells на тех же source/contract hashes. Нового canonical test-файла нет:
addendum включён в прежний `Test-AIQuotaCatalogProbeContracts.ps1`.

Свежий public `Prepare` Stock v25 выполнил терминальный Workbench на387 файлах:
386 frozenR30 production + fixture `77429416…E1CD`. Actual session74802 CLOSED0,
native0, `Script validation successful` в04:31:35.459. Повторно проверены все три
полных остановленных sibling logs: fatal0/other0, RESOURCE(E)50 суммарно
(дублированные UI leak записи console/error). Последние сохранены отдельно:
compile-specific **PASS** не означает отсутствие всех ошибок в журналах.
Stage/source frozen386 SHA drift0, actual/expected387, auditor `ACEA76CA…EB52`,
manifest `1F0ADA6D…1752`, source/compile receipts связаны. Expanded receipt:
`.codex-runtime/issue78-finalization-20261030/catalog-native-v25-slot-preflight/expanded-compile-verdict.json`.
Fresh separate-role native gate этого delta пока **NOT RUN**. Прежний Full не
покрывает позднюю fixture; offline модель и compile не доказывают физическую
покупку, payment или исправление native slot setup. Evidence полного combined
dual gate и последнего exact-hash mutant повтора:
`.codex-runtime/issue78-finalization-20261030/catalog-vehicle-slot-selection-readonly/implementation-v1/`.

Незавершённый controlled GROUP setup удалён из текущих трёх файлов и сохранён
отдельно с точными SHA в `live-group-wip-preserved-manifest.json` этого evidence.
Он **не готов**: standalone Barracks RequestTable не содержит нужного native
ResourceComponent, полная LivingArea hierarchy не экспортирована. Попытки
metadata Workbench v1–v5 не дали пригодного экспорта; их native exit0 не
считается proof. GROUP full basket и native positive остаются неподтверждёнными,
vehicle gate их не заменяет.

## Native v26: регистрация serialized data observer

EOF-only исправление удалило2 лишних LF в конце fixture (77429416 →485970BD),
не меняя остальных байтов или поведения. Fresh v26 public Stock Prepare на
386 frozenR30 production +fixture =387 завершился actual50727 CLOSED0/native0;
validation/full3fatal0/other0, RESOURCE(E)50 сохранены, source drift0. Старый v25
stage с77429416 остался неизменным. EOF proof:
`catalog-vehicle-slot-selection-readonly/implementation-v1/eof-only-proof.json`.

V26 Server81236/Client97717 завершились штатно native0/0 после исходного300s;
полные остановленные sibling logs/audits сохранены. Server DONE04:51:09.503 —
BOUNDED_TIMEOUT300299ms, factory/header/callback/debit0. Server functionalFAIL,
fatal0/resources14/other748; Client bootstrap functionalPASS,
fatal0/resources118/other748; strict оба FAIL.

Раньше первого selector, в04:46:01.700, native deserializer сообщил
`Unknown class 'SCR_EntityCatalogSpawnerData'`. В каждом console362 таких ошибок,
в шести sibling logs суммарно1448 с учётом дублирования. Все наблюдавшиеся
same-faction VEHICLE producer имели assets0; slot observer и funding не были
достигнуты. Это regression новой test fixture, а не доказательство проблемы
Core, rank или GetFreeSlot. Early vehicle dispatch не пропустил native list init:
исходный selector также начинал непосредственно с существующего m_aAssetList.

В отдельном новом source v27 восстановлены **обе** точные исходные аннотации
над `modded class SCR_EntityCatalogSpawnerData`: `BaseContainerProps(configRoot:
true)` и `BaseContainerCustomCheckIntTitleField` с исходными аргументами.
Pinned Script Diff1.8.0.13 original API SHA:
`76D8618626FC0095C5919C24F98B452DD4F4E693AE34CFE4904356B592FF8806`.
Dummy fields, native slot/price/rank/quota/factory условия не добавлялись.

Новый контракт проверяет точный соседний header настоящего code span, а не
comment/string, и4unsafe source controls. Все прежние Catalog/observer/callback/
funding controls и portable39/2old/6request cases сохранены; full targeted
Windows PowerShell5.1 и pwsh7 PASS. Свежий public Prepare v27 прошёл терминальный
Workbench на том же386+1fixture/raw387: actual22131 CLOSED0/native0,
full3fatal0/other0/resource50FAIL/source387SHA0. Fixture3ED078FB,
manifest114957227FD7CC29AA9F5FD15BC0E0D1616C733080A9FE5D3F002D9A3B6C08B3,
expanded receipt в `catalog-native-v27-serialized-data-registration/`.
Native v27 подтвердил отсутствие этого UnknownClass и по13 настоящих assets
у трёх исходных USSR producer. Их slot arrays были пусты, но после обычного
строительства появился producer23FB/slot23F8, type8/compatible1. В05:05:20.707
штатный TraceCallback:306 разыменовал null Physics, VM остановил Tick до
исходного300s deadline. Покупка/payment/tail **FAIL**, factory/debit0.
Оба owned процесса принудительно остановлены после exact PID/profile proof:
Server40559/Client14403 CLOSED1/native-1. Full7logs: Server fatal8/resource10/
other24, Client fatal0/resource2/other24; четыре собственных path-only metadata
Resource.Load дают8 Server resource rows. All-source387SHA drift0 сохранён.
V26 и v27 FAIL не переименовываются в PASS; entity hit category v27 NOT PROVEN.

Evidence: `.codex-runtime/issue78-finalization-20261030/catalog-native-v26-slot-preflight/`
и его `serialized-container-fix/`; старые stage/manifest/auditor неизменны.

Следующее source изменение добавляет только hook наблюдения неизвестного
occupancy hit из [NATIVE_SLOT_PHYSICS_GUARD.md](NATIVE_SLOT_PHYSICS_GUARD.md).
Core уже пометил unknown и возвращает occupied; fixture сохраняет исходный
reason/наличие entity и после решения читает class/EntityID/prefab через
однократный cached EntityPrefabData. Эти snapshots не atomic и не доказывают
категорию прежнего v27 hit. Активный exact LastSlot/frame проверяется после
каждого native getter, затем используется прежний bounded512 Write/Lost.
GetPhysics/IsOccupied/TracePosition для наблюдения не вызываются повторно.
Идентификация удалённой entity проверяется после native context validation;
старый порядок проверок воспроизведён executable unsafe control.

Прежние39/2old/6request cases и4 registration controls сохранены. Новые13
unknown-hit cases покрывают null, неактивный/другой slot, чужой мир,
Stop на metadata getters и удаление hit внутри проверки текущего context.
Source replay границ native — SIMULATED. Новые WB/runtime gates ещё NOT RUN,
включая проверку overridability нового hook между modded class слоями.

## Controlled paid LIGHT_DEPOT setup

V28 прошёл full388 compile (actual60357 CLOSED0, fatal0/other0/resource50),
но native Server37583/Client28741 закончились штатно0/0 без покупки:
BOUNDED_TIMEOUT300100ms, factory/header/debit0. Все72 записанных slot arrays
пусты, TYPE/OCCUPANCY/unknown-hit0, BUILDER_COMPLETED0. Поэтому отсутствие VM
не подтверждает выполнение Physics guard. Full6logs: server fatal0/resource14/
other24, client fatal0/resource118/other24; strict оба FAIL. 25s paid-vehicle
tail не достигнут, его правило не ослаблено. Evidence:
`catalog-native-v28-physics-guard/final-verdict.json`.

Новый **opt-in**, test-only setup включается `-ControlledDepotSetup` у
`tools/Invoke-AICFQuotaCatalogProbe.ps1` во всех фазах Prepare/Server/Client/Audit.
Runner закрепляет bool в manifest, допускает только Case=vehicle и проверяет
точный `-aicfQuotaCatalogControlledDepotSetup 1` в canonical и actual CLI.
Fixture читает flag один раз до original OnGameStart; default-disabled
поведение сохраняет прежнюю стратегию строительства. Это отдельный
`CONTROLLED_SETUP ordinary_proof=0`, не ordinary builder/construction proof.

В точном synchronous scope original Planner.Decide для реальной USSR main
base меняется только начальный cursor original Commander.SelectConstructionType
на LIGHT_DEPOT. Другие базы/фракции идут прежним путём; covered/disabled/
affordability veto не обходятся. Metadata, geometry, provider radius, native
path, clearance, props budgets и original paid Adapter.Place остаются
штатными. Координат или имён баз нет. Pending order привязывается к точным
state/planner/base/provider/faction/IDs/token/revision/metadata; native callback
не может подменить его другим same-faction order.

Полный untouched prefab берётся из ContentProfile: US
`{A0FEA3E718F5B605}.../E_VehicleMaintenance_S_Conflict_US_01.et`, USSR
`{5ACECF41C167567D}.../E_VehicleMaintenance_S_Conflict_USSR_01.et`.
V27 фактически оплатил второй prefab за150 (900→750, debit_count1) и получил
native producer23FB/slot23F8. Но его очередь ждала SMALL_BARRACKS до134s;
новая test-only priority устраняет эту зависимость, не обещая успех при
занятой геометрии или отказе оплаты.

Только после original successful Place, accepted/paid receipt и точного
списания допускается один controlled public
`layout.SetBuildingValue(layout.GetToBuildValue())`. Original CompletionClear
и native EvaluateBuildingStatus/SpawnComposition обязательны. Stop внутри
clear/max/readback не допускает продолжения. Root/layout identities требуют
текущего master Rpl и running/master campaign; async Rpl readiness ожидается
на той же созданной entity, без повторного factory. Planner.Stop и общий
fixture Stop инвалидируют epoch и очищают project references.

Native lifecycle сам создаёт services/resources/slots. Registry arrays,
slot type/cache/Physics не изменяются. Иерархия проверяется ограниченным
scan256; producer identities собираются локально и публикуются только своим
scan/epoch. Старый callback не вставляет элементы и не очищает successor
cache. Для найденных enabled defenders используется только native
EnableSpawning(false, actual playerID), с собственными faction/rank проверками
и readback. Существующие тела сохраняются; constructor births/reservations
входят в текущий stock ledger перед прежним pressure. Цена машины, native
permissions, ACK/funding, factory, debit, occupancy и empty25s tail не менялись.
Причины ожидания пишутся максимум по16 distinct keys, без повторных native
getter ради логов. Весь setup входит в исходные300s, clock не сбрасывается.

Четыре прежних path-only metadata Load заменены canonical ResourceNames:

- `{F8A337C38BB8D4EA}Prefabs/MP/Campaign/RequestTable_US_Barracks.et`;
- `{2337DFDD05C26996}Prefabs/MP/Campaign/RequestTable_USSR_Barracks.et`;
- `{4B1971606FBC57DF}Prefabs/Systems/Compositions/EntitySpawnerSlot/EntitySpawnerSlot_Character.et`;
- `{9540DC5C8AC4E6EE}Prefabs/Systems/Logic/CharacterSpawnerSlot.et`.

Они извлечены независимо из official Client и Server resourceDatabase.rdb;
decoder проверен на10 известных Core/pinned/native ResourceNames. В evidence
сохранены обе SHA, offsets, record headers и GUID bytes. Native Load этих
четырёх новых полных имён **NOT RUN**: новый RESOURCE_SOURCE_NAME обязан
подтвердить точное loaded.GetResourceName(), иначе probe завершается FAIL.
Исходные Wrong GUID ошибки не подавляются и не объявляются внешними.

Actual-source C#5 replay нового setup: **58 cases PASS** в WinPS5.1 и pwsh7;
**8 executed unsafe mutants** в каждой оболочке ожидаемо FAIL. Проверены
default-disabled, original owner calls, native affordability/clear/rank veto,
однократные paid/completion effects, foreign order/state replacement, Stop,
cache successor, master/proxy/campaign loss, pending Rpl без respawn,
metadata identity и exact resource names. Все прежние Catalog controls,
39/2old/6request,4 registration и13unknown-hit cases сохранены; полный
dual contract batch59830 CLOSED0. Native construction/geometry/resource
boundaries этого replay явно SIMULATED.

Первые portable adapter FAIL (implicit local defaults/out/base-name conversion)
и первый Source FAIL при Stop внутри CompletionClear сохранены; post-call
guard исправлен. Peer findings по order/scope, cache ownership и authority
исправлены и включены в cases/mutants. Evidence/source-before/proposal:
`.codex-runtime/issue78-finalization-20261030/catalog-native-setup-readonly-v1/`.
Baseline final r31 All267 и V28 FAIL неизменны. Новый setup WB/runtime пока
**NOT RUN**; ранний WB на older production subset не заменит final frozen
source gates или настоящий paid native purchase.
