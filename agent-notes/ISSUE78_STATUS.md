# Состояние #78 — 31.10.2026

Checkpoint кода `292f09e`: topology fix `ff36669`, Catalog client fix `ef6d226`,
Census diagnostics `292f09e`; [draft PR79](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/79),
issue OPEN. Native новых исправлений ещё NOT RUN; прежние FAIL сохранены.

- Полный frozen V27:262/262 PASS, collector0/exact union/Logistics1/1021SHA0/
  шесть actual sessions closed0. Новый production384 Stock/RHS/ArlandWCSRHS
  WB3/3PASS; combined Soak390/Multi388/Catalog385 отдельно compiled0/fullfatal0.
  V25 имеет свой261PASS; V26 — только подготовка, All NOT RUN.
- RHS+WCS Arland/Everon/North на pre-MOB384+2fixtures:95/95/97cases PASS,
  native0/fullfatal0/source0/hold≥300s; strict resources206/466/466FAIL,
  minFPS42.2/22.9/32.4. Everon общий FPS gate FAIL. Это map bootstrap.
- Multi MOVING/PRODUCTION_BACKGROUND/ROOT_ONLY1800 завершился native0,
  elapsed1800389ms: cohort2/completed0, три boarding deadline и один transit
  deadline, route incomplete;12raw sibling SCRIPT(E) строк, resources28FAIL.
  USSR0 действительно посадил5 и проехал≥878.911м, но исходный deadline не прошёл.
- Диагностические600s USSRv6 и USv7 завершились native0 с boarding/route FAIL;
  USSRfullfatal0/resources32FAIL. USSR last-foot29.2688м вне прежнего25м cap.
  После fallback три periodic readback дали internal_task1/egress_required0:
  узкий MOB predicate наблюдён, arrival/formation/route этим не доказаны.
- USv7 свежий AFTER_DECISION hierarchy:316nodes, character subtrees204,
  complete/stable/parent-world-duplicate errors0. Прежний полный256node обход
  не способен обработать такое дерево. Исправление исключает character subtrees
  из конструкции машины, сохраняя structural256, seat-owner/occupancy и
  bounded sibling проверки. Targeted actual oldFAIL3/new36+8unsafe PASS;
  новый WB3graphs/полный262PASS; новый native ещё требуется.
- Catalogv20 server/client0/0, но300s timeout до выбора/factory/debit, strictFAIL.
  Найден отдельный test-only defect: client использовал server-only identity.
  V22 client helper сохраняет mainworld/entity/Rpl/PC/PM guards; production
  server helper не меняется. Actual30/7unsafe и полный Catalog contract PASS;
  v22 fixture прошёл WB в новом stage-v23 и Full262; native NOT RUN.
  Старый v20 first runtime guard остаётся UNKNOWN.

Evidence: `.codex-runtime/issue78-finalization-20261029/` и
`.codex-runtime/issue78-finalization-20261030/`; команды и scope — TESTING.md.
Native-only persistence/resource blockers сохраняются. Ready/merge/release/
ACCEPTED не объявлены. Следующий шаг — свежие native tests без расширения caps:
Catalog server/client, ordinary1800 обеих сторон, MOVING2→4→6 и текущие WCS карты.

## Исторические checkpoints после передачи

Текущий RHS3 checkpoint на exact human383 (`EC9CDDA8`/`66B86FF5`/`C9CE1F49`):
Арланд95, Everon95, север97 runtime cases PASS; каждый native0/ROSTER_READY/
полный stopped log/hold≥300s/source383+2SHA0. Строгий итог FAIL: resource errors
136/396/396; single-world минимум FPS43.5/27.3/36.4, Everon FPS≥30 FAIL.
Все три процесса завершены, globalCIM[]21:54:29. Полные receipts и границы
terminal-only проверки — [TESTING.md](TESTING.md); ordinary1800 обеих сторон,
MOVING6 и quota acceptance этим bootstrap не закрыты.

Compartment producer уже реализован и отдельно проверен на382:12native checks,
2реальных newborn/entry, exact2character+1vehicle bindings и непрерывный known
15s tail; native0/fatal0/source0, resources28 strictFAIL. Это не PASS human383
под давлением. Более ранние «NOT IMPLEMENTED» и UNKNOWN ниже относятся к своим
историческим checkpoints. Поздний pressure trace завершён: fresh127+reservation1
разрешил tracked factory BEGIN52/BOUND actor3CC1→128/0. Обход квоты в этом запуске
не подтверждён; предыдущее освободившее место тело неизвестно. Исходный
functionalFAIL/resources28/minFPS20.7 сохранены; подробности в
AI_QUOTA_STOCK_PRESSURE.md.

Обе ordinary1800 на immutable human383+3observers завершены: US1800022ms/
native0/wall1824.221s и USSR1800687ms/native0/wall1825.528s. Duration/liveness/
provenance/stopped PASS, но оба functionalFAIL PRESET_BOARDING_DEADLINE
(347288/469000ms) и route incomplete. US12SCRIPT(E) MOB-egress/bridge,
USSRfullfatal0; resource32FAIL каждый. Source383+3SHA0. Эти отрицательные
baseline не пересчитываются по последующим исправлениям.

В USSR последний foot actor находился1.92м от собственного пустого доступного
места: owned/EVALUATED GetIn, current+executed passive Idle, без transitions.
Fix `8f2f7ac` разрешает прежнюю bounded short-seat recovery после5s без progress
только при exact member/group Idle и полном отсутствии competing actions.
Контекст повторно проверяется после cancellation и quota/native getters до
pure commit. Квоты,25м/5м/1м·с⁻¹, one-shot budget и deadlines не увеличены.
Actual scope/flow41+5unsafe, counter27+6unsafe и own-seat133/6/9 PASS;
fixture-free384 Stock/RHS/ArlandWCSRHS Workbench3/3PASS/fullfatal0/rawSHA0.
Native proof нового384 ещё не получен. Долгий CensusUNKNOWN этим fix не обойдён.

Read-only Census/recruitment observers и optional public Soak flags сохранены
в `e7c3a17`; combined diagnostic389 StockWB0/fullfatal0/source0. Native NOT RUN.
После восстановления инструментов frozenV23 содержит246завершённых PASS из259,
1014SHA0; исходное выполнение прервано, старые6normal exits не подтверждены.
Ровно13missing выполняются отдельно на том же frozen source. До collector
это не полный PASS. Поздний Catalog subscriptions/pending WIP в V23 не входит.
Подготовлены два384+2readonly WCS MapBoot graph: ArlandWCSRHS и EveronWCSRHS/
North, оба WB0/validation/fullfatal0/raw386SHA0, resource78Erows отдельно FAIL.
Native ожидает завершения CPU-проверок. Multi384+3diagnostics=387 StockWB0/
fullfatal0/SHA0, native NOT RUN. Точные receipts — в evidence finalization.

### История checkpoints после возобновления

Новый checkpoint: human bridge383 compile3graphs и frozen255/1005SHA0/6closed0
PASS. Native player-body retirement+census restored подтверждены v11 server/client0/0;
Catalog purchase остаётся FAIL300s до выбора записи. Scope и полный разбор —
TESTING.md. Два чистых pressure controls далиFPS≥30, но functionalFAIL позднего
extra-агента; его тип ещё UNKNOWN, overshoot не объявлен. Прежние native-only
blockers и доступные ordinary/Multi/client/JIP gates сохраняются.

Пользователь вновь запросил исправления и RHS Арланд/Эверон/север. Текущий
checkpoint:382production compile Stock/RHS/ArlandWCSRHS3/3 PASS; full frozen
`resumed-fixed-v16`247/247 PASS/987SHA0/6sessionsclosed0. Исправлены stale site/
assignment/prefab/final slot generation и fresh-clock attempt fences, existing
inactive character custody, compartment newborn producer и postquota RecoveryBan
проверка FIA. Targeted Root58/21, constructor44/13, character37/5, compartment27/7.
Native RHS3 на предыдущем380:300s/ROSTER_READY/quotaREADY/native0/project0/fatal0,
resources136/396/396 FAIL и FPS42.5/27.3/29.5. Это не pressure нового382.

Catalog client/server первый control дал serverAV до actualavatar/покупки,
причина UNKNOWN. Поздние native controls доказали human requester proxy/master0
и потерю weak census row при compartment admission; узкие fixes131/11 и28/8
прошли actual/independent peer. Frozen249 и3graphs запущены заново, native positive
выполняется. Stock pressure выявил strict prefab/root→variant отказ;
quarantine/holds сохранены. Узкий closed-set fix прошёл actual28/8 и native BOUND;
новый frozen250/994SHA0/6closed0 и current383 three-graph compile PASS. Native
stock-pressure-v2 функционально подтвердил128stock/0extra и normal4paidjoins в
прежний managed отряд до5, но resources28/minFPS22.2 оставляют strict FAIL.
Новые native log auditors отклоняют
ENGINE(F)/GUI(E) и прочие full-log errors в настоящих WinPS5.1 CLI controls.
Native catalog/compartment/pressure и ordinary/Multi6/clientJIP остаются gates.
Issue OPEN, PR79 draft, native-only blockers неизменны.
Подробности и последующие результаты — актуальный [TESTING.md](TESTING.md).

## Историческая передача

Пользователь запросил обновить issue, убрать старые артефакты, сделать итоговый
комментарий, отправить PR и выключить компьютер. PR должен оставаться draft:
строгий результат FAIL, issue OPEN до native fix по отдельному решению пользователя.
Merge и release не выполняются. Этот документ не является ACCEPTED.

Опубликован [draft PR #79](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/79)
с `Refs #78`; основной checkpoint `b5272ed`. Полный All текущего объединённого
кода не завершён. Fresh frozen380 Workbench Stock/RHS/ArlandWCSRHS3/3 PASS,
fullfatal0/source drift0. После него только trailing whitespace удалены в трёх
новых native projections; Catalog/Defender actual contracts повторены PASS.

Шесть short native bootstrap на историческом376production завершены native0,
project0/fatal0: RHS95cases/resources136FAIL/minFPS47.2; ArlandWCSRHS95/206FAIL/
40.1; EveronRHS95/396FAIL/22.9; EveronWCSRHS95/466FAIL/17.6;
EveronNorthRHS97/396FAIL/27.6; EveronNorthWCSRHS97/466FAIL/28.3.
North проверил7баз/2HQ/5points. Это не native quota pressure текущего380.
Constructor reentry закрыт43actual/13unsafe/9strong и independent full Observe8
cases PASS; первоначальный counterexample сохранён. Собственных native/WB
процессов0. Всего17завершённых stages архивированы с ZIP-entry SHA проверкой;
полные logs/manifests и старые FAIL оставлены.

## Сохранённый результат

Ветка `codex/issue-78-vehicle-presets` содержит пресеты техники, поведение
TRANSPORT/COMBAT с изменяемым возвратом300м, bounded recovery, исправление
конфликта own Cargo/Pilot, geometry/anti-cycle, расширенный native control trace,
raw roster damage observer и exact lease rpl_id в восстановлении приказа.
Новый слой раздельных stock/AICF quotas и producer adapters сохранён как
незавершённая интеграция. Подробные контракты — [AI_QUOTAS.md](AI_QUOTAS.md).

## Проверки и границы

- Последний полный исторический `recovery-v14`:219/219 PASS,6sessionsclosed0,
 919frozenSHA0; его Workbench Stock/RHS/ArlandWCSRHS3/3 PASS. Он предшествует
 новым atomic/observer/handoff/quota deltas и не является PASS текущего checkout.
- Own-seat atomic:133actual/6source/9unsafe, fixture17/48/6CLI,7affected audits,
 peer0/StockWB0. Два controlled native, включая LOCAL_THREAT cancellation,
 functional/fatal/source PASS, resources28 FAIL каждый; exact5/прежние IDs/Rpl/
 health/одна покупка50/движение11.147м и13.5366м.
- Новый ordinaryUS1800: native0/wall1817.619s/DONE1800637ms/fixture failures0.
 Functional FAIL из-за отсутствовавшего rpl_id ORDER_RESTORE_RESULT, resources32
 FAIL;2исходных выживших восстановили пеший порядок и прошли10.3859м после
 объяснённого повреждения/потерь. Исправление log formatter4016a2c прошло
20actual/1unsafe/peer0/StockWB0; свежий native на этом исправлении NOT RUN.
- Owned factory actual common+Trip+FIA+raw factory38cases/9unsafe PASS,
 service adapter BOUNDARY_ONLY. Logistics actual derived guard44/5 PASS.
 Эти targeted replay не заменяют объединённый Workbench/native.
- Catalog3pins/108actual/8unsafe/peer0/scopedStockWB0; Defender15source/33actual
 и16factory/4unsafe; Startup30actual/3unsafe/peer0. Native catalog/Defender
 pressure не запускались. Generic MoveIn16actual/5unsafe/4pins, vehiclebackend
116actual/8unsafe, short-seat24actual/6unsafe и прежние133own-seat PASS.
- Frozen376production+RuntimeScenarioProbe377 прошёл3graphs Workbench0;
 этот snapshot предшествует новым owned factories/backend/reentry fixes.
 Новые RHS/RHS+WCS bootstrap и полный набор фиксируются отдельными receipts
 в итоговом комментарии. Нельзя переносить их PASS на другой source hash.

## Что оставалось недоказанным в исторической передаче

Полная native partition под давлением stock/managed/pending queues, initial
inactive characters и native compartment newborn producer. На том checkpoint
последний был NOT IMPLEMENTED; текущая реализация и scoped native proof описаны
выше. Original MOVING2→4→6 с production background (2/4 доказаны
ранее,6нет), новые ordinary1800 обеих сторон, native correction branch,
текущие client/JIP/retail identity и fixture-free player build остаются gates.
Visual UX NOT RUN: нужны действия игрока с превью/hover/Automatic/ползунком/
подтверждением состава, terminal model это не доказывает.

Native-only8persistence leaks воспроизведены с0production AICF scripts;
resource/font/GUID и lighting ошибки остаются строгими FAIL. Installed/vendor/
world/mission не менялись. Недостаток coverage и найденный дефект не являются
невозможностью проверки. Исправлять и повторять gates при продолжении.

## Evidence и команды

Полный локальный журнал: ignored
`.codex-runtime/issue78-finalization-20261029/REPORT.md`; там сохранены полные
stopped logs, manifests/SHA, первоначальные FAIL, команды и exits. Старые
неиспользуемые собственные stages архивируются с проверкой каждой ZIP entry
до удаления, полные логи не удаляются. Новые wrap receipts имеют собственный label.

Команды: `tools/Invoke-AICFChecks.ps1 -Suite All`, отдельные
`tests/contracts/Test-AIQuota*Contracts.ps1`, терминальный Workbench из
[DEVELOPMENT.md](DEVELOPMENT.md), `tools/Start-AICFRuntime.ps1` для каждого
из RHS/EveronRHS/EveronNorthRHS/ArlandWCSRHS/EveronWCSRHS/EveronNorthWCSRHS.
Все runtime logs анализировать полностью после штатной остановки.
