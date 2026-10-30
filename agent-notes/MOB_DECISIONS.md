# Read-only observer решений MOB

`tests/fixtures/AICF_MobDecisionProbe.c` предназначен только для отдельного
stage; копируется как `Core/Scripts/Game/AIConflict/AICF_ZMobDecisionProbe.c`
(относительно addon `AIConflictCore`). Включение: `-aicfMobDecisionProbe 1`.
Production gameplay не меняется. Нет своих timers/events, terrain/physics
queries, повторного вычисления native utility, action/waypoint/pose/health
writes или изменения deadline.

Public `tools/Invoke-AICFVehicleSoak.ps1 -MobDecisionObserver` копирует fixture
в собственный isolated stage и передаёт CLI flag. При заранее подготовленном
общем stage runner требует точные fixture manifest/SHA и compiled source
assertion; отсутствие/изменение/непроверенный source не обходится этим флагом.
Опция по умолчанию выключена и не меняет Duration/ProofVersion или log auditor.

Три single-super override исходно объявленных методов `AICF_MatchController`
сохраняют actual result и все out parameters: `ResolveAllowedIdleReason`,
`TryApplyHiddenMobEgressRecovery`, `TryFindDistinctMobEgressDestination`.
Scope ограничен player authority с текущим POSITION target; наличие сохранённого
player intent отдельно записывается и не предполагается. Observer не подменяет
`AICF_PlayerMobHoldPolicy.IsReachedCurrentHold` и не признаёт формацию прибывшей.
Для `ALLOWED_IDLE` запись появляется только при actual result `NONE`:
штатные разрешённые holds/vehicle lifecycle не расходуют32distinct records.
PRE snapshot снимается до исходного delegate; скрытая recovery и вложенный
поиск destination сохраняют все свои actual результаты. Это фильтр наблюдений,
а не изменение возвращаемого policy результата или условий восстановления.

Для actual allowed-idle/hidden decisions сохраняются последовательные
`BEFORE_PARENT` и `AFTER` snapshots: faction/numeric slot/group generation,
assignment/intent/waypoint, pending/failed/approach flags, player intent/target,
MOB owner/origin, current/queued waypoint, group current/executed actions и
related waypoint. Записаны group threat sample, лидер и расстояние до waypoint,
до32actual roster members с расстояниями, parent/controller/compartment facts
и selected/executed action/cause. `GetCause` вызывается через закреплённый
`SCR_AIActionBase`, где он объявлен в Script Diff1.8.0.13.

`SCR_AIObserveThreatSystemBehavior` может иметь `DANGER_HIGH`, `COMBAT` или
`DANGER_LOW`; одно имя класса не определяет cause. Sector flags остаются
`UNOBSERVED`, формационный predicate не вычисляется повторно. Member distances
и флаги — sampled inputs, а не фактическое возвращённое значение внутреннего
`IsFormationAtWaypoint`. Native moving/defend activity и личные combat reactions
могут существовать одновременно.

Внутри actual hidden-recovery wrapper вложенный destination wrapper записывает
точные переданные `memberOrigin`, `targetPosition`, `mobOrigin` и actual result/
destination. Уже вычисленные local candidate reject counters в исходном методе
не доступны; они явно `UNOBSERVED`. Новый terrain query ради диагностики не
выполняется. Вложенный receipt использует `BEFORE_PARENT`, а не притворяется
собственным атомарным preflight snapshot.

В static состоянии сохраняются только до32distinct diagnostic keys и один
временный synchronous frame. Duplicate не расходует record; cap/overflow и
scope loss явно означают неполное наблюдение. Замена frame не перезаписывает
successor или его keys; scope повторно проверяется после snapshot getters и
после проверки текущей identity перед записью. В frame cached world/group
EntityID/Rpl component+ID и MOB owner повторно сверяются с текущими bindings;
native поля weak, lifetime игровых объектов не продлевается статическим архивом.
`OnGameEnd` очищает ссылки/keys. Raw snapshots имеют `non_atomic=1`;
первый guard исходного compound predicate из них автоматически не выводится.

Контракт `Test-MobDecisionProbeContracts.ps1` подготовлен для actual wrappers,
out parameters, native cause, dedup/cap/Stop и unsafe controls. Свежий targeted
прогон30.10: **81actual cases/8executed unsafe PASS**, в том числе замена frame
в snapshot/identity getter с сохранением successor и нулём ошибочных keys.
Отдельный unsafe вариант сохраняет начальный scope guard и убирает только
проверки после getters, чтобы counterexample относился к этому boundary.
Census/Recruitment observer contracts —2/2PASS; независимый Root replay81/8 PASS.
Первый harness FAIL сохранён: перевод out parameters затронул также format
arguments; затем недостижимый код внутри cap-mutant не компилировался. Исправлен
сам replay/unsafe harness, исходный delegate/result/out contract не ослаблен.
Evidence: `.codex-runtime/issue78-finalization-20261030/mob-decision-probe/`
`contract-v5-scoped-unsafe.txt`; соседние аудиты — `mob-decision-neighbours-v1/`.
Первый combined Soak v5 terminal Workbench — **FAIL**: локальное имя `owned`
совпало с зарезервированным Enforce modifier (`Core/proto/proto.c:12`, pinned
Workbench `SCR_ClassRenamingPlugin.c:73`). Переименовано только в `ownedWaypoint`,
с доказанным обратным текстовым соответствием; события/поля логов сохранены.
Новый actual replay81/8 — PASS (`contract-v6-owned-rename.txt`). Исправленный
combined Soak v6 — terminal Stock Workbench **PASS**, exit0/validation/full3log
fatal0/raw384+6fixtures=390/sourceSHA0, manifest0B349BDA…0F191. Combined Multi v3
с тем же observer также WB0/fullfatal0/raw384+4fixtures=388/sourceSHA0.
Полные receipts — `ordinary-mob-diagnostic-source-v6/compile-verdict.json` и
`multi-mob-diagnostic-source-v3/compile-verdict.json` внутри finalization evidence.
Frozen V24 и ошибочный v5 stage не изменялись; V24 All не запускался. Новый
frozen V25 завершён **261/261 PASS**, collector0/exact union/Logistics1,
1019SHA0 и6actual sessions closed0. Это snapshot до позднего doc-only обновления
результатов компиляции; source fixture/contract bytes совпадают. Native **NOT RUN**.
Наличие fixture не меняет старый US383 ordinary1800 FAIL и не подтверждает
исправление отставшего участника. Исправление ошибочной egress-цели не заменяет
отдельную проверку фактического прибытия всех живых участников.
