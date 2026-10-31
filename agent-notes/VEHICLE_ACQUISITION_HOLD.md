# Реальная задача удержания при acquisition WAIT

## Причина

В обычном USSR1800 на frozen topology V27 отряд оплатил4 пополнения и собрал
живой состав5. После повторного Census UNKNOWN (02:28:30–02:29:51) он находился
в `SPAWN_COMMIT` без значимой задачи: в02:28:58 записан
`MEANINGFUL_TASK_DEADLINE_MISSED`, taskless30170мс при исходном deadline30000мс.
Длительный прогон сохранён в
`.codex-runtime/issue78-finalization-20261030/ordinary-topology-USSR-1800-v1/`.
Это исходный FAIL до исправления. В старом Soak stage v8 нет нового acquisition
outcome observer; точный WAIT reason этого решения не записан и остаётся
`UNOBSERVED`. `SPAWN_COMMIT_READY` — прежняя transition reason, не actual WAIT.

Источник дефекта проверен по коду: после успешного all-member staging flow
снимает собственные индивидуальные подходы. Штатный All-members Move waypoint
к этому моменту может завершиться и исчезнуть из очереди. Следующие
`VEHICLE_QUOTA_PENDING`, `SPAWN_COMMIT_CAP_WAIT` и `SPAWN_BACKOFF` возвращают
typed WAIT. Прежний Controller не назначал в этом ожидании новую задачу.
`HasMeaningfulTask` обоснованно отвергал отсутствующий waypoint.

## Владельцы и proof

`AICF_VehicleAcquisitionHold.c` содержит отдельные physical body receipts,
immutable `AICF_AcquisitionStagingReceipt` и plan-owned hold token.
AcquisitionFlow после фактического `ProcessSpawnCommit` прикладывает receipt
только к трём перечисленным typed WAIT. Сам outcome сохраняет kind, reason,
causation и retry timestamp. Flow не создаёт waypoint и не меняет phase.

Receipt связывает trip/group generations, assignment/intent/base revisions,
request pointer/generation, plan/reservation pointers и token, выбранные
staging/spawn positions, исходные сроки и точный состав AI bodies. Повторные
native reads подтверждают authority/world/EntityID/component Rpl, управление
исходным agent и parent group, сознание, отсутствие player possession,
vehicle/seat transitions, прежнюю staging geometry и отсутствие текущей угрозы.
Это отдельный свежий proof, а не атомарная запись прежних локальных переменных
quota/economy. Дополнительные quota/economy решения, path/ray/physics queries
не выполняются.

Controller оркестрирует создание через Handoff и construction-only factory.
Handoff сначала проверяет receipt, создаёт `SCR_DefendWaypoint` в выбранной
точке, привязывает отдельный token к plan и повторяет identity после native
queue read непосредственно перед `AddWaypointAt`, а также после привязки.
Factory штатно проецирует Y на surface; исходная staging position может быть
navmesh point с другим Y. Token принимает только неизменный точный X/Z,
запоминает фактический origin созданного waypoint и далее требует неизменность
всего этого XYZ. Исходный receipt сохраняет прежний navmesh point; radius не
расширяется и произвольный перенос waypoint не допускается.
Тот же valid queued token повторно используется при очередном WAIT.
Собственный pending RESERVED lease без physical asset допустим; hold остаётся
при same-phase `LEASE_RESERVED_FOR_ATTACH` retry.

Defend preset использует существующую service policy: `SetUseTurrets(false)`,
`SetFractionOfSA(0)`, `SetFastInit(false)`. Native удерживает пехоту у выбранной
точки; malformed/null preset не допускает привязку. Это не отмена текущего
combat action. Physical foot proof также отдельно проверяет `IsInVehicle`,
compartment state, vehicle link и все переходы.

Holding time ограничен минимумом прежних request/trip/site deadlines. Сроки
не сдвигаются и не сбрасываются. Approach deadline не становится новым
commit deadline: существующий `ProcessSpawnCommit` его не применяет.
Factory получает исходный absolute expiry и считает оставшееся время после
native создания waypoint, непосредственно перед `SetHoldingTime`.
Перед queue mutation и после native reads используется свежий clock.
Окончательный pure `HasTaskFields` повторяет captured phase/terminal,
assignment/preset/slot revisions, request, selected site/base, plan geometry,
manual flag, staged counts, reservation token/expiry, исходную deadline identity
и pending lease policy. `AcquisitionHoldDeadlineAtMs(nowMs)` в TransportTrip
читает прежние stored clocks с переданным временем; внутреннего clock read
или изменения сроков в нём нет. Смена script fields во время native getter
не может сохранить прежний task proof. Ownership-only cleanup имеет отдельный
fence и не требует этих уже закончившихся task conditions.

`VEHICLE_ACQUISITION_HOLD_BOUND` сообщает `queue_bound=1` и
`native_running=UNOBSERVED`: успешная постановка в очередь ещё не доказывает
native выполнение. SlotView выдаёт executable proof только при exact
queued/current waypoint и current/executed `SCR_AIDefendActivity:RUNNING`,
связанной с этим waypoint, без suspended/remove state. Проверяется повторная
актуальность token после native reads. 30-секундный taskless контроль и
общий meaningful-task/allowed-idle контракт продолжают действовать.

## Завершение и устаревшие вызовы

При смене outcome, потере staging, replan, retarget, BOARDING, terminal и Stop
Controller вызывает Handoff для снятия конкретного expected token. Сначала
waypoint снимается с исходной группы и проверяется отсутствие в очереди,
затем очищается exact plan pointer, затем повторяются ownership/authority
guards и удаляется replicated entity. Утраченная health/staging/calm или уже
активный физический lease не блокируют удаление собственной закончившейся задачи.
Cleanup fence сохраняет original trip/group/trip generation, request pointer/
generation, plan/reservation pointers/token, original group identity и точный
waypoint EntityID/Rpl/world.

Вложенный вызов может поставить новый token. Внешний rollback передаёт прежний
expected token, а не читает текущего владельца заново. `superseded` прекращает
внешнее применение outcome и terminal fallback. После clear новый token не
перезаписывается; при утрате proof прежний token восстанавливается только в
пустой собственный slot. Неподтверждённый detach/delete оставляет ownership
fail-closed; новое terminal решение не объявляет cleanup завершённым.
Небезопасный fresh staging receipt приводит к обычному infantry fallback;
текущий native combat не отменяется для принудительного удержания.

Перед первым release в `EndVehicleControl` на exact hold сохраняется
`AICF_AcquisitionHoldRetirement`: исходные terminal phase/reason/causation/
restore flag и immutable epoch trip/assignment/request/plan/token/slot.
Даже valid staging receipt не исключает отказ native queue removal.
Следующий public `Tick` после authority/identity guards сначала обслуживает
pending retirement, до любого acquisition/economy/phase flow. После
подтверждённого cleanup продолжается именно исходный End с прежними полями.
Смена assignment/revision/phase или появление successor token позволяет только
снять старую задачу; исходный End к преемнику не применяется. Pending intent
не является idle exemption и не продлевает сроки. Newly queued/EVALUATED
Defend без retirement intent проходит обычный lifecycle; RUNNING не служит
основанием для retirement. Stop по-прежнему снимает собственный task через
Handoff до уничтожения plan.

## Проверки

Перед правкой canonical `Invoke-AICFChecks.ps1` выполнил9 профильных проверок:
**9/9 PASS/0**, session27589 closed0,02:36:21.548–02:37:06.963 MSK31.10.2026.
Evidence и исходные source copies/SHA:
`.codex-runtime/issue78-finalization-20261030/acquisition-real-staging-hold/baseline/`
и соседние `before-source/`, `before-source.json`.

Первый after-v1 тех же9 проверок: **9/9 PASS/0**, session78899 closed0,
02:43:02.477–02:43:47.368 MSK. Он предшествует дополнительному reentry/authority
hardening, поэтому финальный профильный запуск фиксируется отдельно.
Новый `Test-VehicleStagingHoldContracts.ps1`: **250 actual cases/28 executable
unsafe PASS**, evidence `acquisition-real-staging-hold/wcs-contract-retirement-final/`.
Промежуточный test-development FAIL `retirementLatch` сохранён: прежний тест
перехватывал reentry раньше immutable latch; добавлен прямой actual
`BeginRetirement` retry с проверкой исходного payload. Production FAIL этим
тестом не выявлен; исходные native failures сохранены отдельно.

Финальные canonical9 проверок на том же source после retirement/origin fix:
**9/9 PASS/0**, session8059 closed0,03:18:15.785–03:18:54.343 MSK.
Receipt `acquisition-real-staging-hold/source-ready-final.json` подтверждает
9 source hashes без drift и прежний test SHA `E381DC87209FB55B3F03202B7C56B8E64DEA17F2B45E7AEEB3570DAF7A6A6BDA`.

Production-only Workbench frozen V28 (386 production files): **3/3 PASS**,
Stock/RHS/ArlandWCSRHS, native0/validation successful/script fatal0;
session8904 closed0. Exact DryRun manifests, commands и полные sibling logs:
`.codex-runtime/issue78-finalization-20261029/staging-cleanup-v28-compile-summary.json`
и `wb-staging-cleanup-v28-{Stock,RHS,ArlandWCSRHS}/`. Первая production-only
подготовка без launcher/tools не запускала Workbench; preparation exit1 сохранён
Root отдельно. Compile не является runtime или Full offline PASS.

Полный frozen `r29`: **264/264 PASS**, collector0/exact unique union/
Logistics1/1027SHA0; все6 actual sessions закрылись0. Production386
byte-identical compiled V28 (`r29-production-compile-identity.json`).
Full включает новый250/28 contract. Первые V28 harness failures сохранены,
их причины исправлены перед повтором. Отдельный late-human cleanup defect,
найденный peer после freeze, этим результатом не снимается; он требует
самостоятельного исправления и нового gate. Scope и команды — TESTING.md.

Native нового hold исправления пока **NOT RUN**. Frozen V27
262/262 PASS относится к исходному production до этого исправления. Оба
US/USSR1800 завершились на неизменном старом stage: native0, runner/audit1.
Они не проверяют новый код. Для USSR подтверждены полное окно1800296мс,
wall1821.8496768с,4 оплаченных joins,5 exact seats и accepted short entry
1.88272м при прежнем cap25м; functional/fatal/resource gates остались FAIL.
Полные stopped sibling SHA и независимый review сохранены в
`ordinary-topology-USSR-1800-v1/offline-stopped-review-v1.json`.
Сохранены все исходные taskless, dismount и retained-cleanup failures;
успешный targeted replay не меняет их verdict.
