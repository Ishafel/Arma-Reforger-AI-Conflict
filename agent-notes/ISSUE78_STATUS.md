# Состояние #78 — 31.10.2026

## Текущий срез r33/r34 для проверки игроком

Issue OPEN, PR79 draft/`Refs #78`. Код сохранён отдельными коммитами:
`6bdb476` cleanup assignment fence, `b756a96` dropped-weapon safety,
`cdcd3ab` canonical catalog metadata/controlled depot fixture. Передача
экспериментальная, не release/ready/ACCEPTED.

- **Full r34:269/269 PASS**, exact unique union/Logistics1/1038SHA0;
  actual77074/6995/87915/71699/49754/23935 все closed0. R33 был268/269FAIL
  только по IOException в cleanup тестового temp-каталога. Исходный FAIL
  сохранён. В r34 изменён только finally одного контракта: прежние95 assertions
  неизменны, cleanup7controls dualPASS; все388 игровых файлов равны r33.
- Current production388 terminal WB Stock/RHS/ArlandWCSRHS3/3 compilePASS,
  actual14760closed0/native0/full9fatal0/source1038SHA0. Fixture-free player
  bundle отдельно WB4graphsPASS: Stock/RHS/ArlandWCS/EveronWCS,
  actual84077closed0, full12logs fatal0/source388SHA0. Resource50/52/78/78
  остаются strictFAIL. Наличие Workbench PASS не означает исправления ресурсов.
- Новая защита свободного оружия: actual-source94/9 dualPASS, прежние late-lock,
  TTL и infinite-normal failures сохранены и исправлены. Только известный
  AI inventory-removal, mainworld/master/identity/human/subtree/terrain proof;
  нет teleport/physics setter/лечения. Отдельный native intervention пока NOT RUN.
- Multi r32 native1800: native0/runner1/elapsed1800907ms, полный FAIL9,
  completed0. Максимальные readbacks US4/USSR2; подтверждён только cohort2
  в31354ms, полного2→4→6 нет. PreflightUnavailable0; восемь выбранных составов
  получили32 оплаченных recruits/560supplies. Source392/full3fatal0/E28/SHA0.
  Census384captures:279true/105actualfalse;259false decision rows сохранены;
  overflow1 означает INCOMPLETE, не PASS непрерывного покрытия.
- Catalog V29: native0/0, public audit1/1, source388/full6fatal0/E172/SHA0.
  Все4 metadata ResourceName exactPASS. Строительство депо исчерпало штатный
  поиск122693ms/188candidates; отменённый заказ дал SETUP_ORDER_IDENTITY_LOST.
  Core identity defect этим не установлен. Оплата/покупка/slot physics guard
  не достигнуты и остаются NOT EXERCISED.
- RHS+WCS r33 Arland/Everon/North: свежие native проверки выполняются отдельно;
  до полного stopped verdict результаты не объявляются PASS. Prepared2graphs
  raw390/compiled378 WB0/fatal0/SHA0, other78FAIL каждый.
- Сборка игроку:7addons/388sourcefiles,18DryRun plansPASS, обычный retail
  Frontend, без тестовых сценариев и modifiers. Fresh profile каждого запуска,
  отдельная постоянная библиотека внутри bundle. Ручное прохождение25–30min
  по [чек-листу](VEHICLE_VISUAL_CHECK.md) — **NOT RUN**.
- Удалены8 известных старых stage-деревьев только после8ZIP exact verification:
  3143files/400dirs/40771777bytes→11039745bytes архивов; net≈28.36MiB.
  Полные profiles/logs/manifests/исходные FAIL сохранены, archive index связывает
  прежние пути с проверенными ZIP. Пользовательские файлы не удалялись.

Evidence: finalization20261029 `r34-*`, `r33-expanded-compile-verdict.json`,
`quota/r33-current-cut-preparation/`; finalization20261030
`r34-actual-session-closures.json`, `r33-r34-exact-source-delta.json`,
`multi-log-contract-cleanup-r33/`, `multi-catalog-final-stopped-v1/`,
`player-current-cut-20261031/`, `obsolete-stage-archive-plan-v1/archive-index.json`.
Native-only8 persistence, resource/font/GUID/lighting, длительные functional
FAIL и непроверенное человеческое прохождение остаются отдельно; issue не закрыта.

## Исторический checkpoint r31/r32

Issue остаётся OPEN, PR79 — draft/`Refs #78`. Опубликованный head `df62bed`;
последующие исправления пока локальные. Прежние результаты ниже исторические.

- Frozen r31: **267/267 PASS**, exact unique union/Logistics1/1034SHA0,
  все шесть actual sessions closed0. Этот snapshot ещё не содержит нового
  cleanup assignment fence.
- Frozen r32: **267/268, FAIL** — старый exact-prefix контракт native gate не
  учитывал новый `StopCleanupAssignmentRebind`. Исходный FAIL сохранён;
  исправленный контракт отдельно PASS29 +2 executed unsafe в обеих оболочках.
  Новый полный набор после этого изменения ещё не выполнен.
- Production387 r32: терминальные Stock/RHS/ArlandWCSRHS compile3/3 PASS,
  actual66431 closed0/native0/validation/full9 sibling fatal0/1035sourceSHA0.
  Прочие Error50/52/78 остаются strict FAIL, не исключены из результата.
- Контролируемый r31 native900 закончился штатно, но физическая очистка FAIL:
  actual admission guard отвергал assignment5 после принятого retarget6.
  Новый отдельный cleanup fence переносится только через проверенную пару
  Begin/actual Commit/Complete; исходные admission actors неизменны. Targeted
  rebind48/4, прежние90/15 и observer57/3 PASS в обеих оболочках.
- Controlled r32 native900: actual62153 closed1/native-1/audit1, полный
  duration FAIL. До assertion наблюдались два Kick, DEAD physical detach,
  release собственной lease/cap0/ack и4ALIVE. В628s симуляции свободный VZ58
  упал доY=-34011 и вызвал `enf_entity.cpp:280`; все4 stopped sibling logs,
  fatal1/other24/source396SHA0 сохранены. Точный собственный PID остановлен
  после CIM identity check; происхождение freefall пока UNKNOWN.
  Этот запуск не исполнил accepted retarget до RESET и не доказывает новый
  перенос assignment fence. Cached canonical IDs добавлены без aliases;
  строгий seat join требует отдельного source-backed observer.
- Multi r30 native1800 завершился штатно, но MOVING FAIL: genuine preflight
  stock465 при цене475, два повреждения машин и один native abandonment.
  Перенос test-only funding до actual preflight проверен targeted и скомпилирован
  в Multi392; нового native подтверждения ещё нет. Причина повреждений неизвестна.
- Catalog v27 исправил собственную потерю native data registration, но встретил
  null Physics в штатном slot callback. Guard fail-closed compiled; v28 штатно
  завершился timeout300100ms без native slot observations, поэтому native
  исполнение guard и покупка пока не доказаны. Новый depot-first setup — только
  отдельный controlled fixture, не доказательство обычного строительства.

Evidence: finalization20261029 `r31-*`, `r32-*`; finalization20261030
`r31-r32-actual-session-closures.json`, `controlled-r31-USSR-900-v2/`,
`controlled-r32-decision-v1/`, `controlled-r32-USSR-900-v1/`,
`multi-r30-moving-1800-v1/`, `multi-preflight-funding-race/`,
`catalog-native-v27-serialized-data-registration/`,
`catalog-native-v28-physics-guard/`, `native-gate-completion-r32-contract/`.
Native-only8 persistence, строгие resource/font/GUID/lighting gates и visual
NOT RUN остаются blockers; compile/static PASS их не заменяет.

## Новый код и gates r30

Локальный code head `2ca63e5`: hold `1e85305`, captured-dead cleanup `79b85ca`,
exact Census diagnostics `2ca63e5`; harness long-path fix `99620f3`.
Передача остаётся draft PR79/Refs78. Issue OPEN до native fix.

- Полный frozen `r30`: **265/265 PASS**, collector0/exact unique union/
  Logistics1/1029SHA0; все6 actual sessions2294/76993/98211/13667/92134/30786
  закрылись0. Полные commands/closures/source hashes — TESTING.md и evidence.
- Production386: terminal WB Stock/RHS/ArlandWCSRHS3/3PASS, actual98203closed0,
  native0/validation/full3siblingfatal0/source0. Прочие Error rows50/52/78
  сохранены как strict baseline, не объявлены отсутствующими.
- Staged hold: реальный owned Defend, исходные сроки, отдельный terminal intent
  до любого следующего acquisition/economy, actual250/28 и canonical9/9PASS.
  Actual waypoint Y хранится после construction, XZ staging не меняется.
- Cleanup: исходный DEAD actor вне сохранившегося native roster разрешён только
  через admission/terminal/lease identity. Последний actor nonplayer proof идёт
  после seat reads, далее только own context/budget. Peer старого кода реально
  воспроизвёл4 late-human и weak-group counterexample; исправленный90cases/
  15source mutants/2executed unsafe controls, dual+8neighbors PASS. Начальные
  V28 harness и поздний production FAIL сохранены, не скрыты под Full r29PASS.
- Exact Census: actual bool+8original arguments, no-op Core hook, zero extra
  native getters; отдельные dynamic row witnesses и snapshot-only cached q10.
  Dual479/21PASS/inverseEXACT/legacy4120/6PASS. Формула квоты не изменена.
- Mixed Soak393/Multi391/controlled394 отдельно WB0/fullfatal0/rawSHA0.
  Controlled observer48/10PASS вызывает один native Kill и обычный exactrevision
  RESET; старый Soak audit не ослабляет. Первоначальный fixture Formula too
  complex FAIL сохранён, исправлены ordered guards и два fixture races.

RHS+WCS v4 на этих386production подготовлен и compiled в двухgraphs, raw388,
compiled376/fullfatal0/SHA0; resource78rows каждый остаётся FAIL. Три native
прогона завершены по одному world после265/265 и закрытия всех6 offline sessions:

| Variant | Cases | Hold после DONE, с | Мин. FPS всех samples | Прочие native Error rows |
|---|---:|---:|---:|---:|
| ArlandWCSRHS | 95/95 PASS | 301.829 | 41.6 | 206 FAIL |
| EveronWCSRHS | 95/95 PASS | 304.331 | 23.0 FAIL | 466 FAIL |
| EveronNorthWCSRHS | 97/97 PASS | 304.199 | 33.8 | 466 FAIL |

Actual runner sessions61936/6292/15356 закрылись1, native exit0 каждый;
полные9 stopped sibling logs: fatal0/project0, raw388 sourceSHA drift0,
exact canonical CLI/ROSTER_READY. У каждого20 read-only quota samples known,
UNKNOWN0; это не непрерывное доказательство Census. Everon23.0 наблюдался
до ROSTER_READY; стартовые samples не исключены из FPS gate. Все strict verdict
FAIL сохранены. Категории и количества console Error совпадают с прежним V27;
старый FAIL не является исключением. Native/WB0 подтверждены04:20:58,
свободно20.568GiB. На время трёх native windows согласован HOLD WB/Full/targeted
CPU; Root делал docs/Git/light readonly. Причина FPS просадки этим не установлена.
Ни map bootstrap, ни compile не закрывают ordinary1800/MOVING6/
controlledphysical/client/JIP.
Native нового exact observer и физическая регрессия cleanup пока NOT RUN.

Evidence: ignored finalization20261029 `r30-*`,
`ordinary-r30-exact-diagnostic-v10/`, `multi-r30-exact-diagnostic-v8/`,
`quota/wcs-final-cleanup-mapboot-v4-preparation/`,
`quota/wcs-final-cleanup-mapboot-v4/three-map-summary.json`
(SHA256 `A7F279893FF4C6D983E688E70F8AB77B66D017BF0E90F05F55BC3F5D22EB641E`);
finalization20261030
`captured-casualty-cleanup-fix/controlled-native-r30-v2/`,
`census-exact-capture-diagnostic-v1/`, `r30-native-preparations-v2/`.
Неготовый GROUP Catalog WIP сохранён отдельно, не попал в265. Поздний vehicle
selector не входил в этот All. Fresh Stock Catalog v26 (386+1fixture/raw387)
прошёл WB50727 CLOSED0/native0/full3fatal0/SHA0; первый commit attempt поймал
лишнюю blank EOF, два LF удалены с proof неизменности всего code prefix.
Server/Client81236/97717 CLOSED0/native0/0, public audit1/1: server
`BOUNDED_TIMEOUT`300299ms/factory/header/callback/debit0, client functionalPASS.
Full6logs: fatal0, resources14/118, прочие Error748/748FAIL. По362 console
`Unknown class 'SCR_EntityCatalogSpawnerData'` на роль объясняют724 raw sibling
rows; native vehicle catalogs пусты. Это новый собственный fixture FAIL,
не внешний baseline. У modded data class отсутствуют обе штатные annotations;
узкое исправление и новый native positive пока ожидают проверки. v26 stage,
полные логи, exits и source387SHA0 сохранены в finalization20261030
`catalog-native-v26-slot-preflight/`. Strict native-only
8persistence/resource/font/GUID/lighting blockers и visual NOT RUN сохраняются.
Ready/merge/release/ACCEPTED не объявлены.

## Проверка гипотезы startup stagger

Отдельный startup-stagger control на r30 EveronWCSRHS проверял гипотезу
одновременного старта:386production +2 прежних readonly +1 ignored prototype,
raw389, terminal WB v2 native0/full3fatal0/source0. Первая WB v1 имела
несовпадение имени аргумента override и сохранена FAIL. B1000 действительно
разнёс20 dispatch, initial95/95PASS, но minFPS22.9 сохранился до ROSTER.
В04:41:52 после наблюдавшихся300.092s от DONE test-only teardown получил
VM/null `m_bScheduled` в `AICF_StartupCancel`; процесс пришлось завершить.
Actual71396 CLOSED1/native-1, fullfatal8/resource462FAIL/raw389SHA0.
Это FAIL normal shutdown/исходного hold gate, а не production regression.
Fresh elevated CIM04:43:53 game/Workbench0. A0 повтор не запускался: B уже
не устранил просадку; prototype в production не переносился. Причина FPS
по-прежнему UNKNOWN. Evidence: finalization20261030
`startup-stagger-r30-prototype-v1/native-B1000-v1/`.

## Остановленные ordinary1800 на topology V27

Оба новых прогона завершены штатно, native exit `0`; actual runner sessions
`61868`/`64400` закрылись `1` из-за строгого аудита. Production384 +6fixtures,
raw390/compiled378, source SHA drift0, exact CLI/ROSTER_READY и полные stopped
logs подтверждены. В обоих реальный оплаченный набор четырёх бойцов дал исходный
состав5 и пять exact member==occupant мест; `CLASSIFY_VEHICLE_ROW_FAILED=0`.
Это native positive исправления topology, но не доказательство отсутствия всех
transient Census UNKNOWN или положительного обхода более256nodes после fix.

- US:1800990ms, fullfatal0, прочие native Error32FAIL. Первая ошибка observer
  `NO_SURVIVING_MOBILE_CREW` на617572ms: четыре исходных DEAD и один INCAP;
  последний позднее также погиб. Машина освобождена в WORLD_POOL с cap0;
  новый generation2 — платная замена, не воскрешение. INCAP→ALIVE наблюдалось,
  причина неизвестна. Route0/functionalFAIL; естественные потери не объявляются
  проектным дефектом, полный исходный маршрут этим не подтверждён.
- USSR:1800296ms, fullfatal12raw sibling rows/прочие native Error32FAIL.
  `MEANINGFUL_TASK_DEADLINE_MISSED` во время SPAWN_COMMIT — реальный taskless
  интервал30170ms. Точный typed WAIT outcome старый observer не записывал.
  После боя `VEHICLE_CLEANUP_RETAINED` удержал исходного DEAD водителя0EED,
  исключённого native из сохранившейся группы. Exact seat/vehicle/Rpl и birth
  identity подтверждены; `player_scan=1` не означает наличие игрока. Route0.

Два отдельных production исправления ожидания и captured-dead cleanup проходят
targeted проверки; Workbench/полный All/native для них ещё NOT RUN. Они не входят
в эти игры и не превращают старые FAIL в PASS.

Текущий RHS+WCS topology bootstrap: Arland95/Everon95/North97cases PASS,
native0/fullfatal0/source386SHA0/hold≥300s; resources206/466/466FAIL,
minFPS42.7/22.1/28.9. Контроль Everon с0Core gameplay.c, неизменным map adapter
и1timer завершён native0/303.084s/source25SHA0/minFPS34.1; strict fatal18raw и
прочие Error432FAIL. Изначальный zero-production контроль упал в native
CallsignPool и не дал FPS. Нагрузка контроля отличается: причина просадки
полного режима остаётся UNKNOWN, освобождение Core от ответственности не доказано.

Catalogv24 подтвердил actual already-funded15600/quote200, но остался
functionalFAIL300616ms: selected/factory/debit0, GetFreeSlot=null следует из
контрольного потока, тип/занятость прямо не наблюдались. GROUP WIP не готов:
standalone RequestTable не имеет ResourceComponent. Он исключается из следующего
completed-repair snapshot до доказательства штатной полной hierarchy.

Evidence: `.codex-runtime/issue78-finalization-20261030/ordinary-topology-US-1800-v1/`,
`ordinary-topology-USSR-1800-v1/`, `cleanup-protected-occupant-readonly/`,
`catalog-native-v24-already-funded/`; FPS —
`.codex-runtime/issue78-finalization-20261029/quota/wcs-everon-coreless-mapcompat-fps-v3/`.
Issue OPEN, PR79 draft; strict native-only persistence/resource blockers сохранены.

## Предыдущий checkpoint V27

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
