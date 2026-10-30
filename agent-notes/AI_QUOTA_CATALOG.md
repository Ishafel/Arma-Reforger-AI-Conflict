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
