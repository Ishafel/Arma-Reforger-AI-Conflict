# Проверки и baseline

Перед правкой выполни относящиеся к домену проверки и сохрани полный вывод,
commit/branch, dirty status и exit codes. После правки повтори тот же набор и
сравни конкретные rule IDs. Известный FAIL не скрывай и не исправляй regex ради PASS.

Для работы по issue применяй [правила приёмки и привязки PR](../AGENTS.md#работа-по-issue-и-привязка-pr):
в активном checklist остаются только самостоятельно проверяемые агентом критерии.
Выполни их все; недоступные проверки явно отклони с причиной и ограничением
`NOT RUN` в отдельном разделе issue, без ожидания игрока/тестировщика. Исторические
verdict ниже сохраняют фактический статус; отклонение критерия не даёт новый PASS.

## PR #43: срочность и очистка manual intent — 2026-10-14

Продолжение `codex/issue-42-adaptive-targets` от `b16b792`; исходное дерево чистое.
Исправлены два замечания review: появление угрозы на уже выбранной цели
`FORWARD_DEFEND` немедленно переводит её в `QRF`; недопустимый player intent
очищается planner до Select, вместе со старым waypoint и assignment revision,
после чего новое AI-решение применяется в том же проходе. Valid manual intent,
повторный `QRF`, deescalation gates и vehicle suspension сохранены.
Evidence: `.codex-runtime/issue42-qrf/`; Game/Server/Workbench **1.8.0.13**.

Команды и отдельные verdict (пути helper относительно этого evidence):

- `tools/Invoke-AICFChecks.ps1 -Name Test-Stage35Static.ps1,Test-Stage35RecoveryPolicy.ps1,Test-Stage4Static.ps1,Test-AICommanderModeStatic.ps1,Test-SquadCommandsContracts.ps1,Test-AdaptiveCommanderContracts.ps1 -EvidenceRoot <before|after>`:
  оба **4 PASS / 2 прежних FAIL**, общий exit 1. Сохранены только прежние
  `AI_COMMANDER_COMPONENT` и `STABLE_NEAREST`; новых failures нет.
  Adaptive audit расширен с 22/19 до **26 правил / 23 отрицательных source
  mutations PASS/0**; `static-comparison.json`, полные outputs и argv сохранены.
- `compile.ps1 -Variant Stock|Everon|EveronNorthWCSRHS -Attempt production-final -SourceRoot .`:
  три production Workbench **PASS/0**, validation successful, SCRIPT E/F и
  ENGINE F — 0; прочие resource (E) **25/25/39**. Полные
  `workbench-*-production-final/console.log`, `compile-command-*.json`,
  `production-compile-summary.json`. Fixture: `-Variant Stock|Everon
  -Attempt fixture-verified -SourceRoot <stage-after>` — оба **PASS/0**;
  frozen baseline `-Variant Stock -Attempt baseline-assignment -SourceRoot
  <stage-before>` — **PASS/0**.
- `runtime.ps1 -Stage stage-before -Probe aicfAdaptiveProbe -Attempt baseline-verified -Port 2333`:
  frozen production `b16b792` с тем же новым fixture. **75 checks / 14 failures**,
  native exit 0, `audit.ps1 -Attempt baseline-verified` — ожидаемый **FAIL/1**.
  Preconditions существующих HQ/base `FORWARD_DEFEND` проходят; восемь проверок
  QRF и шесть проверок нового manual assignment/удаления waypoint воспроизводят
  оба дефекта. Полный остановленный log:
  `server-baseline-verified/logs/logs_2026-10-14_16-08-40/console.log`.
- `runtime.ps1 -Stage stage-after -Probe aicfAdaptiveProbe -Attempt arland-verified -Port 2334`
  и отдельная сессия `-Variant Everon -Attempt everon-verified -Port 2335`:
  на каждой карте **75/75 PASS**, native exits 0. Canonical launcher:
  `tools/Start-AICFRuntime.ps1 -Role Server -Difficulty Easy -AICommanderMode BOTH`,
  свежие profiles, `-aicfAdaptiveProbe 1 -aicfRequirePlayerForResult 0 -addr
  127.0.0.1:<port>`. Exact CLI arrays и `AICF_RUNTIME_MANIFEST_JSON` —
  `launcher-*-verified.txt`. Arland **16:09:54–16:10:38 MSK**, Everon
  **16:10:06–16:10:53 MSK**, штатные Stop/RequestClose и `Game destroyed.`.
  Полные logs: `server-arland-verified/logs/logs_2026-10-14_16-09-54/console.log`
  и `server-everon-verified/logs/logs_2026-10-14_16-10-06/console.log`.
  `audit.ps1 -Attempt arland-verified|everon-verified` — оба **PASS/0**.
  SCRIPT E/F, ENGINE F и VM exceptions — 0, прочие (E) **14/47**;
  engine logs не полностью чистые. `summary-*-verified.json` сохранены.
- `negative-log-audits.ps1`: **2 позитивных / 17 отрицательных контроля PASS/0**.
  Реальный baseline и удаление каждого из 16 новых required cases отклонены
  анализатором, expected exit 1; `negative-logs/summary.json`.
- `production-stage-hashes.json`: **258/258** production `.c/.conf/.gproj`
  совпадают со stage по SHA-256; baseline hashes и точный fixture snapshot
  сохранены. `git diff --check` — **PASS/0**. Два известных stage удалены после
  сохранения evidence; своих native test/Workbench процессов нет.

Исходная спокойная оборона обычной базы задаётся через production ReplaceOrder
в stage-only wrapper: stock radio может временно отключать spawn point после
смены владельца. Threat input controlled, assignment/queue/entity cleanup —
production. Ранние `baseline`/`baseline-controlled` не PASS: не проходило это
precondition. Fixture исправлен, frozen baseline и обе матрицы повторены целиком.
Промежуточные logs сохранены; native exit 0 не заменяет отсутствие test failures.

Изменены восемь файлов: `AICF_AICommander.c`, `AICF_AdaptiveCommanderPolicy.c`,
`AICF_OrderPlanner.c`, adaptive contracts/probe/log audit, ADAPTIVE_COMMANDER и
эта заметка; точные пути — `changed-files.txt`. Полный offline набор, client/JIP,
настоящий transport trip, естественный бой до победы, длительный soak,
packaged build и visual — **NOT RUN**. Merge и релиз не выполнялись.

## Issue #42: адаптивные цели — 2026-10-14

Ветка `codex/issue-42-adaptive-targets` от полученного `main` `03d08c5`, исходное
дерево чистое. Контракты, веса и fallback: [ADAPTIVE_COMMANDER.md](ADAPTIVE_COMMANDER.md).
Evidence: `.codex-runtime/issue42/`; Game/Server/Workbench **1.8.0.13**.
Outcome: локальная реализация проверена; `ACCEPTED` не объявляется.
Push, PR, merge и релиз в эту задачу не входят.

Команды и отдельные verdict:

- `tools/Invoke-AICFChecks.ps1 -Name Test-Stage35Static.ps1,Test-Stage35RecoveryPolicy.ps1,Test-Stage4Static.ps1,Test-AICommanderModeStatic.ps1,Test-SquadCommandsContracts.ps1 -EvidenceRoot .codex-runtime/issue42/before-main`:
  **3 PASS / 2 прежних FAIL**, общий exit 1. После добавления
  `Test-AdaptiveCommanderContracts.ps1`, тот же runner в `after-complete/`:
  **4 PASS / 2 прежних FAIL**, exit 1. IDs прежних failures:
  `AI_COMMANDER_COMPONENT` — отсутствует unique `SCR_GameModeCampaign`;
  `STABLE_NEAREST` — прежний SquadCommands guard. Новых failures нет;
  `static-comparison.json`. Новый audit: **22 правила / 19 отрицательных source
  mutations PASS / 0**. Stage35 distribution contract обновлён по #42: score
  и allocation заменили обязательное распределение по role index. Readiness,
  vehicle, recovery и authority guards не ослаблены.
- `.codex-runtime/issue42/compile.ps1 -Variant Stock|Everon|EveronNorthWCSRHS -Attempt production-final -SourceRoot .`:
  три production Workbench **PASS / native exit 0**, validation successful,
  SCRIPT E/F и ENGINE F — 0; прочие resource строки (E) **25 / 25 / 39**.
  `production-compile-summary.json`, `compile-command-*.json` и полные
  `workbench-*-production-final/console.log` сохранены.
- `compile.ps1 -Variant Stock|Everon -Attempt fixture-clock-fixed -SourceRoot <stage-after>`:
  оба fixture graphs **PASS / 0**, validation successful, SCRIPT E/F и ENGINE F
  — 0. Baseline fixture `-Variant Stock -Attempt baseline-applied -SourceRoot
  <stage-before>` также **PASS / 0**. Статика и compile не заменяют runtime.
- `runtime.ps1 -Stage stage-before -Probe aicfAdaptiveBaseline -Attempt baseline-applied -Port 2331`:
  **native exit 0**, frozen production `03d08c5`, stock Arland/Easy/BOTH.
  Native initial snapshot имел одну допустимую цель. Controlled три допустимые
  цели дают **1/2/2** в двух одинаковых snapshot; три настоящих initial seed
  groups с fixture desired size 1 получают эти AI assignments и bound waypoints.
  `Test-AdaptiveCommanderLog.ps1 -LogPath <полный log> -Baseline` — **PASS / 0**.
  Полный остановленный log:
  `server-baseline-applied/logs/logs_2026-10-14_14-19-00/console.log`.
  SCRIPT/fatal — 0, прочие (E) — 14. Input, module CRC, версии и source hashes
  сохранены. Это controlled воспроизведение фиксированной схемы, не подтверждение
  эпизода игрока в обычном матче. Дополнительный frozen baseline выполнен после
  разработки fixture; production stage всё время оставался исходным.
- `runtime.ps1 -Stage stage-after -Probe aicfAdaptiveProbe -Attempt arland-verified -Port 2331`
  и отдельная сессия `-Variant Everon -Attempt everon-verified -Port 2332`:
  **59/59 PASS на каждой карте**, native exits 0. Canonical launcher:
  `Start-AICFRuntime.ps1 -Role Server -Difficulty Easy -AICommanderMode BOTH`,
  свежие profiles, `-aicfAdaptiveProbe 1 -aicfRequirePlayerForResult 0 -addr
  127.0.0.1:<port>`; точные CLI arrays и `AICF_RUNTIME_MANIFEST_JSON` —
  `launcher-*-verified.txt`. Arland **14:16:26–14:17:03 MSK**, Everon
  **14:16:38–14:17:39 MSK**, Stop/RequestClose и `Game destroyed.`.
  Полные logs: `server-arland-verified/logs/logs_2026-10-14_14-16-26/console.log`
  и `server-everon-verified/logs/logs_2026-10-14_14-16-38/console.log`.
  `audit.ps1 -Attempt arland-verified|everon-verified` вызывает
  `Test-AdaptiveCommanderLog.ps1`: оба **PASS / 0**. SCRIPT E/F, ENGINE F,
  VM/null exceptions — 0; прочие resource/backend (E) **14 / 47**.
  Engine logs не полностью чистые; отдельного native Everon before/after error
  comparison нет. Сводки: `summary-*-verified.json`.
- `negative-log-audits.ps1`: **3 позитивных контроля / 14 отрицательных log
  mutations PASS / 0**. Отсутствие distribution/joint/manual/urgency/fallback/
  identity/loss cases, неверное число cases, duplicate, fatal, отсутствие
  Stop/trace/roster request и неверный actual baseline assignment отклоняются.
  Команды и exits каждого входа: `negative-logs/summary.json`.
- `production-stage-hashes.json`: **258/258** `.c/.conf/.gproj` совпадают по
  SHA-256. Frozen baseline hashes и точные fixture snapshots сохранены отдельно.
  `git diff --check` — **PASS / 0**. Два известных stage удалены после сохранения
  evidence; собственных native test/Workbench процессов нет.

Pure cases вызывают production Choose/Evaluate: victory/radio value, position,
unavailable/deferred/known-length route, HQ/base threat, stale/unknown fallback,
full/weakened/not-ready force, distribution/concentration, continuation/progress,
deterministic tie и отсутствие кандидатов. Runtime проверяет настоящие full async
rosters, assignments/replicated waypoints, same-cycle/manual force, graph cut/bypass,
gates/срочность, failure expiry/HQ fallback, owner change, loss TTL/rebuild,
stale generation/graph/assignment, независимость фракций и vehicle suspension.
Radio eligibility/threat и равные pair inputs поданы fixture; это controlled
проверка, не наблюдение естественного баланса матча.

Ранние попытки не PASS: sandbox Workbench не получил SteamAPI; production compile
отверг слишком сложное Enforce expression; fixture compiles выявили имя параметра
Stop, конфликт имени helper с constructor и отсутствие explicit faction cast.
Неудачный server attempt остановлен адресно. `arland-final`/`everon-final`
остановлены из-за отрицательных synthetic timestamps: gate считал возраст 0.
Исправлены только fixture часы, обе полные матрицы повторены на свежих profiles.
Все промежуточные logs/exits сохранены. `before/` создан до switch и не используется
вместо согласованного `before-main/`.

Полный offline набор, client/JIP, настоящий transport trip, естественный бой до
победы, балансный/длительный soak, packaged build и visual — **NOT RUN**. Эти
границы не входят в выполненные controlled критерии и не превращаются в PASS.
Новой версии релиза нет.

Изменены **21 файл** (также `changed-files.txt` в evidence):

- Production: `Command/AICF_AICommander.c`, `AICF_AdaptiveCommanderPolicy.c`,
  `AICF_AdaptiveScore.c`, `Config/AICF_AdaptiveCommanderConfig.c`,
  `Objectives/AICF_TargetSelector.c`, `Orders/AICF_OrderPlanner.c`,
  `State/AICF_GroupSlot.c` под `AIConflictCore/Scripts/Game/AIConflict/`.
- Документация: корневой `README.md`; `agent-notes/README.md`, `ARCHITECTURE.md`,
  `GAMEPLAY.md`, `ADAPTIVE_COMMANDER.md`, `TESTING.md`, `TEST_FIXTURES.md`, `TOOLS.md`.
- Проверки: `tests/static/Test-Stage35Static.ps1`,
  `tests/contracts/Test-AdaptiveCommanderContracts.ps1`,
  `tests/log-audits/Test-AdaptiveCommanderLog.ps1`,
  `tests/fixtures/AICF_AdaptiveBaselineProbe.c`, `AICF_AdaptiveCommanderProbe.c`,
  `AICF_AdaptiveProbeWorld.c`.

## PR #40: возврат оружия после idle — отчёт 2026-10-14

Продолжение `codex/issue-38-idle-greetings`, baseline `1d265b8`, исходное дерево
чистое. Проверки выполнены 2026-10-13 на Game/Server/Tools `1.8.0.13`, полные
результаты оформлены после возобновления задачи. Evidence:
`.codex-runtime/issue-38-weapon-restore/`; команды, manifests, native exits и
полные остановленные logs сохранены. Старые результаты ниже не заменяют этот gate.

`SelectWeapon(null)` заменён native contextual holster внутри `StartLoitering`.
Для отмены до начала loiter controller ждёт завершения принятого unequip в
`OnApplyControls`, проверяет исходные member/generation/weapon identity и наличие
оружия, затем возвращает его. Ожидание ограничено 4000 ms; player control,
другой предмет и потеря identity прекращают восстановление. Новых таймеров нет.

- `Invoke-AICFChecks.ps1 -Name Test-SocialContracts.ps1,Test-SquadCommandsContracts.ps1,Test-AICombatInputContracts.ps1 -EvidenceRoot <before|after-final>`:
  **2 PASS, прежний FAIL `STABLE_NEAREST`, exit 1** до/после. Social —
  **24 правила / 20 отрицательных мутаций PASS**; `static-comparison.json`.
- `compile.ps1 -Variant Stock|ArlandWCSRHS -Attempt production-final -SourceRoot .`:
  обе **PASS / native exit 0**, validation successful, SCRIPT E/F = 0;
  прочие E/F **25/39**. WCS fixture `-Attempt fixture-final -SourceRoot <stage>`
  также **PASS / 0**.
- `runtime.ps1 -Attempt baseline -Stage stage-before -Probe aicfSocialProbe -Commander USSR -Port 2316`:
  исходный Stock код воспроизвёл **8 failures возврата оружия**, 26/34 assertions
  PASS; исключение — уже безоружный боец. Native exit 0 означает завершение
  процесса, не успешную проверку. SCRIPT E/F = 0, прочие E/F 14.
- `runtime.ps1 -Attempt final-wcs -Probe aicfSocialProbe -Commander USSR -Port 2315 -Variant ArlandWCSRHS -ClientProbe`,
  отдельная сессия `client.ps1 -Attempt client-final-wcs -Server final-wcs -Variant ArlandWCSRHS -Port 2315`:
  **34/34 server assertions и 9/9 client observations PASS**, оба native exits 0.
  WCS + RHS US, Easy fixture. Проверены четыре idle deadline, приказ, угроза,
  отмена в том же frame, изначально безоружный боец и Stop. На время проверки
  выбранный AI agent приостановлен, чтобы его weapon behavior не скрывал дефект;
  предмет в руках через 4000 ms сравнивается с исходной entity. Deadline ускорен,
  полного ожидания 20 секунд для каждого emote нет. Клиент сравнивает RplId.
  SCRIPT E/F = 0; прочие E/F server/client **104/173**, engine logs не чистые.
- `audit.ps1` запускает `Test-SocialWeaponLog.ps1 -LogPath ... -ClientLogPath ...`:
  **PASS / 0**, исходный дефект и отрицательное client observation отклонены.
  Production/stage SHA256 совпали; `summary.json`, `production-stage-hashes.json`.

Промежуточные попытки не PASS: первоначальный fixture не включал обязательный
client companion; затем ошибочно выбирал гранату либо ждал продолжающего движение
бойца. Три зависшие подготовительные попытки остановлены принудительно; их logs
не используются для приёмки. Приостановка AI устранила также возможность ложного
PASS от его собственного перевооружения. Первый native contextual вариант дал
33/34: немедленный recover не отменял ещё ожидающий unequip. Финальный путь через
`OnApplyControls` подтвердился отдельным Stock early probe (2/2) и полной WCS
матрицей. У log audit исправлена UTF-8 BOM для Windows PowerShell 5; требования
к результату не ослаблены.

Полный offline набор, повтор полной greeting/audio/JIP матрицы на этом изменении,
визуальная оценка и прослушивание — **NOT RUN**. Отдельная полная Stock matrix
окончательного кода не запускалась: Stock early и WCS full имеют указанные выше
границы. `ACCEPTED` не объявляется.

## Issue #38: исправление скольжения с активным idle — 2026-10-13

Продолжение `codex/issue-38-idle-greetings`, baseline `71ae38e`, чистое дерево.
После сообщения игрока о US бойце, который отжимался при движении по приказу,
добавлена проверка именно client graph. Прежний server command gate не доказывал
его завершения. `AICF_SocialLoiterExit` завершает собственный idle через native
`CMD_Gestures=-2` и DONE, с ordered reliable start/stop на character controller.

Evidence: `.codex-runtime/issue-38-loiter-exit/`. Game/Server/Tools `1.8.0.13`.
Helpers `compile.ps1`, `runtime.ps1`, `client.ps1`, `audit.ps1` находятся там же;
manifest/argv, native exits и полные остановленные logs сохранены.

- `Invoke-AICFChecks.ps1 -Name Test-SocialContracts.ps1,Test-SquadCommandsContracts.ps1,Test-AICombatInputContracts.ps1 -EvidenceRoot <before|after>`:
  **2 PASS, 1 прежний FAIL / exit 1**. `STABLE_NEAREST` сохранён;
  `static-comparison.json`. Social: **22 правила, 18 отрицательных мутаций PASS**.
- `compile.ps1 -Variant Stock -Attempt production-second -SourceRoot .` и
  `-Variant ArlandWCSRHS -Attempt production-final -SourceRoot .`:
  **PASS / native exit 0**, validation successful, SCRIPT E/F = 0;
  прочие resource E/F — **25 / 39**. Финальный WCS fixture
  `-Attempt final-fixture-movement -SourceRoot <stage-after>` — **PASS / 0**.
- Исходный Stock US, `runtime.ps1 -Attempt before -Stage stage-before -Probe aicfSocialProbe -ClientProbe -Commander USSR -Port 2311`,
  `client.ps1 -Attempt client-before -Server before -Stage stage-before -Port 2311`,
  затем `client-jip-before -JIP`: старые 52 server cases прошли, но новая
  диагностика воспроизвела **4/4 client graph FAIL**: через 1500 ms после отмены
  `tag=1 command=0` для Smoke, Pushups, Chair и Sit. Полные logs остановлены;
  `baseline-regression-verdict.txt`, `baseline-client-exit-failures.txt`.
- Финальный WCS + RHS US (Easy fixture):
  `runtime.ps1 -Attempt verified-wcs -Stage stage-after -Variant ArlandWCSRHS -Probe aicfSocialProbe -ClientProbe -Commander USSR -Port 2314`,
  `client.ps1 -Attempt client-verified-wcs -Server verified-wcs -Stage stage-after -Variant ArlandWCSRHS -Port 2314`,
  затем `client-jip-verified-wcs -JIP`: **57/57 server cases**, пять client exit
  observations **PASS**. Каждый emote отменён strategic assignment с сохранением
  waypoint, отдельно Pushups отменён через 500 ms. Клиент видел активный tag
  перед отменой; через 1500 ms tag и command отсутствовали. Свободный SALUTE,
  5 audio events, 5 dedupe, respawn/cooldown и JIP без replay также прошли.
  Все три native exits 0. `audit.ps1` запускает
  `Test-SocialLog.ps1 -GreetingPriority -ExitGuard -ClientExit` — **PASS / 0**;
  отрицательные полные logs с зависшим graph и отсутствующим early exit отклонены.
  Server/client SCRIPT E/F = 0, прочие resource E/F — **105 / 174**;
  engine logs не полностью чистые.

Промежуточные попытки не объявляются финальным PASS: первый adapter нельзя было
скомпилировать из-за modded native serialization; первое расширение fixture
загружалось раньше своего базового modded класса. Один JIP запуск получил эту
ошибку компиляции, несмотря на native exit 0. Ранний observer принимал следующий
emote предварительной matrix за продолжение предыдущего; финальный observer
связан только с live greeting/order cases. В `final-wcs` пять exit observations
прошли, но старый тест свободного SALUTE ждал уже ушедшего бойца и завершился
timeout; исправлен только выбор допустимого кандидата в fixture.

Изменённые production scripts сверены с final stage по SHA256
(`production-stage-hashes.json`). Полный offline набор, отдельный Stock runtime
на новом коде, визуальная оценка blending и прослушивание — **NOT RUN**.
Ранее заявленные результаты ниже исторические; они не подтверждали отсутствие
оставшегося idle tag на клиенте. `ACCEPTED` не объявлялся.

## Issue #38: idle emotes и независимое приветствие — 2026-10-13

Ветка `codex/issue-38-idle-greetings` от обновлённого `origin/main` `80a1403`,
исходное дерево чистое. По уточнению пользователя реплика не прерывает emote;
SALUTE вызывается только без другой анимации. Общий cooldown на игрока 30000 ms,
радиусы 5/7 м. После собственного loiter действует дополнительный guard 4000 ms
на native out-переход. Контракты описаны в [SOCIAL_BEHAVIOR.md](SOCIAL_BEHAVIOR.md).

Evidence: `.codex-runtime/issue-38-idle-greetings/`; Game/Server/Tools `1.8.0.13`.
Все команды из корня репозитория. Helpers `compile.ps1`, `runtime.ps1`,
`client.ps1`, `negative-logs.ps1` находятся в evidence; argv, native exits,
launcher manifests и полные logs сохранены. Fixture только в isolated stage.

- `tools/Invoke-AICFChecks.ps1 -Name Test-RecruitmentSequenceContracts.ps1,Test-RecruitmentPriorityContracts.ps1,Test-InfantryRecruitmentStatic.ps1,Test-SquadCommandsContracts.ps1,Test-Stage35Static.ps1,Test-Stage4Static.ps1,Test-AICombatInputContracts.ps1 -EvidenceRoot <before>`:
  **6 PASS, 1 FAIL / exit 1**. Повтор с `Test-SocialContracts.ps1`, `<after-final>`:
  **7 PASS, тот же 1 FAIL / exit 1**. Baseline `STABLE_NEAREST` сохранён,
  прежние exit codes совпали (`baseline-comparison.json`). SocialContracts:
  **19 правил, 15 отрицательных мутаций PASS**.
- `compile.ps1 -Variant Stock|EveronNorthWCSRHS -Attempt production-final -SourceRoot .`:
  **обе PASS / native exit 0**, `Script validation successful`, SCRIPT E/F = 0.
  Прочие resource E/F: **25 / 38**; engine resource logs не полностью чистые.
  Сводка: `compile-summary-final.json`.
- `runtime.ps1 -Attempt policy-4 -Probe aicfSocialProbe -Port 2303`:
  **42/42 cases, native exit 0**, `Test-SocialLog.ps1 -Policy` — **PASS**.
  Реальные initial groups, четыре native emote, запрет рестарта, selection,
  конкуренция двух ботов, muster/приказ/угроза, смерть, replacement и Stop.
- Stock US: `runtime.ps1 -Attempt voice-2 -Probe aicfSocialProbe -ClientProbe -Commander USSR`,
  `client.ps1 -Attempt client-voice-2 -Server voice-2`, затем
  `client.ps1 -Attempt client-jip-voice-2 -Server voice-2 -JIP`:
  **47/47 cases**, `Test-SocialLog.ps1 -ClientLogPath ... -JIPLogPath ...` — **PASS**.
  Четыре реплики без SALUTE при сохранении каждого idle, отдельный SALUTE
  свободного бойца с клиентским animation tag, 5 проигранных audio events,
  5 отклонённых дублей, настоящий respawn с прежним cooldown и JIP без replay.
  Все три native exits 0; сохранён один native support-area SCRIPT diagnostic.
- WCS + RHS USSR: `runtime.ps1 -Attempt wcs-voice-1 -Probe aicfSocialProbe -ClientProbe -Commander US -Side USSR -Variant ArlandWCSRHS`,
  соответствующие `client.ps1 -Attempt client-wcs-voice-1 -Server wcs-voice-1 -Variant ArlandWCSRHS -Commander US -Side USSR`
  и новый client `-JIP`: **48/48 cases**, log audit с `-GreetingPriority` — **PASS**.
  `RHS_USAF`/`RHS_AFRF` mapping и English project разрешены; подтверждена также
  немедленная отмена SALUTE/реплики при смене приказа с сохранением waypoint.
  Этот runtime выполнен до дополнительного out-transition guard; окончательный
  WCS compile включает guard, его runtime проверяется отдельно на Stock USSR.
- `negative-logs.ps1`: **PASS / 0** — положительный контроль и 8 отклонённых
  полных логов с нарушениями playback, SALUTE, idle continuity, dedupe, mapping,
  JIP, незавершённым логом и отрицательным fixture case.

- Окончательный код, Stock USSR:
  `runtime.ps1 -Attempt final-ussr -Stage stage-final -Probe aicfSocialProbe -ClientProbe -Commander US -Side USSR -Port 2304`,
  `client.ps1 -Attempt client-final-ussr -Server final-ussr -Stage stage-final -Port 2304 -Commander US -Side USSR`,
  затем новый `client-jip-final-ussr` с `-JIP`: **52/52 cases**,
  `Test-SocialLog.ps1 -ClientLogPath ... -JIPLogPath ... -GreetingPriority -ExitGuard`
  — **PASS / 0**. Все три native exits 0; два native support-area SCRIPT diagnostics.
  Подтверждены четыре out-transition отказа SALUTE, voice-only/continuity,
  native SALUTE + клиентский tag, прерывание приказом, cooldown/respawn/dedupe/JIP.
  Полные logs: `server-final-ussr/logs/logs_2026-10-13_18-07-54/console.log`,
  `client-final-ussr/logs/logs_2026-10-13_18-09-17/console.log`; JIP —
  `client-jip-final-ussr/logs/`. SHA256 всех 188 production scripts и двух
  fixtures сверены с final stage; `production-stage-final-sha256.json`.

Матрицы `policy-4`, `voice-2` и `wcs-voice-1` предшествовали добавлению только
out-transition guard. Финальная матрица Stock USSR проверяет guard на каждом
emote; оба финальных production compile включают его. Статические gates после
последней production правки повторены в `after-final`.

Предшествующие диагностические попытки сохранены отдельно и не объявляются
PASS новой концепции. Среди них — отказ клиентского SALUTE при немедленном
переходе из idle, ошибки компиляции промежуточного fixture и startup crash
`live-12` в native Rocket_RPG7VM resource до запуска social service.
Новые production ресурсы не добавлялись; английский ACP/event/sample и четыре
emote configs прочитаны из vanilla PAK без изменения установленных файлов.

Визуальная естественность/clipping и субъективное прослушивание — **NOT RUN**,
как отклонённые критерии issue. API/animation tag/audio playback подтверждают
выполнение, но не эстетическое качество или слышимость для человека. GUI,
screenshots/video и ручной тестировщик не использовались. `ACCEPTED` не объявлялся.

## Issue #37: последовательный набор — 2026-10-13

Ветка `codex/issue-37-sequential-recruitment` от обновлённого `origin/main`
`6f86130`, исходное дерево чистое. Одна автоматическая заявка на фракцию,
независимо от казарм; закреплённый владелец не вытесняется новой атакой.
Ручной приоритет #32 сохранён. Первый и последующие шаги — 5000 ms;
прогноз моделирует предшествующие заявки и отдельный склад каждой базы.
Timeout завершается до gate ожидания очереди, поэтому просроченный визит
не удерживает waypoint до общего VISIT_TIMEOUT.

Evidence: `.codex-runtime/issue-37-sequential-recruitment/`. Game/Server/Tools
`1.8.0.13`. Команды ниже выполняются из корня репозитория; `compile.ps1`,
`runtime.ps1`, `collect.ps1` и negative helpers находятся в этом evidence.
Полные logs, argv, native exits и launcher `AICF_RUNTIME_MANIFEST_JSON`
сохранены рядом. Fixture только в отдельных stage, после Core в Arland
integration; production не содержит тестовых overrides.

- `tools/Invoke-AICFChecks.ps1 -Name Test-RecruitmentSupplyContracts.ps1,Test-RecruitmentPriorityContracts.ps1,Test-InfantryRecruitmentStatic.ps1,Test-BarracksCombatContracts.ps1,Test-Stage35Static.ps1,Test-Stage4Static.ps1,Test-SquadCommandsContracts.ps1,Test-DefendWaypointInputContracts.ps1 -EvidenceRoot <before>`:
  **7 PASS, 1 FAIL / exit 1**. Повтор с дополнительным
  `Test-RecruitmentSequenceContracts.ps1`, `<after-timeout-final>`:
  **8 PASS, тот же 1 FAIL / exit 1**. Baseline `STABLE_NEAREST` в
  SquadCommandsContracts сохранён, все восемь прежних exit codes совпали.
  SequenceContracts — **14 правил / 14 отрицательных мутаций PASS**;
  PriorityContracts — 15/15, SupplyContracts — 14 мутаций PASS.
- `compile.ps1 -Variant Stock|EveronNorthWCSRHS -Attempt production-timeout-final -SourceRoot .`:
  обе terminal Workbench validation **PASS / native exit 0**,
  `Script validation successful`, SCRIPT/fatal = 0; прочих resource E/F —
  **25/39**. Это не полностью чистые resource logs.
- Fixture compile: `-Variant Stock -Attempt sequence-isolated-final`,
  `-Attempt priority-timeout-final -SourceRoot <priority-stage>` и
  `-Attempt lifecycle-timeout-final -SourceRoot <lifecycle-stage>`:
  **3 PASS / native exit 0**, validation successful, SCRIPT/fatal = 0,
  по 25 прочих resource E/F. Сводка — `compile-summary.json`.
- `runtime.ps1 -Attempt sequence-isolated-final -Probe aicfRecruitmentSequenceProbe`:
  **native exit 0**, `RequestClose`, полный остановленный лог
  `server-sequence-isolated-final/logs/logs_2026-10-13_14-31-04/console.log`.
  `Test-RecruitmentSequenceLog.ps1 -LogPath <лог>` и
  `Test-InfantryRecruitmentLog.ps1 -LogPath <лог>` — **PASS / 0**:
  **33/33 cases**, 12 визитов, 21 debit/transfer. Подтверждены общая и разные
  физические базы/казармы, ATTACK/FIFO, удержание начатой обороны при новой атаке,
  независимый USSR, ручное повышение, временный дефицит без обхода, timeout,
  отмена и stale generation; следующие заявки продолжают покупки.
  Девять вакансий дают **45000 ms** в управляемых production clock checks,
  forecast — 45 s собственного набора и 50 s с последующей одной вакансией.
  В runtime первый запрос был через **5016 ms**, девятый — через **56165 ms**,
  состав 10/10 — через **57181 ms** после допуска. Join→следующий spawn:
  **5001–14082 ms** (верхняя задержка включает manual wait); spawn→join:
  **998–1017 ms**. Нет мгновенного первого заказа или догоняющих пакетов.
  `nine-member-timeline.json` сохраняет все девять пар, roles/costs и времена.
  SCRIPT/fatal = 0, 14 native resource/world/shutdown E/F сохранены.
  `negative-audits.ps1 -LogPath <лог>` — **PASS/0**: положительный контроль
  и 12 отрицательных копий, в том числе ранний spawn, неверный первый срок,
  отсутствие владельца, оплаты/transfer, неверный roster и незавершённый лог.
- `runtime.ps1 -Attempt lifecycle-timeout-final -Probe aicfRecruitmentLifecycleProbe -Stage lifecycle-stage`:
  **native exit 0**, `RequestClose`, полный остановленный лог
  `server-lifecycle-timeout-final/logs/logs_2026-10-13_14-28-33/console.log`.
  `Test-RecruitmentAcceptanceLog.ps1 -Mode Lifecycle -LogPath <лог>` и
  `Test-InfantryRecruitmentLog.ps1 -LogPath <лог>` — **PASS / 0**:
  **11/11 cases**, 9 debit/transfer. Настоящие native ownership/death/replacement,
  новая generation/entity, новая заявка и отказ старых callback/spawn/debit.
  SCRIPT/fatal = 0, 14 native resource/world/shutdown E/F сохранены.
  Копия без `REPLACEMENT_STALE_CALLBACK` отклонена с exit 1.
- `runtime.ps1 -Attempt priority-timeout-final -Probe aicfRecruitmentPriorityProbe -Stage priority-stage`:
  **native exit 0**, `RequestClose`, полный остановленный лог
  `server-priority-timeout-final/logs/logs_2026-10-13_14-35-00/console.log`.
  `Test-RecruitmentPriorityLog.ps1 -LogPath <лог>` и
  `Test-InfantryRecruitmentLog.ps1 -LogPath <лог>` — **PASS / 0**:
  **31/31 cases**, 6 debit/transfer и одна отмена неоплаченного donor.
  Включены RESERVE, удержание активной заявки после изменения роли,
  manual promotion/preemption, pending authority, FIFO, identity/deadline
  fences и точные списания. SCRIPT/fatal = 0, те же 14 native E/F.
  `priority-negative-audits.ps1 -LogPath <лог>` — **PASS / 0**:
  два положительных контроля и десять отрицательных копий (exit 1).

Все runtime запущены последовательно через canonical
`tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock -RepositoryRoot <stage>
-ProfileRoot <fresh-profile> -AdditionalArguments @('-<probe>','1',
'-aicfRequirePlayerForResult','0','-addr','127.0.0.1:2302')`, Easy.
Launcher manifests и точные CLI сохранены в `launcher-<attempt>.txt`.
`stage-source-hashes.json`: 195 production `.c`/`.conf`/`.gproj` файлов совпали
с каждой из трёх stage (585 сравнений); `fixture-hashes.json` отдельно сверяет
три fixture. Static/compile/runtime остаются разными gates.
14 native E/F совпали после удаления timestamps с сохранённым полным логом
#32 `server-priority-authority`; `native-baseline-comparison.json` фиксирует
источник и отсутствие различий. Это сохранённый resource baseline, не новый
чистый engine PASS. Сводки итоговых server gates — `*-summary.json`.
`git diff --check` и локальные ссылки Markdown — **PASS / 0**.
Все свои engine процессы завершены, три stage удалены после сохранения SHA-256;
полные profiles/logs и helper-команды остались в evidence.

Подготовительные `sequence-1/2/3`, `sequence-final`, `sequence-final2` и
`sequence-timeout-final` не являются итоговым PASS. Сохранены compile/fixture
ошибки, ранний отказ до загрузки navigation tile, неготовность отменённого slot
к новому приказу и контролируемые остановки. `sequence-final2` — **31/33 FAIL**:
выявил отсутствие немедленного Finish после supply timeout и неверный запас
fixture (stock fallback стоил дешевле предположенного). Timeout исправлен в
production, budget берётся из production quote. Это не ослабление проверок.
Первый sandbox Workbench завершился `SteamAPI_Init failed`, exit -1;
успешные terminal повторы выполнялись с доступом к Steam.

**NOT RUN:** полный offline набор, естественная навигация/боевые потери во время
sequence matrix, client/JIP и player UI/RPC, WCS/RHS runtime, долгий soak,
packaged build и публикация. Sequence fixture управляет admission, arrival,
ролями, запасом и income snapshot; expiry и stale generation задаются явно.
Lifecycle отдельно подтверждает настоящую гибель и replacement. Плановые
45 секунд не выдаются за реальное wall-clock время с async spawn/tick.

Изменённые файлы (22): Core `AICF_InfantryRecruitmentConfig.c`,
`AICF_RecruitmentSupplyForecast.c`, `AICF_InfantryRecruitSpawner.c`,
`AICF_InfantryRecruitmentOrder.c`, `AICF_InfantryRecruitmentService.c`,
`AICF_RecruitmentPriority.c`; README; agent-notes `ARCHITECTURE.md`, `GAMEPLAY.md`,
`README.md`, `RECRUITMENT_SUPPLY_PLANNING.md`, `TEST_FIXTURES.md`, `TOOLS.md`,
`TESTING.md`; tests contracts `Test-RecruitmentPriorityContracts.ps1`,
`Test-RecruitmentSequenceContracts.ps1`; fixtures `AICF_RecruitmentLifecycleProbe.c`,
`AICF_RecruitmentPriorityProbe.c`, `AICF_RecruitmentSequenceProbe.c`; log-audits
`Test-RecruitmentAcceptanceLog.ps1`, `Test-RecruitmentPriorityLog.ps1`,
`Test-RecruitmentSequenceLog.ps1`. Точные пути — `changed-files.txt` в evidence.
Работа самостоятельно не объявляется ACCEPTED.

## Issue #32: приоритет пополнения — 2026-10-13

Ветка `codex/issue-32-recruitment-priority` от обновлённого `origin/main`
`2711c00`, исходное дерево чистое. Очередь: ручное пополнение, автоматическая
атака, оборона/резерв; FIFO внутри ранга. Приоритет действует в прогнозе,
ожидании плана, допуске donor и непосредственно перед оплатой. Ручная очередь
защищена от AI commander, неоплаченный donor может быть отменён; купленные
бойцы сохраняются. Контракт: [RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md).

Изменены Core `AICF_RecruitmentPriority`, `AICF_InfantryRecruitmentOrder`,
`AICF_InfantryRecruitmentService`, `AICF_InfantryRecruitmentEconomy`;
`Test-RecruitmentPriorityContracts.ps1`, `AICF_RecruitmentPriorityProbe.c`,
`Test-RecruitmentPriorityLog.ps1`, `Test-InfantryRecruitmentLog.ps1`;
README, ARCHITECTURE, указатель agent-notes, профильная заметка, TOOLS,
TEST_FIXTURES и этот отчёт. Stock/world/layout и RHS/WCS integration не менялись.

Evidence: `.codex-runtime/issue-32-recruitment-priority/`. Game/Server/Tools:
`1.8.0.13`. Команды ниже запускались из корня репозитория; helper scripts,
точные аргументы, manifests, native exits, полные логи и SHA-256 сохранены
в evidence. Только новые прогоны этой задачи:

- `tools/Invoke-AICFChecks.ps1 -Name Test-RecruitmentSupplyContracts.ps1,Test-InfantryRecruitmentStatic.ps1,Test-BarracksCombatContracts.ps1,Test-Stage35Static.ps1,Test-Stage4Static.ps1,Test-SquadCommandsContracts.ps1,Test-DefendWaypointInputContracts.ps1 -EvidenceRoot <before>`:
  **6 PASS, 1 прежний FAIL / exit 1**. Повтор с дополнительным
  `Test-RecruitmentPriorityContracts.ps1`, `<after-authority>`:
  **7 PASS, тот же 1 FAIL / exit 1**. Сохранён `STABLE_NEAREST` в
  SquadCommandsContracts; семь прежних exit codes совпали. Новый контракт:
  **15 правил и 15 отрицательных мутаций PASS**. Полный offline набор не
  запускался; несвязанные прежние failures не считаются исправленными.
- Evidence `compile.ps1 -Variant Stock|EveronNorthWCSRHS -Attempt production-authority -SourceRoot .`:
  терминальный Workbench **PASS / native exit 0**, `Script validation successful`,
  SCRIPT/fatal = 0. Соответственно 25/39 прочих resource E/F сохранены;
  это не полностью чистая validation ресурсов.
- `compile.ps1 -Variant Stock -Attempt priority-authority -SourceRoot <stage>`:
  fixture **PASS / 0**, SCRIPT/fatal = 0, 25 прочих resource E/F.
- `runtime.ps1 -Attempt priority-authority`: только `Start-AICFRuntime.ps1`,
  Stock Easy, отдельный server profile, native **exit 0**. Полный остановленный
  лог `server-priority-authority/logs/logs_2026-10-13_13-35-03/console.log`:
  **30/30 сценариев**, 6 реальных списаний и transfer (3 × 15, 3 × 20),
  одна отмена неоплаченного donor. SCRIPT/fatal = 0; 14 native resource/world/
  shutdown E/F сохранены и не выдаются за полностью чистый runtime.
- `tests/log-audits/Test-RecruitmentPriorityLog.ps1 -LogPath <этот полный лог>`
  и `Test-InfantryRecruitmentLog.ps1 -LogPath <тот же лог>` — **PASS / 0**;
  9 заявок/визитов, 6 joins. Проверены порядок покупок, exact debit,
  дефицит/остаток, FIFO, достаточный запас, динамическая роль, ручное повышение
  и отмена, защита от commander, stale identities/deadlines и native gates.
- `negative-audits.ps1 -LogPath <этот полный лог>` — **PASS / 0**:
  два положительных контроля и десять отрицательных копий полного лога.
  Missing/failed case, shutdown, SCRIPT error, finish, debit, source, join,
  paid cancellation и покупка без визита отклонены с exit 1.
- Existing `AICF_RecruitmentLifecycleProbe.c` в отдельном `lifecycle-stage`:
  `compile.ps1 -Variant Stock -Attempt lifecycle -SourceRoot <lifecycle-stage>`
  — **PASS / native 0**, SCRIPT/fatal = 0; `runtime.ps1 -Attempt lifecycle
  -Probe aicfRecruitmentLifecycleProbe -Stage lifecycle-stage` — **native 0**.
  Полный остановленный лог `server-lifecycle/logs/logs_2026-10-13_13-37-36/console.log`:
  `Test-RecruitmentAcceptanceLog.ps1 -LogPath <лог> -Mode Lifecycle` —
  **10/10 PASS / 0**, `Test-InfantryRecruitmentLog.ps1 -LogPath <лог>` —
  **PASS / 0**, 4 визита, 9 joins и полный roster replacement. Проверены
  фактическая смерть seed, новая generation/entity, освобождение старого спроса,
  новая заявка, смена владельца базы и окончание набора. Fixture не изменялась.
  SCRIPT/fatal = 0; те же 14 native E/F категорий ENTITY/RESOURCES/WORLD,
  включая stock resource GUID, world keywords/hierarchy и shutdown font leak.
- `git diff --check` и проверка локальных ссылок изменённых Markdown — **PASS / 0**.
  Свои процессы завершены, обе stage-копии удалены после сохранения source hashes.
  Полные логи, commands, manifests и `final-summary.json` сохранены в evidence.

Оба runtime helper вызывают canonical launcher с отдельными stage/profile:
`tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock -RepositoryRoot <stage>
-ProfileRoot <свежий profile> -AdditionalArguments @('-aicfRecruitmentPriorityProbe','1',
'-aicfRequirePlayerForResult','0','-addr','127.0.0.1:2302')`.
Для lifecycle имя probe заменено на `-aicfRecruitmentLifecycleProbe`.
Stages содержат Core/Arland и только соответствующую test fixture;
подготовка и терминальный compile следуют [DEVELOPMENT.md](DEVELOPMENT.md).

Подготовительные прогоны, включая SteamAPI failure в sandbox, неверный
массив CLI wrapper и исправленные условия fixture, сохранены отдельно;
они не дают PASS финальному коду. `priority-final` относится к версии до
последней защиты ручного ожидания; итоговый verdict — `priority-authority`.

**NOT RUN:** реальный player UI/RPC, client/JIP, естественная навигация в
priority fixture, длительный soak, RHS/WCS runtime, packaged build.
Fixture вызывает authoritative manual API, задаёт роли, запас/доход и позиции,
приостанавливает стратегический loop и отдельно вызывает настоящий commander.
Она доказывает очередность и транзакции, а не перечисленные внешние сценарии.
Работа самостоятельно не объявляется ACCEPTED.

## Issue #33: цены штатных меню RHS/WCS — 2026-10-13

Ветка `codex/issue-33-menu-prices` от обновлённого `origin/main` `381501c`,
исходное дерево чистое. RHS/WCS integration нормализует возвращаемый CAMPAIGN
quote; native permissions, faction/provider/traits, debit/refund и registry
сохраняются. Core, доход баз, тикеты и auto reinforcement 10/15/20 не менялись.
Политика и вся таблица: [MENU_PRICES.md](MENU_PRICES.md).
Evidence: `.codex-runtime/issue-33-menu-prices/`; исходный анализ —
`.codex-runtime/price-audit-20261013/`. Версии: 1.8.0.13, RHS 0.16.5237
(packs 0.16.5236), WCS 8.3.0. Ниже только новые прогоны этой задачи.

- `tools/Invoke-AICFChecks.ps1 -Name Test-RHSIntegrationStatic.ps1,Test-WCSIntegrationStatic.ps1,Test-Stage4Static.ps1,Test-InfantryRecruitmentStatic.ps1
  -EvidenceRoot <before>` — **4 PASS / exit 0**. После изменения тот же набор
  плюс `Test-MenuPriceStatic.ps1`, `<after-final>` — **5 PASS / exit 0**.
  Baseline failures выбранного набора отсутствуют. Полный offline набор не
  перезапускался; прежние несвязанные failures, включая sparse-slot restore,
  не исправлялись и не считаются закрытыми.
- Evidence `compile.ps1 -Variant Stock|RHS|EveronNorthWCSRHS -Attempt production
  -SourceRoot .` — терминальный Workbench **PASS / native exit 0** для всех
  трёх production graphs, `Script validation successful`, SCRIPT/fatal = 0.
  Fixture compile `-Variant EveronNorthWCSRHS -Attempt final-contracts` —
  **PASS / 0**. Production compile сохраняет 25/26/39 прочих E/F соответственно;
  это не полностью чистая validation ресурсов. JSON команд и полные логи
  находятся рядом с `compile-exit-*`.
- `runtime.ps1 -Variant <вариант> -Attempt matrix -Mode client`, затем в другой
  терминальной сессии `runtime.ps1 -Variant <тот же> -Attempt matrix -Role Client
  -ServerProfileRoot <свежий server-profile>` — **12 native exit 0**.
  Только canonical launcher, Easy, `-addr 127.0.0.1:2301`, отдельные profiles;
  подтверждены живой process, CLI Params и ROSTER_READY. Manifests и readiness
  сохранены в `launcher-*.txt`.

| Variant | Server / JIP client: строки с PASS | Native E/F server / client | SCRIPT/fatal |
|---|---:|---:|---:|
| RHS | 115 / 115 | 69 / 94 | 0 / 0 |
| ArlandWCSRHS | 189 / 189 | 104 / 127 | 0 / 0 |
| EveronRHS | 115 / 115 | 199 / 201 | 0 / 0 |
| EveronWCSRHS | 189 / 189 | 234 / 234 | 0 / 0 |
| EveronNorthRHS | 115 / 115 | 199 / 201 | 0 / 0 |
| EveronNorthWCSRHS | 189 / 189 | 234 / 234 | 0 / 0 |

`tests/log-audits/Test-MenuPriceLog.ps1 -LogPath <полный console.log>
[-ExpectedVehicles 48] [-Client -CompareLogPath <server-console.log>]` —
**12 PASS / 0**. Проверены все 67 roster, 48/122 spawner entries,
server/client prefab/index/цены, group debit budget, повторные чтения,
накопление в непустой массив и CopyFrom. North WCS и финальный RHS probe
дополнительно сравнивают non-CAMPAIGN budgets с native snapshot.
`summarize.ps1` сохраняет verdict, пути и SHA-256 в `final-summary.json`.

Native E/F не скрыты: resource/material/world/hierarchy/pathfinding ошибки,
при direct-connect — `Trying to register connection of possibly unauthorized
client` до успешного подключения. Arland baseline содержит 69/104 E/F;
в новом Arland добавлен этот network marker, отсутствует старый WORLD
`m_bShowDebugShape` marker. Everon/North и клиентские E/F — фактические
наблюдения этих прогонов, не доказанно идентичный старый baseline.

- `runtime.ps1 -Variant ArlandWCSRHS -Attempt final -Transactions -Measure`
  и `-Variant RHS -Attempt final-measure -Transactions -Measure`:
  **native exit 0**, audits `-Transactions -Measurements` — **PASS / 0**.
  122/48 actual vehicle instances, compartments и weapon entities;
  все 48 общих измерений RHS/WCS совпали. Четыре native building транзакции:
  US FireTeam **300**, M1025 M2HB **400**, RF FireGroup **275**, БТР-70 **1000**.
  В каждом случае один callback и реальный расход полной цены; отказ при
  `price-1`, разрешение при точном запасе, отмена preflight без расхода.
  North WCS matrix повторяет четыре транзакции: **4/4 PASS**.
- `build-table.ps1` сверяет все 189 старых цен с исходными CSV анализа,
  общие цены/roster с новым RHS и формирует отслеживаемую
  `agent-notes/MENU_PRICE_TABLE.csv` — **PASS / 0**.
- `negative-audit.ps1`: положительный контроль **PASS/0**, все **14**
  отрицательных копий полного лога отклонены с exit 1: failed/missing case,
  result, shutdown, SCRIPT error, цена/debit, measurement, transaction,
  лишний failed marker и неверный peer; `negative-audit/summary.json`.

Подготовительные compile и Steam/PowerShell startup failures сохранены
отдельно и не дают PASS. Runtime выявил и помог исправить native merge mutation
при повторных чтениях US group budgets и базовый UIInfo у Mi8MTT-A;
финальные аудиты не ослаблены ради результата.

**NOT RUN:** визуальное открытие/повторное открытие меню, реальная покупка через
игрока и vehicle factory RPC/PerformSpawn, интерактивная отмена, неудачный spawn
после подтверждения, фактический refund action и его процент/условия;
combat balance/penetration, soak, packaged build. Терминальная fixture
доказывает каталог и native building preflight/debit, но не заменяет эти
приёмочные сценарии. Stock runtime и runtime auto reinforcement отдельно не
перезапускались; stock compile и неизменность sources/dependencies проверены.
Работа самостоятельно не объявляется ACCEPTED.

## Оформление 0.1.27 — 2026-10-12

Отдельная ветка `codex/release-0.1.27` от обновлённого `origin/main` `1bb3b35`;
исходное дерево чистое. Добавлены `releases/0.1.27.md` и ссылки в README/
указателе. Игровой код, headers и зависимости не менялись. Состав сверен
с `git log 0.1.26..HEAD`, diff и сохранёнными результатами изменений ниже.
В патчноутах отражены итоговый Carl Gustaf вместо промежуточного M72,
исправленный `STAGE4_ATTACKED_BASES`, открытый sparse-slot restore FAIL
и границы каждого targeted PASS.

Свежие проверки: `git diff --check` до/после — **PASS/0**; проверка локальных
Markdown-ссылок до/после — **PASS/0**, включая наличие целей в Git для чистого
checkout. Команда: `pwsh -NoProfile -File
.codex-runtime/release-0.1.27/Verify-ReleaseDocs.ps1 -Phase before|after`.
Evidence: `.codex-runtime/release-0.1.27/` — helper, исходный SHA/status,
история релиза и полные `before-check.txt`/`after-check.txt`.
Baseline failures выбранных проверок документации отсутствуют; исторические
runtime/static failures сохранены в патчноутах, заново не проверялись.

Полный offline набор, Workbench, server/client runtime, JIP, ручные проверки,
soak и packaged build при оформлении — **NOT RUN**. Старые PASS не являются
новым прогоном релиза. Push, merge, Git-тег и Workshop upload не выполнялись.
По запросу пользователя тег `0.1.27` создаётся на итоговом commit `main`
после слияния патчноутов отдельным действием.

## Issue #28: начальная погода — 2026-10-12

Ветка `codex/issue-28-random-weather` от актуального `origin/main` `44cead0`,
исходное дерево чистое. Четыре базовых stock/RHS MissionHeader включают
`m_bRandomStartingWeather` и `m_bRandomWeatherChanges`; поля времени и production
`.c` не изменены. North/WCS/Medium/Hard наследуют настройку. Обновлены
ScenarioHeaders audit, README и агентские заметки; добавлены MatchWeatherProbe
и MatchWeatherLog. Evidence: `.codex-runtime/issue-28-random-weather/`.

- `tools/Invoke-AICFChecks.ps1 -Name Test-AICommanderModeStatic.ps1,Test-AICommanderUIContracts.ps1,Test-ScenarioHeadersStatic.ps1,Test-RHSIntegrationStatic.ps1,Test-WCSIntegrationStatic.ps1
  -EvidenceRoot <before|after-headers>` — **5 PASS / exit 0** до и после.
  Baseline failures выбранного набора отсутствуют. Новый ScenarioHeaders audit
  на исходном stage — ожидаемый **FAIL / 1**, восемь нарушений новых погодных
  правил; `headers-negative-baseline.txt`. Прежние guards не ослаблены.
- Evidence `compile.ps1 -Variant Stock -Label headers-production-stock`,
  `-Variant EveronNorthWCSRHS -Label headers-production-wcs` и
  `-Variant EveronNorthWCSRHS -Label headers-fixture -SourceRoot <stage>`:
  терминальный Workbench **PASS / native exit 0**, `Script validation successful`.
  Версия 1.8.0.13; SCRIPT/fatal — 0, прочих E/F — 25/39/39. Полные logs,
  source graph manifests и argv сохранены в `wb-headers-*` и соседних файлах.
- `runtime.ps1 -Variant Stock|EveronNorthWCSRHS -Tag header-baseline -Mode baseline
  -Stage stage-baseline`: исходный код — **12/12 PASS** в каждом сценарии.
  `runtime-matrix.ps1` запускает финальные `runtime.ps1 -Variant <тот же Variant>
  -Tag headers1|headers2`: четыре новых матча — **13/13 PASS** каждый.
  Погода по порядку: `Overcast`, `Rainy`, `Cloudy`, `Clear`. Stock загрузил три
  своих headers, WCS/RHS — все 27; везде random daytime выключен, старт 08:00,
  day/night acceleration 6/24. Фактическая длительность суток днём 14400 с,
  дата и auto advance сохранены; скорость часов измерена за 10 с engine time.
- `tests/log-audits/Test-MatchWeatherLog.ps1 -LogPath <четыре финальных полных лога>
  -RequireVariation` — **PASS / 0**. Отдельный `-Baseline` — **PASS / 0** для
  обоих исходных логов. Шесть отрицательных копий лога (failed/missing case,
  duplicate result, отсутствие shutdown, SCRIPT error, отсутствие вариативности)
  отклонены с exit 1; `negative-audit/summary.json`.

Canonical launcher, Easy, отдельные stages и свежие profiles, listener
`-addr 127.0.0.1:2301`, `-aicfWeatherProbe baseline|random`,
`-aicfWeatherHeaders <ресурсы>`, `-aicfRequirePlayerForResult 0`.
Точные `AICF_RUNTIME_MANIFEST_JSON`/CLI находятся в `*-launcher.txt`.
Все шесть итоговых серверов завершены через RequestClose, native exit 0;
ROSTER_READY, listener и Game destroyed подтверждены полными логами.
SCRIPT/fatal — 0; прочие resource/world/shutdown E/F — 14 stock / 233 WCS,
полностью совпадают с новым baseline после удаления timestamp. Это сохранённые
ошибки движка, а не общий error-free PASS; `error-baseline-comparison.json`.

Финальные console logs относительно evidence:

- `profile-Stock-headers1/logs/logs_2026-10-12_15-51-48/console.log`;
- `profile-Stock-headers2/logs/logs_2026-10-12_15-52-11/console.log`;
- `profile-EveronNorthWCSRHS-headers1/logs/logs_2026-10-12_15-52-36/console.log`;
- `profile-EveronNorthWCSRHS-headers2/logs/logs_2026-10-12_15-53-18/console.log`.

`collect-final.ps1`, `runtime-summary.json`, `compile-summary.json` сохраняют
команды и раздельные verdict. `final-source-hashes.json`: 282 production/resource
файла совпадают со stage после нормализации CRLF/LF; у 218 файлов raw SHA различается
только из-за `git archive` LF против рабочего CRLF. Fixture hash отдельный.
Тестовые процессы завершены; временные stages удалены после сохранения evidence.
`git diff --check` — **PASS / 0**.

Отброшенные эксперименты сохранены отдельно: поздний `ForceWeatherTo` сдвигал
часы на шаг 0.1 с engine time, обычный queue API ограничивал переход минимумом
0.167 игрового часа. Эти варианты не входят в production. Первый compile probe
получил ошибку типа World вместо ChimeraWorld; старое сравнение скорости по world
time дало FAIL на WCS из-за разных фаз update. Итоговый probe использует
синхронный `GetEngineTime`; начальные часы проверяются строго по header.
Старые FAIL не выдаются за новые PASS.

**NOT RUN:** client/JIP, визуальная проверка погоды/меню, ночной переход ускорения,
долгий погодный цикл/soak, отдельный runtime каждой из 27 плиток, packaged build,
полный offline набор и публикация. Чтение всех headers не заменяет 27 runtime.

## Carl Gustaf вместо M72 US — 2026-10-12

Ветка `codex/us-carl-gustaf` от `ac8c801`, исходное дерево чистое.
WCS `AICF_WCSInfantryKit`/`AICF_WCSInfantryEquipment` выдают USMC LAT/TL/AMG
M2 Carl Gustaf (`{DA376E83952F1DFD}Prefabs/Weapons/Launchers/M2CG/Launcher_M2CG.et`)
с одним заряженным выстрелом и без запасных. Разрешена замена только точного
штатного M72 в local draft; остальные authority/identity/rollback границы прежние.
Обновлены WCS static audit и три fixtures: InfantryAT, USExtraLauncher,
WCSInfantry; README/WCS README/GAMEPLAY/TEST_FIXTURES описывают новый default.
Evidence: `.codex-runtime/us-carl-gustaf/`.

- `tools/Invoke-AICFChecks.ps1 -Name Test-WCSIntegrationStatic.ps1,Test-RHSIntegrationStatic.ps1,Test-AILoadoutStatic.ps1,Test-InfantryRecruitmentStatic.ps1
  -EvidenceRoot <before|after>`: **4 PASS / exit 0** до и после. Baseline failures
  выбранного набора отсутствуют. Старый запрет любой замены второго оружия
  уточнён до замены только M72 согласно новому контракту; неизвестное оружие
  по-прежнему отклоняется.
- Evidence `compile.ps1 -Label final-fixture -SourceRoot <stage>` и
  `compile.ps1 -Label final-production-wcs`: Workbench Validate/Compile
  **PASS / native exit 0**, версия 1.8.0.13. SCRIPT/fatal — 0,
  прочие resource E/F — 39, как в предыдущем наблюдении.
- `runtime.ps1 -Tag draft`: **32/32 PASS**, 20 стандартных draft обеих сторон,
  точные M2CG/заряд/отсутствие запасных у US 3/6/7/8/9, прежний пустой второй
  слот РФ 6/7, полная signature Capture/Restore всех пяти комплектов с M2CG.
- `runtime.ps1 -Tag live-hard -Probe aicfATProbe -Difficulty Hard`:
  **104/104 PASS**, 26 живых бойцов полного roster и donors. У всех US LAT/TL/AMG
  точный M2CG с `loaded=1 spare=0`, у РФ прежний РПГ-7 с `loaded=1 spare=2`;
  роли, комплекты, повторный Apply, mappings и FIA reject прошли.
- `runtime.ps1 -Tag retention -Probe aicfWCSInfantryProbe -Difficulty Hard`:
  WCS roster **20/20 PASS**, сохранность остальных защищённых предметов,
  совпадение editor/live kit и catalog; PMC **13/13**, player preparation **2/2**,
  пять cache guards — **PASS**. Это серверная проверка preparation без клиента.
- `runtime.ps1 -Tag live-easy -Probe aicfATProbe -Difficulty Easy`:
  **96/96 PASS**, три Carl Gustaf в полном US roster, donors и прежние роли.

Runtime использует canonical `tools/Start-AICFRuntime.ps1`, отдельный stage,
fresh profiles, `EveronNorthWCSRHS`, `-addr 127.0.0.1:2301 -<Probe> 1
-aicfRequirePlayerForResult 0`. Аргументы/manifest, полный console/script log
и native exit сохранены. `collect.ps1` читает полные логи и сохраняет все E/F
вместе с `runtime-summary.json`/`compile-summary.json`.
`final-stage-hashes.json`: 282 production/resource файла, 0 расхождений;
три fixture hashes сохранены отдельно.
Полные console logs относительно evidence:

- `profile-EveronNorthWCSRHS-draft/logs/logs_2026-10-12_15-04-42/console.log`;
- `profile-EveronNorthWCSRHS-live-hard/logs/logs_2026-10-12_15-06-07/console.log`;
- `profile-EveronNorthWCSRHS-retention/logs/logs_2026-10-12_15-07-34/console.log`;
- `profile-EveronNorthWCSRHS-live-easy/logs/logs_2026-10-12_15-08-36/console.log`.

Все четыре тестовых сервера завершились с native exit 0; listener,
ROSTER_READY и Game destroyed подтверждены полными остановленными логами.
SCRIPT/fatal — 0, прочие resource/shutdown E/F — 233 в каждом запуске,
как в предыдущем наблюдении; общий error-free gate не заявляется.
Тестовый stage удалён после завершения процессов, полные логи сохранены.
`git diff --check` — **PASS**.

**NOT RUN:** стрельба/перезарядка в бою, client/UI/JIP, пользовательский Save/Apply,
переход donor в отряд и оплата, отдельный Medium runtime, packaged build и soak.
Прежний sparse-slot restore FAIL не затронут: новые проверки не создают дырки
в native-карманах. Сохранённые пользовательские шаблоны не переписываются
и могут переопределить новый default. Игровая сессия на порту 2001 остаётся
на предыдущем коде до перезапуска.

## Два дополнительных M72 у US — 2026-10-12

Ветка `codex/us-at-ammunition` от `1cae572`, исходное дерево чистое.
Изменены WCS `AICF_WCSInfantryKit`/`AICF_WCSInfantryEquipment`, WCS static audit,
fixtures `AICF_InfantryATProbe`/`AICF_WCSInfantryProbe`; добавлен
`AICF_USExtraLauncherProbe`. README, WCS README, GAMEPLAY и TEST_FIXTURES
обновлены вместе с кодом. Контракт: USMC TL/AMG получают по M72 с одним зарядом
во втором native primary slot, без изменения ролей и основного оружия.
Evidence: `.codex-runtime/us-at-ammunition/`.

- `tools/Invoke-AICFChecks.ps1 -Name Test-WCSIntegrationStatic.ps1,Test-RHSIntegrationStatic.ps1,Test-AILoadoutStatic.ps1,Test-InfantryRecruitmentStatic.ps1
  -EvidenceRoot <before|after-final>`: **4 PASS / exit 0** до и после;
  baseline failures выбранного набора отсутствуют. Первая попытка `after`
  остановилась до audits на Git dubious ownership sandbox-пользователя;
  повторена в штатном контексте пользователя, без изменения правил проверок.
- Evidence `compile.ps1 -Label baseline-fixture -SourceRoot <stage>`:
  исходный код с диагностикой — **PASS / 0**. `runtime.ps1 -Tag baseline`:
  20 draft построены, у US/USSR 6/7 второй слот пуст; `failures=0`, native exit 0.
- `compile.ps1 -Label final-fixture -SourceRoot <stage>` и
  `compile.ps1 -Label final-production-wcs`: финальный stage и production
  Workbench Validate/Compile **PASS / native exit 0**. SCRIPT/fatal — 0,
  resource errors — 39, как в baseline. Движок/Workbench 1.8.0.13.
- `runtime.ps1 -Tag draft-final`: **26/26 PASS**, включая точный M72/заряд,
  неизменный второй слот РФ 6/7 и полную signature Capture/Restore у US 6/7.
- `runtime.ps1 -Tag live-hard -Probe aicfATProbe -Difficulty Hard`:
  **104/104 PASS**; `-Tag live-easy -Probe aicfATProbe -Difficulty Easy`:
  **96/96 PASS**. По 26 живых бойцов: полные US/USSR roster и donors,
  включая US 6/7. Prefab, роли, WCS kit, заряженное ПТ-оружие, повторный Apply,
  mappings stock/RHS и FIA reject прошли. US Hard: M72 у индексов 3/6/7/8/9;
  Easy: 3/6/7. РФ Hard сохранила РПГ-7 с `loaded=1 spare=2` у 3/8/9.
- `runtime.ps1 -Tag retention -Probe aicfWCSInfantryProbe -Difficulty Hard`:
  **20/20 PASS** для WCS roster (прочие защищённые предметы, одинаковые комплекты
  редактора/бойца, catalog, повторная выдача); PMC **13/13**, player preparation
  **2/2**, пять cache guards — **PASS**. Это серверная проверка preparation,
  а не подключение клиента.

Все runtime команды используют canonical `tools/Start-AICFRuntime.ps1`,
Variant `EveronNorthWCSRHS`, отдельный stage/fresh profile,
`-addr 127.0.0.1:2301 -<Probe> 1 -aicfRequirePlayerForResult 0`.
Manifests и аргументы сохранены в launcher outputs. Полные остановленные логи,
все E/F и анализ находятся в evidence; `collect.ps1` формирует
`runtime-summary.json`/`compile-summary.json`. В baseline/draft/Hard/Easy/retention
native exit 0, listener/ROSTER_READY/Game destroyed подтверждены;
SCRIPT/fatal — 0, прочие resource/shutdown E/F — 233, как до изменения.
Общий error-free gate не заявляется. `final-stage-hashes.json`: 282
production/resource файла, 0 расхождений; три fixture hashes сохранены отдельно.
Финальные console logs относительно evidence:

- `profile-EveronNorthWCSRHS-draft-final/logs/logs_2026-10-12_14-49-40/console.log`;
- `profile-EveronNorthWCSRHS-live-hard/logs/logs_2026-10-12_14-50-51/console.log`;
- `profile-EveronNorthWCSRHS-live-easy/` — точный путь в `runtime-summary.json`;
- `profile-EveronNorthWCSRHS-retention/logs/logs_2026-10-12_14-52-43/console.log`.

Тестовые серверы завершились сами, собственный stage удалён.
`git diff --check` — **PASS**. Полные evidence logs сохранены.

**NOT RUN:** реальная стрельба по бронетехнике, client/UI/JIP, пользовательские
шаблоны через Save/Apply, переход donor в отряд и оплата, отдельный Medium runtime,
packaged build и soak. Известный sparse-slot restore FAIL из проверки общего
содержимого разгрузки не затронут; новый Capture/Restore не удаляет предметы
из середины native-кармана и не доказывает исправление этого ограничения.
Текущая пользовательская игровая сессия остаётся на прежнем коде до перезапуска.

## Категории основного и второго оружия — 2026-10-12

Ветка `codex/loadout-weapon-categories` от `378f257`, исходное дерево чистое;
сохранены ранее сделанные общий список и оптимизация каталога. Изменены
`AICF_LoadoutEditor`, добавлен `AICF_LoadoutWeaponCategories`; уточнения типов
в Core/RHS/WCS content profiles, RU/EN строки и таблицы, static audit,
`AICF_LoadoutWeaponCategoriesProbe`, README и агентские заметки.
Правила и ручной gate: [LOADOUT_WEAPON_CATEGORIES.md](LOADOUT_WEAPON_CATEGORIES.md).
Evidence: `.codex-runtime/weapon-categories/`.

- `tools/Invoke-AICFChecks.ps1 -Name Test-AILoadoutStatic.ps1,Test-PersonalLoadoutContracts.ps1
  -EvidenceRoot <before|after-ordered>`: **2 PASS / exit 0** до и после.
  Финальные audits: 164/54 отрицательных мутации. Baseline failures выбранного
  набора отсутствуют. Scope dropdown расширен с карманов на native primary slots;
  прежние guards карманов и совместимости сохранены.
- `pwsh -NoProfile -File tests/static/Test-LocalizationStatic.ps1`: **PASS / 0**,
  до 591, после 602 записей; встроенный `Build-AICFLocalization.ps1 -Check` — PASS.
  Известный PowerShell 5 parse FAIL LocalizationStatic из предыдущей задачи
  не исправлялся; в этой задаче проверка выполнялась напрямую через pwsh.
- Evidence `compile.ps1 -Variant Stock -Label final-production-stock`,
  `-Variant EveronNorthWCSRHS -Label final-production-wcs`,
  `-Variant EveronNorthWCSRHS -Label final-ordered -SourceRoot <stage>`:
  production и fixture Workbench Validate/Compile **PASS / native exit 0**.
  SCRIPT E/F, ENGINE F, VM/null exceptions — 0; resource E в console.log:
  stock 25, WCS/fixture 39. Полные logs, argv и manifests сохранены.
- Evidence `runtime.ps1 -Variant EveronNorthWCSRHS -Tag ordered` и
  `runtime.ps1 -Variant Stock -Tag ordered`: canonical launcher Server/Easy,
  отдельный stage/fresh profile, `-addr 127.0.0.1:2301 -aicfWeaponCategoriesProbe 1
  -aicfRequirePlayerForResult 0`. **WCS 36/36 PASS**, **stock 16/16 PASS**;
  проверено 1480 (683 US / 797 USSR) и 46 primary catalog записей соответственно.
  Два primary slots, ALL, полное разбиение с сохранением порядка, контрольные
  prefab и source intersection прошли. Native exit 0, ROSTER_READY, listener
  и Game destroyed подтверждены полными остановленными логами.

Финальные runtime logs относительно evidence:

- `profile-EveronNorthWCSRHS-ordered/logs/logs_2026-10-12_14-29-17/console.log`;
- `profile-Stock-ordered/logs/logs_2026-10-12_14-30-07/console.log`.

`collect.ps1` читает полные логи и сохраняет все E/F, `runtime-summary.json`
и `compile-summary.json`. SCRIPT/fatal gate — **PASS**. Прочие resource/shutdown
ошибки соответствуют наблюдавшемуся baseline: stock 14, WCS 233; общий error-free
gate не заявляется. `final-stage-hashes.json`: 284 production/resource файла,
0 расхождений. Fixture hash сохранён отдельно. Движок/Workbench 1.8.0.13.
Тестовые процессы завершились, собственный stage удалён; игровая сессия на 2001
продолжает работать с предыдущим кодом. `git diff --check` — PASS.

Промежуточные отказы сохранены: первая diagnostic fixture не задавала faction
рецепта и получила DEFAULT_LOADOUT_FAILED (исправлена); первый audit новой
scope-проверки ошибочно искал строковый literal после его удаления parser helper
(исправлена проверка). Runtime `final` выявил 8 FAIL: новый Filter переставлял
элементы через Remove, а source assertion сравнивал порядок после старой native
compatibility. Filter заменён линейным сохранением порядка; source проверяет
состав, порядок Filter проверяется отдельно. Финальные проверки повторены.

**NOT RUN:** реальный client dropdown/input, личный/отрядный Apply/Undo/Save,
Close/reopen, визуальные RU/EN подписи, JIP/respawn, packaged build, soak;
runtime дробовиков и отдельных гранатомётов — таких записей в этих каталогах нет.
Ранее выявленный sparse-slot restore FAIL не затронут этой задачей.

## Общее содержимое разгрузки — 2026-10-12

Ветка `codex/loadout-contents-view` от `e1d5dab` (оптимизация issue #27),
исходное дерево чистое. Изменены `AICF_LoadoutNavigation`, `AICF_LoadoutEditor`,
локализация RU/EN, static audit; добавлен `AICF_LoadoutContentsProbe` и обновлены
README/агентские заметки. Контракт и ручной gate:
[LOADOUT_CONTENTS.md](LOADOUT_CONTENTS.md). Evidence: `.codex-runtime/loadout-contents/`.

- `tools/Invoke-AICFChecks.ps1 -Name Test-AILoadoutStatic.ps1,Test-PersonalLoadoutContracts.ps1,Test-LocalizationStatic.ps1
  -EvidenceRoot <before|after-final-clean>`: **2 PASS / 1 FAIL, exit 1** до и после.
  Сохранён baseline FAIL: runner запускает Windows PowerShell 5, который неверно
  читает UTF-8 regex существующего LocalizationStatic и получает parse error.
  Финальные loadout/personal audits — **PASS**, 159/54 отрицательных мутации.
- `pwsh -NoProfile -File tests/static/Test-LocalizationStatic.ps1` и
  `pwsh -NoProfile -File tools/Build-AICFLocalization.ps1 -Check`: **PASS**, 591 запись.
- Evidence `compile.ps1 -Variant Stock -Label final-production-stock` и
  `-Variant EveronNorthWCSRHS -Label final-production-wcs`: окончательный production
  Workbench Validate/Compile **PASS / exit 0**. `-Label final-fixture -SourceRoot <stage>`:
  fixture compile **PASS / 0**. Версия движка/Workbench 1.8.0.13.
  SCRIPT E/F, ENGINE F и VM exceptions — 0; resource errors в production console:
  stock 25, WCS 39. Полные logs, argv и manifest сохранены в `wb-<label>/` и рядом.
- Evidence `runtime.ps1 -Variant Stock -Tag final` и
  `runtime.ps1 -Variant EveronNorthWCSRHS -Tag final`: canonical
  `tools/Start-AICFRuntime.ps1 -Role Server -Difficulty Easy -RepositoryRoot <stage>`
  с fresh profile и `-addr 127.0.0.1:2301 -aicfContentsProbe 1 -aicfRequirePlayerForResult 0`.
  Полные остановленные логи подтверждают listener, ROSTER_READY, probe completion
  и Game destroyed; native exit **0**. Manifest сохранён в launcher output.
  Stock **24/27 PASS**, WCS **62/65 PASS**; общий runtime verdict — **FAIL**.
  Новое представление, точные адреса, отсутствие дубликатов, сохранность остальных
  предметов после удаления и Undo проходят. В обеих конфигурациях US:
  `CONTENTS_REMOVED_RESTORE`, `CONTENTS_REMOVED_SIGNATURE`,
  `CONTENTS_REMOVED_VALIDATE` — **FAIL / INVENTORY_RESTORE_FAILED**.
  Это выявленное ограничение прежнего restore: WCS stage-диагностика подтверждает
  сохранённый slot 1 при создании slot 0 в пустом native-кармане. Тот же адрес
  доступен через прежний Rows; production Inventory/Restore не изменены.
  Сохранение/чтение файла рецепта проходит, но не заменяет runtime restore.

Полные финальные логи относительно evidence:

- `profile-Stock-final/logs/logs_2026-10-12_14-09-15/console.log`;
- `profile-EveronNorthWCSRHS-final/logs/logs_2026-10-12_14-06-15/console.log`.

`collect.ps1` читает полные logs и сохраняет `runtime-summary.json` и все E/F.
SCRIPT/fatal gate — **PASS**; прочие resource/shutdown ошибки: stock 14, WCS 233,
соответствуют ранее наблюдавшемуся baseline. Общий error-free gate не заявляется.
Промежуточные attempts сохранены: первый fixture compile не прошёл из-за
зарезервированного имени `owned` (исправлено); runtime `first` — 3 FAIL;
`restore-fixed` и `diagnostic` — 4 FAIL, включая неверное fixture-ожидание
вложенных предметов непосредственно в wearable TV-102. Оно заменено проверкой
фактически надетого wearable; точный Pouch Preset [7] со снимка не проверен.
Эксперимент с порядком удаления/вставки не исправил restore и полностью отменён.

`final-stage-hashes.json` подтверждает 283 совпадающих production/resource файла;
fixture hashes сохранены отдельно. Stage после проверок удалён, тестовые серверы
завершились сами через RequestClose. Ранее запущенную игровую сессию эти probes
не перезапускали. `git diff --check` — **PASS**.

**NOT RUN:** ручное отображение и input в клиенте, точный Pouch Preset [7] EMR,
длинные RU/EN подписи, client RPC save/respawn/JIP, packaged build, полная матрица
и soak. Save/reopen после удаления ограничен описанным restore FAIL.

## Issue #27: каталог редактора пресетов — 2026-10-12

Ветка `codex/issue-27-preset-performance` от обновлённой `origin/main`,
`fb6f3e12fcac614e0d259aaeb6246588bcb31a0a`; исходное дерево чистое.
Изменены `AICF_LoadoutCatalog`, `AICF_LoadoutEditor`, `AICF_LoadoutItemGrid`,
AILoadout static audit, добавлен `AICF_LoadoutPerformanceProbe`; обновлены
README и агентские заметки. Контракты и ручной gate:
[LOADOUT_PERFORMANCE.md](LOADOUT_PERFORMANCE.md).
Evidence: `.codex-runtime/issue27/`.

- `tools/Invoke-AICFChecks.ps1 -Name Test-AILoadoutStatic.ps1,Test-PersonalLoadoutContracts.ps1
  -EvidenceRoot <before|after-final>`: **2 PASS / exit 0** до и после.
  Baseline failures в выбранном наборе отсутствуют. Финальные audits:
  150 и 54 отрицательных мутации. Устаревшее требование detach на каждый
  SetItems заменено отменой предыдущего Render; guards lifecycle detach сохранены.
- Evidence `compile.ps1 -Variant Stock -Label final-stock` и
  `-Variant EveronNorthWCSRHS -Label final-production-wcs`: production
  Workbench Validate/Compile **PASS / native exit 0**, validation successful.
  `-Label final-fixture-wcs -SourceRoot <stage>`: отдельный fixture compile
  **PASS / 0**. SCRIPT E/F, ENGINE F и VM exceptions отсутствуют.
  Полные console/script/error logs и argv сохранены в `wb-<label>/` и рядом.
  Resource errors сохранены: 25 в stock console.log и 39 в WCS.
- Evidence `runtime.ps1 -Variant EveronNorthWCSRHS -Probe performance -Tag measured`
  и `-Variant Stock -Probe performance -Tag final`: canonical
  `tools/Start-AICFRuntime.ps1 -Role Server -Difficulty Easy -RepositoryRoot <stage>`
  с fresh profile, `-aicfLoadoutPerformanceProbe 1 -aicfRequirePlayerForResult 0`.
  WCS **80/80**, stock **48/48**, native exit **0**, ROSTER_READY и штатный
  RequestClose / Game destroyed. Проверены все записи обеих фракций и точный
  порядок выдачи по source/category/mode. SCRIPT/fatal gate **PASS**.
- `runtime.ps1 -Variant EveronNorthWCSRHS -Probe personal -Tag final` использует
  `-aicfPersonalLoadoutProbe 1`: **37/37 PASS / native exit 0**. Проверены
  validation, неизвестный предмет, blacklist, owner/context/revision isolation,
  save/readback и восстановление после прерванной записи. SCRIPT/fatal gate **PASS**.
  Это существующий server probe, не клиентский RPC/input pipeline.

Медианы шести замеров, каждый — пять проходов List(0)+Name; legacy Find/List
из `fb6f3e1` и новый код исполнялись попеременно в одном процессе на прогретых
данных. Checksums совпали во всех samples. Это **не время кадра/клика**:

| Каталог | Записей | До, мс | После, мс | Отношение |
|---|---:|---:|---:|---:|
| WCS RHS_USAF | 3704 | 6447 | 142 | 45,4× |
| WCS RHS_AFRF | 3897 | 7381,5 | 154,5 | 47,8× |
| Stock US | 187 | 15 | 4,5 | 3,3× |
| Stock USSR | 166 | 13 | 4 | 3,25× |

Полные остановленные логи относительно evidence:

- `profile-EveronNorthWCSRHS-performance-measured/logs/logs_2026-10-12_13-34-27/console.log`;
- `profile-EveronNorthWCSRHS-personal-final/logs/logs_2026-10-12_13-36-55/console.log`;
- `profile-Stock-performance-final/logs/logs_2026-10-12_13-37-56/console.log`.

В каждом profile сохранены также script/error logs; launcher output содержит
`AICF_RUNTIME_MANIFEST_JSON`, точный CLI и native exit. `collect.ps1` читает
полные логи, сохраняет ошибки и `runtime-summary.json`. В WCS по **233** прочих
строки (E), в stock **14**, включая resource/shutdown font leak; общий error-free
gate не заявляется. Это соответствует ранее наблюдавшемуся baseline, не исправлено
этой задачей. Неполных или неуспешных runtime attempts в этой задаче нет.

WCS benchmark выполнялся до последней UI-only правки flush перед RPC;
код каталога в нём окончательный. Personal и stock probes — с окончательным
stage. `stage-hashes.json` и `final-stage-hashes.json` фиксируют совпадение
283 production/resource files с соответствующим stage, fixtures сохранены
в tests и имеют отдельные hashes. Версия движка/Workbench — 1.8.0.13.
`git diff --check` — PASS. Stage после проверок удалён; свои процессы завершены.

**NOT RUN:** ручная плавность и latency всего интерфейса до/после, отображение
native thumbnails, быстрый ввод в реальном клиенте, client RPC save/respawn/JIP,
полная offline матрица, packaged build и soak. Полная пересборка draft/3D-персонажа
остаётся потенциально затратной; ускорение каталога не гарантирует устранения
всех задержек. Критерий ручной проверки issue #27 остаётся открытым.

## PR #26: полный graph gate усиленной точки — 2026-10-12

Исправление review P1 в той же ветке от `f177309`, исходное дерево чистое.
Evidence: `.codex-runtime/issue25-graph-review/`. `Eligible` проверял только
компоненту кандидата, поэтому оба ранних выхода radio setup могли принять
изолированную objective или HQ без исходящего ребра. Новый общий `IsGraphReady`
проверяет весь граф и оба HQ exits во всех трёх успешных путях setup. Hard
planner повторяет gate: отказ setup больше нельзя обойти через сохранённого
кандидата после rollback.

- `tools/Invoke-AICFChecks.ps1 -Name` с FIARandomGarrisonContracts,
  FIAGarrisonContracts, EveronNorthStatic, WCSIntegrationStatic:
  `before/` — **4 PASS**, `after-final/` с новым FIAStrongpointGraphContracts —
  **5 PASS**. Новый audit содержит шесть отрицательных мутаций; на исходниках
  `f177309` даёт ожидаемый **FAIL** (`baseline-regression.txt`, exit 1).
  Промежуточный `after/` сохранён: проверка диагностической строки ошибочно
  искала её в code-only тексте аудитора; исправлена проверка строки, не production.
- Evidence `compile.ps1 -Variant Stock|RHS|EveronNorthWCSRHS`, labels
  `production-stock`, `production-rhs`, `production-wcs`: Workbench
  Validate/Compile **PASS**, native exit 0. Отдельный stage с обновлённым
  `AICF_FIARandomGarrisonProbe.c` — `fixture`, **PASS**.
- `runtime.ps1 -Variant <variant> -Mode plan -Tag graph-fixed` вызывает только
  `tools/Start-AICFRuntime.ps1 -Role Server -Difficulty Hard` с fresh profile и
  `-aicfRandomGarrisonProbe plan`. `Test-FIARandomGarrisonLog.ps1 -PlanOnly`
  на полных остановленных логах: **PASS**, native exit 0 во всех трёх запусках.

| Variant | Checks / failures | SCRIPT/fatal gate | Сохранённые native/resource `(E/F)` |
|---|---|---|---|
| Stock | 48 / 0 | PASS | 14 |
| Everon | 171 / 0 | PASS | 47 |
| EveronNorthWCSRHS | 43 / 0 | PASS | 233 |

В каждом запуске проверены шесть synthetic graph cases: пригодный кандидат с
изолированной целью; исправный связный граф; HQ только с входящими рёбрами;
HQ с выходом только к другому HQ; пустой список кандидатов; null graph.
`LIVE_GRAPH_READY` отдельно подтверждает готовность реального Hard graph.
`runtime-summary.json` содержит пути полных логов, `final-stage-hashes.json` —
совпадение 245 production/fixture/resource files со stage. Manifest/argv/exit
сохранены; временный stage удалён, тестовые процессы завершены.

Общий error-free runtime gate не заявляется: native/resource baseline выше
сохранён. Full spawn/boarding, бой, client/JIP и остальные шесть вариантов
сценария в этом исправлении — **NOT RUN**; прежние результаты не повторно
обозначены как свежие. Изменены policy, radio setup, planner, graph audit,
существующий runtime fixture и соответствующие агентские заметки.

## Тактика РПГ — 2026-10-12

Продолжение той же ветки от `71ab985`, исходное дерево чистое. Реализация и
границы: [ANTI_TANK_TACTICS.md](ANTI_TANK_TACTICS.md). Evidence:
`.codex-runtime/at-engagement/`; результаты ниже относятся к этому изменению.

Команды и verdict:

- `tools/Invoke-AICFChecks.ps1 -Name` с `Test-AICombatInputContracts.ps1`,
  `Test-RHSIntegrationStatic.ps1`, `Test-Stage4Static.ps1`,
  `Test-WCSIntegrationStatic.ps1`: baseline `before/` — **4 PASS**.
  `after-final/` повторяет их и добавляет `Test-AntiTankContracts.ps1` —
  **5 PASS**. После добавления проверки возврата к waypoint новый audit
  повторён в `after-waypoint-contract/`: **PASS**, 13 отрицательных мутаций.
- Evidence helper `compile.ps1 -Variant Stock|RHS|EveronNorthWCSRHS`, labels
  `final-stock-waypoint`, `final-rhs`, `final-wcs`: production Workbench
  Validate/Compile **PASS**, native exit 0, `Script validation successful`.
  Окончательный stage с fixture отдельно прошёл `fixture10`, exit 0.
- `tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthWCSRHS
  -Difficulty Hard -RepositoryRoot <stage> -ProfileRoot <fresh-profile>
  -AdditionalArguments @('-aicfAntiTankProbe','1','-aicfRequirePlayerForResult','0')`:
  `profile-probe9` и `profile-probe10`, native exit **0**, `ROSTER_READY`,
  штатный `Game destroyed`.
  Сохранены полный launcher output с `AICF_RUNTIME_MANIFEST_JSON`, полные
  console/script/error logs и `final-stage-hashes.json`.
- `tests/log-audits/Test-AntiTankLog.ps1 -LogPath <probe10-console>`:
  целевой engagement и SCRIPT/fatal gate **PASS**. AI самостоятельно распознал
  `UnitType_VehicleMedium` как `ENEMY`, выбрал РПГ и выпустил реальный
  `RHS_AmmoRocket_PG7VL` с **347,651 м**; предыдущий probe9 подтвердил выстрел
  с **348,603 м**. Accessor вернул 400 м при native
  значении 10000 м; native minimum в selector — 14 м. Ручного назначения target,
  подмены perception/skill или команды на выстрел в fixture нет.
  В probe10 отдельный aim task на тех же observer/target подтвердил допуск
  РПГ `0,25 -> 0,125`, винтовки `0,5 -> 0,5` при angular size 1°.
  Production move task на отдельном move state подтвердил ожидание на 9 с,
  готовность перемещения на 10 с и request длительностью 2 с с `CROUCH` и
  прицеливанием после остановки. Это проверка построения запроса, не маршрута.
- `check-log-audit.ps1` в evidence: synthetic positive PASS; отсутствие projectile,
  незавершённый shutdown, близкий выстрел, отрицательный move marker и SCRIPT
  error дают ожидаемый FAIL.

Общий runtime gate **не полностью зелёный**: probe9/10 содержат по 233 строки `(E/F)`,
как завершённые диагностические probe3/6/7/8. Это resource/addon/navmesh errors и
shutdown leak `ui/fonts/robotomono_msdf_28.edds`; полные сообщения сохранены, а не
исключены из логов. `PMC_EQUIPMENT_FAILED`, `NULL weapMgr`, SCRIPT/VM/fatal и
`BridgeRoad` в успешных probe9/10 не обнаружены. Старые общие FAIL не объявляются
исправленными этим изменением.

Неуспешные попытки сохранены: probe1 не стартовал replication на занятом порту;
probe2/4/5 остановлены при отладке стенда (не PASS); probe3/7/8 завершились без
выстрела из-за видимости/нераспознанной цели, probe6 не нашёл линию для размещения.
В probe8 перенесённый БТР скатился со склона; окончательный fixture фиксирует
мишень в `SimulationState.COLLISION`, оставляя collision и штатное восприятие.
`registration-negative` — намеренная ошибка типа в stage, ожидаемый compile FAIL;
ошибочная строка удалена до последующих запусков. Это диагностика регистрации
fixture, а не production compile failure.

**NOT RUN:** сравнение hit rate до/после, подтверждение попадания, движущаяся
бронецель, отдельный runtime-тест обхода препятствия/выхода без укрытия,
стрельба stock/RHS и multiplayer client/JIP. Статические guards и compile не
заменяют эти проверки. Клиентский прежний disconnect `REPLICATION/FLOODED` с
последующей diagnostic assertion этим серверным probe не проверялся;
evidence остаётся в `.codex-runtime/north-wcs-play-20261012-01/disconnect-114020/`.
Прежний игровой сервер остановлен с явного разрешения пользователя; полные
логи — `north-wcs-play-20261012-01/stopped-server/` внутри `.codex-runtime/`.

## Issue #25: случайные гарнизоны — 2026-10-12

### Дополнительное исправление ошибок проверок и runtime

По запросу пользователя работа продолжена в той же ветке от `f5d2dd1`,
исходное дерево чистое. Evidence отдельного продолжения:
`.codex-runtime/issue25-fixes/`. Исторические FAIL ниже не переписаны.

Причины и изменения:

- `STAGE4_ATTACKED_BASES`: после `ceabc6a` маркеры объединяют display names
  через запятую, а regex всё ещё требовал `+` и slot keys. Обновлён только
  контракт теста; негативные мутации удаляют накопление/сохранение списка
  и обязаны отвергаться. Production UI не изменён.
- PMC: гарнизон вызывает `BoardGarrison`, минуя patrol `Board`, где уже был
  pending-барьер. RHS adapter теперь проверяет всех членов перед обоими входами.
  Diagnostic baseline повторил семь `PMC_EQUIPMENT_FAILED`; ожидаемый/фактический
  inventory совпадал до основного оружия, которое native restore не заменил.
- `NULL weapMgr`: новый Core `AICF_AIMagazineSwitchGuard` отклоняет task до
  вызова super, если у текущего compartment отсутствует weapon manager.
  Сигнатуры/путь отказа сверены с закреплённым Script Diff 1.8.0.13.
- `BridgeRoad`: ранний `RequestClose` внутри `MatchController.Start` в режиме
  plan воспроизвёл leak в одном из трёх запусков Everon. Три диагностических
  запуска с отключённым AICF bootstrap ошибок не имели. Plan probe теперь
  закрывается на первом Update после `ROSTER_READY`, до spawn гарнизонов.
  Три повторных запуска завершились без leak; проверки 167/167/166, failures=0.
  Это исправление преждевременного shutdown теста, не изменение native bridge
  lifetime и не гарантия отсутствия любых engine leaks во всех условиях.

Команды и результаты:

- `tools/Invoke-AICFChecks.ps1 -Name` с Stage4, RHSIntegration, WCSIntegration,
  AICombatInputContracts, FIAGarrisonContracts, FIARandomGarrisonContracts:
  `before/` — **5 PASS / 1 FAIL**, `after-final/` — **6 PASS / 0 FAIL**.
  Единственный baseline FAIL `STAGE4_ATTACKED_BASES` устранён.
- `compile.ps1 -Variant Stock|RHS|EveronWCSRHS`: production **PASS**, labels
  `final-stock`, `final-rhs`, `fixes-production`; native exit 0 и успешная
  validation. Stage с regression probes также компилируется отдельно.
- `runtime.ps1` вызывает только `tools/Start-AICFRuntime.ps1 -Role Server`
  с fresh profile, `-Difficulty Hard`, `-RepositoryRoot <stage>` и
  `-aicfRandomGarrisonProbe full|plan`. Сохранены argv/manifests/exit codes и
  полные остановленные логи; `stage-source-hashes.json` сверяет 207 production
  scripts с checkout. `bridge.ps1` использует тот же launcher и отдельный stage.
- `Test-FIARandomGarrisonLog.ps1 -LogPath ... [-PlanOnly]` требует
  `ROSTER_READY` также для plan. Synthetic positive — PASS; отрицательная копия
  без readiness — ожидаемый FAIL. SCRIPT/fatal errors не подавляются.

Финальные full-прогоны, все native exit 0, целевой probe и полный SCRIPT/fatal
gate — **PASS**; PMC/VM/BridgeRoad errors отсутствуют:

| Variant / tag | Checks / failures | Машины READY | Бойцы с проверенным PMC armament | Pending checks / failures |
|---|---|---|---|---|
| EveronWCSRHS / fixed2 | 244 / 0 | 75 / 75 | 289 / 289 | 250 / 0 |
| EveronNorthWCSRHS / fixed | 54 / 0 | 12 / 12 | 60 / 60 | 50 / 0 |
| RHS / fixed | 58 / 0 | 13 / 13 | 85 / 85 | 66 / 0 |
| EveronNorthWCSRHS / magazine-bound | 54 / 0 | 12 / 12 | 51 / 51 | 44 / 0 |

Во всех четырёх прогонах отдельная fault injection подтверждает отказ
`BoardGarrison` при pending и сохранение identity/занятых мест. Последний
`magazine-bound` дополнительно привязывает все штатные компоненты AI в task:
на 27 фактических местах без weapon manager получен FAIL без VM exception,
а не ранний отказ из-за незаданного character controller/inventory.
Его stage с окончательными fixtures прошёл compile `magazine-bound-fixture`;
`final-stage-hashes.json` фиксирует 207 production scripts и три fixtures.
Предыдущий full WCS `fixed1` также имел 75/75 READY, 338 проверенных бойцов
и чистый SCRIPT gate, но ещё без fault injection и полного binding в magazine
fixture; он не заменяет эти проверки.

Три финальных plan-прогона Everon — полный gate **PASS**, `ROSTER_READY`
предшествует завершению; число READY машин 0, как и требуется plan-режиму.
`runtime-summary.json` сохраняет также исходные FAIL: семь PMC в diagnostic
baseline и преждевременные plan-логи. `regression-summary.json` содержит счётчики
дополнительных probes. Полные Resource/WORLD/native `(E)` сохранены: SCRIPT/fatal
PASS не означает error-free verdict для всех подсистем движка.

Промежуточная компиляция `fixes-fixture-injection` выявила неверный порядок
modded-классов теста; fixture переименован в `AICF_RHSPMCGarrisonProbe.c`, после
production equipment source, и повторно прошёл compile/runtime.

Затронутые файлы: `AICF_RHSPMCEquipment.c`, новый
`Integration/AICF_AIMagazineSwitchGuard.c`, Stage4/RHS static audits,
`Test-FIARandomGarrisonLog.ps1`, `AICF_FIARandomGarrisonProbe.c`; новые fixtures
`AICF_MagazineGuardProbe.c`, `AICF_RHSPMCGarrisonProbe.c`,
`AICF_BridgeLifecycleProbe.c`; заметки ARCHITECTURE, FIA_GARRISONS,
TEST_FIXTURES и этот отчёт. Client/JIP, ручная UI-проверка, packaged build,
длительный soak и отдельный combat gate — **NOT RUN**.
Локальные Markdown links и `git diff --check` — PASS. Свои тестовые процессы
завершены, `stage/` и `bridge-stage/` удалены; profiles/logs и helpers сохранены
в evidence. Push, merge и публикация не выполнялись.

### Исходная реализация

Ветка `codex/issue-25-fia-random-garrisons` от обновлённого `origin/main`,
baseline `fe69fca69e5ab1d0e407ebcc02bc2af68ca6ddd6`, исходное дерево чистое.
Game/Server/Workbench — 1.8.0.13. Правила состава, fallback без танков и
подготовка реального radio graph: [FIA_GARRISONS.md](FIA_GARRISONS.md).
Зависимости, world resources, GUID и whitelist не изменены.

Evidence: `.codex-runtime/issue25/`. Полные baseline/after outputs, manifest,
argv, native exit codes, Workbench и остановленные server logs сохранены.
`stage-source-hashes.json` подтверждает 206 production scripts в изолированном
stage; единственный добавленный script — `AICF_FIARandomGarrisonProbe.c`.

Команды и свежие результаты:

- `tools/Invoke-AICFChecks.ps1 -Name` с FIAGarrisonContracts, ScenarioHeaders,
  EveronNorth, RuntimeLauncher, WCSIntegration, RHSIntegration, FIAPatrol,
  AICombatInput, Stage3, Stage35, Stage35RecoveryPolicy, Stage4: до — 11 PASS,
  1 FAIL; после в `after-complete/` — те же результаты плюс новый
  FIARandomGarrisonContracts PASS, итого **12 PASS / 1 FAIL**, exit 1.
  Полные argv и имена файлов — `before/summary.json` и `after-complete/summary.json`.
  Сохранён единственный baseline FAIL `STAGE4_ATTACKED_BASES`.
- `tests/static/Test-LocalizationStatic.ps1` и
  `tools/Build-AICFLocalization.ps1 -Check` через PowerShell 7: **PASS** до/после,
  587 строк. RU/EN descriptions и обе runtime tables обновлены.
- `check-auditors.ps1`: положительный структурный fixture — **PASS**;
  удаление RplProp — ожидаемый **FAIL JIP**. Положительный synthetic stopped log —
  PASS; удаление `Game destroyed.` — ожидаемый FAIL STOPPED_LOG_REQUIRED.
- `check-links.ps1` и `git diff --check`: **PASS**.
- `compile.ps1 -Variant Stock|EveronRHS|EveronWCSRHS`, labels
  `complete-stock`, `complete-rhs`, `complete-wcs`: **PASS**, native exit 0,
  `Script validation successful`, SCRIPT E/F и ENGINE F отсутствуют.
  Fixture compile `fixture-complete` — **PASS**. Resource/shutdown errors
  перечислены в полных логах; это не error-free verdict.

Dedicated запускался последовательно через `runtime.ps1`/`matrix.ps1`, которые
вызывают `tools/Start-AICFRuntime.ps1 -Role Server -RepositoryRoot <stage>
-Variant <variant> -Difficulty <difficulty> -ProfileRoot <fresh profile>
-AdditionalArguments @('-aicfRandomGarrisonProbe','full|plan',
'-aicfRequirePlayerForResult','0')`. Listener не переопределялся.
Каждый probe завершает сервер через `RequestClose()`.
Анализ: `tests/log-audits/Test-FIARandomGarrisonLog.ps1 -LogPath <console.log>`;
для планирования добавляется `-PlanOnly`. Итоговая матрица — `runtime-summary.json`.

Промежуточные отказы сохранены, но не являются итоговым PASS: North initial
не имел допустимого узла; Arland выявил отрицательный native random sample;
Everon normalized выявил раздельные компоненты HQ. Реализация исправлена:
bounded range setup, нормализация signed sample, nearest-component bridges с
проверкой фактического графа и rollback. Sandbox Workbench `production-1`
остановился на `SteamAPI_Init failed`; последующие терминальные прогоны выполнены
в пользовательской учётной записи. Ошибка имени переменной только в fixture
`fixture-final` исправлена и повторно проверена.

Итоговый stage `complete` совпадает с финальными 206 production scripts по SHA.
Все 11 запусков штатно остановлены, native exit 0. Целевой probe — **PASS**
во всех строках; общий gate оценивается отдельно по полному остановленному логу:

| Variant / difficulty | Режим | Checks / failures | Машины plan / READY | Общий log gate |
|---|---|---|---|---|
| Stock Hard | plan | 41 / 0 | 14 / — | PASS |
| RHS Hard | plan | 41 / 0 | 14 / — | PASS |
| ArlandWCSRHS Hard | plan | 40 / 0 | 13 / — | PASS |
| Everon Hard | plan | 164 / 0 | 75 / — | FAIL: shutdown `BridgeRoad` leak |
| EveronRHS Hard | plan | 164 / 0 | 75 / — | PASS |
| EveronWCSRHS Hard | full | 241 / 0 | 76 / 76 | FAIL: 16 PMC + 2 VM exceptions |
| EveronNorth Hard | plan | 37 / 0 | 12 / — | PASS |
| EveronNorthRHS Hard | plan | 37 / 0 | 12 / — | PASS |
| EveronNorthWCSRHS Hard | full | 47 / 0 | 11 / 11 | FAIL: 1 PMC |
| EveronNorthWCSRHS Medium | full | 35 / 0 | 5 / 5 | PASS |
| EveronNorthWCSRHS Easy | full | 25 / 0 | 0 / 0 | PASS |

В полном WCS Everon представлены все три обычных состава (10 / 13 / 13 баз),
усиленная точка с 2 БТР + 2 танками, 15 Т-72А, 20 Т-72Б, 41 БТР и 19 разных
шаблонов десанта. Выбор и фактические seats/roster проверены fixture;
сводка разнообразия — `variation-summary.json`.

Предыдущий stage `normalized` также прошёл full probe: Stock Hard, RHS Hard,
ArlandWCSRHS Hard — по 55 checks, 14/14 READY; Stock Medium — 39 checks,
6/6 READY; Stock Easy — 27 checks, 0 машин. Failures=0 и общий gate PASS.
Эти прогоны выполнены до финального character-type guard в catalog и расширения
radio setup для раздельных компонентов; их нельзя считать full-проверкой
финального кода. Финальное планирование повторено для всех девяти вариантов.

`full-script-errors.json` содержит точные строки ошибок финальной матрицы.
PMC `rollback=1 restored=0` — ранее наблюдавшийся класс ошибки экипировки.
Два новых наблюдения `NULL pointer ... weapMgr` имеют стек
`SCR_AIUtils.c:354` → `SCR_AISwitchMagazine.c:36` →
`WCS_Armament_AISwitchMagazine.c:21`; отсутствие регрессии для них не доказано.
В vanilla Everon при shutdown: `Leaked 'BridgeRoad' script instance (84x)`
и итоговая строка leaks. Эти ошибки не исправлялись и не исключались из gate.
ENGINE F отсутствует. Resource/WORLD/pathfinding и другие native `(E)` остаются
в полных логах; PASS выше означает только заявленный script/fatal gate.

Client/JIP, ручной вид меню, packaged build, бой и длительный soak — **NOT RUN**.
Probe проверяет повторный server Start и immutable snapshot, но не имитирует
реальное подключение клиента. Старые fixtures фиксированного состава помечены
историческими в TEST_FIXTURES.md, их прежние PASS не перенесены на случайный план.
Тестовые процессы завершены; созданные `stage/` и `contract-fixture/` удалены.
Полные profiles/logs, manifests, hashes и helper scripts оставлены в evidence.
Push, merge и публикация не выполнялись.

Изменённые области/файлы:

- Core: новые `AICF_FIAGarrisonPlanner.c`, `AICF_FIAGarrisonCatalog.c`,
  `AICF_FIAStrongpointPolicy.c`, `AICF_FIAGarrisonState.c`; изменены
  `AICF_FIAGarrisonService.c`, `AICF_FIAGarrison.c`, `AICF_FIAGarrisonCrew.c`,
  `AICF_FIAPatrolService.c`, `AICF_ContentProfile.c`, `AICF_Difficulty.c`,
  `AICF_MatchController.c`.
- Integration: новый `AICF_FIAStrongpointRadioSetup.c`, изменены
  `AICF_ArlandRadioBridgeNormalizer.c`, `AICF_WCSRHSContentProfile.c`.
- `AIConflictCore/Language/AICF_Localization.st` и `.en_us.conf`/`.ru_ru.conf`;
  `Test-FIARandomGarrisonContracts.ps1`, `AICF_FIARandomGarrisonProbe.c`,
  `Test-FIARandomGarrisonLog.ps1`.
- README в корне, ArlandWCSRHS и EveronWCSRHS; `agent-notes/README.md`,
  `ARCHITECTURE.md`, `DEVELOPMENT.md`, `GAMEPLAY.md`, `TESTING.md`,
  `TEST_FIXTURES.md`, `TOOLS.md`, новый `FIA_GARRISONS.md`.

## Оформление 0.1.26 — 2026-10-09

По запросу пользователя работа продолжена в `codex/scenario-difficulties`
без новой ветки. Исходный commit `36ec545`, дерево до правки чистое.
Добавлены `releases/0.1.26.md` и ссылки в README/указателе; игровой код,
headers и зависимости не менялись. Состав сверен с `git log 0.1.25..HEAD`
и сохранёнными результатами ниже. Прежние FAIL и NOT RUN сохранены в патчноутах.

Свежие проверки документации: `git diff --check` до/после — PASS/0;
PowerShell-проверка локальных ссылок README, указателя и патчноутов — PASS/0.
Evidence: `.codex-runtime/release-0.1.26/` (исходный SHA/status,
`before-check.txt`, `after-check.txt`, `links.txt`). Полный offline набор,
Workbench, server/client runtime, JIP, ручные проверки, soak и packaged build
при оформлении — NOT RUN. Старые PASS не являются новым прогоном релиза.
Push и Workshop upload не выполнялись.

## Т-72 на сложной сложности всех WCS — 2026-10-09

Продолжение `codex/scenario-difficulties`, baseline `96093a1`.
В Hard headers WCS Arland и полного WCS Everon включён `m_bAICFFIATank`.
Medium остаётся с одним БТР, Hard во всех WCS получает БТР + Т-72А.
Десант сохранён только в North WCS; новые машины имеют по три члена экипажа.
Production scripts, GUID и `.gproj` dependencies не менялись.
Обновлены два headers, их RU/EN descriptions и generated string tables,
RuntimeLauncherStatic, RolloutProbe и текущая документация.

Evidence: `.codex-runtime/wcs-t72-all/`.
`Invoke-AICFChecks.ps1 -Name` (FIAGarrisonContracts, ScenarioHeaders,
RuntimeLauncher, WCSIntegration, RHSIntegration, EveronNorth): **6/6 до и после**.
`Test-LocalizationStatic.ps1` через PowerShell 7: **PASS до/после**, 587 строк.
Терминальный `compile.ps1 -Variant EveronWCSRHS`: production и fixture **PASS**,
validation successful, без SCRIPT E/F и ENGINE F. RuntimeLauncherStatic
отвергает обе отрицательные мутации: выключенный танк в WCS Hard и включённый
в vanilla Hard, ожидаемый `DIFFICULTY_TANK_SCOPE` FAIL.

Dedicated через `Start-AICFRuntime.ps1`, `run.ps1 -Variants ArlandWCSRHS`
(2351) и `-Variants EveronWCSRHS` (2353), Hard, отдельный stage с
`AICF_DifficultyRolloutProbe`: оба завершены, exit 0. Целевой fixture **PASS**:
Arland — 79 checks, 6 БТР + 6 Т-72А; полный Everon — 327 checks,
37 БТР + 37 Т-72А, в обоих случаях failures=0. У всех машин crew=3/seats=3,
по две стационарные машины на карте. Проверены native headers трёх сложностей,
состав, отсутствие десанта и выборка прибытия патруля после 60 секунд наблюдения
за полностью созданными гарнизонами. В fixture отключён урон готовым машинам
и экипажам; это не проверка боевой эффективности.

Полные остановленные console/script/error logs сохранены в `logs-*`, команды
и `AICF_RUNTIME_MANIFEST_JSON` — в `*-launcher.txt`, сводка — `runtime-summary.json`.
Общий runtime log gate **FAIL**: повторился `PMC_EQUIPMENT_FAILED`
с `rollback=1 restored=0` (2 на Arland, 6 на Everon); это все SCRIPT E.
SCRIPT F, ENGINE F и VM exception — 0. Также присутствуют ошибки ресурсов,
world/entity и, на Everon, material/pathfinding. Эти ошибки не исправлялись
в изменении состава техники. Оба тестовых сервера остановлены.

Предыдущие результаты северного WCS ниже относятся к прошлому коммиту;
северный header в этом уточнении не изменён. Новый client/JIP, ручной вид меню,
packaged build, бой и длительный soak — **NOT RUN**. Старый Stage4 baseline FAIL
не исправлялся и в этом узком наборе не перезапускался.

## Перенос сложностей на все сценарии — 2026-10-09

Ветка `codex/scenario-difficulties`, baseline `ffa0ac2`.
Добавлены 16 inherited headers (27 вариантов сложности суммарно), header flags
танка/десанта, три essential места новых БТР, общая таблица трёх ПТ, stock US LAT,
стационарный fallback площадки и отдельная квота AIWorld под initial crews.
Зависимости `.gproj` не изменены. North WCS+RHS сохраняет БТР с десантом и Т-72.

Evidence: `.codex-runtime/scenario-difficulties/`. `before/` и
`after-capacity/`: `Invoke-AICFChecks.ps1 -Name` с одинаковыми 12 gates
(FIAGarrisonContracts, ScenarioHeaders, EveronNorth, RuntimeLauncher,
WCS/RHSIntegration, FIAPatrol, AICombatInput, Stage3, Stage35, Stage35RecoveryPolicy,
Stage4). До/после: **11 PASS, тот же FAIL STAGE4_ATTACKED_BASES**.
`pwsh -File tests/static/Test-LocalizationStatic.ps1` и
`pwsh -File tools/Build-AICFLocalization.ps1 -Check`: **PASS**, 587 строк.
Первый расширенный запуск runner включал Localization через WinPS 5.1 и получил
ошибку разбора существующего UTF-8 audit без BOM; прямой PowerShell 7 gate до/после PASS.

Терминальный Workbench: `compile.ps1 -Variant Stock|EveronRHS|EveronWCSRHS`,
labels `release-stock`, `release-rhs`, `release-production`: **PASS**,
validation successful, без SCRIPT E/F и ENGINE F. Полные argv и logs сохранены.
Runtime fixture с обоими probes: `capacity-fixture` — **PASS**.
Промежуточные compile failures (операции с временным значением array и
неявным float в выражении modulo) сохранены, исправлены до финальных запусков.
Native exit 0 в этих неудачных запусках не трактуется как PASS.

`Test-RuntimeLauncherStatic.ps1` проверяет Server/Client manifests для всех
18 Medium/Hard комбинаций, точные GUID/paths, inherited difficulty/flags.
Позитивный вход PASS; копия header с запрещённым tank flag даёт ожидаемый
`DIFFICULTY_CREW_ONLY` FAIL. FIAGarrisonContracts: позитивный PASS,
отключение preflight в изолированной копии даёт ожидаемый `SPAWN_PATROL_PREFLIGHT` FAIL.

Dedicated запущены только через `Start-AICFRuntime.ps1`, обёртка `run.ps1`
сохраняет manifest, exit и полную копию остановленного profile `logs-<label>/`.
`AICF_DifficultyRolloutProbe` читает все три native headers каждого Variant.
Итоги targeted probe (число готовых машин / checks, failures):

| Сценарий | Hard |
|---|---|
| Stock Arland | 12 / 79, 0 |
| RHS Arland | 12 / 79, 0 |
| WCS+RHS Arland | 12 / 79, 0 |
| Stock Everon | 74 / 327, 0 (повтор после зависания) |
| Stock Everon North | 10 / 70, 0 |
| RHS Everon | 74 / 326, 0 |
| RHS Everon North | 10 / 70, 0 |
| WCS+RHS Everon | 74 / 326, 0 |
| WCS+RHS Everon North | 10 / 65, 0; пять БТР по 10 бойцов, пять Т-72 по 3 |

Stock Arland Medium: 6 машин, 54/54; Easy: 0 машин, 29/29, без добавления квоты.
На Арланде Hard две машины стационарны (ближайшая дорога одной базы в 233 м).
На полном Эвероне стационарны 1–2 машины, на северном — одна. Это
согласованный fallback, а не успешный дорожный патруль. Остальные проверяются
по составу; arrival sample подтверждает движение хотя бы одного патруля,
не всех машин. Deadline/retired guard не отключались ради PASS.

`AICF_DifficultyInfantryProbe -aicfATProbe 1`, Medium stock/RHS/WCS+RHS:
**60/60 каждый**, по 24 фактических бойца, initial groups и donors позиций 8/9.
Stock US — заряженный M72A3; USSR — РПГ-7. В первом stock прогоне отсутствующий
`Character_US_AT.et` приводил к обычному fallback; Medium/Hard исправлены на
штатный `Character_US_LAT.et`, Easy сохранён. Эти проверки не доказывают огневую
эффективность, оплату пополнения или переход donor в боевой отряд.

Полный log gate отделён от targeted probe: в WCS Arland зарегистрирован один
`PMC_EQUIPMENT_FAILED rollback=1 restored=0`, в полном RHS Everon — шесть,
в полном WCS Everon — девять (одни и те же события дублируются в трёх log-файлах).
Это ранее наблюдавшийся сбой FIA equipment adapter, в этой задаче не исправлялся;
**полный combat log gate для этих запусков FAIL**, несмотря на PASS состава.
Resource/backend/pathfinding errors сохранены отдельно; VM и ENGINE F не обнаружены.

До исправления квоты полные RHS/WCS упирались в 260 активных персонажей и
`INITIAL_CREW_FAILED`; финальная квота 481 позволила посадить все 74 экипажа.
Первый полный stock прогон после создания 74 машин перестал обновлять log на
`t_ms=200753`; процесс принудительно остановлен, диагностика `stock-hang.json`.
Этот прогон **не PASS**. Отдельный повтор `Everon-Hard-retry` завершился
327/327, без SCRIPT E/F, VM и ENGINE F; причина первого зависания не установлена,
единичный успешный повтор не доказывает долгую стабильность.

Все свои серверы завершены, stage и негативные копии удалены после сохранения
логов. Изменённые файлы перечислены в `changed-files.txt`, итог по полным
остановленным console logs — `runtime-summary.json` (без тройного учёта дублей).

Client/JIP, реальный игрок у границы 50 м, packaged build, ручной вид меню,
долгий soak и полный бой до уничтожения гарнизонов — **NOT RUN**. Прежние
длительные recovery-проблемы этим переносом не объявлены исправленными.

## Выбор проверок

Статика находится в `tests/static/`, контракты — в `tests/contracts/`, анализаторы
логов — в `tests/log-audits/`. Полный каталог, параметры и статусы:
[TOOLS.md](TOOLS.md), runtime probes: [TEST_FIXTURES.md](TEST_FIXTURES.md).
Шаблон запуска из корня:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/static/Test-Stage3Static.ps1
```

Полный offline набор с сохранением argv, exit codes и полного вывода:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1`.
`-List` показывает точные пути без запуска, `-Name Test-Stage3Static.ps1`
выбирает отдельный тест. Известные baseline failures не исключаются из exit code.

| Область | Минимальная статика / contracts |
|---|---|
| Markdown и ссылки | `git diff --check`; локальные ссылки должны работать из чистого checkout, команды и пути сверяются с исходниками |
| Offline runner и размещение скриптов | `Test-CheckRunnerContracts.ps1`; при переносе — весь offline набор до/после и проверка ссылок/зависимостей |
| Vehicle ownership, trips, cleanup | `Test-Stage3Static.ps1`, `Test-Stage35Static.ps1`, `Test-Stage3StaticContracts.ps1`, `Test-Stage35RecoveryPolicy.ps1` |
| Экономика, strategic snapshot, RPC | `Test-Stage4Static.ps1` и профильный audit |
| Bootstrap, command authority | `Test-AICommanderModeStatic.ps1`, `Test-AICommanderUIContracts.ps1` |
| Приказы в точку, навигация, recovery | `Test-MapPointOrdersStatic.ps1`, `Test-DefendWaypointInputContracts.ps1`, `Test-MovementRecoveryContracts.ps1`, `Test-RecoveryEpisodeContracts.ps1`, `Test-Issue12RecoveryContracts.ps1`, `Test-Issue14RecoveryContracts.ps1`, `Test-EndgameContracts.ps1` — выбери затронутые контракты |
| Пехотный spawn, движение и набор | `Test-InfantrySpawnPlacementStatic.ps1`, `Test-InfantryAdvanceContracts.ps1`, `Test-InfantryApproachContracts.ps1`, `Test-InfantryRecruitmentStatic.ps1`, `Test-BarracksCombatContracts.ps1` |
| Прогноз снабжения и приоритет набора | `Test-RecruitmentSupplyContracts.ps1`, `Test-RecruitmentPriorityContracts.ps1`, InfantryRecruitment, BarracksCombat, Stage35, Stage4 |
| Команды отряда и respawn | `Test-SquadCommandsContracts.ps1`, `Test-SquadRespawnContracts.ps1` |
| Победа и полная замена группы | `Test-VictoryRespawnStatic.ps1`, `Test-EndgameContracts.ps1`, Stage35, Stage4 |
| Логистика и карта снабжения | `Test-LogisticsStatic.ps1`, `Test-LogisticsContracts.ps1`, `Test-ManualSupplyStatic.ps1`, `Test-SupplyMapUIStatic.ps1`, `Test-LogisticsMapMarkersStatic.ps1` |
| Construction и worker lifecycle | `Test-ConstructionStatic.ps1`, `Test-ConstructionContracts.ps1`, `Test-BaseBuildersStatic.ps1`, `Test-BaseBuilderDangerContracts.ps1`, `Test-ForcedSmallBarracksContracts.ps1` |
| Loadouts и личные пресеты | `Test-AILoadoutStatic.ps1`, `Test-PersonalLoadoutContracts.ps1` |
| Маркеры, локализация, rank | `Test-GroupMapMarkersStatic.ps1`, `Test-LocalizationStatic.ps1`, `Test-RankRestrictionsStatic.ps1` |
| Headers, карты, launcher | `Test-ScenarioHeadersStatic.ps1`, `Test-EveronNorthStatic.ps1`, `Test-RuntimeLauncherStatic.ps1` |
| Начальная погода | ScenarioHeaders, RHSIntegration, WCSIntegration; runtime `AICF_MatchWeatherProbe` и `Test-MatchWeatherLog.ps1`, повторные новые матчи с `-RequireVariation` |
| RHS/WCS content и integration | `Test-RHSIntegrationStatic.ps1`, `Test-WCSIntegrationStatic.ps1`, затронутые scenario/authority/Stage audits |
| Сложность гарнизонов FIA | `Test-FIAGarrisonContracts.ps1`, `Test-FIARandomGarrisonContracts.ps1`, `Test-RuntimeLauncherStatic.ps1`, ScenarioHeaders, FIAPatrol, AICombatInput и Stage3/35; runtime `AICF_FIARandomGarrisonProbe.c` в stage |
| Combat inputs, FIA | `Test-AICombatInputContracts.ps1`, `Test-FIAPatrolStatic.ps1` |

Изменение аудитора требует позитивного и негативного representative input.
Новые tests нужны для проверяемого контракта, а не для дублирования реализации.
Изменение production `.c`/Enfusion API требует Workbench для затронутых graph;
изменение поведения — соответствующий runtime. Для новых headers нужны source
load и packaged build; недоступная агенту ручная проверка плитки фиксируется
отдельно как `NOT RUN` и отклоняется из критериев issue по правилам приёмки.

## Раздельные gates

| Gate | Что подтверждает |
|---|---|
| Static audit | Проверенные структурные/текстовые контракты |
| Terminal Workbench Validate/Compile | Совместимость Game module с API и выбранным source graph |
| Server runtime | Поведение authority в заданном сценарии |
| Client/JIP | Проверенные по полным client/server logs сетевые свойства |
| Soak | Устойчивость на указанной длительности и конфигурации |
| Manual | Внешний вид и интерактивные действия, проверенные пользователем |

Один gate не заменяет другой. Запущенный процесс, native exit 0 или отсутствие
ошибок в коротком фрагменте лога не образуют runtime PASS. `NOT RUN` не означает
`PASS`, агент самостоятельно не присваивает `ACCEPTED`. Недоступные агенту визуальные
проверки остаются `NOT RUN` в отчёте и отклоняются из активных критериев issue
с объяснением; ожидание ручного verdict не является gate завершения issue.
Screenshots/GUI automation не используются.

Команды Workbench и launcher: [DEVELOPMENT.md](DEVELOPMENT.md). Runtime fixture
из `tests/fixtures/` копируется только в изолированный stage; обычный addon не
должен содержать probe. Укажи, что fixture моделирует и чего не доказывает.

Анализаторы `Test-Stage2Log.ps1`, `Test-Stage4Log.ps1`,
`Test-InfantryRecruitmentLog.ps1`, `Test-LogisticsLog.ps1`,
`Test-ConstructionLog.ps1`, `Test-BaseBuildersLog.ps1` и другие `*Log.ps1`
читают **полные остановленные** логи. Параметры и требуемую матрицу смотри в
`param` конкретного script; для recruitment supply используй профильные
[команды и ограничения](RECRUITMENT_SUPPLY_PLANNING.md).

## Baseline реорганизации tools, 2026-10-07

Исходный commit `2cd6bf3abc8b019729f5f3a65e7d013a9d6f6872`, ветка
`codex/docs-player-readme`, до правки рабочее дерево чистое. Свежий baseline
46 offline проверок через `powershell.exe`: **41 PASS, 5 FAIL** (exit 1):

| Проверка | Сохранённая причина FAIL |
|---|---|
| `Test-InfantryApproachContracts.ps1` | `APPROACH_CLEANUP` |
| `Test-LocalizationStatic.ps1` | ParserError в Windows PowerShell: UTF-8 без BOM с кириллицей в regex. Через PowerShell 7 файл разбирается; это не PASS выполнения |
| `Test-NorthFailureContracts.ps1` | `MOVE_CALLBACK_FENCE`, `MOVE_ACTIVITY` |
| `Test-SquadCommandsContracts.ps1` | `STABLE_NEAREST` |
| `Test-Stage4Static.ps1` | `STAGE4_ATTACKED_BASES` |

Evidence: `.codex-runtime/tools-reorganization-20261007/`, `before/summary.json`
и полные `before/*.txt`. FAIL не отключены и не объявлены устаревшими контрактами;
их причины требуют отдельных исправлений. Общая матрица возвращает exit 1.

Полный повтор после переноса: **42 PASS, те же 5 FAIL** (47 проверок, включая
новый `Test-CheckRunnerContracts.ps1`), общий exit 1. Вывод — `after/summary.json`
и `after/*.txt`. После окончательного выбора `tests/log-audits/` ещё раз выполнены
семь зависимых gates: CheckRunner, Construction, ForcedSmallBarracks, Logistics,
RecoveryEpisode, SquadCommands contracts и Stage4Static — 5 PASS, прежние 2 FAIL,
exit 1 (`final-paths-verified/`). Runner проверен через `-File` и вложенный `&`
в Windows PowerShell 5.1 и PowerShell 7. Сводный `final-summary.json` и
`comparison.txt` подтверждают прежние exit codes и причины всех пяти FAIL,
новых регрессий нет. Промежуточный `final-paths/` прерван при отладке runner и
не является финальным verdict.

`Build-AICFLocalization.ps1 -Check` — PASS/0 до и после,
вывод в `localization-build-*.txt`. Проверены ссылки на отслеживаемые файлы,
каталог всех tools/tests, относительные зависимости, PowerShell 7 parsing и
`git diff --cached --check`; перенесённые fixture и четыре локальных файла
сохранены побайтово (`layout-verification.txt`).

Production `.c` и содержимое runtime probes не менялись. Workbench, server/client,
JIP, soak, визуальные проверки, showcase и watchdog — **NOT RUN** в этой задаче.
Переезд файлов не подтверждает их runtime совместимость.

## Исключение Кермована, 08.10.2026

Продолжение той же задачи на `codex/everon-north-powerplant`, baseline
`d3c9baa`, чистое дерево до правки. По уточнённому запросу Кермован удалён
из обоих North whitelist без замены; WCS+RHS наследует RHS. Остаются 7 баз,
2 HQ и 5 целей. Полные сценарии и production `.c` не изменены. Обновлены
RU/EN описания, runtime tables, North audit и fixture, README и эти заметки.

Evidence: `.codex-runtime/everon-north-exclude/`.

- `tools/Invoke-AICFChecks.ps1 -Name Test-EveronNorthStatic.ps1,Test-ScenarioHeadersStatic.ps1,Test-WCSIntegrationStatic.ps1,Test-RHSIntegrationStatic.ps1,Test-LocalizationStatic.ps1 -EvidenceRoot <before|after>`:
  4 PASS и прежний 1 FAIL, exit 1 до/после. FAIL — ParserError
  `Test-LocalizationStatic.ps1` в Windows PowerShell 5.1; новых failures нет.
- `pwsh -NoProfile -File tests/static/Test-LocalizationStatic.ps1` и
  `pwsh -NoProfile -File tools/Build-AICFLocalization.ps1 -Check`:
  PASS/0 до/после, 533 записи. Генерация таблиц тем же builder без `-Check` — PASS.
- North audit с корректным representative input — PASS/0; возврат Кермована
  вместо Тайрона при том же числе баз — ожидаемый FAIL/1 `NORTH_BASE_IDENTITIES`.
- Terminal Workbench stage для `EveronNorth` и `EveronNorthWCSRHS`:
  PASS/0, `Script validation successful`, SCRIPT E/F и ENGINE F отсутствуют.
  Команды/полные logs/exit codes: `compile.ps1`, `wb-<Variant>-argv.json`,
  `wb-<Variant>/`, `compile-summary.json`. Остались 25/39 строк resource errors,
  включая shutdown leaks; это не error-free verdict.

Runtime запускается последовательно через `runtime.ps1`, вызывающий
`tools/Start-AICFRuntime.ps1 -Role Server -Variant <Variant> -RepositoryRoot <stage>`
в PowerShell 7. Дополнительные аргументы: `-aicfNorthProbe 1`
и `-aicfRequirePlayerForResult 0`. Установленная версия — 1.8.0.13.
Fixture проверяет активный whitelist, механизмы захвата и radio routes;
завершение — `RequestClose()`, задержка сокращена с 150000 до 1000 ms только
в stage. 232 production source-файла stage совпадают с checkout по SHA-256:
`source-hashes.json`, `source-match.txt`.

Все три остановленных server logs проверены целиком через
`pwsh -NoProfile -File .codex-runtime/everon-north-exclude/analyze-runtime.ps1`
(PASS/0, `runtime-summary.json`):

| Variant | Targeted runtime | Native exit | Остальные строки ошибок |
|---|---|---|---|
| `EveronNorth` | PASS, 15/15 | 0 | 47 |
| `EveronNorthRHS` | PASS, 15/15 | 0 | 198 |
| `EveronNorthWCSRHS` | PASS, 15/15 | 0 | 233 |

Везде `nodes=7 missing=0`, есть `ROSTER_READY`, Кермована нет среди активных
баз и `GRAPH_NODE`, SCRIPT E/F и ENGINE F отсутствуют. Остальные ошибки
относятся к ресурсам/свойствам мира, pathfinding и shutdown leaks;
сохранены `errors-<Variant>.txt`. PASS относится к составу/механизмам/связности,
а не к отсутствию всех engine errors. Полные logs и временные метки:
`server-<Variant>-run/logs/*/console.log`; точные CLI, manifest и profiles:
`server-<Variant>-launcher.txt`. Все тестовые процессы завершились сами;
stage и negative-input copy удалены после сохранения hashes. `git diff --check`
и staged diff check — PASS/0.

Client/JIP, ручная карта, боевой захват/победа, строительство, soak и packaged
build — NOT RUN. Полный offline набор и его остальные известные failures
не перепроверялись. Старый неудачный опыт с электростанцией приведён ниже
как история; он не описывает финальную конфигурацию этого изменения.

## Проверка лагеря у электростанции, 08.10.2026

Исходник — `0bc09b5` (`origin/main`), чистое дерево; ветка задачи
`codex/everon-north-powerplant`. В эксперименте North headers stock/RHS
заменяли `TownBaseKermovan` на `StartingPos06`; WCS+RHS наследовал RHS.
Изменения headers и fixture **отменены после runtime FAIL**, игровой перенос
не выполнен. Сохраняется исходный состав баз.

Evidence: `.codex-runtime/everon-north-powerplant/`. Запуск
`tools/Invoke-AICFChecks.ps1 -Name Test-EveronNorthStatic.ps1,Test-ScenarioHeadersStatic.ps1,Test-WCSIntegrationStatic.ps1,Test-RHSIntegrationStatic.ps1`
с отдельным `-EvidenceRoot` дал 4 PASS/0 до и после эксперимента
(`before/`, `after/`); baseline failures в этом наборе отсутствуют.
После отмены эксперимента тот же набор — 4 PASS/0 (`restored/`),
`git diff --check` — PASS/0. Финальные изменения только в агентских заметках.
Representative positive/negative для изменённого North audit — 0/1;
это доказательство проверки списка, а не возможности захвата.

Терминальный Workbench для изолированного stage, варианты `EveronNorth` и
`EveronNorthWCSRHS`: exit 0, `Script validation successful`, без SCRIPT E/F
и ENGINE F. Полные логи: `wb-<Variant>/`, команды: `wb-<Variant>-argv.json`.
Первый stock запуск wrapper прерван обработкой native stderr в PowerShell 5.1;
он не считается PASS. Успешные повторные прогоны сохранили native exit codes.

Сервер `EveronNorth` 1.8.0.13 через `tools/Start-AICFRuntime.ps1`, PowerShell 7:
**FAIL, 18/19 checks**, `CAPTURE_MECHANISM_StartingPos06 passed=0`.
Положение, spawn point, 8 активных баз, 2 HQ, 6 control points, roster и
radio graph (`nodes=8 missing=0`) прошли. Native exit 0 не отменяет FAIL.
Fixture в stage завершила сервер через `RequestClose()` спустя 1000 ms после
проверок; штатная задержка fixture 150000 ms сокращена только в stage.
Полный stopped log: `server-EveronNorth-run/logs/*/console.log`; точные CLI,
profile и `AICF_RUNTIME_MANIFEST_JSON` — `server-EveronNorth-launcher.txt`.
Предварительный запуск в PowerShell 5.1 прервался на native stderr и не даёт
runtime verdict. SCRIPT E/F и ENGINE F в завершённом прогоне нет;
ошибки ресурсов, мира и pathfinding сохранены, логи не объявляются чистыми.

RHS/WCS server runtime после выявленного функционального FAIL, client/JIP,
ручная карта/захват, стройка, soak и packaged build — **NOT RUN**.
Старые 5 baseline failures полного offline набора не перепроверялись.
Диагностические копии удалены после сохранения evidence; production `.c`
не менялись. Вывод по архитектурному ограничению — в [GAMEPLAY.md](GAMEPLAY.md).

## Гарнизоны FIA и три сложности — 2026-10-08

Ветка `codex/fia-difficulty` от `origin/main` `27fc20a`. Baseline снят
на `9fbf85f`, чей source tree совпадает с этой целевой веткой. Evidence:
`.codex-runtime/fia-difficulty/`; полные argv/exit codes, manifests и логи
сохранены там, а не в Git. Изменены Core difficulty/garrison/crew, composition
root, fleet/spawner/cleanup, WCS content profile, два inherited North header,
локализация, launcher, профильные проверки и технические заметки.

Команда `tools/Invoke-AICFChecks.ps1 -Name <список> -EvidenceRoot <каталог>`:
`before/` — 10 PASS/0; `after-final/` — те же 10 плюс новый
`Test-FIAGarrisonContracts.ps1`, 11 PASS/0. Список: AICombatInputContracts,
DefendWaypointInputContracts, EveronNorthStatic, FIAPatrolStatic,
RuntimeLauncherStatic, ScenarioHeadersStatic, Stage35RecoveryPolicy,
Stage35Static, Stage3Static, Stage3StaticContracts. В `expanded/` также
CheckRunnerContracts, RHSIntegrationStatic и WCSIntegrationStatic — PASS/0.
Launcher audit проверяет Medium/Hard server/client manifests и отказ для
несовместимого Variant. `pwsh -File tests/static/Test-LocalizationStatic.ps1`
и `pwsh -File tools/Build-AICFLocalization.ps1 -Check` — PASS/0, 539 записей.

В выбранном baseline failures нет. Полный offline набор **NOT RUN**:
исторические пять failures (InfantryApproach APPROACH_CLEANUP, Localization
ParserError в PowerShell 5.1, NorthFailure MOVE_CALLBACK_FENCE/MOVE_ACTIVITY,
SquadCommands STABLE_NEAREST, Stage4 STAGE4_ATTACKED_BASES) не перепроверялись
и не объявляются исправленными.
После защиты null-цели повторены AICombatInputContracts, FIAGarrisonContracts
(20 guards) и FIAPatrolStatic: `after-null-guard/` — 3 PASS/0.

Терминальный Workbench 1.8.0.13: `compile.ps1 -Label production-final` и
`-Variant EveronNorth -Label stock-production-final` — PASS/0,
`Script validation successful`, без SCRIPT E/F и ENGINE F. Логи:
`wb-production-final/`, `wb-stock-production-final/`, аргументы в
соответствующих `wb-*-argv.json`. Fixture graph `wb-fixture3/` также
PASS/0. Прочих строк (E) соответственно 39/25/39; resource logs не чистые.
`source-hashes.json`/`source-match.txt` фиксируют совпадение 240 production
файлов с runtime stage; диагностические fixture добавлялись отдельно.
После защиты null-цели `wb-production-guard/`, `wb-stock-production-guard/`
и `wb-combat-guard/` вновь прошли Validate/Compile, exit 0, validation successful,
без SCRIPT E/F и ENGINE F. Финальные SHA-256 — `source-hashes-final.json` /
`source-match-final.txt`, 240/240 совпадений.

Dedicated server 1.8.0.13, Variant `EveronNorthWCSRHS`, источник — stage.
Запуск через `tools/Start-AICFRuntime.ps1 -Role Server -Difficulty <уровень>`
с `-aicfGarrisonProbe 0|1|2 -aicfRequirePlayerForResult 0`; точные команды
в `runtime.ps1`, CLI и `AICF_RUNTIME_MANIFEST_JSON` в
`server-<label>-launcher.txt`, свежие profiles — `server-<label>/`.
Все три процесса завершены fixture через `RequestClose()`, native exit 0,
в полном логе есть `ROSTER_READY` и `Game destroyed.`.

| Уровень / label | Состав на пяти FIA-точках | Проверки | Время MSK | Verdict |
|---|---|---|---|---|
| Easy | Без новых гарнизонов | 2/2 | 17:53:08–17:54:41 | PASS |
| Medium | 5 БТР-70, по 10 бойцов со всеми десантными местами | 30/30 | 17:48:56–17:51:45 | PASS |
| Hard / Hard3 | 5 БТР-70 + 5 Т-72А, по 3 бойца в танке | 55/55 | 17:45:19–17:48:18 | PASS |

Проверены полный roster/посадка, начальная дистанция до базы, отсутствие
waypoint и сдвига машины более 5 м за 60 с. Затем fixture захватывает одну
базу и убивает одного члена экипажа; спустя ещё 60 с проверяет сохранение
остальных защитников и отсутствие пополнения. Это не тест гибели всего
гарнизона. Полные остановленные логи: `server-<label>/logs/*/console.log`;
`runtime-summary.json` содержит их точные пути, CLI и время.
`pwsh -File tests/log-audits/Test-FIAGarrisonLog.ps1 -LogPath <полный лог> -Difficulty 0|1|2`
— PASS/0 для трёх логов, SCRIPT E/F и ENGINE F отсутствуют, по 233 прочих
строки (E) сохранены. Negative input без READY — ожидаемый FAIL/1
(`log-audit-negative.txt`).

Предварительные попытки не считаются PASS: `wb-initial` — неверный root
wrapper; `compile1` — ошибка float/modulo, исправлена; `server-Hard` и
`server-Hard2` — неверные prefab GUID/нулевая lease capacity, исправлены.
Hard2 остановлен адресно по process/profile, exit -1, запись
`Hard2-forced-stop.json`. Остальные тестовые серверы останавливает fixture.

Состав/удержание выше проверены до последней защиты null-цели; после неё
повторён боевой Hard, а полный трёхуровневый probe повторно не запускался.

Дополнительная изолированная `AICF_FIAGarrisonCombatProbe.c` сохранена в evidence.
Она создаёт активные BLUFOR AI-группы с неуязвимыми пехотными целями в 45 м
перед машинами, наблюдает расход боеприпасов у стрелков и удержание всех машин
в пределах 5 м в течение 90 с. Это проверка стрельбы по пехоте, а не всей
огневой эффективности. Запуск — `runtime-combat.ps1 -Difficulty Hard -Label <label>`,
штатный launcher с `-aicfGarrisonCombatProbe 1`, остановка через `RequestClose()`.

Ранние `Combat`/`Combat2` — FAIL: цели не были активированы через AI-группы.
`Combat3` подтвердил огонь БТР/танков, но весь gate — FAIL из-за трёх VM
exceptions `currentTarget` в stock `SCR_AIUpdateTargetAttackData`. Повтор
`CombatDiag4` без исправления прошёл: сбой непостоянный. По source API 1.8.0.13
два threat callback не проверяли пустую цель; добавлен ранний return, при
существующей цели остаётся super. Финальная fixture отдельно вызывает оба
callback без цели и отмечает `GARRISON_NULL_GUARD escalation=1 damage=1`.
`CombatGuard` прошёл функциональные критерии и не имел VM exceptions, но весь
log gate — FAIL из-за одной `PMC_EQUIPMENT_FAILED`: выдача нового комплекта
не удалась, `rollback=1` вернул исходный. Этот отдельный сбой RHS equipment
не исправлялся в данной задаче и не выдаётся за старый подтверждённый baseline.
`CombatGuard2` — **PASS/0**: 18:18:01–18:20:30 MSK, `btr=1 tank=1`,
оба null-callback проверены, машины удержались на месте, SCRIPT E/F и ENGINE F
нет. Полный лог — `server-CombatGuard2/logs/*/console.log`, 233 прочие строки (E).
Пулемётный огонь подтверждён расходом боеприпасов; стрельба танковой пушки
по бронецели не проверялась. Ошибка PMC из предыдущего прогона не повторилась,
но её лог и ограничение сохранены.
`git diff --check` — PASS/0. Тестовые engine processes остановлены; stage
удалён после проверки абсолютного пути, полные логи и baseline сохранены.
Точный список 33 изменённых файлов — `changed-files.txt` в evidence.
Client/JIP, ручная проверка плиток меню, packaged build, долгий soak,
танковая пушка против бронетехники и полный бой до гибели гарнизона — **NOT RUN**.

## Локальный патруль гарнизонов FIA — 2026-10-08

Продолжение той же ветки от `3d8253a`: прежний тест неподвижной обороны выше
не подтверждает новое поведение. Добавлен локальный vehicle patrol с immutable
home, короткими дорожными участками, возвратом при удалении и identity-проверкой
текущего waypoint. Изменены crew policy, failure recovery, handoff, spawn geometry,
локализация, профильный probe и заметки. Полные логи и команды:
`.codex-runtime/fia-local-patrol/`.

Baseline `before/`: AICombatInputContracts, FIAGarrisonContracts, FIAPatrolStatic,
Stage35RecoveryPolicy, Stage35Static, Stage3Static, Stage3StaticContracts — 7 PASS/0.
Перед изменением AI node отдельно `before-node/`: NorthFailureContracts —
FAIL/1, `MOVE_CALLBACK_FENCE` и `MOVE_ACTIVITY`. В `after-final/` те же
7 PASS/0 и тот же NorthFailure FAIL/1; этот baseline не исправлен.
Команда: `tools/Invoke-AICFChecks.ps1 -Name <список> -EvidenceRoot <каталог>`;
точные аргументы и exit codes находятся в `summary.json` каждого каталога.
Локализация до/после: `pwsh -File tests/static/Test-LocalizationStatic.ps1` и
`pwsh -File tools/Build-AICFLocalization.ps1 -Check` — PASS/0, 539 записей.

Runtime запускается `runtime.ps1 -Difficulty Hard -Expected 2 -Label <label>`
через штатный launcher, Variant `EveronNorthWCSRHS`, CLI
`-aicfGarrisonProbe 2 -aicfRequirePlayerForResult 0`. Источник — отдельный stage,
полные logs в `server-<label>/logs/*/console.log`, manifest/CLI/native exit —
`server-<label>-launcher.txt` и `server-<label>-exit.txt`.
Ранние `Patrol1`–`Patrol4` и короткий `Diagnostic` — FAIL: выявили blocked
native avoidance, неудачные площадки у берега, слишком длинные участки и
отклонённые старые действия, конкурирующие с новым приказом. Это сохранённое
диагностическое evidence, а не успешные проверки. В Patrol1/2/4 также есть
`PMC_EQUIPMENT_FAILED rollback=1` — ранее наблюдавшийся сбой выдачи экипировки;
его не исправляли и не исключали из log gate. Подробности ошибок из полных
остановленных логов сохранены в `runtime-attempt-errors.json`.

`Patrol5` — FAIL/4 cases, `Patrol6` — FAIL/2 cases (и PMC equipment error),
`Patrol7` — FAIL/1 case: БТР у причала начинал на коротком ответвлении без
подходящего местного участка. Исправление: продолжение текущей дороги имеет
приоритет, а такой маршрут проверяется до spawn. Crew policy завершает
отклонённое действие, сохраняя bounded native avoidance/boarding.

Финальные production Workbench команды:
`compile.ps1 -Label patrol8-production` и
`compile.ps1 -Variant EveronNorth -Label stock-patrol8-production` — PASS/0,
`Script validation successful`, SCRIPT E/F и ENGINE F нет. Версия 1.8.0.13.
Полные логи: `wb-patrol8-production/`, `wb-stock-patrol8-production/`; точные
аргументы — `wb-*-argv.json`, сводка — `compile-final-summary.json`.
Прочих строк (E) 39/25: engine resource logs не чистые.
`source-hashes-final.json` подтверждает SHA-256 совпадение 241 production файла
со stage. Fixture добавлен только в stage; его damage protection не входит
в production. `contracts-patrol8.txt`: FIAGarrisonContracts — PASS/0, 34 guards.
Анализ старого Hard3 stationary log новым анализатором — ожидаемый FAIL/1
(`log-audit-old-stationary-negative.txt`): старая проверка неподвижности не может
подтвердить патруль.

После preflight повторён весь выбранный набор: `after-road-preflight/` —
7 PASS/0. Финальный `Patrol8` (19:48:46–19:55:12 MSK) — **PASS/0**:
55/55 cases, пять БТР-70 с 10 бойцами и пять Т-72А с 3 бойцами, каждый выполнил
не менее двух физических прибытий. Наблюдаемый максимум удаления от домашней
базы в samples — 153.863 м. Probe затем меняет владельца базы, убивает одного
защитника и ещё 60 с проверяет сохранение гарнизона без пополнения.
Полный остановленный лог:
`server-Patrol8/logs/logs_2026-10-08_19-48-46/console.log`;
`runtime-final-summary.json`, `Patrol8-analysis.txt`/`-exit.txt` — сводки.
`pwsh -File tests/log-audits/Test-FIAGarrisonLog.ps1 -LogPath <этот полный лог> -Difficulty 2`
— PASS/0, `ROSTER_READY` и `Game destroyed.` присутствуют, SCRIPT E/F, ENGINE F
и VM exceptions отсутствуют; 233 прочих строки (E) сохранены.

Это изолированная навигационная проверка: после READY fixture отключает damage
машин и экипажей, затем включает его для одного убиваемого бойца. В production
неуязвимости нет. Допуск probe 250 м учитывает разворот после порога возврата
200 м; к концу интервала машина обязана находиться внутри 200 м. В финальном
прогоне запас не понадобился. До этой правки `Patrol3/4` наблюдали временный
выход до ~230 м и фактический возврат; новый PASS не выдаётся за жёсткий
физический барьер в 200 м.

Изолированный `CombatFinal`: `compile.ps1 -Label combat-final-fixture -SourceRoot <stage>`
— Workbench PASS/0; `runtime-combat.ps1 -Difficulty Hard -Label CombatFinal`
— native exit 0, штатный launcher с `-aicfGarrisonCombatProbe 1`.
Полный остановленный лог:
`server-CombatFinal/logs/logs_2026-10-08_19-56-49/console.log`.
За 90 с наблюдения расход боеприпасов БТР и танка подтверждён, машины оставались
в пределах 250 м от дома; `GARRISON_FIRE_FINISHED passed=1 btr=1 tank=1`.
Fixture создаёт десять активных неуязвимых пехотных целей; гарнизоны в этом
прогоне обычные, damage им не отключён. Пулемётный огонь подтверждён,
танковая пушка по бронецели не проверялась.
**Полный combat log gate — FAIL**: одна `PMC_EQUIPMENT_FAILED rollback=1`
(сбой нового комплекта, восстановление прежнего вернуло true). Это повтор
наблюдавшейся ранее ошибки, а не новый чистый PASS. VM/null exceptions и
ENGINE F нет; ещё 233 строки (E) — resource baseline.
Сводка: `combat-final-summary.json`; fixture сохранена в evidence отдельно.

Полный offline набор, повтор Medium/Easy runtime после этой правки,
ручная проверка меню, packaged build, долгий soak, JIP, стрельба пушки по
бронетехнике и бой до гибели всего гарнизона — **NOT RUN**.
`git diff --check` — PASS/0. Перечень 20 изменённых файлов:
`changed-files.txt` в evidence. Тестовые процессы завершены через RequestClose;
stage удаляется перед запуском production server/client для ручной проверки.

Игровой запуск после commit `4779d8d`: Hard / EveronNorthWCSRHS, обе роли
через `tools/Start-AICFRuntime.ps1` в отдельных терминальных сессиях.
Helper и manifests сохранены в
`.codex-runtime/fia-local-patrol-play-20261008-195359/`.
Свежие profiles: `C:\Users\retar\AppData\Local\AICF\fia-local-patrol-play-20261008-195359\server`
и соседний `client`. Launcher подтвердил точный CLI, живой PID и ROSTER_READY;
20:03:23 MSK сервер зарегистрировал игрока, client — `Entered online game state`.
`connection-evidence.json` фиксирует процессы 28928/15876, пути полных логов и
10 READY-гарнизонов на сервере. Оба процесса оставлены пользователю по запросу.
Это подтверждение подключения; полный остановленный client log gate и ручная
игровая оценка остаются NOT RUN. Тестовые fixture/stage в production отсутствуют.

## Результаты прежних изменений

Источники — [0.1.25](../releases/0.1.25.md) и
[проверки recruitment](RECRUITMENT_SUPPLY_PLANNING.md). Это сохранённые результаты
предыдущих изменений, не новый прогон текущей задачи.

- `Test-Stage4Static.ps1`: известный **FAIL**, exit 1,
  `STAGE4_ATTACKED_BASES`. Его нужно заново фиксировать в baseline релевантной правки.
- Recruitment supply contracts: PASS с 14 отрицательными мутациями; production/
  fixture Workbench и stock fixture прошли свои targeted gates. Lifecycle и
  stability матрицы после финальных исправлений полностью не повторялись.
- Everon WCS headers/launcher: профильная статика, manifests обеих ролей и
  Workbench — PASS; server/client runtime, JIP, меню и visual — NOT RUN.
- Resource errors и shutdown leaks в старых логах сохранены. Targeted PASS
  не означает полностью чистые engine logs или единую проверку всего релиза.
- Новые WCS/RHS recruitment runtime, полная матрица всех сценариев, client/JIP,
  физическое переполнение stock склада, долгий soak, packaged build и upload
  остаются NOT RUN, пока не появится отдельное evidence.

При новом прогоне зафиксируй фактические failures; старые PASS не наследуются.
Датированное evidence не переписывай задним числом, новый вывод явно привязывай
к новому commit и конфигурации.

## Три ПТ-бойца на Medium/Hard — 2026-10-08

Ветка `codex/infantry-at-balance` от `9b03304`, исходное дерево чистое.
Изменён `AICF_WCSRHSContentProfile.BuildCharacterRoleCandidates`: Medium/Hard
назначают позиции 8/9 тем же ПТ-prefab, что позиция 3. Добавлена fixture
`tests/fixtures/AICF_InfantryATProbe.c`; обновлены README обоих уровней,
GAMEPLAY, указатель, каталог fixtures и эта заметка.
Evidence: `.codex-runtime/infantry-at-balance/`.

`tools/Invoke-AICFChecks.ps1 -Name <семь проверок> -EvidenceRoot <before|after>`:
в обоих случаях **6 PASS, 1 прежний FAIL**, exit 1. WCSIntegration,
RHSIntegration, AILoadout, PersonalLoadoutContracts, InfantryRecruitment,
FIAGarrisonContracts — PASS/0; Stage4 — тот же `STAGE4_ATTACKED_BASES`, FAIL/1.
Полные argv, исходный commit, status и вывод сохранены в before/after.

`compile.ps1 -Label production` и `compile.ps1 -Label fixture -SourceRoot <stage>`
— terminal Workbench **PASS/0**, `Script validation successful`, SCRIPT E/F,
ENGINE F и VM exceptions отсутствуют. По 39 прочих resource строк (E).
Manifest графа, argv и полный вывод находятся рядом с wb-production/wb-fixture.

`runtime.ps1 -Difficulty Hard|Medium|Easy -Label hard1|medium1|easy1` вызывает
canonical launcher с отдельным stage и свежим profile, Variant EveronNorthWCSRHS,
`-aicfATProbe 1 -aicfRequirePlayerForResult 0`. Все три native exit 0,
штатное завершение через RequestClose и `Game destroyed.`. Проверки
Hard **84/84**, Medium **84/84**, Easy **76/76**, failures=0. Полные остановленные
логи: `hard1-logs/`, `medium1-logs/`, `easy1-logs/`; manifest/CLI — в
`server-*-launcher.txt`. `collect-runtime.ps1 -Label <label> -Difficulty 0|1|2`
читает полные логи, сверяет число случаев и ошибки; summary JSON рядом.
SCRIPT E/F, ENGINE F и VM exceptions — 0, прочих (E) по 233 на запуск.

Каждый запуск проверил 24 бойца: два полных roster и четыре one-member donor,
prefab/role соответствие, WCS-комплект, 1/3/3 ПТ на сторону в зависимости от
сложности, фактически заряженный launcher у каждого ПТ. US: M72A3, loaded=1,
spare=0; USSR: RPG7_PGO7, loaded=1, spare=2. Проверены неизменные stock/RHS
role mappings и отказ для FIA. Это проверка выдачи и resolver пополнения,
не полная оплаченная операция набора или проверка боевой эффективности.
`source-hashes.json`: все 221 production файла совпадают с изолированным stage,
fixture hash сохранён отдельно. Stage удалён, своих engine processes не осталось.
`git diff --check` — PASS/0.

Перед runtime по прежнему разрешению пользователя собраны показатели четырёх
серверов предыдущего кода и выполнена адресная остановка в 21:26:05–21:26:06 MSK.
Snapshot: `.codex-runtime/fia-four-servers-20261008-210112/snapshot-20261008-212521/`;
полные остановленные логи — соседний `stopped-logs/`, PID/profile checks —
`stopped.json`. Принудительный native exit -1 не означает crash. Прогон этих
серверов не проверяет новый ПТ-состав.

Стрельба ПТ по БТР/танкам, длительный балансный прогон, полная транзакция
пополнения, client/JIP и ручная визуальная проверка — **NOT RUN**.

## Передача результата

Отчёт содержит outcome и изменённые файлы, branch/commit и dirty status,
команды с exit codes, отдельные verdict, baseline failures и новые regressions,
все `BLOCKED`/`NOT RUN`. Для runtime также нужны версии, scenario/Variant,
точный CLI и `AICF_RUNTIME_MANIFEST_JSON`, profiles, начало/конец и способ остановки,
пути к полным Workbench/server/client logs. Нужное evidence сохраняй в
игнорируемом каталоге задачи, временные copies/stages убирай.

## Восстановление и десант гарнизонов FIA — 2026-10-08

Продолжение `codex/fia-difficulty` от `26122a4`. Evidence этой задачи:
`.codex-runtime/fia-recovery/`. Добавлены `AICF_FIAGarrisonRecovery`,
exact-seat возврат живого экипажа, обычная высадка семи пассажиров БТР,
отдельный controller десанта без spawn, диагностика удержания/переноса.
50 м проверяются через controlled/main player entities у обеих позиций,
при переносе бойца — также у самого бойца. Погибшие не восстанавливаются.

До изменений `before/`, после окончательного production кода `after-final/`:
`tools/Invoke-AICFChecks.ps1 -Name <восемь выбранных проверок> -EvidenceRoot <каталог>`.
В обоих случаях **7 PASS, 1 прежний FAIL**, общий exit 1: AICombatInputContracts,
FIAGarrisonContracts, FIAPatrolStatic, Stage35RecoveryPolicy, Stage35Static,
Stage3Static, Stage3StaticContracts — PASS/0. NorthFailureContracts — FAIL/1,
те же `MOVE_CALLBACK_FENCE`, `MOVE_ACTIVITY`. Точные argv, исходный SHA,
dirty status, exit codes и полный вывод находятся в evidence. Контракт
SURVIVORS обновлён под вызов crew update перед patrol update; новый контракт
не отменяет прежнюю проверку сохранения выживших.

Терминальный Workbench production, `compile.ps1 -Label production-final`
и `compile.ps1 -Variant EveronNorth -Label stock-final`: **PASS/0**, оба
`Script validation successful`, SCRIPT E/F и ENGINE F нет; прочих строк (E)
39/25. Все compile argv и полные логи сохранены, `compile-summary.json`.
API 1.8.0.13: GetInVehicle/GetOutVehicle, GetCompartmentName, GetAllPlayers,
threat state, group transfer, SetWorldTransform и physics velocity проверены
по закреплённому reference и соседним production adapters.

Runtime: `runtime.ps1 -Label probe2`, canonical launcher с отдельным stage,
Variant `EveronNorthWCSRHS`, Hard, `-aicfRecoveryProbe 1`,
`-aicfRequirePlayerForResult 0`. **46/46 PASS**, native exit 0;
20:52:11–20:57:29 MSK. Полный остановленный console.log:
`probe2-logs/logs_2026-10-08_20-52-11/console.log`; сохранены также script/error
logs, launcher manifest и exit. `runtime-summary.json`: SCRIPT E/F 0,
ENGINE F 0, VM exceptions 0, 233 прочих resource/backend строк (E).
`source-hashes-final.json`: 221 production `.c`/`.conf`/`.gproj` совпадают со
stage по SHA-256, fixture присутствует только в stage.

Проверены три роли экипажа БТР и танка, точное повторное занятие каждого места
с последующим переносом машины, отсутствие мутаций при synthetic player veto,
граница 49/50/51 м, destination и height cases, перенос без изменения entity IDs,
высадка семи пассажиров, сохранение трёх членов экипажа, физическое прибытие
БТР после высадки, recovery после failed move, отказ при stale member identity,
отсутствие восстановления/замены убитого водителя.
Probe выключает damage только в изолированной копии и задаёт threat искусственно.
Имитированный player veto не заменяет проверку PlayerManager с реальным клиентом.

`pwsh -File tests/log-audits/Test-FIAGarrisonRecoveryLog.ps1 -LogPath <полный лог>`:
**PASS/0**. Тот же анализатор на копии без PLAYER_VETO_NO_VEHICLE_MUTATION —
ожидаемый **FAIL/1**, вывод `negative-audit.txt`. `git diff --check` — PASS/0.
Ранний `probe1` — **FAIL**, остановлен адресно (native -1): unqualified static
внутренний вызов обходил synthetic override, а общий controller десанта блокировал
новое vehicle movement. В production используется единый qualified PlayersClear;
пешие переведены в собственную identity-проверяемую группу, затем весь probe повторён.
Не выдавать первый прогон за PASS.

Четыре ранее запущенных сервера остановлены по разрешению пользователя после
нового сбора показателей; точный process/profile проверен перед Stop-Process.
Полные остановленные логи: `.codex-runtime/fia-four-servers-20261008-201345/stopped-logs/`.
В конце 25 минут ownership целей US/USSR/FIA: 2301 — 1/2/2, 2303 — 1/2/2,
2305 — 3/2/0, 2307 — 2/3/0. На 2307 зарегистрировано 111 MOVE_FAILED;
на 2303/2305 сохранились две начальные PMC_EQUIPMENT_FAILED, новых SCRIPT E/F нет.
Это старый код, загруженный до правок, и baseline длительного прогона, не проверка
нового recovery. Остановка принудительная, native exit -1; сама по себе не crash.

Реальный connected player рядом с телепортом, client/JIP, визуальная проверка
анимаций, долгий soak нового кода и полный бой до гибели гарнизона — **NOT RUN**.
Все свои тестовые процессы остановлены, stage удалён после сохранения evidence.
Изменены Core state/crew/patrol/service/cleanup, новый recovery, README и
архитектурные/gameplay/testing/fixture заметки, FIAGarrisonContracts,
RecoveryProbe и RecoveryLog; точный список — `changed-files.txt` в evidence.
