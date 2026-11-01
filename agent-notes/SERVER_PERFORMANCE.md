# Производительность сервера

Новый длительный WCS Everon/Standard/Hard controlled mixed-fleet soak:
32min server/30min client, peak302/303AIChar. При подключённом client и
>=140AIChar server156windows avg16,30/min0,6FPS, max6786,8мс.
Client114qualified frontline windows avg52,74/min29FPS, max146,8мс.
**FPS FAIL**, обе исправленные vehicle ошибки0; общий runtimeFAIL из-за
4meaningful-task deadlines и4cleanup retained после client exit.
`RefreshCensus` занял46,65% inclusive elapsed high-load profiler windows,
max424мс/call; вложенные timings не суммируются. Производственной оптимизации
этого нового bottleneck в проверочном snapshot нет. Условия, реальные поездки,
метод квалификации камеры и полные gates — [WCS_HIGHLOAD_SOAK.md](WCS_HIGHLOAD_SOAK.md).

Обновление WCS vehicle blockers: `codex/wcs-assignment-scope` исправляет ложный
ASSIGNMENT_SCOPE для BASE с разными cached/resolved позициями и восстанавливает
минимальную native регистрацию до tactical reboard. Final WCS North12min:
same-vehicle resume +104.469м, full script/VM/registration/ASSIGNMENT_SCOPE0,
resource466FAIL. Это vehicle functional gate, не новое доказательство устранения
FPS stalls. Подробные попытки и команды — [TESTING.md](TESTING.md), evidence
`.codex-runtime/wcs-assignment-scope-20261101/REPORT.md`.

## Полный бой, PMC frame budget и найденные blockers — 01.11.2026

Evidence: `.codex-runtime/fps-highload-20261101/REPORT.md`. Продолжение
`codex/fps-followup`, исходный production `b44b00f`.
Остановленная прежняя сессия:255server samples, peak180AIChar;154samples
при >=140AIChar дают avg54,83/min36,8FPS, maxframe844,6мс. Просадки сохранялись.
Клиент переподключён с новым `-ClientLogStats`; подтверждение картинки рядом
с боем без участия игрока отсутствует, даже при большом global AIChar.

Stage-only PMC profiler измерил30actual initialization callbacks:
avg79,8/min64/max164мс. Теперь callback допускается через main-world frame
budget, остальные identity-safe callbacks повторяются через50мс. Численность,
inventory transaction, pending boarding gates и lifecycle Remove сохранены.
Actual-source budget106casesPASS; профильные3checksPASS; launcher+Coop2checksPASS;
terminal production WB Stock/RHS/WCS3/3compilePASS, source drift0, resources
50/52/78FAIL. Новый отдельный20min native0/stopped,31successful PMC callbacks
в31разныхframes, PMCfailures0. Но callback_total=31: native deferral path в этом
случайном мире не исполнился. Нельзя выдавать этот прогон за измеренное устранение
пакетного stall или доказанную причину первоначальной просадки.

Новый20min peak80AIChar, порог140не достигнут: high-load comparison NOT RUN.
После первого загрузочного sample server119samples avg59,07/min37,2FPS,
maxframe3576,4мс; client119samples avg143,94/min140FPS, maxframe170мс.
Разные миры/камера/численность не допускают causal FPS comparison.
Server3576,4мс попал в окно15:17:27–35 с двумя native VM exceptions:
`SCR_AIGetEmptyCompartment` → `NodeError` → `Debug.Error`, причина
`vehicle provided is not registered as used inside group!`. Это временная
связь, отдельная причинность не доказана. Плюс3VEHICLE_CLEANUP_RETAINED
с PLAYER_POSITION_UNKNOWN (всего8console SCRIPT E с bridge duplicates).
Строгий runtime gateFAIL, несмотря на native0/Game destroyed; raw sibling492E.
Исходная нагрузка и устранение просадок обоих FPS не подтверждены.

В старом полном бою подтверждены10покупок:5Ural,2BTR70,K4386,2T72,
всего4567supplies. Максимальное горизонтальное удаление от первой trace позиции:
Ural slots1/3 —716/1116м, BTRslot5 —720м, K4386 —816м. Второй BTR/первый T72
почти не двигались после TRANSIT и получили VEHICLE_UTILITY_ATTACH_REJECTED.
Есть TACTICAL_BEGIN_REJECTED, TACTICAL_UTILITY_ATTACH_REJECTED,
TACTICAL_REBOARD_TIMEOUT и boarding deadline. Это сбор данных, не исправление
надёжности всего парка. Полная таблица/operation IDs — `vehicles.json`.

Полные stopped logs/manifests/commands/source hashes и byte-verified stage ZIP
сохранены; stages удалены, тестовые процессы остановлены. Старые player sessions
завершены адресно с проверкой CommandLine; forced stop не равен graceful PASS.
Ранний RHS static FAIL был запретом нового retry: whitelist сужен до exact
frame-budget branch, общий запрет сторонних loops сохранён. Дополнительный frozen
launcher baseline первый раз FAIL из-за execution policy ручного запуска;
повтор через powershell.exe -ExecutionPolicy Bypass PASS. Все попытки сохранены.

## Повторное обращение игрока и перезапуск — 01.11.2026

Игрок подтвердил просадку server FPS и изображения клиента. Найденная живая
сессия стартовала в13:01/13:02, до commit `b44b00f` (14:09); её исходный
snapshot — `6941517`. Она не проверяла исправления census/topology. По явному
разрешению игрока остановлены только PID54204/70056 после проверки CommandLine;
полные остановленные profiles сохранены. Принудительное завершение не является
доказательством штатного shutdown. Новый fixture-free сервер PID66724 и клиент
PID60068 запущены canonical launcher на root `b44b00f`, NorthWCS/CoopUS/Easy,
AICommanderMode USSR; SERVER_READY перед client подтверждён. Они оставлены игроку.

Отдельный isolated ordinary-wcs,180s, без topology/body benchmark: WB exit0,
validation1/fatal0; server native0/Game destroyed/fatal0, resource466rawE — FAIL.
После ROSTER_READY17samples: min49,6/avg58,18FPS, maxframe238мс, max71AI/45AIChar.
Параллельно работала пользовательская сессия, нагрузка ниже исходной: это не
paired FPS proof. Последнее окно census27calls/397ms/max21ms; BuildCounts212ms,
VehicleUseCurrent128ms inclusive, вложенные времена не складывать.
Новые проверки Census/BodyLookup/VehicleTopologyPerformance3/3PASS; production
в этом продолжении не менялся. Клиентский лог не содержит FPS samples;
полная high-load проверка обоих FPS и нового игрового shutdown — NOT RUN.

Evidence/команды: `.codex-runtime/fps-followup-20261101/` (Measure-Server.ps1,
baseline/, ordinary-wcs/compile/, ordinary-wcs/logs/, ordinary-summary.json,
old-session-stopped/, server-launcher.txt, client-launcher.txt). Изолированный
stage архивирован и удалён; тестовый сервер штатно завершён. Свежие player
profiles: `C:/Users/retar/AppData/Local/AICF/FpsFollowup-20261101/{server,client}`.

Исходное наблюдение WCS / CoopUS / Northern Everon, 01.11.2026:
`.codex-runtime/north-wcs-coopus-playtest-20261101-130116/REPORT.md`.
В 13:11:02–13:21:02 MSK сервер падал до19,6 FPS, отдельный кадр2513,6мс.
Это наблюдение активной игры, а не остановленный runtime PASS.

## Два независимых горячих пути

1. `AICF_AIWorldQuotaService.RefreshCensus` заново исполняет полный census и
   ownership/ledger/custody проверки. При148–149activeAI baseline измеритель
   фиксировал около9секунд суммарного inclusive wall time на15-секундное окно.
   Вложенные времена нельзя складывать с временем внешнего метода.
2. Bulk чтение vehicle seats раньше повторяло полный DFS hierarchy на каждом
   сравнении insertion sort. Дерево растёт при посадке экипированных бойцов.
   Контракт и отдельные проверки — [VEHICLE_TOPOLOGY_PERFORMANCE.md](VEHICLE_TOPOLOGY_PERFORMANCE.md).

`AICF_AIQuotaBodyLookup` индексирует существующие records только на время одного
синхронного `RefreshCensus`. Он сохраняет первый live record, проверяет его
текущее присутствие в owner array и повторно ищет appended/released/removed
record. Отсутствие записи не кешируется. До снятия busy guard ссылка на индекс
очищается. Никакие результаты census, разрешения, native counters, epochs,
ownership или identity fences между вызовами не кешируются и не ослабляются.
Вне этого scope остаётся прежний линейный поиск.

## Измерители

Stage-only `tests/fixtures/AICF_ServerPerformanceProbe.c` оборачивает реальные
методы одним вызовом `super` и возвращает неизменный результат. Агрегаты
`SERVER_PERFORMANCE` раз в15секунд содержат calls/inclusive_ms/max_ms/window_ms.
GroupTrace, CensusCapture/Current, BuildCounts, CharacterBindings и VehicleUse
измеряются отдельно. Fixture не должна входить в production addon.

Явный `-aicfBodyLookupBenchmark 1` добавляет controlled64refreshes:4пары по8
вызовов с frozen старым `FindBody` и текущим индексом в одном native мире.
Active/intended/body counts и успешность повторяются после каждого блока.
Это read-only сравнение стоимости поиска, **не ordinary FPS gate**; оно может
само создать длинный кадр. Без CLI дополнительные refreshes не выполняются.

`-aicfServerPerformanceDurationMs 30000..1200000` завершает только изолированный
тест через `RequestClose` после выбранного времени существующих Update callbacks.
В обычной пользовательской сессии fixture не подключается.

Для WCS paired topology benchmark отдельно подключается
`AICF_VehicleTopologyPerformanceProbe.c` с `-aicfTopologyPerformanceProbe 1`.
Он ждёт настоящий уже оплаченный транспорт с10экипированными settled AI;
не создаёт и не рассаживает их. Точные guards и допустимые выводы — в профильной
заметке. Без CASE/DONE native сравнение считается NOT RUN.

Все native запуски — только `tools/Start-AICFRuntime.ps1`, отдельные profiles и
фактический listener через `-addr`. Compile — terminal Workbench по DEVELOPMENT.
Evidence текущей задачи: `.codex-runtime/server-fps-census-20261101/`.
Запущенные сервер/клиент игрока не останавливаются этими измерителями.

## Зафиксированные результаты и ограничения

Frozen Stock paired-index-v2,148active/intended и148body records, stable=1:
32старых полных refresh заняли965мс,32с индексом821мс, улучшение14,9%.
Это стоимость всего refresh при смене только стратегии FindBody; другие его
проверки остаются существенной нагрузкой. Сравнивать FPS разных случайных
stock-pressure матчей как доказательство индекса нельзя.

Три контрольных Stockpressure запуска завершились native0/stopped/fatal0, но
их функциональные pressure gates остаются FAIL: EXTRA_STOCK_IDENTITY_OR_BIRTH,
PRESSURE_BUDGET_UNKNOWN и CONTINUOUS_STOCK_PRESSURE_LOST. Raw resource errors28
также сохраняются. Эти тесты дали timing evidence, а не PASS разделения квот.
Неудачная sandbox WB попытка before сохранена; elevated before-v2 завершился
exit0 с успешной validation.

WCS North current-v1: обычная оплаченная покупка Ural и actual10equipped crew,
672raw inventory entries,15мест; один и тот же native vehicle до/после,
identity/generations/occupancy сохранены. Old/new Collect299/0мс,
MatchesSeats10/1мс, CollectCaptured26/0мс;0означает ниже разрешения целочисленного
таймера, а не бесплатную операцию. Все references/keys/results совпали.
Native0, exactCLI, frozen395source drift0, полный остановленный log/fatal0;
resource466raw sibling E остаются FAIL.

После ROSTER_READY71samples: min47,1/avg57,39FPS, максимум кадра424,4мс
в sample, содержащем намеренно дорогой старый benchmark; остальные samples
до364,7мс. Максимальная нагрузка этого запуска111AI/81AIChar по native stats.
Это меньше исходного игрового среза, а клиента и тех же действий игрока здесь
нет. До ROSTER_READY остались startup20FPS/5048мс; весь лог не объявляется
performance PASS. Исправлены и измерены два горячих пути, но universal FPS fix
и воспроизведение той же human/high-load сессии — NOT RUN.

Terminal production Workbench Stock/RHS/ArlandWCSRHS3/3 compile PASS,
395source drift0/fatal0; raw resources50/52/78 сохраняются. Offline baseline41/41;
после44unique PASS с повтором исправленного reentry adapter, final-focused7/7.
Ранний after43PASS/1FAIL и первый reentry rerunFAIL сохранены: адаптер не включал
новый call-local field, затем повторял имя типа между unsafe namespaces.
Actual helper521cases/5unsafe, reentry10/4unsafe и topology2319/3unsafe PASS.
Это отдельные gates; полный текущий All не запускался.
