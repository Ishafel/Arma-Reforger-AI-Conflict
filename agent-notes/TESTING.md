# Проверки и baseline

## Co-op: обе стороны, карты и integrations — 2026-10-18/19

Ветка `codex/coop-scenario-matrix` от чистого `aeafb17`. Evidence:
`.codex-runtime/coop-matrix-20261018/`. Все три бинарника game/server/Workbench
**1.8.0.13**, `binaries.json`. Production `.c` не менялись.

Добавлен единый `tools/config/AICF_Scenarios.json` и resolver
`AICF_Scenarios.ps1`: девять Variant × Standard/CoopUS/CoopUSSR × три сложности.
Генератор `Update-AICFCoopScenarios.ps1` создаёт **54 Co-op headers** (51 новый,
три прежних Stock USSR сохранены), metadata, локализацию и список native probe.
Вместе со Standard в меню **81** сценарий. Новые Co-op наследуют standard
parent той же сложности; North сохраняет whitelist, RHS/WCS — свой content graph.
Launcher выбирает режим через `-MatchMode`, старый `StockCoopUSSR` остаётся alias.
`standard-compatibility.json`: **9/9** исходных Standard Easy argv совпали
побайтно после замены единственного свежего profile path на placeholder.

Baseline до правки: `Invoke-AICFChecks.ps1 -Name @('Test-CoopScenarioStatic.ps1',
'Test-CoopScenarioLogContracts.ps1','Test-FIARandomGarrisonContracts.ps1',
'Test-RuntimeLauncherStatic.ps1','Test-ScenarioHeadersStatic.ps1')`
с evidence `before/` — **5/5 PASS/0**.

Полный `Invoke-AICFChecks.ps1 -EvidenceRoot <evidence>/offline-all-final`:
**69/70 PASS, runner exit 1**. Три правила WCSIntegrationStatic устарели:
искали IDs/resource непосредственно в launcher после переноса данных в каталог.
Проверка теперь сверяет те же точные dependencies и ресурс через resolver,
а также использование его полей launcher. Ничего не исключено из runner.
Повтор Co-op log/matrix/static и WCS static в `verification-final/` —
**4/4 PASS/0**. `offline-verdict.json` сверяет полное множество текущих checks:
последние результаты **70/70 PASS**, с явным сохранением первоначального FAIL.
Профильные итоги: matrix — **54 headers, 36 launcher manifests, 8 negative catalog
mutations**, synthetic FutureMap; log contracts — **20 positive / 75 negative**;
Co-op static — **39 negative mutations**. Новый каталог защищает также
уникальность generated localization GUID prefixes. Generator `-Check` и
RU/EN runtime localization **662** entries прошли; повторная генерация не меняет файлы.

`CompileAll.ps1` → `Compile.ps1 -SourceRoot <stage> -Variant <variant>`:
terminal Workbench **6/6 PASS/0** для Stock, Everon, RHS, EveronRHS,
ArlandWCSRHS, EveronWCSRHS. Graph/argv взяты из canonical launcher DryRun;
полные logs, validation success и native exit сохранены. После уточнения roster
fixture дополнительный `Compile.ps1 -Label matrix-stability -Variant EveronWCSRHS`
— **PASS/0**. Это компиляция production + test-only fixtures, не packaged build.
`source-integrity.json`: все **414/414** production `.c/.gproj/.conf/.meta/.st/.layout`
совпали со stage. Обе fixture revisions сохранены отдельно:
`fixture-integrity-original.json`, `fixture-integrity.json`, **2/2** совпадения.

Все игровые запуски: `Run.ps1 -Case <case> -Variant <variant> -MatchMode <mode>
-Difficulty Easy -Duration 35000|180000 -Port <port>` → только
`tools/Start-AICFRuntime.ps1`, свежие profiles. Четыре direct-connect клиента
запущены из отдельных сессий с `-Client -Player` после уведомления пользователя;
launcher подтвердил exact CLI, live server PID и ROSTER_READY. Manifests сохранены.

`FinalizeRuntime.ps1` → `Audit.ps1` → `Test-CoopScenarioLog.ps1` по полным
остановленным logs: **20 итоговых серверных и 4 клиентских gates PASS/0**,
native exits 0, SCRIPT/VM/fatal **0** в этих итоговых runs. Server: все девять
Variant на обеих Co-op сторонах плюс Standard Stock и North WCS. Co-op —
**19/19** cases, два территориальных runs — **20/20**, Standard — **16/16**.
North везде имеет семь nodes. Сторона людей — 0 slots, противник — 10;
Standard — 10+10. Helpers, budget, строительство, content mapping, replicated
mode и запрет command summary людей проверены. Это короткая контрактная матрица.

Все четыре реальные client пары (Stock/CoopUS, RHS/CoopUSSR,
EveronNorthWCSRHS/CoopUS, EveronWCSRHS/CoopUSSR) прошли
`-Client -RequirePlayer -RequireWelcome`: только выбранная native faction tile
видима, сервер отверг opponent/FIA requests, персонаж нужной стороны появился.
Каждая пара использует server/client одного запуска. Native resource probe
в North WCS US прошёл `-RequireHeaderMatrix`: **54/54** headers прочитаны через
`MissionHeader.ReadMissionHeader`, mode/difficulty/world/legacy flags и вся
base whitelist совпали с parents. Это не 54 полноценных игровых матча.

`Stock-CoopUSSR` и `EveronRHS-CoopUS` дополнительно прошли `-ExpectedWinner USSR`:
fixture подготовила ownership реальных целей, production VictorySystem завершил матч.
Stage1Diagnostics нормализует RHS runtime faction keys в US/USSR; исходная
ошибка audit ожидала RHS_AFRF в логе. Исправлен именно log contract,
добавлены positive/negative cases; winner resolution в игре не менялся.

**Сохранённый runtime FAIL и открытый blocker:** первый `EveronWCSRHS-CoopUS`
получил SCRIPT(E) `SCR_InstigatorContextData: ... INSTIGATOR_OTHER ... is not supported!`
во время Physics simulation, после чего slot 2 дал GROUP_EMPTY и штатную очередь
восстановления. Точный источник физического повреждения не установлен.
Это native damage context в закреплённом API, ошибка не исправлена и не подавлена.
Чистый `EveronWCSRHS-CoopUS-retry` не доказывает её устранение. Она остаётся
blocker для ready PR/релиза, хотя добавленные сценарии и итоговая матрица работают.

Тот же первый run выявил неверное требование fixture: STABLE_ROSTER ожидал
сохранения всех живых NPC после начала боя. Initial READY/наличие бойцов теперь
проверяются отдельно; стабильность проверяет число, numeric IDs и identity
исходных slot objects, а у людей — ноль managed agents. Потери и replacement
противника допустимы, invincibility/остановка боя не добавлялись. Новая fixture
прошла compile, WCS retry и отдельный Stock-CoopUS-stability — **PASS/0**.
Старый FAIL сохранён целиком. Также исправлен guard анализатора при failed/missing
completion: скобки предотвращают null indexing, два новых negative cases проходят.

Прочие engine `(E)` сохраняются отдельно в `other-engine-errors.json`:
итоговые servers **15–254**, clients **71/124/264/264**. Среди них world Parent,
Hierarchy, resource/font и shutdown RPC diagnostics; общий error-free engine
не заявляется. Полные counts каждого run — `runtime-verdicts.json`.
История неудачных подготовок/проверок — `validation-history.json`: stale runtime
localization и UTF-8 BOM исправлены; первый offline запуск не дошёл до checks
из-за Git safe.directory; прерванный North USSR сохранён и повторён в свежем profile.

**NOT RUN / отклонённые визуальные критерии:** manual UI input и внешний вид
меню недоступны в terminal-only workflow. Действие игрока для отдельной проверки:
открыть оба окна выбора стороны и проверить единственный разрешённый вариант;
native Welcome visibility и RPC readbacks не доказывают внешний вид Deployment.
Полные Medium/Hard кампании, физический бой до победы, законченные supply/build
операции, покупка/найм RHS/WCS/Everon, смерть/повторный respawn, save/load,
packaged/Workshop build и долгий soak здесь не запускались. Прежняя проверка
stock покупки/найма не превращается в новый PASS для модов.

Исходный и финальный stage архивированы с SHA-256 и адресно убраны;
игровые/Workbench процессы задачи завершены. Полные logs/argv/manifests/exits
сохранены. Изменены: Co-op MissionHeader/meta шести integrations, три файла
локализации Core, каталог/resolver/генератор/launcher, Co-op fixtures/log/static
contracts, WCS static, README и соответствующие agent-notes.
Push, PR, merge и публикация не выполнялись.

## Казармы во всех режимах и support station area — 2026-10-18

Ветка `codex/barracks-support-zone` от чистого `d5c8f90`. Evidence:
`.codex-runtime/barracks-support-zone-20261018/`; game/server/Workbench
**1.8.0.13**, `binaries.json`. Это продолжение пользовательских исправлений
Arland; исходный Everon WCS Co-op из issue #46 не объявляется реализованным.

До production правки:
`Invoke-AICFChecks.ps1 -Name @('Test-CoopScenarioStatic.ps1',
'Test-CoopScenarioLogContracts.ps1','Test-SquadCommandsContracts.ps1',
'Test-ConstructionStatic.ps1') -EvidenceRoot <evidence>/before` — **4/4 PASS/0**.
Первая передача имён внешнему PowerShell как одной строки отклонена до аудитов;
запуск с массивом выполнил все четыре. Baseline Workbench и диагностические
graphs `baseline`, `lookup`, `vehicles` — **PASS/0**.

**Причина runtime error установлена по реальным repair trucks:** на сервере
visual child станции имеет точное `<0,0,-2.5>`, на клиенте после сетевого spawn
local position уже отличается, до первого GenerateAreaMesh. В `vehicles-before`
штатный Init дал три SCRIPT(E): FIA Ural `<-0.00286865,0.0183182,-2.49963>`,
USSR Ural `<0.00140381,0.00782013,-2.49927>`, M923
`<-0.00354004,-0.00318909,-2.5014>`. Точный механизм округления внутри движка
не измерялся. Это проблема клиентской привязки visual child, а не доказанный
дефект авторинга prefab, как слишком категорично говорилось в старом отчёте.
Первый обычный повтор `baseline` дал 0/0 SCRIPT(E), поэтому отсутствие ошибки
в случайном запуске не использовалось как доказательство исправления.
`baseline-runtime.json` сохраняет все полные stopped logs, counts и native exits.

Production `AICF_SupportStationArea` восстанавливает local translation visual
child из offset непосредственного parent station до native Init и генерации.
Сам station/vehicle, offset/range и gameplay state не меняются; native validation
и enabled lifecycle остаются. Логи не фильтруют эту ошибку.
`AICF_CoopRecruitment` переименован в общий `AICF_BarracksRecruitment`: снят
STANDARD opt-out во всех трёх gates. Разрешение остаётся только у группы,
заказанной текущим игроком через troop provider своей разрешённой фракции;
async membership/identity, native лидерство, ownership и cap сохраняются.

`Compile.ps1 -Label production-stock`, `-Variant EveronRHS -Label production-rhs`,
`-Variant EveronWCSRHS -Label production-wcs` — terminal Workbench **3/3 PASS/0**,
Script validation successful, SCRIPT/VM/fatal 0. Graph берётся из canonical
launcher DryRun, exact argv/manifest/full logs сохранены. Финальный fixture
graph `fixed-fixture2` — **PASS/0**. Первая `fixed-fixture` — **FAIL/-1** из-за
дублирования тестовых файлов в Integration и UI при подготовке stage; дубли
удалены, production дефекта здесь не было. Неудачный compile log сохранён.
`source-integrity.json`: **233/233** production `.c/.gproj/.conf/.meta` Core/Arland
совпали со stage; `fixture-integrity.json`: **3/3** точные tracked fixtures.

Все игровые запуски — только `Run.ps1` → `tools/Start-AICFRuntime.ps1`, server
и client в отдельных сессиях, свежие profiles. Перед каждым видимым client
пользователь уведомлён, что тест автоматический и действий не требует.
Exact CLI/live PID/ROSTER_READY подтверждены launcher, manifests сохранены.

| Run | Server/client strict gates | Native roster checks | Script errors | Прочие `(E)` server/client |
|---|---|---|---|---|
| `fixed-coop`, port 2430 | **PASS/0 + PASS/0** | **17/17**, US 10 / USSR 0 | **0/0** | 16/78 |
| `fixed-standard-ussr`, port 2431 | **PASS/0 + PASS/0** | **14/14**, 10+10 | **0/0** | 24/79 |
| `fixed-standard-us`, port 2432 | **PASS/0 + PASS/0** | **14/14**, 10+10 | **0/0** | 25/81 |

Команды: `Run.ps1 -Case <run> -Variant StockCoopUSSR|Stock -Faction USSR|US
-Duration 130000|140000 -SourceRoot <evidence>/fixed-stage -Port <port>` и
отдельно те же аргументы с `-Client -Player`. `Audit.ps1 -Case <run>
[-Faction US]` выполняет по обеим ролям
`Test-CoopScenarioLog.ps1 -RequireBarracks -RequireSupportArea
[-BarracksFaction US]` на полных остановленных logs с native exits **0**.
`runtime-verdicts.json`: все **6/6 PASS/0**; это три успешные пары, а не
объединение ролей из разных запусков. Указанный выше stock area blocker снят
этим новым evidence; прежние FAIL остаются историческими FAIL.

В каждом run: native покупка Guard за **300** supplies; реальный найм **4/4**,
native cap **4**, неоплаченные Guard **4/4** сохранили `Recruitable=0`.
Цена и balance проверены native consumer: Co-op **685→385**,
STANDARD USSR **645→345**, STANDARD US **665→365**. Суммы raw debit/credit
включают пересборку агрегата, это не отдельные денежные транзакции.
Все три клиентские зоны в каждом run пришли с исходным расхождением и получили
точный local offset, `center_error=0`, неизменный station transform/range и
созданную native mesh. Остальные resource/backend/world/shutdown `(E)` не
устранялись и сохранены; это не заявление о полностью чистом engine log.

Полный `Invoke-AICFChecks.ps1 -EvidenceRoot <evidence>/offline-final` —
**69/69 PASS/0**, runner exit **0**, без исключений. `offline-verdict.json`
сверяет точное множество checks с текущим полным набором.
Профильные проверки: Co-op static **39** negative mutations, support area static
**9** negative mutations, log contracts **14 positive / 63 negative**.

**NOT RUN:** ручной UI input/внешний вид — запрещены terminal-only workflow,
native readbacks их не заменяют. Runtime покупки RHS/WCS/Everon, save/load,
packaged/Workshop build и долгий soak не запускались; RHS/WCS подтверждены
компиляцией, а не такой runtime матрицей.

Тестовые game/Workbench процессы завершены. Три task stages архивированы
с SHA-256 (`stage-archives.json`) и адресно удалены. Для повторного запуска
нужно восстановить соответствующий source ZIP и использовать свежие profiles;
полные logs/argv/manifests/native exits сохранены в evidence.
Изменённые файлы:

- Production `Integration/AICF_BarracksRecruitment.c` (rename прежнего
  `AICF_CoopRecruitment.c`) и `Integration/AICF_SupportStationArea.c`.
- `tests/fixtures/AICF_CoopScenarioProbe.c`, `AICF_CoopBarracksProbe.c`,
  `AICF_SupportStationAreaProbe.c`; `tests/static/Test-CoopScenarioStatic.ps1`,
  `Test-SupportStationAreaStatic.ps1`; `tests/log-audits/Test-CoopScenarioLog.ps1`;
  `tests/contracts/Test-CoopScenarioLogContracts.ps1`.
- `README.md`; `agent-notes/ARCHITECTURE.md`, `COOP_SCENARIO.md`, `GAMEPLAY.md`,
  `README.md`, [SUPPORT_STATION_AREA.md](SUPPORT_STATION_AREA.md), `TESTING.md`,
  `TEST_FIXTURES.md`.

Push, PR, merge и публикация не выполнялись.

## Co-op: найм оплаченного отделения Guard из казармы — 2026-10-18

Продолжение `codex/arland-coop-ussr` от чистого `22410e7`. Evidence:
`.codex-runtime/coop-barracks-recruit-20261018/`. Game/Server/Workbench
**1.8.0.13**, `binaries.json`. Перед production правкой релевантный
`Invoke-AICFChecks.ps1 -Name Test-CoopScenarioStatic.ps1,Test-CoopScenarioLogContracts.ps1,Test-SquadCommandsContracts.ps1,Test-RuntimeLauncherStatic.ps1 -EvidenceRoot <evidence>/before`
— **4/4 PASS/0**.

Установленная причина: `Group_USSR_FireGroup_Guard` создаёт четыре
`Character_USSR_*_Guard` с `Recruitable=0`; обычные FireGroup/ManeuverGroup
создают recruitable characters. Предыдущий probe использовал другой GROUP
catalog и проверил только одного обычного бойца. Он не проверял Guard из
реального списка казармы, поэтому этот дефект остался незамеченным.
Серверный `source-prefabs` подтвердил различие prefab flags. `native-before`
прошёл настоящий заказ Guard через native building RPC: лидер 1, slave 0,
max AI 4, но native Recruit вернул отказ с recruitable 0. Это намеренный
baseline отказ, не PASS исправления; при раннем закрытии editor клиент также
получил две cleanup SCRIPT(E), исправленные в lifecycle финальной fixture.

Production `Integration/AICF_CoopRecruitment.c` разрешает найм только группе,
созданной native player order на human side Co-op, после повторной проверки
authority, provider, controlled player и faction. Native callback faction
у группы **null**: она не имеет FactionAffiliationComponent. Фракция берётся
через `SCR_AIGroup.GetFaction`, ненулевое несовпадение отклоняется. Marker
конкретной группы распространяется на готовых и async members с повторной
parent/faction проверкой. `SetRecruitable` вызывается только при изменении;
native лидерство, ownership RPC, лимит и `SetRecruited` не подменяются.

Терминальные compile gates:

- `Compile.ps1 -Label production-final` — **PASS/0**, production без fixtures.
- `Compile.ps1 -SourceRoot <acceptance-stage> -Label acceptance` и
  `-SourceRoot <standard-stage> -Label standard-regression` — **PASS/0**.
- Co-op static — **38 negative mutations PASS**; log contracts —
  **8 positive / 52 negative PASS**. Полный
  `Invoke-AICFChecks.ps1 -EvidenceRoot <evidence>/offline-final` — **68/68 PASS/0**,
  без исключений; `<evidence>/offline-final/summary.json`.

Runtime запускается только `Run.ps1` → `tools/Start-AICFRuntime.ps1` из отдельных
терминальных сессий. Для `native-final`/`acceptance2` — `StockCoopUSSR`, Easy,
170000/180000 ms, client `-Player`, подтверждённые listeners 2423/2425.
Client launcher проверил свежий CLI, PID и ROSTER_READY; manifests сохранены.

- `native-final`: реальная покупка Guard, четыре последовательных native
  `RequestAddAIAgent`, readback recruited/membership и AIAgent parent на сервере;
  native limit 4 сохранён, отдельные неоплаченные Guard остались unrecruitable.
  Оба полных stopped logs имеют **0 script errors**, 16 server / 78 client
  прочих `(E)`. Client `Test-CoopScenarioLog.ps1 -Client -RequireBarracks` —
  **PASS/0**. Server strict gate — **FAIL/1** из-за ошибочного чтения цены 0
  в ранней fixture; это не оплата 0 в игре и не зелёная пара gates.
- Цена Guard берётся через `SCR_EditableGroupUIInfo.GetGroupBudgetCost`:
  обычный `GetEntityBudgetCost` намеренно возвращает пустой список для группы.
  `acceptance2` подтвердил quote **300**, supplies **625 → 325**, всю покупку,
  найм **4/4**, cap **4** и неизменённых неоплаченных Guard. Resource invoker
  фиксирует изменения агрегата, включая пересборку графа: `debited=925` и
  `credited=625` дают net 300, это не журнал валовых денежных транзакций.
  Server strict `-RequireBarracks` — **PASS/0**, script errors 0.
- **Открытый runtime blocker:** `acceptance2` client strict gate — **FAIL/1**:
  stock `SCR_SupportStationAreaMeshComponent` для REPAIR в `<2265.115967,
  88.860016,1216.567871>` сравнивает local offset `<0.000610352,-0.00376129,
  -2.50098>` с `<0,0,-2.5>` точным равенством. Полный client log содержит одну
  SCRIPT(E); отказ не исключён из анализатора. Дефект stock asset/float precision
  ранее уже наблюдался и в этой задаче не исправлен. Финальная **пара** strict
  barracks gates не объявляется PASS; ready PR/release этим отчётом не разрешён.
- `acceptance` ранее упал **до покупки** с `DXGI_ERROR_DEVICE_HUNG` и native
  exit `-1073741819`. Полные console/error logs и dump сохранены, PASS не заявлен.
- `Run.ps1 -Case standard-regression -Variant Stock -Duration 45000 -Port 2426`:
  **14/14**, roster **10+10**, full stopped audit **PASS/0**, script errors 0.
  Дополнительная попытка `AICF_AllowPurchasedRecruitment` в STANDARD не меняет
  Guard flags: отдельный readback **4/4 остаются unrecruitable**.

`runtime-verdicts.json` и `*-audit.txt` содержат все verdicts, без объединения
логов разных запусков в вымышленную успешную пару. `source-integrity.json`
подтвердил **235/235** одинаковых production Core/Arland файлов для
native-final, acceptance и STANDARD; hash barracks fixture совпал. Все 12
task stages адресно архивированы и удалены; ZIP SHA-256 — `stage-archives.json`.
Тестовых игровых процессов нет. Неудачные compile/diagnostic runs сохранены,
включая неверный BaseGameMode cast, имя fixture variable `owned`, prototype
parameter names и занятый listener; их исправление подтверждено новыми gates.

**NOT RUN:** ручной UI input/визуальная оценка игроком (terminal-only workflow),
новый packaged/Workshop build, RHS/WCS/Everon, длительный soak и сохранение/
загрузка кампании. Они не выдаются за PASS. Новые client runs заранее объяснялись
пользователю как автоматические; участия игрока в этих проверках не требовалось.

Изменены Core `AICF_CoopRecruitment.c`; `AICF_CoopBarracksProbe.c`, Co-op
static/log audit/log contracts; README и agent-notes ARCHITECTURE,
COOP_SCENARIO, README, TEST_FIXTURES, TESTING. Push/PR/merge/публикация не выполнялись.

## Co-op: стартовый список фракций, группы игроков и найм — 2026-10-18

Продолжение `codex/arland-coop-ussr` от `279e2d2`, начальное дерево чистое.
Evidence: `.codex-runtime/coop-join-recruit-20261018/`; binaries game/server/
Workbench **1.8.0.13**, `binaries.json`. Пользователь подтвердил заказ отделения
из списка казармы и отсутствие прав лидера; его снимки и симптомы не заменяют
терминальный native readback.

Production: WelcomeScreen скрывает US/FIA после native создания tiles,
сохраняя соответствие sorted indices. Co-op явно разрешает native меню групп
и выдвижение в лидеры; server join/create страхует отсутствие membership после
faction callbacks и не меняет существующую группу. STANDARD делегирует native
policy; лимит ИИ и проверки найма не ослаблялись. В обычном native UI контроль
уже получал группу и лидера: гипотеза о безусловно сломанном автовступлении
не подтвердилась. Исправление не приписывает этому единственную причину отказа.

Свежий `Invoke-AICFChecks.ps1 -Name Test-CoopScenarioStatic.ps1,
Test-SquadCommandsContracts.ps1,Test-SquadRespawnContracts.ps1,
Test-VictoryRespawnStatic.ps1 -EvidenceRoot <evidence>/baseline` — **4/4 PASS/0**.
Первое ошибочное имя check было отклонено runner до выполнения, не тестовый FAIL.
Полный `Invoke-AICFChecks.ps1 -EvidenceRoot <evidence>/offline-final` —
**68/68 PASS/0**, без исключений; `offline-verdict.json`.
После расширения log gate отдельно повторены Co-op static и log contracts:
`-Name Test-CoopScenarioStatic.ps1,Test-CoopScenarioLogContracts.ps1
-EvidenceRoot <evidence>/leader-contracts` — **2/2 PASS/0**;
**29 negative static mutations**, **5 positive / 37 negative log contracts**.
Последнее расширение log gate проверено этим отдельным повтором; production
после полного runner не менялся. Это не второй чистый полный запуск.

`Compile.ps1 -Label final-production`, `-SourceRoot <evidence>/stage
-Label leader-fixture` и `-SourceRoot <evidence>/standard-stage
-Label standard-control` — **Workbench PASS/0**, `Script validation successful`,
script/VM/fatal ошибок нет. Exact argv, stdout, exit и полные logs — `wb-*`.
`production-stage-hashes.json`: **234/234** production Core/Arland файлов
совпадают с обоими stage. Fixtures в production отсутствуют.

`Run.ps1 -Case leader-final -Duration 160000` и отдельная клиентская сессия
с `-Case leader-final -Client -Player -Duration 160000` используют только
canonical launcher, fresh profiles и listener 2412. Клиент прошёл точный
CLI/live PID/ROSTER_READY gate. Native intro callbacks дали US=hidden,
USSR=visible, FIA=hidden. Fixture сняла лидерство на сервере; клиент из состояния
`leader=0` отправил **штатное голосование**, сервер подтвердил нового лидера.
Затем `CanBeExecuted` native Recruit разрешил бойца, клиент отправил
`RequestAddAIAgent`, сервер подтвердил IsRecruited + slave membership + реальный
parent AIAgent, клиент подтвердил membership. Ни оплаты, ни штатных authority
проверок fixture не подменяет; NPC подготовлен из stock GROUP catalog.

Оба `Test-CoopScenarioLog.ps1 -RequireRecruit` по **полным остановленным** logs
— **PASS/0**, server **17/17** scenario cases; script errors **0/0**, прочие
native `(E)` **16 server / 71 client**. Commands, manifest, native exit 0,
full logs и `leader-final-*-audit.txt` сохранены. Предыдущие `native-recruit3`
и `final` также подтвердили native найм; последний gate дополнительно требует
непосредственно голосование из состояния без лидера.

Обычный сценарий: `Run.ps1 -Case standard-control -Variant Stock -Duration
30000 -SourceRoot <evidence>/standard-stage -Port 2413` — **14/14**, roster
10+10, native exit 0, полный `Test-CoopScenarioLog.ps1` **PASS/0**, script errors
**0**, прочие native `(E)` **16**. Сценарии не делили listener.

Диагностические неудачи сохранены, не объявляются PASS: первые fixtures имели
ошибки сигнатуры/порядка загрузки; ранний spawn без выбранного native loadout
дал VM в stock CanPlayerSpawn_S; исходный purchase probe не дошёл до покупки,
а выбор старого catalog spawner не соответствовал доступным services. Один
контроль был запущен до освобождения 2412: server exit 0 без ROSTER_READY,
client readiness корректно отказал. В ранних runs встречалась stock
SupportStationAreaMesh offset SCRIPT(E) при auto construction США. Этот дефект
asset/precision не исправлялся: финальная recruitment fixture останавливает
auto construction для изоляции, не доказывая отсутствие этой диагностики в
полном матче. Свежий обычный Stock control прошёл без SCRIPT(E).
Снимки прежнего пользовательского матча — `manual-*-logs/`, это не stopped gate.

**NOT RUN:** внешний вид и ручной input меню — workflow запрещает GUI automation;
проверено native состояние widgets, не визуальная раскладка. Повторная оплата и
создание полного отделения через интерфейс казармы не проверялись; это не новый
PASS оплаты. Также не выполнялись бой до победы, respawn после смерти, RHS/WCS,
packaged build/Workshop и длительный soak. Общий engine error-free gate не
заявляется. Push, merge, PR и публикация не выполнялись.

Изменённые файлы: production
`UI/AICF_CoopFactionMenu.c`, `Integration/AICF_CoopPlayerGroups.c`;
`tests/fixtures/AICF_CoopRecruitProbe.c`,
`tests/static/Test-CoopScenarioStatic.ps1`,
`tests/log-audits/Test-CoopScenarioLog.ps1`,
`tests/contracts/Test-CoopScenarioLogContracts.ps1`;
`README.md`, `agent-notes/README.md`, `ARCHITECTURE.md`, `COOP_SCENARIO.md`,
`TEST_FIXTURES.md` и этот отчёт.
`git diff --check` — **PASS/0**. Тестовые native процессы завершены; оба stage
сохранены в source ZIP с SHA-256 (`source-archives.json`) и удалены адресно.
Полные profiles/logs, manifests, команды, native exits и индекс неудачных
попыток оставлены в evidence. Игровой сеанс по запросу пользователя запускается
отдельно из production без fixtures и не является stopped runtime gate.

## Arland Co-op: выбор фракции, строительство и Muzzle lifetime — 2026-10-18

Продолжение `codex/arland-coop-ussr` от `e73da70`, начальное дерево чистое.
Evidence: `.codex-runtime/arland-coop-followup/`; game/server/Workbench —
**1.8.0.13**, `binaries.json`. Production изменения: policy player access и
auto construction, фильтр native faction menu, authority request/assignment
guards и локальная muzzle reference в EquipmentPlan.Build.
Workers, native IsPlayable/HQ/opponents и gameplay entity cleanup не меняются.

Свежий selected baseline через `Invoke-AICFChecks.ps1 -Name ... -EvidenceRoot
<root>/baseline` — **6/6 PASS/0**: Co-op, construction, builders, equipment,
match shutdown и shutdown log contracts. Первый вызов с ошибочным именем audit
отклонён runner до проверок; это не тестовый FAIL.
`access-after` — **4/4 PASS/0**, `access-ui-after` — **2/2 PASS/0**.
После защиты native IsPlayable отдельно повторён Co-op static — **PASS/0**,
19 negative mutations, `coop-static-final2.txt`.
Полный `Invoke-AICFChecks.ps1 -EvidenceRoot <root>/offline-final` —
**68/68 PASS/0**, без исключений; `offline-final-verdict.json`.
Последнее усиление только Co-op static (запрет глобального IsPlayable override)
проверено отдельным повтором с 19 mutations выше; production после full runner
не менялся. Equipment contracts: 22 rules / 19 negative mutations; Co-op log
contracts: 3 positive / 20 negative; shutdown log: 4 positive / 35 negative.

Терминальный `Compile.ps1 -Label production-final` без fixtures и
`-SourceRoot <root>/stage -Label final-fixture`, а также
`-SourceRoot <root>/shutdown-stage -Label shutdown-plan-batches` — **PASS/0**:
`Script validation successful`, script/VM/fatal errors нет. Первые два
sandbox Workbench attempts завершились `-1 / SteamAPI_Init failed`, validation
не завершена; разрешённые повторные terminal runs дали PASS. Все logs сохранены.
`final-source-hashes.json`: **233/233** production Core/Arland файлов совпадают
с Co-op stage; test-only fixture проверяется отдельным SHA-256.

Runtime запускается только canonical launcher из отдельных server/client
сессий. `Run.ps1 -Case coop-final -Difficulty Hard -Duration 150000 -Winner US`:
**18/18 cases**, native exit 0, full stopped log audit **PASS/0**,
script errors **0**, прочие native `(E)` **16**. Client `-Client -Player`
прошёл exact CLI/live PID/ROSTER_READY gate, получил authority отказ US и FIA,
затем вступил в СССР и получил controlled character. Его строгий
`Test-CoopScenarioLog.ps1 -Client -RequirePlayer` — **PASS/0**, script errors **0**,
прочие native `(E)` **86**. Это initial spawn и native intents, не ручной UI input.
В runtime подтверждён допуск auto construction только США при работающем
BaseBuilderService; законченная player construction операция не заявляется.

`Run.ps1 -Case standard-final -Variant Stock -Difficulty Medium -Duration 30000
-Winner USSR`: **15/15 cases**, roster 10+10, native HQ/IsPlayable сохранены,
production territorial winner USSR, full stopped audit **PASS/0**, script errors
**0**, прочие native `(E)` **16**. `muzzle-local` — первый контроль минимальной
правки, **18/18**, full stopped audit **PASS/0**, script errors **0**.

`RunShutdown.ps1` / `shutdown-fixed`: три пакета по 8 Draft операций плюс
**24/24 EquipmentPlan Build/destructor**. Full `Test-ShutdownLog.ps1 -Batches
-RequireStop` — **PASS/0**, script errors и leaked objects **0**; native exit 0.
Через 15 секунд покоя **0 из 2160** sampled entities живы; после Stop —
**0 из 2250**, drafts **0**, created=destroyed=**50**.

Причина Muzzle подтверждена контролем `shutdown-mutant`: в отдельном stage
возвращён только inline-вызов Muzzle в условие for, все остальные source files
совпадают (`mutant-source-comparison.json`). Compile — **PASS/0**, native exit 0,
но full runtime audit — **FAIL/1**: **41 script errors / 128 leaked objects**,
неполные operation/plan records и completion. Этот FAIL ожидаемо доказывает,
что расширенный native gate отклоняет дефект; он не является production PASS.
Прежние Draft batches не вызывали Plan.Build/destructor в каждой операции;
обычная остановка OnGameEnd не доказывала раннюю Stop(true). Старые результаты
ниже сохраняются как исторические, а не подтверждение этого пути.

Все диагностические неудачи сохранены: перестановка roots, ранний unarm,
transient plan и очистка до EndGameMode не устранили удержание. Первая попытка
ограничить глобальный IsPlayable нарушила native HQ/opponent setup; вариант
полностью исключён, запрет перенесён в меню и authority. Индекс —
`runtime-attempts.json`; production не содержит этих экспериментов.

**NOT RUN:** визуальный осмотр меню/HUD и ручной input — terminal-only workflow
запрещает GUI automation; нужен игрок, который открывает выбор фракции и
проверяет единственный вариант СССР. Static UI contracts и authority RPC
readbacks не выдаются за визуальный PASS. Также не проверялись физический бой
до победы, законченная доставка/строительство, смерть и повторный respawn,
новые RHS/WCS runtime runs, packaged build/Workshop и длительный soak.
Команды, manifests, exit codes и полные stopped logs находятся в evidence;
resource/asset diagnostics не исключены из сохранённых logs, общий engine
error-free gate не заявляется. Push, merge, PR и публикация не выполнялись.
Собственные native процессы завершены; три известных stage-каталога сохранены
в source zip с SHA-256 и удалены. Полные profiles/logs оставлены в evidence.
`git diff --check` — **PASS/0**; проверены 356 локальных Markdown file links,
битых нет. Список изменённых файлов — `changed-files.txt` в evidence.

## Ванильный Arland Co-op СССР — 2026-10-18

Ветка `codex/arland-coop-ussr` от актуального `origin/main`
`bb081ebae639beea09a6c85c10be9cc0bbeae38a`; начальное дерево чистое.
Добавлены три inherited headers, Core match policy, пустой roster стороны
игроков, per-faction ticket defeat, replicated mode и UI gate.
Контракты и запуск — [COOP_SCENARIO.md](COOP_SCENARIO.md).
Evidence: `.codex-runtime/arland-coop/`. Binaries game/server/Workbench —
**1.8.0.13**, версии сохранены в `binaries.json`.

**BLOCKER:** полный runtime gate завершения матча остаётся **FAIL**.
После успешной production территориальной победы `AICF_EquipmentDraft.Clear`
выдаёт `Can't delete instance (of class: 'MuzzleComponent') with 1 references`,
VM exception и leaked native classes. Это воспроизводится также в обычном
Stock / STANDARD на данной ветке; production equipment cleanup не изменялся.
Отдельный контроль на неизменённом `bb081eb` воспроизвёл ту же ошибку (ниже).
Это сохранённый baseline FAIL, а не разрешённое исключение: Co-op не готов к ready PR.

Baseline `Invoke-AICFChecks.ps1 -Name <шесть профильных audits>
-EvidenceRoot .codex-runtime/arland-coop/targeted-before` — **6/6 PASS/0**:
AICommanderModeStatic, ConstructionStatic, RuntimeLauncherStatic,
ScenarioHeadersStatic, Stage35Static, VictoryRespawnStatic. Baseline failures нет.
Избыточный полный `offline-before` остановлен адресно; его неполный результат
не является полным baseline (`NOT-COMPLETE.txt`, список PID сохранён).

Первый полный `offline-after1` — **65 PASS / 2 FAIL из 67**, exit 1:
RuntimeLauncher требовал явных legacy tank/passenger flags в новых headers;
Stage4 требовал hardcoded десять slots в цикле создания faction state.
В headers добавлены нулевые flags, Stage4 проверяет предусмотренное scenario
число slots, а новый Co-op audit отдельно сохраняет STANDARD=10+10.
Проверки не исключались. `Invoke-AICFChecks.ps1 -Suite Static
-EvidenceRoot <root>/static-final` — **27/27 PASS/0**.
`-Name <CoopScenarioLogContracts,RuntimeLauncherStatic,Stage4Static>
-EvidenceRoot <root>/offline-corrections` — **3/3 PASS/0**.
`latest-offline-results.json` содержит актуальные **68/68 PASS** по объединённым
результатам этих запусков; это не один чистый полный запуск.

Окончательный полный `Invoke-AICFChecks.ps1 -EvidenceRoot
.codex-runtime/arland-coop/offline-final` — **68/68 PASS/0**, без исключений.
После расширения player readback отдельно повторён Co-op log contract —
**3 positive / 18 negative PASS/0**, `coop-log-contracts-final2.txt`.
Изменение не затрагивало production source после полного runner.

`Compile.ps1 -Label production-final` и
`Compile.ps1 -SourceRoot <root>/stage -Label fixture-final` — **PASS/0**,
`Script validation successful`, script/VM/fatal ошибок нет.
Exact argv, stdout, exits и полные logs — `wb-*-argv.json`, `wb-*`.
`final-source-hashes.json`: **229/229** production файлов Core/Arland совпадают
со stage, единственное дополнительное `.c` — test-only Co-op fixture в UI.

`Run.ps1` вызывает только canonical launcher, fresh profiles, порт 2411.
Сервер и client запускаются отдельными терминальными сессиями; launcher
подтверждает CLI, живой PID и ROSTER_READY до подключения. Commands, manifests
и exits — `<case>-{server,client}-{command.json,launcher.txt,exit.txt}`;
полные stopped logs — `profile-<case>-<role>/logs/logs_*/console.log`.

Первый `easy` server — **15/15 cases**, 120 секунд наблюдения, native exit 0,
полный `Test-CoopScenarioLog.ps1` **PASS/0**. Connected JIP client — native exit 0,
replicated readback **PASS**, полный log audit **PASS/0**. Script errors — 0;
прочие native `(E)` — 16 server / 82 client, общий engine/assets error-free gate
не заявляется. Это наблюдение bootstrap/JIP, не завершение боя.

`medium3` и `hard-final` проверяют реальные headers и готовность 10/0 slots,
нулевой SSR roster, budget 100, отсутствие ticket defeat СССР, helpers и
production territorial winner US / USSR соответственно: по **16/16 cases**.
Ownership целей подготавливает fixture. Оба **full runtime gates — FAIL/1**:
10 script errors после COOP_DONE, несмотря на native exit 0 / Game destroyed.
`audit-medium3-server.txt`, `audit-hard-final-server.txt` сохраняют отказ.
Нельзя выдавать положительные cases за полный runtime PASS.

Финальный `easy-final-server` — **15/15**, 90 секунд, полный log audit **PASS/0**.
`easy-final-client` подтвердил snapshot, но полный audit — **FAIL/1**:
одна stock `SCR_SupportStationAreaMeshComponent` SCRIPT(E) об offset REPAIR зоны.
Эта строка не исключена из gate; native prefab не изменялся.
Следующий `player-final`: server **15/15**, 180 секунд, полный audit **PASS/0**;
connected client штатно вступил в СССР, запросил native respawn и получил
controlled character своей фракции. `COOP_CLIENT_PLAYER pass=1` и JIP snapshot
с 0 managed SSR agents подтверждены. Строгий audit `-Client -RequirePlayer`
требует точный manifest flag и native readback — **PASS/0**. Script errors 0;
прочие native `(E)` — 16 server / 87 client. Это initial spawn, не смерть/повторный respawn.
`standard-final`: **12/12**, 90 секунд, roster 10+10, floor 200,
обычная остановка и полный audit **PASS/0**. Все native exits — 0.

Контроль ошибки завершения: `git archive bb081eb` создал отдельный
`baseline-source.tar`; в baseline stage добавлена только `BaselineEndProbe.c`
без Co-op API. `Compile.ps1 -SourceRoot <baseline-stage> -Label baseline-control`
— **PASS/0**. `RunBaseline.ps1` вызывает canonical launcher Stock, тот же graph
и версии: `[BASELINE_END_CASE] ended=1 us_slots=10 ussr_slots=10`, native exit 0,
но **10 SCRIPT(E), MuzzleComponent VM и leaks**, обычный full runtime gate — **FAIL**.
Exact archive hash, full stopped log и verdict — `baseline-runtime-verdict.json`.
Первый запуск был отклонён по тайм-ауту auto-review до старта процесса;
разрешённый повтор выполнен. Это не runtime failure запуска до его исполнения.

Все неудачные attempts сохранены. Fixture сначала загружалась раньше campaign
state и не компилировалась; перенесена в UI. Исправлены отдельный owner victory
и порядок явного fixture cleanup. Первое изменение PowerShell audit потеряло
BOM и дало parser error; BOM восстановлен. Эксперименты cleanup затронули только
stage: late weapon action, selected weapon release, private AI controller,
local delete, освобождение native collections и nested attachments. Они не
дали доказанного исправления и полностью удалены из финального stage.
`medium4` с экспериментальным SelectWeapon(null) — native access violation
**-1073741819**, это FAIL, dump и crash logs сохранены.
Остальные failed runs имеют полные logs; `runtime-attempts.json` — индекс.

**NOT RUN:** визуальная оценка пунктов меню/HUD и ручной input игрока — требуется
человек, GUI workflow запрещён; физический захват всех целей игроками;
завершённая доставка припасов и достройка layout в Co-op; смерть и повторный respawn игрока
pipeline, packaged build/Workshop, долгий soak. Наличие services не объявляется
доказательством законченной доставки или строительства. Нет нового релиза,
push, merge или готового PR.

Для ручного UI smoke требуется выбрать Co-op СССР в меню, вступить в СССР,
открыть карту и проверить отсутствие панели боевых отрядов при доступном
снабжении. Эта визуальная проверка агенту недоступна по terminal-only правилам.
`git diff --check` — **PASS/0**, проверены 210 локальных Markdown-ссылок, битых нет.
Полный список 35 изменённых файлов — `changed-files.txt` в evidence.
Тестовые процессы завершены, собственные `stage` / `baseline-stage` удалены;
логи, profiles, baseline archive и hashes сохранены (`cleanup.txt`).

## Defend allocation dialog — продолжение PR #64, 2026-10-16

После игрового запуска North WCS Hard на `8d199ec` server дважды выдал
`Defend Waypoint not provided to the node!` (13:45:53 и 13:45:59). Снимки полных
живых logs и stack сохранены в `.codex-runtime/everon-north-hard-wcs-20261016-1337/`.
`Debug.Error` вызывается native allocation node, а прежний guard относится к
другому BT node. Контракт: [DEFEND_ALLOCATION.md](DEFEND_ALLOCATION.md).

Изолированный checkout — `.codex-runtime/defend-allocate/checkout`, ветка
`codex/defend-allocate-waypoint` от head текущего PR `8d199ec`. Файлы работающего
матча в основном checkout не редактируются. Production изменён только
`Forces/AICF_AINodeLifecycle.c`: invalid allocation input управляемой server
infantry завершается FAIL с bounded warning; native valid/unmanaged delegate
сохранён. Guard не меняет waypoint, activity или callbacks.

Свежий selected baseline: четыре contracts/static audits — **4/4 PASS/0**,
`before/summary.json`. Это не доказывало корректность позднего allocation.
Native final oracle на исходном source (`baseline-checked`) даёт 22/22 readbacks,
exits 0, но две VM exceptions с исходной причиной: обычный full-script gate
должен отклонить этот run. `-ExpectReproduction` подтверждает воспроизведение
и явно печатает `runtime_gate=FAIL`, а не PASS исходного source.

`Compile.ps1 -Stage checkout -Variant Stock|RHS|EveronNorthWCSRHS -Label <label>`
в evidence берёт точный graph из canonical launcher DryRun и вызывает terminal
Workbench `-noThrow -wbsilent ... -wbModule=ScriptEditor -run -validate`.
Production graphs — **3/3 PASS/0**, validation successful, script/VM/fatal 0.
Окончательная fixture отдельно скомпилирована на baseline Stock и after
Stock/North WCS: `compile-baseline-checked/`, `compile-checked-stock/`,
`compile-checked-north-wcs/`, все **PASS/0**.

`Run.ps1 -Stage <stage> -Case <case> -Variant <variant> -Port 2003` использует
только `Start-AICFRuntime.ps1 -Role Server -Difficulty Hard`, fresh profile и
отдельный `-addr 127.0.0.1:2003`. `-noThrow` одинаков на before/after и не
исключает VM diagnostics из full log. `source-*.json` фиксируют hashes;
final baseline production совпадает с `8d199ec`, after — с checkout.
Native positive control исполняется реальным BT, затем planner удаляет прежний
waypoint; late input проверяется 64 раза на сторону и спустя 30 секунд.

Окончательные `checked-stock` и `checked-north-wcs` — **22/22 PASS** каждый,
128 late calls с ожидаемым FAIL, два bounded warnings, сохранность новых
waypoints и штатные native exits **0**. Строгий `Test-DefendAllocationLog.ps1`
на обоих полных stopped logs — **PASS/0**, script/VM/fatal 0. Тот же обычный
аудит исходного `baseline-checked` — **FAIL/1** с двумя исходными exceptions.
Exact commands, hashes, exits, error counts и outputs — `runtime-verdicts.json`;
вспомогательный `Audit.ps1` сохраняет оба вида verdict. Native asset/RPL
diagnostics считаются отдельно; общий engine/assets error-free gate не заявлен.
Все 308 tracked addon files checkout совпали с проверенным final stage;
единственная production разница с baseline — `AICF_AINodeLifecycle.c`.
Fixture hash тоже совпадает: `production-source-equality.json`,
`final-source-equality.json` и `source-*.json`.

Все ранние неуспешные попытки сохранены. Первый standalone node prototype
завершился native access violation в `Node.GetCallstackStr`: такой объект
не имел native BT context; dump и logs — `profile-before/`, это **FAIL**.
Следующие prototype assertions ошибочно требовали MOVE waypoint другого класса
и совпадения cached activity waypoint с input; native API таких контрактов не
задаёт. Эти неуспешные runs не выдаются за final PASS. Финальная fixture
использует настоящие BT nodes без повторного OnInit и проверяет native
preconditions, новые entity IDs, queue и отложенный current waypoint.

Первый полный offline запуск из длинного nested checkout имеет **FAIL**
`Test-AuditSelectionContracts.ps1`: Windows PowerShell не скопировал путь
длиннее MAX_PATH. Только собственное дерево offline-runner было остановлено;
его точные PID сохранены, защищённые 7768/29440 в нём отсутствуют.
Повтор использует временный `subst X: <checkout>` и тот же набор без исключений;
полные commands/context/results — `offline-final/`.

Окончательный полный offline-набор из `X:` — **66/66 PASS/0**, без исключённых
проверок и без оставшихся static failures. Synthetic allocation log contracts —
**2 positive / 12 negative**, source contracts — **11 mutations**. Production
adapter не менялся в течение compile/runtime/offline gates. `git diff --check`
— **PASS/0**, 279 локальных ссылок затронутых заметок проверены, битых нет.

Полное evidence этой правки — `.codex-runtime/defend-allocate/`: commands,
manifests, exits, compile logs, полные stopped runtime logs, `runtime-verdicts.json`,
`binaries.json`, source hashes и результаты offline runner. Все девять созданных
source stages удалены после проверки отсутствия использующих их процессов;
исходники production adapter и всех вариантов fixture сохранены в `sources/`,
перечень удаления — `removed-stages.json`. Изолированный Git checkout сохранён
для продолжения работы, основной checkout работающего server не обновлялся.

Игровой server PID 7768 на порту 2001 сохраняется. Исходный client PID 29440
разорвал соединение в 14:05:58: `Connection is flooded by data ... Terminating
connection`, затем процесс завершился exit 0. Команд его остановки агент не
вызывал; остановка offline-runner была позже, в 14:07:02. Причина сетевой
перегрузки не установлена. В полном client shutdown log также **36 leaked Color
instances** и отдельный FUEL mesh-offset SCRIPT(E). Эти реальные diagnostics
не объявляются безвредными/исправленными и блокируют ready PR до отдельного
воспроизведения и устранения. Оригинальный клиентский log сохранён полностью;
`NOT RUN`: повтор реального client UI/игрового ввода на исправленном source,
packaged build, длительный soak. Короткий server oracle не заменяет эти gates.

## Shutdown equipment worlds — issue #63, 2026-10-16

Ветка `codex/issue-63-shutdown-leaks` от актуального `origin/main` `7703d4e`;
дерево при начале чистое. Его production tree совпадал с `eef5f74` (#61).
Game/Server/Workbench — **1.8.0.13**, RHS Status Quo **0.16.5237**,
WCS Weapon Scripts/NATO/Weapons **8.4.0**, Attachments **8.4.1**, RU **8.3.0**.
Полные версии, metadata и hashes — `.codex-runtime/issue63/binaries.json`,
`dependency-versions.json`, `dependencies.json`. Механизм и команды fixtures:
[SHUTDOWN_LIFECYCLE.md](SHUTDOWN_LIFECYCLE.md).

Свежий baseline `baseline3-{server,client}` повторил исходный reproducer:
по **283/283** native loadout checks, exits **0**, но server full-log audit —
**FAIL/1**, 358 `SCRIPT (E)`, 355 leaked-class lines, **5170 экземпляров**.
Client — 0 script errors. Это новый запуск, а не прежнее evidence #61.

Изоляция на том же WCS/Hard source graph:

| Case | Условие | Server script errors / leaked objects |
|---|---|---|
| `idle-traced` | Без loadout fixture/preview редактора, connected, `-checkInstance` | 358 leak diagnostics + allocation records; 5170 objects |
| `idle-standalone` | Без клиента и loadout fixture, equipment service работает | 358 / 5170 |
| `grace-idle` | Пауза 10 секунд после disconnect | 358 / 5170 |
| `lifecycle-before`, `lifecycle-hook`, `nocache` | Clear кеша цен, ранний world-cleanup Clear, полное исключение кеша | Каждый сохраняет 358 / 5170 |
| `noplans` | Baseline, fixture исключает update equipment service | **0 / 0**, native exit 0 |
| `native-clear`, `stop-idle` | Явный Stop без удаления detached roots | Raw GetItems: **75 и 67 live entries** (71/63 уникальных entities после дедупликации); 358 ошибок сохраняются |
| `delete-roots` | Явное удаление тех же native roots | **0 / 0**, все items null, drafts 2 → 0, exit 0 |
| `unique-baseline` | Тот же final oracle и три пакета по 8 на исходном production | 358 / **35619**; удержано **536 → 1072 → 1608 уникальных entities**, сохраняются спустя 10 секунд и после 15 секунд покоя |

`idle-traced-client` исходного control также содержит 12 shutdown script errors
(35 native objects); это не PASS. После исправления три свежих connected client
runs ниже имеют 0 script errors. Сводка обычных попыток — `results.json`;
прерванный полный DumpInstances хранится отдельно.
Ошибки native allocation trace не исключаются из error counts.

`Compile.ps1 -Stage <stage> -Variant <Stock|RHS|EveronWCSRHS> -Label <label>`
в evidence извлекает graph из canonical launcher DryRun и вызывает только
terminal Workbench `-noThrow -wbsilent ... -wbModule=ScriptEditor -run -validate`.
Финальные `regression-stock`, `regression-rhs`, `regression-wcs` — **3/3 PASS/0**,
validation successful, script/VM/fatal ошибок нет. Measurement fixture дополнительно
прошла `final-fixture`, затем окончательная версия unique oracle — `unique-final`
**PASS/0**. Baseline с тем же oracle — `unique-baseline` **PASS/0**.
Сохраняются exact argv, native exits и полные
Workbench logs. Source/fixture hashes не подменяются generated rdb hashes.

`Run.ps1 -Stage <stage> -Case <case> [-Client] [-Standalone] [-Variant <variant>]
[-Port <port>] -Extra <arguments>` использует только `Start-AICFRuntime.ps1`,
fresh profiles и одинаковые Difficulty/graph. Каждый client прошёл exact CLI,
live server PID и `ROSTER_READY` gate. Manifests и native exits сохранены.

| Окончательный run | Native checks / verdict |
|---|---|
| `regression-stock` | **127/127**, contents log audit PASS/0 |
| `regression-rhs` | **234/234**, contents log audit PASS/0 |
| `reproduced-final-1-{server,client}` | По **283/283**, оба полных contents audits PASS/0 |
| `reproduced-final-2-{server,client}` | По **283/283**, оба полных contents audits PASS/0 |
| `fixed-connected-{server,client}` | До расширения oracle: full script/fatal gate PASS/0, native items после Clear — 0, drafts после Stop — 0 |
| `unique-idle`, `unique-batches` | Окончательные полные shutdown audits PASS/0, включая delayed readback; три пакета по 8 EquipmentDraft операций, **24/24**, все 24 operation identities уникальны |

Все эти процессы завершились штатно с native exit **0**, script/fatal/VM errors
и leaked instances — **0**. В исходных kits сохранены exact native addresses,
remove/Undo/Capture/Restore и шесть запасных WCS AK-12 magazines обеих connected
проверок. Анализаторы читают полные остановленные console logs, включая shutdown.

План размеров/расписания, периода покоя и idle envelope сохранён **до** измерений
в `measurement-plan.json`. Ранние frozen/tracked runs сохранены отдельно;
окончательные unique runs дедуплицируют GetItems и weak witnesses по IEntity
identity. До/после каждого пакета живых drafts **2 → 2**, после Stop **0**,
created = destroyed = **55**. Native readback каждого EquipmentDraft.Clear,
включая пакеты, имеет **alive=0**. После последнего пакета есть не менее 15 секунд
покоя: **0 из 1608** unique sampled entities остаются живы; после Stop —
**0 из 1742**, включая две service models. Baseline после того же покоя сохраняет
все **1608**, то есть накопление во время работы подтверждено native identities.
Сравнение всех шести native stats — `memory-unique-comparison.json`:
`MemC` ready→rest **+711374 idle / +263167 batches**, `Mem (KB)` **+184986 / +85162**.
Рост counts, memory, small-block counts/size и pages находится в idle envelope.
Small-block overhead уменьшился: **−5695 / −6778 KB**; это отдельно сохранённое
отличие, а не рост памяти.
Проверка подтверждает отсутствие удержания собственных native items и добавочных
drafts на этих пакетах; длительный soak и влияние на FPS не заявляются.

Полный baseline `Invoke-AICFChecks.ps1 -EvidenceRoot .codex-runtime/issue63/offline-before`
— **63/63 PASS/0**; полный `offline-final` и повторный окончательный
`offline-frozen` — **65/65 PASS/0** каждый,
без исключённых проверок. Production source в течение финального runner не менялся.
Новые shutdown contracts отклоняют пять lifecycle mutations; equipment contracts —
21 rule / 17 mutations, включая удаление root delete и world fence. Full-log
contracts: три positive / одиннадцать negative inputs; реальный baseline server log
по-прежнему отклоняется аудитом (**FAIL/1 SCRIPT_FATAL**). `NOT RUN` не считается PASS.

Ранние неуспешные попытки сохранены: первый comparable `baseline-equipment`
выполнил три пакета, затем завершился native access violation **−1073741819**
без штатного shutdown; log и mdmp сохранены как **FAIL**, не memory PASS.
Повтор с unique oracle (`unique-baseline`) завершился exit 0, сохранив все
native leaks и количественное удержание; исправленный source проходит те же
операции и покой. Причина отдельного native crash по одному dump не объявляется
доказанной. Windows PowerShell redirection прерывала
две launcher попытки до полного runtime; canonical launcher из PowerShell 7 прошёл.
Первый Workbench helper не дождался GUI-subsystem executable, затем исправлен
по documented pipeline. Unbounded `DumpInstances` задержал simulation и дал
413 MB diagnostic log; только его собственные server/client остановлены адресно,
run не используется как memory PASS. `final-idle-server` имел две VM exceptions
из-за empty optional grace parameter в fixture; параметр исправлен,
`fixed-connected` полностью повторён и проходит. Эти ранние FAIL не скрыты.

Основное evidence: `.codex-runtime/issue63/`: `*-command.json`, `*-launcher.txt`,
`*-exit.txt`, `profile-*/logs/`, `compile-*/`, `offline-*/summary.json`,
`results.json`, `stage-hashes.json`, `memory-unique-comparison.json`,
`runtime-verdicts.json`, `final-source-equality.json` и `audit-*.txt`.
Все девять source/fixture hash comparisons совпадают с проверенными final stages.
Native diagnostics WORLD/ENTITY/RPL/NETWORK/RESOURCES/MATERIAL посчитаны отдельно;
общий error-free engine/assets/backend gate не заявляется. В частности native
DamageManager пытается RPC из unregistered local models: baseline connected —
2 сообщения, после явного удаления models — 4; в unique batch — 50/52.
Эти сообщения не вырезаны, нулевой RPL-error gate не заявляется.
Установленные game/Server/Tools и dependencies
не изменялись. **NOT RUN:** визуальная оценка/FPS, player input, полный
save/respawn/JIP pipeline, packaged build, длительный soak. В active acceptance
checklist issue таких ручных критериев нет.

### P2 PR #64: обязательный EquipmentDraft batch manifest — 2026-10-16

[Замечание](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/64#discussion_r4293307638)
воспроизведено на `e20db8a`: `Test-ShutdownLogContracts.ps1` проходит, однако
synthetic batch с 24 успешными operations, `idle` manifest и без equipment-флага
ошибочно получает audit **PASS/0**. Это дефект проверяющего правила; реальный
`unique-batches-server` уже запускался с обоими правильными параметрами.

Теперь `Test-ShutdownLog.ps1 -Batches` требует ровно одну точную пару
`-aicfShutdownMode batches` и `-aicfShutdownEquipmentBatch 1` в manifest,
а также `SHUTDOWN_DONE mode=batches`. Positive batch fixture исправлена;
contracts проверяют **3 positive / 22 negative cases**, включая отсутствие,
неверное/непарное значение, дубликаты и несовпадение completion mode.

Evidence — `.codex-runtime/issue63-review-batch/`. Baseline targeted runner
`Invoke-AICFChecks.ps1 -Name Test-ShutdownLogContracts.ps1 -EvidenceRoot <root>/before`
— **1/1 PASS/0**; ошибочный synthetic input до изменения — **PASS/0**, после
изменения — ожидаемый **FAIL/1**. `Replay.ps1` повторно анализирует все восемь
сохранённых полных final runtime logs — **8/8 PASS/0**. Подмена реального batch
manifest на idle и удаление equipment-флага дают ожидаемые **FAIL/1**;
исходный native-leak baseline по-прежнему отклоняется с `SCRIPT_FATAL`
(358 script errors / 35619 leaked instances). Exact commands, audit hashes,
exits и outputs — `replay-verdicts.json`, исходный false positive сохранён
в `misconfigured-before.txt`.

Полный `Invoke-AICFChecks.ps1 -EvidenceRoot <root>/offline-after` после изменения
— **65/65 PASS/0**, без исключений и baseline failures. `git diff --check` —
**PASS/0**; проверены 206 локальных Markdown-ссылок затронутых заметок,
битых нет. Два PowerShell-аудита и четыре заметки — весь scope этой правки.

Production `.c`, Enfusion API и runtime fixture не изменялись. Workbench и
новые native процессы **NOT RUN**: здесь проверяется усиленный анализ уже
остановленных logs, прежние runtime результаты не выдаются за новые запуски.
Ограничения и исходные native diagnostics предыдущего отчёта сохраняются.

### P2 PR #64: точный idle mode для server audit — 2026-10-16

[Замечание](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/64#discussion_r4294297490)
воспроизведено на `278b658`: существующие shutdown contracts дают **PASS/0**,
и сохранённый настоящий `unique-batches-server` без audit-флага `-Batches`
ошибочно получает **PASS/0**. Сам measurement run не менялся: в его manifest
и completion указано batches. Значит, анализатор мог принять batch за idle
и разрешить загрязнённый idle envelope.

Теперь server audit без `-Batches` требует ровно одну точную пару
`-aicfShutdownMode idle` в manifest и `SHUTDOWN_DONE mode=idle`.
Ошибки `IDLE_MODE_REQUIRED` и `IDLE_COMPLETION_MODE` независимы.
Batch/equipment validation сохранена; client может сопровождать оба mode.
Contracts: **4 positive / 32 negative cases**. Десять новых отрицательных
случаев покрывают неверные mode, отсутствие/непарное значение, дубликаты,
регистр и несовпадение manifest/completion; новый positive client batches
проверяет прежнее допустимое поведение.

Evidence — `.codex-runtime/issue63-review-idle/`. Свежий selected baseline
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File
tests/contracts/Test-ShutdownLogContracts.ps1` — **PASS/0**; исходный
false positive и argv сохранены в `misconfigured-before*`, baseline source —
в `baseline-*.ps1`. После исправления тот же real batch без `-Batches` —
ожидаемый **FAIL/1** с обеими новыми причинами. Отдельно real batch с idle
manifest отклоняется по completion, а real idle с batch manifest — по аргументу.

`Replay.ps1` повторно анализирует восемь сохранённых полных final runtime
logs — **8/8 PASS/0**. Все семь negative controls отклонены ожидаемо, включая
исходный leaked-instance baseline с `SCRIPT_FATAL`; дефект не исключается
из gate. Exact commands, audit hashes, exits и outputs — `replay-verdicts.json`.
Это новый запуск анализаторов прежних остановленных logs, не новый native run.

Полный `RunChecks.ps1` вызывает `tools/Invoke-AICFChecks.ps1 -EvidenceRoot
<root>/offline-after` через временный `X:` alias изолированного checkout,
чтобы избежать ранее установленного Windows PowerShell MAX_PATH.
Итог: **66/66 PASS/0**, без исключённых проверок и baseline failures. Alias снят.
`git diff --check` — **PASS/0**; локальные ссылки заметок — **211/211 PASS**.

Scope — два PowerShell-аудита и четыре заметки. Production `.c`, API и
runtime fixture не менялись. Workbench и новые game/server/client runs —
**NOT RUN**, не требуются для этой правки анализатора. Основной игровой
процесс не останавливался и его checkout не обновлялся. Ранее описанные
runtime diagnostics, FAIL baseline и ограничения остаются в отчёте.

## Issue #61: магазины исходного комплекта ВС РФ — 2026-10-16

Ветка `codex/issue-61-loadout-magazines` от актуальной `origin/main`
`6a8b8fb`; исходное дерево чистое. Evidence — `.codex-runtime/issue61/`.
Причина: `IsContentPocket` отбрасывал `EquipmentStorageComponent`, включая
native `RHS_EquipmentStorageComponent` трёх magazine pouches и pouch TV-115.
Исправление расширяет типы cargo, сохраняя subtree, fixed-slot, weapon и
native entity/storage/slot gates. Имена оружия в production не проверяются.

Исходный комплект: `{F0F0A8C16C9901B8}` MSV VKPO DS SL, personal source через
production content profile; исходный Pouch Preset [7]
`{B89BB29B04D26E70}Vest_TV102_rifleman_EMR_7.et` в native clothing slot 6.
Шесть запасных `{62DEC638B985B3FE}` AK12 magazines лежат в equipment storage
`5222CB03D1147687`: MP103 (slot 0), два ArsArma pouches (slots 0/1), TV-115
(slot 0). Загруженный магазин оружия не входит в cargo subtree.
Baseline `EveronWCSRHS-baseline-final` на исходном production: native cargo
**11**, ContentRows **5**, шесть магазинов отсутствуют. После удаления выбранного
магазина baseline показывает **5 вместо 10**; после Undo — снова **5 вместо 11**.
Native GetAll/FindItemSlot oracle не использует locations/типовой whitelist
проверяемого списка. Native entity IDs, parent IDs, prefab и полный адрес
записаны в `[MAG_NATIVE]` полного лога.

Stock regression также воспроизвела прежний sparse-slot restore FAIL:
`Stock-regression` — шесть проваленных restore/signature/validate checks.
Причина устранена: preview Restore сохраняет совпадающие контейнеры и согласует
snapshot по native IDs; при замене одежды очищает пересекающиеся LoadoutArea.
Authority/server restore сохраняет прежнюю очистку перед native serializer.

Команды запускались из `.codex-runtime/issue61/` через evidence helpers:
`compile.ps1 -Variant <Stock|RHS|EveronWCSRHS> -Label <label> -SourceRoot <stage>`
переносит точные gproj/addonsDir/addons из canonical DryRun manifest в terminal
Workbench Validate/Compile. `runtime.ps1 -Variant <variant> -Tag <tag>
-Stage <stage>` вызывает только `tools/Start-AICFRuntime.ps1 -Role Server
-Difficulty Hard`; `connected.ps1 [-Client]` использует тот же launcher и
readiness gate. Вся матрица использует fresh profiles. Точные argv, launcher
manifests, native exits и полные logs сохранены рядом с каждым profile.


Результаты:

- Baseline `Invoke-AICFChecks.ps1 -Name Test-AILoadoutStatic.ps1,Test-PersonalLoadoutContracts.ps1 -EvidenceRoot .codex-runtime/issue61/before-native` — **2/2 PASS/0**.
- Полный `Invoke-AICFChecks.ps1 -EvidenceRoot .codex-runtime/issue61/offline-after` — **63/63 PASS/0**. После изменения preview Restore отдельный полный `offline-final` — **63/63 PASS/0**, без исключённых проверок; итог в `summary.json`. Выбранные финальные loadout contracts включают **167** отрицательных мутаций.
- Terminal Workbench `final-stock`, `final-rhs`, `final-wcs` и финальная connected fixture `frozen-connected` — **PASS/0**, `Script validation successful`, script/VM/fatal errors — 0. Production во всех stages совпадает с checkout: `source-hashes.json`, **308** tracked файлов, ровно два production отличия от baseline.
- `runtime.ps1 -Variant Stock -Tag frozen -Stage stage-final` — **127/127 PASS**, RHS с теми же параметрами — **234/234 PASS**, `EveronWCSRHS` — **283/283 PASS**. Полные stopped logs, native exits **0**; script/fatal/VM errors — 0. `Test-LoadoutContentsInventoryLog.ps1` — **PASS/0** на всех трёх.
- `connected.ps1` и `connected.ps1 -Client` — canonical ready gate подтверждён (`SERVER_READY`, PID, exact CLI, ROSTER_READY, port 2361). Исправленный WCS server и client: **283/283 loadout checks PASS** на каждом, native exits **0**. Client full-log audit — **PASS/0**.
- **Connected server full script/shutdown gate — FAIL/1:** в остановленном WCS server log **358** сообщений `SCRIPT (E)` исключительно о native shutdown leaks, `AICF` classes — 0, VM/non-leak script errors — 0. Тот же final harness на исходном production (`baseline-connected-*`) даёт **те же 358** native leak сообщений; исходные server/client дополнительно имеют **54** `MAG_ROWS_EXACT` failures, исправленные — 0. `native-leak-comparison.json` и оба полных server logs сохранены. Shutdown не объявляется PASS и остаётся blocker для ready PR; установленный код игры/RHS/WCS не изменялся. Resource/material/backend E/F отдельно посчитаны в `runtime-summary.json`; общий error-free gate не заявляется.
- Negative-control audit раннего `EveronWCSRHS-after` с VM exception отклонён (**exit 1**, ожидаемо): fixture тогда не задала обязательное имя рецепта. Исправленная fixture задаёт имя, проверяет Decode и отдельно сравнивает native rows после Restore. Этот ранний прогон не входит в PASS.

Основные полные логи — `profile-Stock-frozen/logs/`, `profile-RHS-frozen/logs/`,
`profile-EveronWCSRHS-frozen/logs/`, `profile-frozen-connected-server/logs/`,
`profile-frozen-connected-client/logs/` внутри evidence. Baseline —
`profile-EveronWCSRHS-baseline-final/logs/`, `profile-Stock-regression/logs/`,
`profile-baseline-connected-{server,client}/logs/`. `runtime-summary.json`
содержит абсолютные пути, counts, native exits и отдельные errors.
Промежуточный sandbox Workbench `before` не прошёл SteamAPI_Init; повтор
`before-owner` из окружения владельца — PASS. Зависший sandbox server остановлен
адресно; остальные probes закрылись сами через RequestClose. Финальные
stage hashes сохранены перед удалением временных копий.

**NOT RUN:** визуальная отрисовка и фактический ввод игрока, полный RPC
save/respawn/JIP pipeline, packaged build, soak. Terminal oracle подтверждает
native inventory/draft/rows, адресное редактирование и snapshot, а не пиксели UI.
Визуальная проверка остаётся отдельно отклонённым критерием issue; она не
объявляется PASS. Issue остаётся открытым, ready PR/push/merge не выполнялись.


## PR #60: rollback переноса десанта и runtime-файлы — 2026-10-16

Замечание [P2](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/60#discussion_r4291674923)
воспроизведено на `66b08ba`: native destination Add вызывает внешний
`GetOnAgentAdded` callback, который синхронно снимает agent с destination до
возврата Add. Production rollback возвращает того же бойца в source, но
прежний OnAgentRemoved уже уменьшил logical capacity: **capacity=4, agents=5,
Ready=0**. Baseline `runtime-before`: 10 check events, ровно два ожидаемых FAIL
(`TRANSFER_ROLLBACK_CAPACITY`, `TRANSFER_ROLLBACK_READY`), native exit 0.
Baseline log audit подтверждает воспроизведение, а не PASS исходного кода.

Evidence: `.codex-runtime/issue59-transfer-rollback-20261016/`.
Перед правкой `Invoke-AICFChecks.ps1 -Name <семь FIA/observers/Stage3 audits>
-EvidenceRoot <root>/before` — **7/7 PASS/0**, baseline static FAIL нет.
Production изменены только `AICF_FIAProximityLifecycle.c` и
`AICF_FIAGarrisonCrew.c`: planned transfer удерживает logical seat до результата,
fences повторяются перед обоими Add, receiver не присваивает входящую identity
чужому свободному месту. Rollback не сбрасывает настоящие потери.
Pending state снимается при завершении и Stop; native expansion/dormancy
приостанавливаются на время синхронной попытки.

`ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent ... -wbModule=ScriptEditor
-run -validate` — production Stock/EveronRHS/EveronWCSRHS **3/3 PASS/0**,
validation successful, script/VM/fatal ошибок нет. Полные argv/logs:
`compile-production-stock/`, `compile-production-rhs/`, `compile-production-wcs/`.
Baseline fixture и отдельные Stock/WCS stages также прошли compile.
Первый `compile-baseline/` содержит FAIL fixture: прямой доступ к protected
invoker заменён проверенным публичным `GetOnAgentAdded()`. Это не PASS.

Runtime через canonical `Start-AICFRuntime.ps1 -Role Server`, fresh profiles,
`-aicfFIAProximityProbe us -aicfFIADesantTransferProbe baseline|after
-aicfRequirePlayerForResult 0`. `Run.ps1` в evidence сохраняет точные arguments,
launcher manifests и native exits; source stages изолированы.

| Case | Graph / difficulty / port | Результат |
|---|---|---|
| `runtime-before` | Stock / Medium / 2363, baseline | Дефект воспроизведён, 2 ожидаемых FAIL, native exit 0 |
| `runtime-stock-after` | Stock / Medium / 2363 | 31/31 PASS, native exit 0 |
| `runtime-wcs-after` | Full Everon WCS / Hard / 2365 | 31/31 PASS, native exit 0 |

Оба after проверили native отказ, возвращённый parent, **capacity=5, agents=5,
Ready=1**, последующий успешный retry с тем же entity/ID и неизменным общим
roster. Затем прошли все existing proximity cases: budget, retention, desant,
identity, две dormancy/return итерации, partial/full losses, destroyed vehicle
и road lifecycle. 31 включает семь новых transfer checks.

Свежий полный `Invoke-AICFChecks.ps1 -EvidenceRoot <root>/offline-after` —
**63/63 PASS/0** на окончательном production коде, без пропущенных проверок.
`Test-FIAProximityContracts.ps1` теперь проверяет 40 guards и отдельную
отрицательную мутацию запрета `m_bLost=false` при transfer.

`Test-FIADesantTransferLog.ps1 -LogPath <console.log> -LauncherPath <terminal.txt>
-NativeExitPath <exit.txt> [-Baseline]` — **PASS/0** для всех трёх остановленных
полных логов; after дополнительно запускает полный `Test-FIAProximityLog.ps1`.
Unexpected script/VM/fatal/AICF leaks отсутствуют, native resource/shutdown
diagnostics сохранены отдельно. После удаления TRANSFER_RETRY из копии WCS log
тот же audit даёт ожидаемый **FAIL/1 CASE_TRANSFER_RETRY**; negative вывод
сохранён, временная копия удалена. Точные команды after — `verdict.json`.
`production-hashes.json`: оба production файла совпадают с обоими after stages.

По отдельному запросу пользователя сняты с отслеживания четыре старых файла:
`.codex-runtime/parallel-armed-server.json`, `parallel-transport-server.json`,
`start-parallel-vehicle-servers.ps1`, `stop-parallel-vehicle-servers.ps1`.
Commit `1f14ffa`, `git rm --cached`: локальные файлы сохранены с прежними hashes,
уже существующее `/.codex-runtime/` в `.gitignore` их исключает. Проверка
`git ls-files .codex-runtime` возвращает пустой список. Evidence
`runtime-files-before.json` и `runtime-files-history.txt` фиксирует их прежнее
состояние и происхождение; новые evidence-файлы не добавляются в Git.

Client/JIP, визуальный тест и длительный soak этого исправления — **NOT RUN**.
Прежний client gate ниже относится к `66b08ba`, не к новому запуску. Runtime
этого исправления проверяет server/master group transfer, клиентские RPC/UI
и реплицируемые поля не менялись.

`git diff --check` — PASS/0. Собственные native процессы завершены штатно,
три source stages удалены после сохранения hashes и исходных двух классов;
полные profiles/logs, manifest, baseline и первый compile FAIL сохранены.

## Proximity дополнительных сил FIA — issue #59, 2026-10-16

Ветка `codex/fia-proximity` от `dc1de5e`. Evidence: `.codex-runtime/issue59/`;
исходные offline/Workbench outputs — `.codex-runtime/issue59-baseline/`
и `.codex-runtime/issue59-workbench-baseline-host/`. Контракт и воспроизведение
fixtures описаны в [FIA_PROXIMITY.md](FIA_PROXIMITY.md).

Исходный полный runner стартовал на чистом дереве и завершил **62/62 PASS/0**;
FIA/observers audits выполнены до правок, поздние несвязанные проверки шли
параллельно реализации. Это не frozen baseline всего набора. После реализации
`tools/Invoke-AICFChecks.ps1 -EvidenceRoot .codex-runtime/issue59/offline-final`
— **63/63 PASS/0**, включая новый proximity audit. После последних
callback/health fences отдельно повторены семь затронутых FIA/observers/Stage3
аудитов: **7/7 PASS/0**, `targeted-final/`. Затем на окончательном production
коде и исправленном negative self-check выполнен свежий полный runner:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1
-EvidenceRoot .codex-runtime/issue59/offline-frozen-final` — **63/63 PASS/0**,
без исключённых проверок. Сохранённых offline baseline FAIL нет.
`offline-after-1/` содержит прежний `INITIAL_BOARDING_GATE` FAIL: требование
initial RecoveryAllowed сохранено, проверка расширена под fenced native
reactivation с отрицательной мутацией. Ранний FAIL не считается PASS.

Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent ...
-wbModule=ScriptEditor -run -validate` на production graphs Stock,
EveronRHS и EveronWCSRHS — **3/3 PASS/0**, `Script validation successful`;
script compile/VM/fatal ошибок нет. Точные argv и полные логи:
`production-final-Stock/`, `production-final-EveronRHS/`,
`production-final-EveronWCSRHS/`. Fixtures компилировались отдельно.
`production-hashes-final.json` фиксирует 233 production `.c`.

Каждый runtime запуск использовал `tools/Start-AICFRuntime.ps1 -Role Server`
со свежим profile и isolated source stage, точные аргументы/manifest сохранены
в `command.json`/`terminal.txt`, native exit — `exit.txt`. Общие CLI:
`-aicfFIAProximityProbe <mode> -aicfRequirePlayerForResult 0`.
Остановленные console/script/error logs остаются в `<case>/profile/logs/`.
`Test-FIAProximityLog.ps1 -LogPath <console.log> -LauncherPath <terminal.txt>
-NativeExitPath <exit.txt>` требует завершение, exit 0 и все native readbacks;
сохранённый вывод — `<case>/audit.txt`, сводка — `runtime-verdicts.json`.

| Case | Variant / difficulty / actor | Verdict |
|---|---|---|
| `stock-medium-us-final` | Stock (Arland) / Medium / US | 24/24, PASS/0 |
| `rhs-everon-medium-final` | EveronRHS / Medium / USSR | 24/24, PASS/0 |
| `wcs-full-hard-final` | EveronWCSRHS / Hard / US (`scale`) | 24/24, PASS/0 |
| `wcs-north-hard-final` | EveronNorthWCSRHS / Hard / USSR | 24/24, PASS/0 |
| `easy-north-final` | EveronNorth / Easy / US | 2/2, PASS/0 (`-Easy`) |
| `ownership-final` | Stock / Medium / queued ownership change | 3/3, PASS/0 (`-Ownership`) |
| `wcs-player-occupancy-final-10` | ArlandWCSRHS / Medium / реальный client | 14 уникальных cases, PASS/0 (`-Player`) |
| `ambient-managed-final` | Stock / Medium / US и USSR, regression #54 | 45/45, PASS/0 |

Player case: server/client **native exit 0**, клиент подтвердил реальную
controlled entity при подходе и после передачи уже сидящего исходного члена
экипажа FIA. Readback `occupied=1 allowed=0 alive_before=5 alive_after=5`,
та же vehicle identity. При передаче управления native engine передаёт
authority техники владельцу character; server dormancy отказывает уже на
authority/identity gate. Direct `DespawnMembers` также не удаляет бойцов.
Полные server и `wcs-player-occupancy-client-final-10/` client logs проверены
с `-ClientLogPath`, `-ClientLauncherPath`, `-ClientNativeExitPath`; точная
команда и verdict — `<server-case>/verdict.json`. Все 76 check events (включая
повторяющийся TARGET до подключения) успешны. Script/VM/fatal/AICF leak ошибок
в обоих финальных логах нет, client post-world-destruction script diagnostics=0.
Сервер реплицирует завершение и ждёт штатного client RequestClose перед своим
закрытием; CallLater явно снимается в callback и Stop.
Тот же log audit на копии client log без occupancy readback даёт ожидаемый
FAIL/1 `CLIENT_NATIVE_READBACK`; вывод и точные аргументы сохранены в
`negative-audit.txt`/`negative-verdict.json`, временная копия удалена.

Четыре полных FIA cases проверяют дальний пустой roster, native budget refusal
и последующее появление, retention, фактическую высадку/transfer десанта,
две dormancy/return итерации, неизменный план, частичные/полные потери,
уничтоженную машину без замены и дорожный crew active/dormant/return.
Safety cases отвергают stale generation/vehicle ID и unknown occupancy identity.
Ownership case меняет faction после реального queued RequestSpawn и читает
отказ expansion, retirement и пустой roster. Regression #54 дополнительно
проверяет actual FIA observer и controller origin у точки при бойцах далеко,
пустой/погибший roster и Stop/removal observers. Анализ полного лога:
`Test-AmbientPatrolLog.ps1 -LogPath <console.log>` — PASS/0;
330 native shutdown leaks и 378 прочих resource/backend diagnostics сохранены
отдельно; обычных runtime script/VM/fatal/AICF leak ошибок нет.

Full Everon сравнивается с `baseline-wcs-full-hard-fixed-hq/` на `dc1de5e`:
одинаковые HQ, PRNG input `590013`, **буквально одинаковый план 75 entries**,
80 секунд наблюдения. Baseline: materialized **195**, active **193**,
far_materialized **195**; исправление: materialized/active **19/19**,
far_materialized **0** (19 возле HQ допустимы). Подход: **27/27**, far **0**;
уход: **19/19**, dormant groups **3**, far **0**.
`full-everon-comparison.json` содержит планы и readbacks. Baseline probe
намеренно воспроизводит два исходных нарушения; `-Baseline` audit PASS/0
подтверждает воспроизведение, а не корректность исходного поведения.
Первый scale run выполнен до двух дополнительных callback guards;
последние guards проверены fresh North/Easy/ambient/client, compile и audits.

История разработки сохранена: ранние `stock-medium-us-*` выявили блокировку
общего acquisition cadence дальним slot, null occupancy и удержание survivors
исходным погибшим телом; причины исправлены до финальных прогонов. Ошибки
fixtures (порядок modded classes, ранний readiness readback, client movement
и local-only boarding) также сохранены отдельно. Эти прогоны не входят в PASS.
В ранних client logs после `Game destroyed.` сохранены native menu ошибки
`SCR_FilterCategory`/`REPLICATION_SHUTDOWN`; финальный client закрывается до
server disconnect. Client #8 после возврата в menu остановлен адресно по
проверенному PID/profile, native -1; это не PASS и не crash production.
Первый sandbox Workbench не инициализировал SteamAPI; host CLI прошёл.

FPS/визуальная плавность, длительный soak, JIP и полная persistence физиологии —
**NOT RUN**, вне критериев issue. Readback активности не доказывает прирост FPS.
`git diff --check` — PASS/0. Свои игровые процессы завершены, пять isolated
source stages удалены после сохранения hashes и baseline fixture в
`source-manifests/`; profiles, полные логи и история FAIL сохранены.

## Подготовка 0.1.29: полная матрица запуска — 2026-10-15

Задача: [issue #57](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/issues/57),
[PR #58](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/58).
Evidence остаётся в `.codex-runtime/dependency-check-20261015/`.

На WCS-исправлении `2ed3a67` выполнен свежий полный offline runner:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1
-EvidenceRoot .codex-runtime/dependency-check-20261015/offline-final-all` —
**62/62 PASS, native exit 0**. Набор имён совпал с baseline, пропущенных
проверок и baseline failures нет; production snapshot 266 файлов сохранён.
Полные stdout и отдельные outputs — `offline-final-all/`, сопоставление —
`offline-final-all-verdict.json`.

Три независимых stage и listener 2395/2397/2399 использованы параллельно:
Stock/Everon/EveronNorth **9/9 PASS**, RHS/EveronRHS/EveronNorthRHS
**9/9 PASS**, WCS **8/9 PASS, 1 FAIL**. Каждый успешный запуск подтвердил
точные CLI/manifests, header/world/difficulty, production `ROSTER_READY`,
десять faction-correct READY slots каждой стороны, не менее 20 секунд
после готовности и штатные `RequestClose`/`Game destroyed.`, native exit 0.
Fixtures прошли terminal Workbench Validate/Compile. Полные результаты,
argv, hashes и остановленные console/script/error logs —
`matrix-stock/summary.json`, `matrix-rhs/summary.json`, `matrix-wcs/summary.json`.
Предварительные более короткие запуски сохранены отдельно и не заменяют gate.

**Сохранённый исходный FAIL:** `EveronWCSRHS / Easy`, profile
`matrix-wcs/profile-EveronWCSRHS-Easy/logs/logs_2026-10-15_17-56-52/`.
USSR slots 1/2 перешли в READY при `t_ms=6914/6915`, затем погибли
в Physics simulation (`SCRIPT E: INSTIGATOR_OTHER`, две строки),
`GROUP_EMPTY` при `t_ms=7563/7896`. Глобальный `ROSTER_READY` не достигнут,
fixture закрыла сервер через 180 секунд с native exit 0; это **FAIL**.
Replacement ждёт `NO_ONLINE_BARRACKS`, а construction/builders ждут
глобальный roster: подтверждён цикл bootstrap. Исходный failure-index,
полные логи и 266 hashes сохранены; повтор до случайного PASS не выполнялся.
Диагностическая fixture с той же парой HQ воспроизвела физическую причину:
оседающий stock BTR70 `{C012BB3488BEA0C2}` ударяет бойцов, native damage
`COLLISION`, instigator `INSTIGATOR_OTHER`. Обычный Projectile trace, затем
отдельно проверенные Character/Vehicle masks возвращали clear даже в опасной
точке. Эти неудачные эксперименты и отличия timing global READY сохранены
в `matrix-rhs/spawn-damage/`; их нельзя считать startup PASS.

Финальный guard `AICF_InfantryVehicleClearance` читает свежие vehicle bounds
в query radius 16 м и исключает горизонтальный footprint с запасом 2 м.
Exact-HQ regression на окончательных **267 production файлах** — **PASS/0**:
оба исходных slots сохранили generation 1 и живого бойца не менее 64 секунд;
contact/damage/destroyed=0, ordinary SCRIPT E/F, ENGINE F и VM=0, штатное
закрытие. `spawn-damage/verdict-release.json`, `summary.json` и полные
остановленные profiles сохраняют команды, argv, listener и source hashes.

Construction/builders получили отдельный server/master barrier
`INITIAL_SPAWN_QUEUE_SETTLED`; полный `ROSTER_READY` по-прежнему требует
20 текущих READY slots. Fresh targeted baseline и after — **4/4 PASS/0**
(ConstructionStatic, ConstructionContracts, BaseBuildersStatic, Stage4Static).
В after — 30 log inputs и 41 negative source mutation. Placement baseline —
**PASS/0**, три mutations; after — **PASS/0**, десять mutations. Первый запуск
нового static harness упал из-за отсутствующего optional `rule` key при
StrictMode; исправление harness и исходный вывод сохранены отдельно в
`placement-after-harness-failed1.txt`, этот FAIL не был baseline дефектом.

Forced early-loss regression с неизменными fixture/HQ и native AI kill:
baseline 266 файлов — **ожидаемый FAIL**, after 267 — **PASS/0**.
До правки — три ранние смерти, 180 секунд без construction/builders и READY.
После — только одна injected death, barrier с `USSR slot 0=WAITING` при
`roster_ready=0`; реальная казарма построена за 275 supplies, worker завершил
layout и `service_online=1`. Затем оплаченный replacement того же slot:
`ticket_debit=1 supply_debit=50`, generation 39, `ROSTER_READY us=10 ussr=10`.
Fixture наблюдала ещё 10 секунд, общий elapsed 233736 мс. Native exit 0
не использован как замена условий gate; полные after logs/terminal содержат
ordinary SCRIPT/VM/F/AICF errors=0. Evidence —
`matrix-wcs/early-loss/{baseline-summary,after-summary,final-verdict}.json`.

`compile-release-final.ps1 -Variant Stock|EveronRHS|ArlandWCSRHS|EveronWCSRHS`
— **4/4 production Workbench PASS/0**, validation successful, execution
SCRIPT E/F, ENGINE F и VM отсутствуют. Source freeze —
`production-hashes-release.json`, 267 файлов, SHA-256
`7E4A62D8FC7FAAD90E2A8EDBCBD73D93F6E032410D38218BE5F3990A20562E3F`.
Свежая параллельная матрица на окончательных 267 файлах — **27/27 startup
PASS, native exits 0**: `release-final-stock`, `matrix-rhs/summary.json`,
`matrix-wcs/release-matrix`. Каждый запуск удерживался не менее 20 секунд
после production READY; точные CLI/header/world/difficulty, listener и
десять faction-correct READY slots каждой стороны подтверждены.

| Variant | Easy | Medium | Hard |
|---|---|---|---|
| Stock | PASS | PASS | PASS |
| Everon | PASS | PASS | PASS |
| EveronNorth | PASS | PASS | PASS |
| RHS | PASS | PASS | PASS |
| EveronRHS | PASS | PASS | PASS |
| EveronNorthRHS | PASS | PASS | PASS |
| ArlandWCSRHS | PASS | PASS | PASS |
| EveronWCSRHS | PASS | PASS | PASS |
| EveronNorthWCSRHS | PASS | PASS | PASS |

Объединённый индекс — `release-final-runtime-summary.json`. Stock выполнил
198/198 assertions, включая проверку нового queue barrier. Все 267 source
hashes совпали до/после с fixture stages; stages удалены, собственные engine
processes отсутствуют. Полный offline runner после финального source —
**62/62 PASS, native exit 0**, 18:23–18:39 MSK. Команда:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-AICFChecks.ps1
-EvidenceRoot .codex-runtime/dependency-check-20261015/release-final-offline-all`.
Все исходные checks сохранены, набор имён совпал с baseline, дубликатов 0;
baseline failures отсутствуют. `release-final-offline-verdict.json` содержит
сопоставление 267 hashes и полные результаты. Новый итог не заменяет первый
WCS FAIL и не использует прежний PASS как результат окончательного source.

Дополнительный terminal direct-connect `ArlandWCSRHS/Easy` — **PASS/0**
для server и Diag client (18:31–18:33 MSK). Canonical launcher перед клиентом
подтвердил точные server CLI, живой PID, listener 2405 и `ROSTER_READY`.
Client загрузил штатный RHS Arland world, получил player ID 1, разрешил
3347/3347 ресурсов, invalid=0/renamed=0, создал два локальных default preview
US/USSR через production `PrepareDefaultLoadout`; оба PASS. Оба процесса
закрылись через `RequestClose`, native exits 0, `Game destroyed.` подтверждён.
Все полные console/script/error и terminal scopes проверены: ordinary
SCRIPT E/F, ENGINE F/VM, equipment failures и AICF ERROR отсутствуют.
Server log содержит 834 (E), включая 358 shutdown leak строк;
client — 162 (E), shutdown leaks=0. Это **не full clean-log PASS**.
Команды: `compile-stage.ps1 -Label release-client -Stage release-client/stage`,
`release-client/runtime.ps1 -Role Server|Client`, `release-client/collect.ps1`;
точные manifests/CLI, полные logs, summary, fixture и 267 hashes —
`release-client/`. GUI automation и визуальная оценка не выполнялись.
Собственных процессов после закрытия 0, stage удалён после сохранения evidence.

Resource/RPL и shutdown diagnostics сохранены отдельно: startup PASS
не означает full clean-log PASS. Stock: 465 (E), Everon/North: 498 (E),
в том числе 249 SCRIPT shutdown leak строк. RHS: 349/479 прочих (E)
и 352 shutdown leak строки. В успешных WCS запусках 358 SCRIPT shutdown
leak строк; обычных SCRIPT execution errors/VM/ENGINE F нет.
Host UI, JIP, визуальная проверка, soak, packaged build
и Workshop upload — **NOT RUN**.

## Зависимости и вылеты после обновления WCS — 2026-10-15

Ветка `codex/dependency-check-20261015` от свежего `origin/main`
`8819dddb0cba6e0f2bf2995d8397efb538458c0a`, исходное дерево чистое.
Изменены три WCS `.c`: canonical пути 388 ресурсов, включая M4A1 URGI
в infantry kit и FIA pool. GUID и порядок записей сохранены;
`.gproj`, launcher, Core и RHS production не менялись. Обновлены README,
указатель, FIA_ARMAMENT и контракт WCS_POOL с отрицательной мутацией старого пути.
Evidence: `.codex-runtime/dependency-check-20261015/`.

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File
.codex-runtime/dependency-check-20261015/check-graph.ps1` — **18/18 PASS,
exit 0**: девять Variant для Server/Client, каждый прямой и транзитивный GUID
из `addon.gproj` и `ServerData.json` входит в launcher graph. DryRun manifests
сохранены целиком; это структурная проверка, а не запуск игры.

На момент исходного snapshot установка содержала 22 внешних RHS/WCS пакета,
включая необязательный M2A2 Upgrade.
RHS Status Quo — `0.16.5237`, два Content Pack — `0.16.5236`.
WCS RU/Sounds остались `8.3.0`, Attachments — `8.4.1`, прочие WCS — `8.4.0`.
У 16 обязательных обновлённых WCS `corrupted=true`; все 16 `data.pak`
не совпадают по размеру с новым манифестом и датированы 05.10.2026.
Для 44 проверенных остальных файлов, включая RHS Status Quo `data.pak`,
SHA-512 совпал. RHS manifest `resourceDatabase.rdb` пустой; его целостность
этим способом не подтверждена. Полные inventory, sizes, hashes и точный
список повреждённых пакетов сохранены в JSON/CSV. Статус corrupted и
несовпадение файлов — **FAIL целостности**, графовый PASS их не отменяет.

`compile.ps1 -Variant Stock|EveronRHS` с доступом к Steam — terminal Workbench
**2 PASS, native exit 0**, `Script validation successful`, SCRIPT E/F,
ENGINE F и VM exceptions — 0. Прочих resource (E) — 25/26.
Полные `wb-*-verified/`, manifests, argv и `compile-summary.json` сохранены.
Первый sandbox Stock завершился -1 из-за `SteamAPI_Init failed`; это
не compile PASS, его лог оставлен отдельно в `wb-Stock/`.

Скопированы три полных остановленных пользовательских набора logs с dumps
в `user-crashes/`. В 15:02:22 MSK `ENGINE (F): Crashed` непосредственно после
`Corrupted Addon Repair started: ... WCS_Scopes`; до этого native workshop
download sequence давал `NULL pointer ... m_Dependency`. В 15:04:02 crash
имеет стек `SCR_RadioComponent.UpdateVisibility` через RHS equipment refresh;
до него повторяются WCS `DEFAULT_FAILED`/missing URGI resource. Эти наблюдения
не доказывают единую причину обоих native crashes. Пользовательские запуски
использовали установленный контент; они не являются тестом текущего source SHA.

Во время переустановки `check-files.ps1` завершился с exit 1 на исчезнувшем
`ServerData.json`; это не verdict законченной установки. Пользователь
самостоятельно перекачал пакеты. `verify-reinstalled.ps1` — **PASS/0**:
все 48 файлов 16 ранее повреждённых WCS совпали по размеру и SHA-512
с сохранёнными манифестами, у всех 22 пакетов `corrupted=false`.
`check-graph-after.ps1` — повторные **18/18 PASS/0**, corrupted=0.
Исходный FAIL не переписан. Файлы установленных модов агент не изменял.
Во время неудачной загрузки Windows возвращала `0x80070020` при открытии
трёх `data.pak` без записи; Restart Manager не определил владельца блокировки.
Workbench PID 29668 был запущен пользователем в 14:44, до работы агента;
считать его доказанным владельцем блокировки нельзя. Настройки загрузки,
ACL и процессы пользователя не менялись.

После восстановления пользовательский crash в 17:27:31 сохранил стек
`RHS_MorphedEquipmentStorageSlot.RefreshAttachedVisualState`; missing URGI
уже нет, но `DEFAULT_FAILED dressed=1 armed=1 patched=1` повторяется.
Этот полный набор с dump также скопирован в `user-crashes/`.

`compile-stage.ps1 -Label baseline2 -Stage baseline-stage` — **PASS/0**.
`runtime-stage.ps1 -Label baseline-repro -Stage baseline-stage` — воспроизведён
native crash, exit `-1073741819`, тот же RHS stack. Probe загружает 3347/3347
ресурсов, фиксирует **388 canonical переименований** (`resource-renames.json`).
После этого старый source отклоняет default kit из-за сравнения полного
старого имени URGI с новым именем entity. Проектные пути обновлены только
по этим наблюдениям; `rewrite-resource-paths.ps1` проверен на сохранение GUID.
API `Resource.GetResource().GetResourceName()` проверен в reference 1.8.0.13.

`compile-after.ps1 -Variant ArlandWCSRHS|EveronWCSRHS` и
`compile-stage.ps1 -Label fixed|matrix -Stage fixed-stage|matrix-stage` —
**4 PASS/0**, `Script validation successful`. До правки оба production WCS
также компилировались: compile не обнаруживал runtime несовместимость путей.
Аргументы берутся из canonical launcher Client DryRun; manifests и полные
`wb-*-after/`, `wb-stage-fixed/`, `wb-stage-matrix/` сохранены.

`runtime-stage.ps1 -Label fixed -Stage fixed-stage` — **PASS targeted gate,
native exit 0**: 3347 ссылок, invalid=0, renamed=0, ROSTER_READY,
нет DEFAULT_FAILED/KIT_FAILED, штатный RequestClose и `Game destroyed.`.
`runtime-stage.ps1 -Label matrix -Stage matrix-stage -Matrix` — **PASS targeted
gate, native exit 0**: infantry 20/20, FIA 13/13, default player kits 2/2,
пять cache guards, шесть source tabs, arsenal 13/13 обеих сторон,
heavy provider 68/68 US и 5/5 USSR, vehicle catalog failures=0.
Использованы штатные WCSInfantry/Catalog/Access fixtures только в stage.
Server manifest содержит точный CLI, ArlandWCSRHS/Easy и listener 2395.
Полные остановленные логи — `profile-baseline-repro/`, `profile-fixed/`,
`profile-matrix/`; exit codes и transcripts находятся рядом.
ENGINE F и VM exceptions после исправления отсутствуют. Resource/RPL
diagnostics и SCRIPT shutdown leaks сохранены отдельно: targeted PASS
не означает полностью чистые engine logs. В обоих исправленных логах
833 строки (E), из них 358 строк SCRIPT о shutdown leaks; прочих SCRIPT E/F
нет. До исправления crash обрывал запуск раньше, поэтому эти числа нельзя
считать сопоставимым улучшением или ухудшением полного log gate.
Runtime других карт и client/Host,
JIP, визуальная проверка, soak, packaged build и Workshop upload — **NOT RUN**.
Опубликованный мод остаётся прежним до отдельного выпуска обновления.

Offline baseline: `Invoke-AICFChecks.ps1` — **62/62 PASS**, 49 checks
в `offline-before/` до прерывания пользователем, затем 13 оставшихся
в `offline-remaining/` с native exit 0. До окончания обеих частей production
и tests не менялись. Итог с уникальными именами — `offline-baseline-combined.json`.
После правки `Invoke-AICFChecks.ps1 -Name <девять затронутых audits>
-EvidenceRoot <after-targeted>` — **9/9 PASS, exit 0**: WCSIntegration,
RHSIntegration, AILoadout, PersonalLoadout, FIAArmament, InfantryRecruitment,
ScenarioHeaders, Stage35, Stage4. FIAArmament — 16 rules / 13 negative
mutations, включая возврат старого пути. Baseline failures отсутствуют.
Полный offline набор после правки не повторялся; все gates и их ограничения
сохранены раздельно. `git diff --check` и локальные Markdown links — PASS.
`production-hashes-final.json` / `source-stage-verdict.json`: все 266 production
`.c/.conf/.gproj` совпали с проверенным matrix stage. Fixture hashes сохранены
отдельно. Три созданных stage удалены после сохранения полных logs/manifests;
живых engine processes этой задачи не осталось. Push, merge, упаковка и
публикация не выполнялись.

## Оформление 0.1.28 — 2026-10-15

Ветка `codex/release-0.1.28` от обновлённого `origin/main`
`44166247cae4ff129196409dd56e9dc4f6b4ca49`; исходное дерево чистое.
Добавлены `releases/0.1.28.md`, ссылки на последнюю версию в README/указателе
и эта запись. Состав сверен с `git log 0.1.27..HEAD`, first-parent историей
`main`, diff и результатами задач ниже. Production `.c/.conf/.gproj/.st`,
headers, зависимости, `tests/` и `tools/` не менялись.

Свежий `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
tools/Invoke-AICFChecks.ps1 -EvidenceRoot .codex-runtime/release-0.1.28/before`
— **62/62 PASS, native exit 0**. Прогон начат до правки документации;
проверенные исходники и аудиторы остались неизменными до его завершения.
Все checks включены; `context.json`, `summary.json` и полные outputs
содержат source SHA, исходный status, точные argv, native exits и verdict.
Текущих offline baseline failures нет; старые FAIL в записях ниже являются
историческими. Отдельный sparse-slot restore runtime FAIL этим не закрывается.

`pwsh -NoProfile -File
.codex-runtime/release-0.1.28/Verify-ReleaseDocs.ps1 -Phase before|after`:
`git diff --check` и локальные Markdown links — **PASS/0** до/после.
Проверено наличие целей в Git для чистого checkout, включая новый release file.
`git diff --cached --check` — **PASS/0** перед коммитом.
Повтор полного offline набора после правки только Markdown не выполнялся:
неизменность production/tests/tools отдельно проверена через `git diff
--exit-code 4416624 -- '*.c' '*.conf' '*.gproj' '*.st' 'tests/**' 'tools/**'`.

Evidence: `.codex-runtime/release-0.1.28/` — исходные SHA/status, история,
`before/`, `before-check.txt`, `after-check.txt`, `source-scope-check.txt`,
`offline-verdict.json` и итоговые Git/publication metadata. Релизный тег
создаётся на итоговом `main` после слияния патчноутов; публикация GitHub Release
берёт notes из этого commit и проверяет remote tag, текст и статус latest.
Команда: `pwsh -NoProfile -File
.codex-runtime/release-0.1.28/Publish-GitHubRelease.ps1
-ExpectedSourceCommit <полный merged-main SHA>`.

Новые Workbench, server/client runtime, единая runtime-матрица релиза,
полный multiplayer/client-JIP новых механик, ручная визуальная/аудиопроверка,
soak, packaged build, чистая установка и Workshop upload — **NOT RUN**.
Изменение только документации не требует нового compile/runtime gate.
В патчноутах сохранены открытый `INVENTORY_RESTORE_FAILED` редактора,
неподтверждённое исправление `REPLICATION/FLOODED` и native/content diagnostics.
Workshop публикует владелец отдельно; Git-тег не является проверкой пакетов.

## Штатная охрана при подходе ИИ — issue #54, 2026-10-15

Ветка `codex/issue-54-fia-activation` от актуального `origin/main`
`d30629d`. Evidence: `.codex-runtime/issue54-fia-activation/`.
Production: новый `AICF_ManagedObservers` и десять строк orchestration в
MatchController. Native observers следуют за живыми READY US/USSR силами;
состав, spawn queue, budget, elimination и respawn остаются native.
Контракт и запуск fixture: [AMBIENT_PATROLS.md](AMBIENT_PATROLS.md).

Baseline `tools/Invoke-AICFChecks.ps1 -EvidenceRoot <before>` —
**61/61 PASS, exit 0**, включая FIA, authority и lifecycle audits.
Исходных offline FAIL нет. Финальный `Invoke-AICFChecks.ps1 -EvidenceRoot
<after>` — **62/62 PASS, exit 0**; полный вывод и argv каждого check в `after/`.
Новый `Test-ManagedObserversContracts.ps1`: **14 rules / 14 negative
mutations PASS**. Никакой обязательный check не исключался из runner.

Исходный дефект воспроизведён тем же общим stage-only probe на production
`d30629d`: `runtime.ps1 -Label baseline-proof -Port 2375` — native exit 0,
**5/5 reproduction checks**, штатный RequestClose. В полном stopped log
`profile-baseline-proof/logs/logs_2026-10-15_13-59-16/console.log` живой US
боец находится на суше в 525 м от native FIA точки, observer отсутствует при
native радиусе 776.209 м и доступном AI budget. Spawn point не eliminated,
controller отсутствует, живых FIA 0. `AMBIENT_REPRO` явно фиксирует
expected_active=1 / actual_active=0 и `NO_NATIVE_OBSERVER`.

`compile.ps1 -Variant <Stock|Everon|RHS|ArlandWCSRHS>
-Label production-<Variant>` — terminal Workbench **4 PASS / native exit 0**,
`Script validation successful`, SCRIPT E/F и ENGINE F/VM — 0. Прочих resource
строк (E) — соответственно 25/25/26/39. Полные logs `wb-production-*/`,
точные argv `wb-*-argv.json`, итоги `compile-summary-final.json`.
Изолированные fixtures также прошли Workbench; версия игры/API 1.8.0.13.

Итоговые `runtime.ps1` вызывают canonical launcher с fresh profiles,
отдельным stage, реальным listener через `-addr` и `-aicfAmbientProbe us`:

| Variant / difficulty | Label | Checks | Native / log-audit exit |
|---|---|---|---|
| Stock / Easy | final-Stock | 41/41 | 0 / 0 |
| Everon / Easy | final-Everon | 41/41 | 0 / 0 |
| RHS / Medium | rhs-matrix2 | 41/41 | 0 / 0 |
| ArlandWCSRHS / Medium | final-ArlandWCSRHS | 41/41 | 0 / 0 |

Каждый прогон проверил US и USSR: физический подход, живых активных бойцов
через 20 секунд, удержание 15 lifecycle интервалов, native dematerialization
после ухода, повтор без новых controller/лишних бойцов, реальную смерть и
повтор без воскрешения, native отказ при budget limit 1, пустой/погибший roster
с controller у точки. Отдельная исходная база меняет faction и штатно
eliminates ещё не появившуюся охрану. Дополнительные гарнизоны RHS/WCS
сравниваются по исходным vehicle/group/member identities до и после подходов.
В RHS/WCS каждый прочитанный native FIA member сопоставлен с
`PMC_EQUIPMENT_APPLIED inventory_verified=1` того же entity ID.
Повторный Stop и Update после Stop не возвращают observers; native readback
через секунду сохраняет исходное число чужих observers (0).

Все серверы завершены штатным RequestClose и `Game destroyed.`. Полные
console/script/error logs сохранены в `profile-<Label>/logs/`, launcher commands
и `AICF_RUNTIME_MANIFEST_JSON` — `server-<Label>-launcher.txt`; native exits
рядом. `Test-AmbientPatrolLog.ps1 -LogPath <console.log> [-Baseline]` —
**5 PASS / exit 0** с baseline. `check-log-negatives.ps1` — **12 ожидаемых
FAIL/1**, включая missing cases, inactive members, budget/CLI/profile evidence,
незавершённый log, VM error, leaked AICF type и PMC failure. Это PASS проверки
анализатора, а не дополнительные runtime cases.

`runtime-summary-final.json`, `companion-logs-final.json` и `*-analysis.txt`
фиксируют полное чтение 15 остановленных log files. Script execution/VM/fatal
errors, PMC failures и leaked AICF types — 0. **Native/content gate — WARN**:
native leak reports / прочие (E) в console.log: baseline 246/216,
Stock 327/353, Everon 323/336, RHS 387/500, WCS 436/660. Уже исходный код
печатает эту категорию shutdown/resource diagnostics; различающиеся числа
и типы объектов не объявляются неизменным baseline или clean-log PASS.

Ранние попытки не являются финальным PASS: fixture исправлена после неверного
перемещения, чтения death/observer cache в том же кадре, восстановления builder
capacity floor и попытки negative spawn на неподготовленной позиции.
Первый RHS выявил ошибку новой policy: runtime faction state тоже требует
нормализации через content profile; исправлено и повторено обеими сторонами.
Первый расширенный stock log audit ошибочно определял RHS по упоминанию GUID
в resource diagnostics; теперь читает точный `-addons` native CLI, отрицательный
profile case сохранён. Исходные outputs и failed/invalid попытки не удалены.

`source-hashes-final.json`: все **267** production `.c/.conf/.gproj/.st`
совпадают со stage; fixture hashes отдельно. Baseline controller Git blob
точно совпал с `d30629d`; его полный source snapshot hashes сохранён перед
удалением. Оба stage удалены; своих engine processes 0, чужие не затрагивались.
`git diff --check` и 230 local Markdown links — PASS.
Точный перечень затронутых файлов — `changed-files.txt` в evidence.
Client/JIP, визуальный бой, долгий soak и packaged build — **NOT RUN**;
они не входят в server-only checklist issue #54. Отклонённых критериев нет.

## Правила выпуска PR

Известный baseline FAIL не является разрешением на выпуск PR. До создания
готового к review PR или перевода draft в ready полный offline набор и
обязательные compile/runtime gates должны пройти. Каждое падение нужно
разобрать: исправить дефект либо доказать устаревание проверки по реализации,
истории и актуальному контракту. Проверка после исправления должна по-прежнему
отклонять отрицательные мутации; исключение теста или ослабление ради PASS
не допускаются. Старые отчёты ниже описывают исторические результаты и
не устанавливают исключений из этого правила.

Если исправление либо обязательная проверка требуют игрока, явно выдели
blocker в отчёте и PR: наблюдаемая проблема, evidence, конкретное действие
игрока и оставшийся недоказанный результат. Сохрани `FAIL`/`NOT RUN`, продолжи
доступную работу и оставь PR draft. Перенос критерия в отклонённые не снимает
этот blocker и не превращает его в PASS. Внешний вид, JIP и ручной матч,
не входящие в проверенный scope, также перечисляются явно.

## PR #53: документация pending gate — 2026-10-15

Продолжение `codex/issue-52-fia-armament` от `af2269e`. Исправлена устаревшая
формулировка в `FIA_GARRISONS.md`: оба boarding gates ждут успешный commit;
отказ `Apply` сохраняет pending после завершения callback. Снятие флага
принадлежит `AICF_MarkPMCEquipmentApplied`, retry сохраняет выбор бойца.
Production sources и probes не менялись; изменены только эта заметка и
`FIA_GARRISONS.md`. Evidence: `.codex-runtime/issue52-doc-review/`.

`Invoke-AICFChecks.ps1 -Name Test-FIAArmamentContracts.ps1,Test-RHSIntegrationStatic.ps1
-EvidenceRoot <before|after>` — **2 PASS / exit 0** до и после; baseline failures
нет. `git diff --check` и проверка локальных Markdown links — **PASS**.
Полный offline набор, Workbench и runtime в этом исправлении документации —
**NOT RUN**; результаты исходной реализации ниже не являются новым прогоном.

## Issue #52: вооружение FIA RHS/WCS — 2026-10-15

Ветка `codex/issue-52-fia-armament` от актуального `origin/main`
`207c3341b866c1ff3ccb17558cd97d452e84932e`; исходное дерево чистое.
Evidence: `.codex-runtime/issue52/`. Game/Server/Tools **1.8.0.13**,
RHS **0.16.5237**, WCS RHS Weapons **8.3.0**. Пулы, точные canonical weapon/
magazine prefab и правила выбора — [FIA_ARMAMENT.md](FIA_ARMAMENT.md).

Baseline получен на неизменённом `git archive HEAD` (`before-frozen`):
`Invoke-AICFChecks.ps1 -Name Test-RHSIntegrationStatic.ps1,Test-WCSIntegrationStatic.ps1,Test-AILoadoutStatic.ps1,Test-FIAGarrisonContracts.ps1,Test-FIAPatrolStatic.ps1,Test-AICommanderModeStatic.ps1,Test-Stage3Static.ps1 -EvidenceRoot <before-frozen>`
— **7 PASS / exit 0**. После изменения тот же набор и новый
`Test-FIAArmamentContracts.ps1` (`after-targeted`) — **8 PASS / exit 0**;
новый audit: **16 правил / 12 отрицательных мутаций PASS**. Сохранённых
baseline failures профильного набора нет.

Полный финальный offline набор выполнен тем же `tools/Invoke-AICFChecks.ps1`
в четырёх независимых группах с `-Name <group> -EvidenceRoot <after-parallel/group-N>`:
**61 PASS / 0 FAIL, exit 0**. `aggregate-checks.ps1` сверяет объединение с полным
текущим inventory runner: каждый из прежних 60 и новый FIA audit присутствует
ровно один раз, исключённых checks нет. `after-parallel/summary.json` и
`offline-verdict.json` содержат commands, native exits и verdict. Группы
определены в `audit-groups.json`. Начальный `before-full` (60 PASS) читал
менявшееся дерево и не используется как baseline; последовательный `after-full`
явно помечен `INCOMPLETE` после перехода на группы. Финальный PASS основан
только на завершённых `after-parallel`.

`compile.ps1 -Variant Stock|RHS|EveronNorthWCSRHS -Label production-<stock|rhs|wcs>`:
**3 PASS / native exit 0**, `Script validation successful`, script/fatal/VM
errors в полных production logs — 0. `-SourceRoot <stage>` также компилирует
новые fixtures; отдельный `legacy-fixtures` compile подтверждает пять
обновлённых прежних probes на WCS graph. Runtime этих прежних probes заново
не запускался: новую выдачу подтверждает новая полная матрица.

`runtime.ps1 -Variant <variant> -Label <label>` вызывает только штатный
`tools/Start-AICFRuntime.ps1`, Medium, fresh profile, фактический listener
`-addr 127.0.0.1:2367`, `-aicfFIAArmamentProbe 1`,
`-aicfRequirePlayerForResult 0`. Запуски серверов последовательные.

| Variant | Label | Проверки / бойцы | Verdict / native exit |
|---|---|---|---|
| Stock, исходный source | `baseline-stock` | 45 / 16 | PASS / 0 |
| Stock, новый source | `stock-after` | 45 / 16 | PASS / 0 |
| RHS (Arland) | `rhs-verified` | 381 / 61 | PASS / 0 |
| EveronRHS | `rhs-everon` | 381 / 61 | PASS / 0 |
| EveronNorthRHS | `rhs-north` | 381 / 61 | PASS / 0 |
| ArlandWCSRHS | `wcs-arland` | 340 / 54 | PASS / 0 |
| EveronWCSRHS | `wcs-everon` | 340 / 54 | PASS / 0 |
| EveronNorthWCSRHS | `wcs-verified` | 340 / 54 | PASS / 0 |

`Test-FIAArmamentLog.ps1 -LogPath <полный console.log> -Profile Stock|RHS|WCS
-LauncherPath <server-label-launcher.txt> -NativeExitPath <server-label-exit.txt>`:
**8 PASS / exit 0** по полным остановленным logs. Каждый ресурс реально
выдан живому actor с loaded magazine и compatible spares; все варианты каждой
из 13 ролей и трёх AC prefab обязательны. Дополнительно проверены циклическая
выдача обычным riflemen, MG/AMG, secondary, native insertion refusal с rollback,
unavailable resource, retry и сохранение изменения боезапаса после commit.
Реальные patrol/garrison hooks подтверждают pending rejection до посадки и
готовность экипажа/десанта на всех шести профильных картах.

`collect.ps1` — **PASS / 0**: все восемь audits, manifests/CLI/native exits,
точное совпадение stock weapon/ammo inventories всех 16 source prefab до/после
(`stock-comparison.json`), SHA-256 production/stage sources (`source-hashes.json`).
Всего **2253/2253 cases**. `runtime-results.json` хранит пути полных logs
`profile-<label>/logs/<session>/console.log` и категории всех ошибок.

Полные runtime logs **не чистые**: сохранены WORLD/ENTITY/RESOURCES/MATERIAL
diagnostics зависимостей, в WCS также RPL errors, и native script leak reports
при shutdown. Stock категории и количества ошибок до/после совпали полностью.
В профильных logs единственная transaction error находится внутри явно
обозначенного negative section (`restored=0`, `rollback=1`); неожиданных script
errors, compile/VM/fatal failures и проваленных production cases нет.

Удаление обязательного rollback case или canonical readback из копии полного
log при неизменном общем числе cases даёт ожидаемый **FAIL / 1**:
`negative-audit-result.txt`. Это PASS проверки анализатора, не runtime PASS
мутированных входов. Первый uppercase canonical АК был исправлен по реальному
native readback; ранние diagnostic FAIL сохранены и не считаются финальными.

`git diff --check` и локальные Markdown links — **PASS**.
Собственные native test/Workbench процессы завершены; `stage`, `compat-stage`,
`baseline-source` и известный source-copy прерванного audit удалены после
сохранения хешей, полного evidence и исходного `baseline.zip` (`cleanup.json`).
**NOT RUN:** внешний вид, субъективный баланс, client/JIP, soak, packaged build,
повтор gameplay runtime прежних пяти probes, push/PR/merge/публикация. Активные
критерии issue не требуют ручного тестировщика; отклонённых критериев нет.

Изменены: RHS `AICF_RHSContentProfile.c`, `AICF_RHSPMCArmament.c`,
`AICF_RHSPMCEquipment.c`; WCS `AICF_WCSRHSContentProfile.c`; новый
`Test-FIAArmamentContracts.ps1`, `Test-FIAArmamentLog.ps1`, обновлённый
`Test-RHSIntegrationStatic.ps1`; новые `AICF_FIAArmamentProbe.c`,
`AICF_RHSFIAArmamentProbe.c`, `AICF_WCSFIAArmamentProbe.c`; прежние
`AICF_RHSPMCScenarioProbe.c`, `AICF_RHSPMCGarrisonProbe.c`, `AICF_RHSPatchesProbe.c`,
`AICF_RHSWardrobeShowcase.c`, `AICF_WCSInfantryProbe.c`; README, WCS/Everon WCS
README; `agent-notes/README.md`, `ARCHITECTURE.md`, `FIA_ARMAMENT.md`,
`TEST_FIXTURES.md`, `TOOLS.md` и эта заметка. Точный список — `changed-files.txt`
в evidence.

## PR #51: inventory identity и исправление baseline — 2026-10-15

Продолжение `codex/issue-6-squad-resupply`, исходный commit `1b57ec1`.
Evidence: `.codex-runtime/issue6-review/`. PR переведён в draft на время
исправлений; отсутствие новых FAIL больше не считается разрешением выпуска.

`AICF_EquipmentInventory.UniqueItems` исключает null и повторные ссылки на
entity перед накоплением патронов, запасных одноразовых launcher и расходников.
Разные entities одного prefab сохраняются. Оба текущих counter и target
`EquipmentPlan` используют этот контракт. Native getter остаётся штатным;
`RoundsIn`/`ConsumablesIn` позволяют проверить именно production accumulation
на детерминированном перечислении с дублями.

Причины всех шести прежних FAIL установлены до изменения ожиданий:

| Аудит | Причина и исправление |
|---|---|
| Endgame | Commit `b16b792` (#42) перенёс выбор в adaptive policy и ввёл dwell/stable gates обычного возврата из обороны. Тест искал удалённые `attackTarget`/`securityTarget` и немедленный обход gates. Теперь проверяет реальные `BuildCandidates`, `Select`, `ReconcileAICommanderOrder`: пригодные attack candidates, HQ fallback, выбранную posture и gates. 15 отрицательных мутаций. |
| InfantryApproach | `ClearRuntimeReferences` очищает `m_ApproachRoute`, но `73c8339` добавил перед этим сброс счётчика. Прежний regex требовал очистку первой строкой. Теперь проверяется тело метода; удаление очистки отвергается отдельной мутацией. |
| Issue12Recovery | `AssignAICommanderLossResponseOrder` делегирует guarded `ReconcileAICommanderOrder`. Тест требовал inline guards в обёртке. Теперь проверяются и фактический владелец holds, и точная делегация, с мутациями каждого из них. |
| MapPointOrders | Та же делегация: проверка ручного POSITION intent находится в reconcile, а не в loss-response wrapper. Оба звена обязательны; три мутации удаляют делегацию, destination validation или restore. |
| NorthFailure | `df1d77e` и соседние исправления добавили recruitment branch, захваченный `failedWaypoint` и `result`. Проверка искала прежние сигнатуру и callback expression. Обновлена к текущим identity/action fences; отдельно проверяется захват waypoint. Все 28 guards проверяются отрицательными мутациями. |
| Localization | Windows PowerShell 5.1 читал UTF-8 без BOM как ANSI и ломал parse на кириллице в regex. Unicode escapes сохраняют тот же диапазон, включая Ё/ё. Добавлены положительные/отрицательная проверки диапазона; audit исполняется в 5.1 и 7. |

Свежий preflight на неизменённом `git archive HEAD` (`before-frozen`):
7 проверок, **1 PASS / 6 FAIL**, exit 1. Те же 7 после исправлений
(`after-targeted`): **7 PASS**, exit 0. Новые Equipment contracts:
**20 правил / 15 отрицательных мутаций PASS**. Ранний полный preflight `before/`
прерван перед правками ради frozen snapshot, содержит `INCOMPLETE.txt` и
не является завершённым прогоном. Исходный полный baseline задачи сохранён
ниже и в `.codex-runtime/issue6/baseline-clean/`.

Полный `tools/Invoke-AICFChecks.ps1 -EvidenceRoot <after-full>`:
**60 PASS / 0 FAIL**, exit 0. Все исходные 60 проверок включены, runner и
список не сокращались. Сохранённых незакрытых baseline failures больше нет;
исторические 53/6 и 54/6 ниже остаются историческими, а не текущим verdict.

Новые терминальные проверки:

- `compile.ps1 -Variant Stock|RHS|EveronNorthWCSRHS -Label production-<variant>`:
  **3 PASS/native exit 0**, `Script validation successful`; script/fatal/VM
  errors в production validation отсутствуют.
- `runtime.ps1 -Variant Stock -Mode counts -Label counts-stock -Port 2366`:
  **11/11 PASS**, native exit 0. Loaded magazine и chambers, дубль запасного
  магазина, low threshold, расходники, две entities одного prefab и отсутствие
  мутации входного массива/боезапаса.
- В отдельном stage выключен только identity filter (`inventory-mutant.c`).
  Тот же `counts-mutant` имеет native exit 0, но **5 failed cases**, log audit
  возвращает ожидаемый **FAIL/1**. Это **PASS отрицательного regression**,
  зафиксированный в `native-mutation-regression.json`; mutant не является
  production PASS. Перед остальными запусками stage восстановлен и пересобран.
- `runtime.ps1 -Mode safety -Variant Stock|RHS|EveronNorthWCSRHS -Port 2366`:
  **56/56, 67/67, 129/129 PASS**, native exits 0, **3/6/16 native families**.
  `Test-EquipmentLog.ps1 -Mode Safety -ExpectedFamilies 3|6|16` — **PASS/0**
  по полным остановленным logs. Повторные disposable launchers также
  проверяются: одна entity не добавляет несколько выстрелов.
- `runtime.ps1 -Variant Stock -Mode 1 -Label full-stock -Port 2366`:
  **37/37 PASS/native exit 0**. Повторены физический подход/loot, частичное и
  достаточное пополнение, armory visit, принудительный отказ native выдачи,
  сохранение исходных entities при rollback, последующая актуальная revision
  и отсутствие повторного цикла. `Test-EquipmentLog.ps1 -Mode Full` — **PASS/0**.
- `runtime.ps1 -Variant EveronNorthWCSRHS -Mode inventory -Label inventory-wcs
  -Port 2366`: **27/27 PASS/native exit 0**. Живой actor получает актуальный
  WCS preset, native отказ выдачи восстанавливает прежний inventory, retry
  завершается; повторный визит не возникает. Полный log audit — **PASS/0**.
- Удаление обязательного `DUPLICATE_MAGAZINES_ONCE` из копии полного `counts`
  log при сохранении количества строк даёт ожидаемый **FAIL/1**, причина
  `Missing case`; `missing-case-regression.json` — **PASS отрицательной проверки**.
- `pwsh -NoProfile -File tests/static/Test-LocalizationStatic.ps1`:
  **PASS/0**, 602 записи; Windows PowerShell 5.1 проходит тот же audit
  в `after-targeted`.

Полные commands/argv, launcher manifests/native exits, console/script/error
logs и reports сохраняются в evidence. `collect.ps1` — **PASS/0**: CLI всех
семи runtime совпали с сохранёнными manifests, все **265** production
`.c`/`.conf`/`.gproj` и оба fixture совпали со stage по SHA-256. `results.json`,
`source-hashes.json`, `fixture-hashes.json` и `snapshot-provenance.json` фиксируют
проверенные входы; `changed-files.txt` — перечень 16 затронутых файлов.
Свои server/test процессы завершены; stage/frozen snapshot удалены после
сохранения provenance и полных logs. Пользовательские процессы сохранены.
`git diff --check` — **PASS/0**.

`SCRIPT (E)` leaked-instance/total-leak
диагностика при shutdown и resource/native keyword/RPC diagnostics не скрыты:
они отдельно классифицированы в `engine-diagnostics.json`. Например, исходный
Stock safety и новый имеют по 422 engine diagnostics, включая 259 shutdown
leak строк. Это не заявление об отсутствии всех native `(E)`; VM/null/fatal
ошибок и неизвестных script errors в новых успешных прогонах нет.

Участие игрока для исправлений и перечисленных проверок не потребовалось.
**NOT RUN:** визуальное взаимодействие, JIP, полный multiplayer матч; native
Endgame/recovery gameplay probes в этом продолжении отдельно не повторялись.
Обновлённые static contracts не выдаются за новый runtime verdict этих механик.

## Issue #6: живое пополнение и трофейный сбор — 2026-10-15

Ветка `codex/issue-6-squad-resupply` от `origin/main` `3ea79c6`.
Game/Server/Workbench **1.8.0.13**. Evidence: `.codex-runtime/issue6/`:
полные argv/manifests/native exits, остановленные console/script/error logs,
log-audit reports, matrices и source hashes. Ниже перечислены новые запуски;
неудачные промежуточные прогоны также сохранены и не считаются PASS.

- `tools/Invoke-AICFChecks.ps1 -EvidenceRoot <baseline-clean>` на отдельном
  неизменённом snapshot: **53 PASS / 6 FAIL**, 59 checks, exit 1.
  `after-final`: **54 PASS / те же 6 FAIL**, 60 checks, exit 1.
  После последних правок cleanup `after-verified-selected`: **10 PASS**, exit 0:
  Equipment, AILoadout, PersonalLoadout, SquadCommands, SquadRespawn,
  InfantryRecruitment, BarracksCombat, Stage3, Stage3Contracts, Stage4.
  Equipment audit: **16 rules / 11 negative mutations PASS**.
- Сохранённые baseline failures: Endgame — `ATTACK_FIRST`,
  `SECURITY_FALLBACK`, `ATTACK_RESUME`; InfantryApproach — `APPROACH_CLEANUP`;
  Issue12Recovery — `POSTURE_QRF_OWNER`; MapPointOrders —
  `MAP_POINT_REPLAN_PRIORITY`; NorthFailure — `MOVE_CALLBACK_FENCE`,
  `MOVE_ACTIVITY`; Localization — прежний UTF-8/PowerShell 5 `ParserError`.
  Более ранний полный after-final запуск отделён от финального targeted repeat.
- `compile.ps1 -Variant Stock|EveronNorthRHS|EveronNorthWCSRHS
  -Label production-verified-<stock|rhs|wcs>`: три production terminal
  Workbench **PASS/native exit 0**, `Script validation successful`.
  Stage Compile также выполнен; static, compile и runtime не заменяют друг друга.
- `runtime.ps1` использует только `tools/Start-AICFRuntime.ps1 -Role Server
  -Difficulty Medium -RepositoryRoot <isolated stage> -ProfileRoot <fresh profile>`
  и `-aicfEquipmentProbe <mode> -aicfRequirePlayerForResult 0`.
  Собственные listeners 2306/2316 подтверждены полными CLI Params и manifests;
  существующий пользовательский server не останавливался.

| Profile / mode | Новые runtime checks | Что подтверждено |
|---|---:|---|
| `stock-final-owned` / `1` | 37/37 PASS | Физический подход, те же item IDs, partial/sufficient loot, последний binding, rollback с исходными IDs/патронами, target, ledger, no-loop/intent |
| `wcs-detach-preview` / `inventory` | 27/27 PASS | Native живой WCS inventory, safe rollback/retry, последняя revision, target, no-loop и освобождение model worlds |
| `stock-verified-safety` / `safety` | 48/48 PASS, 3 families | Stock actual launcher/round transfers, spent-source refusal, capacity/slots, combat, competition, enemy/FIA sources |
| `rhs-verified-safety` / `safety` | 60/60 PASS, 6 families | Та же матрица с фактическими RHS entities |
| `wcs-verified-safety` / `safety` | 118/118 PASS, 16 families | Все доступные native families US/USSR/FIA текущего WCS/RHS catalog |
| `stock-complete-lifecycle` / `lifecycle` | 13/13 PASS | Nearest ARMORY, owner loss/delete/offline/retry, manual intent, recruitment/transport fences, death/replacement/Stop |
| `stock-search` / `search` | 6/6 PASS | Настоящий 45 s timeout заблокированного native movement, radius, identity tie-break, cooldown, сохранение inventory/потребности |
| `stock-player-unique` / `player` | 5/5 PASS | Connected PlayerController, запрещённый living/dead main source, takeover во время transaction, исходные IDs/rounds, отсутствие stale RestoreNew/revision и released flag |

Все server probes завершены `RequestClose`, native exit 0. Для player case
canonical Client launcher подтвердил точные server CLI, живой PID и ROSTER_READY;
`client-player-unique-launcher.txt` содержит readiness и argument manifest.
Client log сохранён после остановки собственного процесса. Native player
initialization добавляет radio gadget, а `GetItems` может повторять один ID;
сравнение проверяет уникальные исходные identities и реальные rounds.

`Test-EquipmentLog.ps1 -Mode <mode> -LogPath <полный остановленный console.log>
-OutputDirectory <audit> [-ExpectedFamilies <3|6|16>]`: все строки таблицы
проверяются по обязательным cases, завершению, счётчику failures, exceptions
и неполному rollback. Один положительный и **7 отрицательных** log inputs PASS.
Полная E/F-диагностика сохраняется отдельно в каждом report: nonfatal resource/
particle и unregistered private-preview damage RPC сообщения не скрываются;
PASS probes не означает отсутствие этих engine diagnostics. VM с удерживаемым
`MuzzleComponent` в промежуточных WCS runs устранена: equipment draft снимает
native root attachments до удаления private model, основной rifle muzzle
получает с owning entity независимо от выбранного UGL. Финальные logs без VM
и незавершённого inventory recovery.

Family identity — native default projectile/magazine + disposable + well.
Stock: RPG7, RPG22, M72. RHS: MK153, RPG7 PG7VL, RPG22, M72, RPOA, RPOD.
WCS: MK153; RPG7 PG7VL/PG7VR/PG7VM/PG7VM-stock/TBG7VL/TBG7V/OG7V;
RPG18/RPG22/M72; RPOA/RPOD; M2CG; FIM92; 9K38.
Список определяется enabled catalog и реальным `WT_ROCKETLAUNCHER`, не именем
ресурса. Ammo entries и другие native weapon types пропускаются; пустые
одноразовые образцы не переносятся. Точные GUID/prefab/projectile/well и native
readbacks — `audit-*/families.txt` и полные logs. Это матрица установленного
content graph, не обещание поддержки отсутствующих или будущих prefabs.

**NOT RUN:** внешний вид, ручное управление/взаимодействие игрока, JIP и полный
многопользовательский матч. Connected-player identity case не объявляется
проверкой этих сценариев. GUI, screenshots и ожидание ручного тестировщика не
использовались. Настройки/стоимость/lifecycle — [EQUIPMENT_SERVICE.md](EQUIPMENT_SERVICE.md),
scope и постановка — [TEST_FIXTURES.md](TEST_FIXTURES.md).

## Issue #48: постоянный запрет recovery FIA после урона/потерь — 2026-10-14

Ветка `codex/fia-damage-recovery` от актуальной `origin/main` (`07350fc`).
Game/Server/Workbench **1.8.0.13**. Evidence: `.codex-runtime/issue48/`;
helpers `compile.ps1`, `runtime.ps1`, `collect.ps1`, `negative-audits.ps1`,
полные argv/manifests/native exits, console/script/error logs и source hashes.
Результат: server-only latch исходной vehicle identity/generation, события
здоровья всего hit-zone hierarchy, полный исходный roster и повторные gates
до поиска и перед transform/forced boarding, включая синхронные callbacks.
Первый reason сохраняется. Repair/heal не снимают ban; CanDrive независим,
ожидание запрещённого crew recovery снимается. Replacement нет.

Только новые проверки этой задачи:

- `tools/Invoke-AICFChecks.ps1 -Name Test-AICombatInputContracts.ps1,
  Test-FIAGarrisonContracts.ps1,Test-FIAPatrolStatic.ps1,
  Test-Stage35RecoveryPolicy.ps1,Test-Stage35Static.ps1,Test-Stage3Static.ps1,
  Test-Stage3StaticContracts.ps1,Test-NorthFailureContracts.ps1
  -EvidenceRoot .codex-runtime/issue48/before`: **7 PASS, 1 FAIL**, exit 1.
  `after-final/` с добавленным `Test-FIAGarrisonIntegrityContracts.ps1`:
  **8 PASS, 1 тот же FAIL**, exit 1. Сохранённые baseline failures:
  `NorthFailureContracts` — `MOVE_CALLBACK_FENCE`, `MOVE_ACTIVITY`, без изменений.
  Новый integrity audit: **24 правила и 25 отрицательных мутаций PASS/0**;
  дополнительно проверяет initial/post-queue/post-waypoint gates.
- `compile.ps1 -Variant Stock|EveronNorthRHS|EveronNorthWCSRHS
  -Label final-<Variant>` — три production Workbench **PASS/native exit 0**,
  `Script validation successful`; SCRIPT E/F, ENGINE F, VM exceptions — 0.
  Прочих (E) строк соответственно 25/26/39. Stage с DamageProbe,
  `-Label fixture-final-complete -SourceRoot <stage>` — **PASS/0**, 39 прочих (E).
  Точные аргументы и полные логи: `compile-summary.json`, `wb-*-argv.json`.
- `runtime.ps1 -Mode damage -Label damage-final-complete` — canonical
  `tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthWCSRHS
  -Difficulty Hard -RepositoryRoot <stage> -ProfileRoot <fresh profile>
  -AdditionalArguments -addr,127.0.0.1:2348,-aicfDamageRecoveryProbe,damage,
  -aicfRequirePlayerForResult,0`: **37/37 PASS/native exit 0**.
  Настоящий HandleDamage: Hull и Engine_01 танка Т-72А/БТР-70, 5% здоровья;
  сохранение CanDrive, оба recovery reasons, отсутствие transform/counter
  изменений, ремонт между ticks, leg и три retry. Ранение/лечение живого
  essential crew блокируют forced return; отдельно Kill водителя, наводчика,
  командира и пассажира. Обычная высадка живого десанта и positive recovery
  исходного БТР проходят без ban. Ban reason после Kill essential crew может
  быть CREW_WOUNDED: health callback фиксирует первое повреждение до death state.
- `runtime.ps1 -Mode identity -Label identity-final-complete` с тем же launcher
  и отдельным fresh profile: **25/25 PASS/native exit 0**. Реальное удаление
  member, stale/replaced roster identity, stale vehicle/generation, отсутствие
  damage observer/полного roster; synthetic player veto, 49/50/51 м и отказ
  без geometry candidate. HandleDamage инъецирован после production Candidate
  и перед forced return после физического завершения выхода из compartment;
  финальные gates предотвращают мутации.
- `tests/log-audits/Test-FIAGarrisonRecoveryLog.ps1 -LogPath <полный console.log>
  -DamageMode damage|identity` — оба **PASS/0**, по 9 banned identities;
  после ban нет vehicle/crew teleport events для этой identity. Оба сервера
  сами завершились через RequestClose, listener 127.0.0.1:2348 подтверждён.
  В каждом полном log SCRIPT E/F, ENGINE F, VM exceptions — 0;
  233 прочих (E) из ресурсов/keyword compatibility/pathfinding/shutdown.
  `runtime-summary.json`, `damage-audit-final.txt`, `identity-audit-final.txt`.
- `negative-audits.ps1`: 3 копии полного лога с teleport после ban,
  отсутствующим required case или удалёнными ban diagnostics дают ожидаемый
  **FAIL/1** с конкретной причиной; negative regression **PASS/0**.
- `collect.ps1` — **PASS/0**: все **259** production `.c`/`.conf`/`.gproj`
  совпали со stage по SHA-256. Fixture присутствует только в stage.
  `git diff --check` — **PASS/0**.

Финальные остановленные console logs относительно evidence root:

- `profile-damage-final-complete/logs/logs_2026-10-14_23-03-09/console.log`;
- `profile-identity-final-complete/logs/logs_2026-10-14_23-04-36/console.log`.

Ранние `damage-first`, `damage-second`, `damage-port` не являются runtime PASS:
replication listener 2001 был занят пользовательским сервером; `-bindPort`
в source режиме не изменил listener. После перехода на `-addr` фактический
listener проверен в полном log. `identity-final` — **FAIL, 1 case**: probe
проверял final boarding fence до завершения GetOutVehicle. Fixture исправлена
с ожиданием физического transition; `identity-async-final` и окончательный
`identity-final-complete` проходят. Старые и неуспешные логи сохранены отдельно.

Изменённые файлы (15): Core `AICF_FIAGarrison.c`, новый
`AICF_FIAGarrisonIntegrity.c`, `AICF_FIAGarrisonCrew.c`,
`AICF_FIAGarrisonRecovery.c`, `AICF_FIAGarrisonService.c`,
`AICF_VehicleSpawner.c`; tests новый `AICF_FIAGarrisonDamageProbe.c`, новый
`Test-FIAGarrisonIntegrityContracts.ps1`, `Test-FIAGarrisonRecoveryLog.ps1`;
`README.md`, `agent-notes/README.md`, `ARCHITECTURE.md`, `GAMEPLAY.md`,
`TEST_FIXTURES.md`, эта `TESTING.md`.

Полный offline набор, реальный connected player возле переноса, client/JIP,
визуальные проверки, soak и исходный игровой эпизод пользователя — **NOT RUN**.
Synthetic player veto не заменяет discovery реального PlayerManager.
Все активные критерии issue проверены терминально; исходный эпизод не объявляется
воспроизведённым. Свои процессы завершены; stage удалён после сохранения
evidence. Пользовательский сервер на 2001 не останавливался.

Перед правкой выполни относящиеся к домену проверки и сохрани полный вывод,
commit/branch, dirty status и exit codes. После правки повтори тот же набор и
сравни конкретные rule IDs. Известный FAIL не скрывай и не исправляй regex ради PASS.

Для работы по issue применяй [правила приёмки и привязки PR](../AGENTS.md#работа-по-issue-и-привязка-pr):
в активном checklist остаются только самостоятельно проверяемые агентом критерии.
Выполни их все; недоступные проверки явно отклони с причиной и ограничением
`NOT RUN` в отдельном разделе issue, без ожидания игрока/тестировщика. Исторические
verdict ниже сохраняют фактический статус; отклонение критерия не даёт новый PASS.

## PR #47: поздний registered sentinel — 2026-10-14

Продолжение `codex/infantry-move-vm-recovery` от `df1d77e`, дерево до правки чистое.
[Замечание P2](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/47#discussion_r4273866199)
подтверждено: после выхода и нового point order старый `-1` принимался только
по текущему Move/roster, затем failure привязывался к новому waypoint.
Script Diff `1.8.0.13` предоставляет result, handler, waypoint-related flag и
location; identity/generation исходной операции в этих портах нет.

Выбран fail-closed: при `FindVehicleBySubgroupId(-1)` classifier безусловно
возвращает false до infantry/recruitment report и `OnMoveFailed`. На native
границе поведение сохранено; обхода registry по типу нового action больше нет.
Пеший `-1` без registry entry сохраняет физические/parent-group guards,
default/positive handlers и callback fences сохранены. Прежняя попытка
разблокировать ambiguous registry **отозвана**: этот PR не доказывает устранение
исходной native VM и не объявляет зарегистрированный `-1` пешей операцией.
Обещание такого восстановления удалено из README; описание PR соответствует
консервативной защите и regression coverage.

Затронуты `Forces/AICF_AINodeLifecycle.c`, InfantryMoveHandler contract/fixture,
README и agent-notes ARCHITECTURE/README/TEST_FIXTURES/TOOLS/TESTING. Evidence:
`.codex-runtime/move-vm-review47/`. Команды из корня checkout:

- `tools/Invoke-AICFChecks.ps1 -Name Test-MovementRecoveryContracts.ps1,Test-NorthFailureContracts.ps1,Test-DefendWaypointInputContracts.ps1,Test-Issue12RecoveryContracts.ps1,Test-Issue14RecoveryContracts.ps1,Test-InfantryRecruitmentStatic.ps1,Test-RecruitmentPriorityContracts.ps1,Test-AdaptiveCommanderContracts.ps1,Test-InfantryMoveHandlerContracts.ps1 -EvidenceRoot <before|after>`:
  оба **7 PASS / 2 прежних FAIL, exit 1**. Ровно прежние `POSTURE_QRF_OWNER`,
  `MOVE_CALLBACK_FENCE`, `MOVE_ACTIVITY`, новых failures нет. Новый method-scoped
  audit **PASS/0**, семь отрицательных guard mutations отклонены.
- Evidence helper `compile.ps1 -Variant EveronNorthWCSRHS -Attempt fixture-before|fixture-after|production -SourceRoot <stage-before|stage-after|.>`:
  все три терминальных Workbench **PASS/0**, validation successful;
  SCRIPT E/F, VM и ENGINE F **0**, RESOURCES E **39** на каждом.
  Exact argv, manifests и полные logs — `compile-summary.json` и соответствующие
  `workbench-*/console.log`.
- `runtime.ps1 -Stage stage-before|stage-after -Variant EveronNorthWCSRHS -Difficulty Medium -Attempt baseline|fixed -Port 2337 -Probe aicfInfantryMoveHandlerProbe`:
  одинаковый новый fixture, девять живых бойцов, реальный APC, посадка/выход,
  новый Move и waypoint/assignment при той же group generation. Старые location,
  waypoint identity и action сохранены до замены. **До 25 checks / 5 failures,
  после 25/25 PASS**, оба native exits 0. До правки принимается registered
  sentinel, меняются новый failure/action и вызывается callback; после отказа
  новый waypoint, assignment, executed action/state/remove flag сохраняются,
  callback не вызывается. Positive control без registry сообщает свежий failure
  ровно одним callback, generation/assignment fences проходят.
  Полные остановленные logs — `server-baseline/logs/` и `server-fixed/logs/`;
  SCRIPT E/F, VM, ENGINE F **0/0**, RESOURCES E **51/51**.
- `audit.ps1 -Attempt baseline|fixed` вызывает tracked contract с полным
  `-RuntimeLogPath` и actual `-ManifestPath`: baseline ожидаемый **FAIL/1**,
  fixed **PASS/0**, exact CLI и все 25 cases проверены. `log-mutations.ps1`:
  positive **PASS/0**, шесть негативных logs (VM, fatal, failed late-result case,
  missing case, missing completion, altered CLI) отклонены **FAIL/1**;
  outputs/argv/exits — `log-mutations/summary.json`.
- `git diff --check` — **PASS/0**. Свои probe/Workbench процессы завершены,
  ports свободны. Временные stage удалены после сохранения inputs/hashes.
  Три ранее остановленных пользователем сервера и heartbeat не запускались.

Native BT replay, клиент/JIP, долгая кампания и полный offline набор — **NOT RUN**.
Runtime инъецирует вход в production helper, не подменяет native BT wiring.
Исходная VM, старые Easy deadlines и engine/resource baseline не заявлены
исправленными. Merge/релиз не выполнялись, новой версии нет.

## Пеший failed movement с unassigned vehicle — исторический `df1d77e`, 2026-10-14

Ветка `codex/infantry-move-vm-recovery` от свежего `origin/main` `62d9b69`;
исходное дерево чистое. На трёх WCS North серверах полный остановленный log
Medium содержит **11 VM** из stock `NodeError` в `MoveToLocation.bt`: US A4,
`handler=-1`, `SCR_AIMoveActivity`, related/owned/current waypoint совпадают.
В stock registry новая `SCR_AIGroupVehicle` тоже имеет subgroup `-1`.
Прежний classifier безусловно отвергал такую запись даже для пешего roster;
этот отказ воспроизведён отдельным probe. Наличие именно этой записи в исходном
Medium log не логировалось, поэтому исходный случай не является полным доказательством
внутреннего состояния registry.

В первоначальной реализации свободная запись допускала текущий Move с повторной физической
проверкой всех живых members и их parent group. Positive vehicle handler,
boarding/dismount, пустой roster, неизвестный handler и missing access остаются
за native границей. Slot/recruitment ownership, authority и callback fences
сохранены; global `NodeError` не подавлялся. Это допущение отозвано по review #47:
снимок нового action/roster не является identity исходной операции. Исторические
PASS ниже относятся к старому fixture и не подтверждают корректность позднего callback.

Изменённые файлы: `Forces/AICF_AINodeLifecycle.c`,
`tests/contracts/Test-InfantryMoveHandlerContracts.ps1`,
`tests/fixtures/AICF_InfantryMoveHandlerProbe.c`, README и agent-notes
ARCHITECTURE/README/TEST_FIXTURES/TOOLS/TESTING. Все пути Core относительно
`AIConflictCore/Scripts/Game/AIConflict/`. Evidence: `.codex-runtime/move-vm-recovery/`.

Команды из корня checkout и отдельные verdict:

- `tools/Invoke-AICFChecks.ps1 -Name Test-MovementRecoveryContracts.ps1,Test-NorthFailureContracts.ps1,Test-DefendWaypointInputContracts.ps1,Test-Issue12RecoveryContracts.ps1,Test-Issue14RecoveryContracts.ps1,Test-InfantryRecruitmentStatic.ps1,Test-RecruitmentPriorityContracts.ps1,Test-AdaptiveCommanderContracts.ps1 -EvidenceRoot <before|after>`:
  **до 6 PASS / 2 FAIL, после те же 6 PASS / 2 FAIL**, общий exit 1.
  After дополнительно включает новый InfantryMoveHandler audit **PASS/0**:
  итог **7 PASS / 2 FAIL**. Сохранены ровно прежние `POSTURE_QRF_OWNER`,
  `MOVE_CALLBACK_FENCE`, `MOVE_ACTIVITY`; новых failures выбранного набора нет.
- `powershell.exe -NoProfile -File tests/contracts/Test-InfantryMoveHandlerContracts.ps1`:
  **PASS/0**, семь method-scoped guards и семь отрицательных мутаций.
  С `-RepositoryRoot <stage-before>` исходный classifier отклонён **FAIL/1**
  по `SENTINEL_SCOPE`, `UNASSIGNED_MOVE`, `MEMBER_IDENTITY`.
- Evidence helper `compile.ps1 -Variant EveronNorthWCSRHS -Attempt production -SourceRoot .`
  и `-Attempt fixture-restored-before|fixture-restored-after -SourceRoot <stage-before|stage-after>`:
  все три терминальных Workbench **PASS/0**, `Script validation successful`,
  SCRIPT E/F, ENGINE F, VM — **0**; RESOURCES E — **39** на каждом. Source graph
  получен canonical launcher DryRun; exact argv и полные logs сохранены.
- `runtime.ps1 -Stage stage-before|stage-after -Variant EveronNorthWCSRHS -Difficulty Medium -Attempt handler-restored-before|handler-restored-after -Port 2339|2337 -Probe aicfInfantryMoveHandlerProbe`:
  одинаковый окончательный fixture, реальные девять бойцов, свободный APC,
  посадка/выход и новый point order через production planner. **До 19 checks /
  5 FAIL, после 19/19 PASS**; оба native exits 0. Baseline отвергает пеший sentinel
  до/после выхода, не сообщает failure владельцу и потому не создаёт fenced
  failure. Полные остановленные logs — `server-handler-restored-*/logs/`;
  SCRIPT E/F, ENGINE F, VM **0/0**, RESOURCES E **51/51**.
- `audit.ps1 -Attempt handler-restored-before|handler-restored-after` вызывает
  tracked contract с `-RuntimeLogPath` и actual `-ManifestPath`: ожидаемый
  baseline **FAIL/1**, исправление **PASS/0**, exact CLI и все 19 cases проверены.
  `log-mutations-final.ps1`: positive **PASS/0**; шесть испорченных полных logs
  (VM, fatal, failed/missing case, missing completion, изменённый CLI) отклонены
  **FAIL/1**. Outputs/argv/exits — `log-mutations-final/summary.json`.
- `git diff --check` — **PASS/0**. Все isolated тестовые процессы завершены;
  временные stage удалены после сохранения исходников и hashes, логи сохранены.

Ранний baseline `handler-before` с 12-case fixture воспроизводит три failures.
`handler-final-before` с 18-case fixture остановился на `SEAT_TIMEOUT` и не
выдаётся за полный baseline: stock compartment callbacks завершили старый
waypoint. Окончательный 19-case fixture восстанавливает point order через planner
на обеих версиях. `native-before`/`native-after` переносили группу на исходные
координаты, но **не воспроизвели UNKNOWN**: summaries содержат 3/1 failures,
оба native exits 0, VM 0; критерий воспроизведения не пройден.
Экспериментальный replay сохранён только в evidence, из tracked fixture удалён.
Прохождение инъекции helper не подтверждает повтор исходной native ошибки.

Три пользовательских сервера обновлены на прежних `127.0.0.1:2301/2303/2305`
через canonical launcher, свежие profiles, `-addr`, BOTH и Easy/Medium/Hard.
Перед maintenance stop проверены точные PID/profile и Player 0; старые полные logs
сохранены как `stopped-original-*.log`. В старом Easy остаётся отдельный
`MEANINGFUL_TASK_DEADLINE_MISSED` + `CORE_ERROR_BRIDGE` (2 SCRIPT E, VM 0);
эта правка его не устраняет. Старый Hard — SCRIPT E/F и VM 0.
В `deployed/` находятся actual manifests, exact CLI/listener/ROSTER_READY snapshots
и SHA-256 исправленного Core. В свежих snapshots всех трёх SCRIPT E/F и VM 0;
они оставлены работать и **не являются full stopped runtime PASS**. Profiles
запущены из dirty checkout до итогового commit; production SHA-256 совпадает.

Полный offline набор, клиент/JIP, визуальная проверка и повтор исходной native VM
— **NOT RUN / NOT REPRODUCED**. Долгая кампания и все engine/resource errors
не заявлены исправленными; последние сохранены в полных logs. Merge и релиз
не выполнялись, новой версии нет.

## PR #45: индекс узла в stable-order contract — 2026-10-14

Продолжение `codex/issue-44-audit-contracts` от `4b2b8e9`, исходное дерево чистое.
[Замечание review](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/pull/45#discussion_r4271564663):
возрастающий counter не гарантирует возрастающий обход, если `GetNode`
индексируется в обратном порядке. Оба `SELECTION_STABLE_ORDER` теперь требуют
первое чтение `AICF_ObjectiveNode` непосредственно в теле соответствующего цикла:
`GetNode(index)` для nearest и `GetNode(nodeId)` для candidates. Маска Code
исключает подмену comments/strings. Добавлены две source mutations обратных lookup;
production selection и общий helper не изменены. Evidence:
`.codex-runtime/issue44/review45/`.

Команды из корня checkout и отдельные verdict:

- `& tools/Invoke-AICFChecks.ps1 -Name @('Test-AICommanderModeStatic.ps1', 'Test-SquadCommandsContracts.ps1', 'Test-RecruitmentSupplyContracts.ps1', 'Test-RecruitmentPriorityContracts.ps1', 'Test-AdaptiveCommanderContracts.ps1') -EvidenceRoot .codex-runtime/issue44/review45/before`:
  **5/5 PASS, exit 0**, сохранён свежий baseline SHA/branch/status/argv.
  Он не обнаруживал ошибку порядка обхода, выявленную review.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .codex-runtime/issue44/review45/reproduce.ps1 -Phase before|after`:
  helper **PASS/0** на обеих фазах. Две копии текущего production заменяют
  `GetNode(counter)` на `GetNode(GetNodeCount() - 1 - counter)` независимо для
  `index`/`nodeId`. До правки полный Squad audit ошибочно давал **PASS/0** на
  обеих копиях; после правки обе отклонены **FAIL/1, SELECTION_STABLE_ORDER**.
  Полные mutated sources, output/argv/exits — `reproduction-before/` и
  `reproduction-after/`, общий output helper — `reproduction-*-runner.txt`.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-AuditSelectionContracts.ps1 -EvidenceRoot .codex-runtime/issue44/review45/mutations`:
  **PASS/0**, 6 позитивных контролей и **20 commander / 19 recruitment**
  отрицательных мутаций (всего 39); каждый negative input даёт **FAIL/1 с нужным
  rule ID**, включая `nearest-reversed-lookup` и `candidate-reversed-lookup`.
  Полные outputs/argv/exits — `mutations/summary.json`, output runner —
  `mutations-runner.txt`.
- Тот же `Invoke-AICFChecks.ps1 -Name <пять имён из baseline> -EvidenceRoot .codex-runtime/issue44/review45/after`:
  **5/5 PASS, exit 0**, свежих failures/regressions нет. Старые исторические
  failures не удалены; новых baseline failures в выбранном наборе нет.
- `git diff --check` — **PASS/0**. Изменены четыре файла: SquadCommands audit,
  AuditSelection runner, TESTING и TOOLS; `changed-files.txt`, `verification.json`.
  Временные source-copy удалены, свои тестовые процессы завершены.

Полный offline набор, повтор AICommanderUI, Workbench, runtime/client/JIP/
gameplay/visual — **NOT RUN**. Объём tests/docs; исходники игры не менялись.
Merge и релиз не выполнялись. Первоначальные 37 отрицательных контролей ниже —
исторический результат `4b2b8e9`, новый набор содержит 39.

## Issue #44: владельцы состояния и стабильный выбор — 2026-10-14

Ветка `codex/issue-44-audit-contracts` от свежего `origin/main`
`6466e10d32a0ea2a36da2383ba967b777c746234`, исходное дерево чистое.
Объём — tests/docs: production `.c`, resources и общий
`Stage3StaticAudit.Common.ps1` не изменены. Evidence: `.codex-runtime/issue44/`.

Прежний `AI_COMMANDER_COMPONENT` был следствием двух допустимых modded
расширений `SCR_GameModeCampaign`. Теперь локальный selector выбирает ровно
одно тело этого класса с объявлениями commander state/API, отклоняя отсутствие,
полное/частичное второе владение и совпадение в чужом классе. Оба callback-backed
RplProp проверяются на исполняемом объявлении; setter — на ранний server/master
return, unchanged return до assignments и один BumpMe; getters и callback
продолжают проверяться. Общий unique-class helper не ослаблен.

Прежний `STABLE_NEAREST` требовал отменённый distance-only ranking казармы.
Новые method-scoped checks проверяют возрастающий обход graph и исходный порядок
services, строгое улучшение nearest/pending distance, forecast/approach gates,
минимальный feasible completion ETA и приоритет best над pending. Равенство
сохраняет первый кандидат. Production selection не менялся; контракт:
[RECRUITMENT_SUPPLY_PLANNING.md](RECRUITMENT_SUPPLY_PLANNING.md).

Команды из корня checkout и фактические verdict:

- `& tools/Invoke-AICFChecks.ps1 -Name @('Test-AICommanderModeStatic.ps1', 'Test-SquadCommandsContracts.ps1', 'Test-RecruitmentSupplyContracts.ps1', 'Test-RecruitmentPriorityContracts.ps1', 'Test-AdaptiveCommanderContracts.ps1') -EvidenceRoot .codex-runtime/issue44/before`:
  **3 PASS / 2 FAIL, exit 1**. Воспроизведены только `AI_COMMANDER_COMPONENT`
  и `STABLE_NEAREST`; полные outputs, SHA/branch/status и argv — в `before/`.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-AuditSelectionContracts.ps1 -EvidenceRoot .codex-runtime/issue44/mutations-third`:
  **PASS/0**, 6 положительных контролей, **20 commander / 17 recruitment**
  отрицательных source mutations. Каждый испорченный вход дал **FAIL/1 с нужным
  rule ID**. PASS проверяет переименование state-файла, нейтральное modded
  расширение и исходные/восстановленные sources. Отрицательные случаи включают
  missing/wrong/duplicate/partial owner, оба RplProp, неверный/отсутствующий
  callback, authority/unchanged guards, преждевременный/лишний bump, getters,
  равный/худший nearest/pending/ETA, distance-only ranking и comment decoys.
  Полные outputs и argv/exits каждого входа — `mutations-third/summary.json`,
  runner output — `mutations-third-runner.txt`.
- `& tools/Invoke-AICFChecks.ps1 -Name @('Test-AICommanderModeStatic.ps1', 'Test-SquadCommandsContracts.ps1', 'Test-RecruitmentSupplyContracts.ps1', 'Test-RecruitmentPriorityContracts.ps1', 'Test-AdaptiveCommanderContracts.ps1', 'Test-AICommanderUIContracts.ps1') -EvidenceRoot .codex-runtime/issue44/after-final`:
  **6 PASS / 0 FAIL, exit 0**. Дополнительно проверен существующий UI runner:
  18 его отрицательных мутаций отклонены. `after-final/` содержит полный вывод
  каждого теста и commands/exits; `verification.json` — сравнение и scope.
- `git diff --check` — **PASS/0**. Семь изменённых файлов — `changed-files.txt`:
  два исправленных аудитора, новый mutation runner, TESTING, TOOLS, указатель
  agent-notes и заметка recruitment. Production изменения отсутствуют.

Ранние попытки runner не PASS: `mutations-first` остановлен отказом sandbox при
rename в системном temp; `mutations-second` выявил доступ к необязательному ключу
hashtable под StrictMode. Копия перенесена внутрь нового evidence root; optional
keys читаются через indexer. Полная матрица `mutations-third` повторена после
обоих исправлений. Все временные source-copy удалены, своих тестовых процессов
не осталось; полные outputs неудачных попыток сохранены.

Исторические FAIL ниже сохраняются как исторические. В свежем выбранном наборе
прежних failures и новых regressions нет. Полный offline набор, Workbench,
runtime/client/JIP/gameplay/visual — **NOT RUN**; они не требуются для этого
tests/docs изменения и не заявлены подтверждёнными статикой. Merge/релиз
не выполнялись; новой версии нет.

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
с объяснением. Если они нужны для исправления дефекта или обязательного gate,
участие игрока явно обозначается как blocker, а PR остаётся draft по правилам выше.
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
