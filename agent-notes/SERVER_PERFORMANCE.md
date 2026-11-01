# Производительность сервера

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
