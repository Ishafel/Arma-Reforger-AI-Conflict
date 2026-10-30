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
Свежий compile: `catalog-native-v23-client-identity-elevated/compile-source-verdict.json`.
