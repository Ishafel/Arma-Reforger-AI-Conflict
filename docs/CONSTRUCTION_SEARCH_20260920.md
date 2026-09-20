# Поиск площадок автономного строительства — 2026-09-20

Проверка выполняется по пользовательскому случаю RHS Север Эверона и полному
Everon RHS. Отчёт дополняет исторический
[CONSTRUCTION_SPEED_20260919.md](CONSTRUCTION_SPEED_20260919.md).
Статические, compile и runtime gates рассматриваются отдельно. `ACCEPTED`
не присваивается.

## Итог

Поиск переработан и проверен на повторных server-only прогонах. Основной
госпитальный случай теперь завершается физическим строительством обеих сторон:
в контролируемой full-RHS паре US размещает малые казармы за 58,223 с вместо
отсутствия placement за 180 с; path queries 2859 → 90. Однако ориентир 60 с
не выдерживается устойчиво: hospital repeats дают примерно 58–76 с.

Функционально подтверждены **9 из 10** пар «тип × фракция». Для
`HEAVY_DEPOT / US` положительного end-to-end результата нет ни в North Everon,
ни в дополнительном stock Arland case. Некоторые large/heavy поиски остаются
многоминутными; geometry checkpoints могут истечь до path admission.
Поэтому полное выполнение критерия надёжного быстрого поиска всех типов
**не подтверждено**. Это ограничение реализации, а не разрешение обходить
геометрию или считать `COMPUTE_LIMIT` доказанным отсутствием места.

Static — без новых failures, сохранены два исходных UI failures. Все шесть
финальных Workbench validations — PASS. Общий runtime verdict — **не PASS**:
есть непокрытая функциональная пара, превышения целевого времени и ошибки
полных logs, перечисленные по каждому case ниже. Пользовательские изменения
сохранены; существующие server/client не затронуты.

## Исходное состояние и сохранность

Исходный HEAD: `bb3e3cc23b51c9995ef5a9d3de1177da80efa601` с пользовательскими
изменениями builder combat policy. Перед изменением сохранены `git status`,
patch и SHA256 защищённых файлов. Builder service/spawner/work policy и два
пользовательских prompt-файла не менялись этой задачей. Исходная source copy
включает эти пользовательские изменения, а не только чистый HEAD.

Evidence: `.codex-runtime/construction-search-20260920/` (ignored).
`status-before.txt`, `user-changes-before.patch`, `preserved-files-hashes.csv`,
`baseline-source/` и `baseline-audits.json` фиксируют baseline.

Каждый runtime использует собственную source copy и новый profile через
`tools/Start-AICFRuntime.ps1`, без клиента. В каждом case сохранены
`command.json`, `launch.txt` с `AICF_RUNTIME_MANIFEST_JSON`, `source-hashes.csv`,
`exit.json` и полные `profile/logs/.../console.log`/`script.log`.
Отфильтрованные события и `metrics/` служат индексом, не заменяют полный log.
Существующие пользовательские server/client не останавливались.
Финальная сводка включает 22 остановленных cases и 104 orders, исключая
невалидные подготовки, явно перечисленные ниже. `final-evidence-integrity.json`
подтверждает для всех 22 exact `CLI Params` относительно manifest, native
exit 0 и `Game destroyed.`; `final-process-check.json` — ноль оставшихся
тестовых server processes. Native exit 0 не означает PASS runtime-аудитов.

Первоначальное перечисление source hashes через wildcard оказалось пустым.
Оно заменено явным обходом пяти addons с проверкой количества файлов; hashes
ранних cases восстановлены из сохранённых неизменённых source snapshots.
Это отмечено в их `source-hashes-provenance.json`, а не выдано за запись перед
запуском. Финальные cases записывают полные hashes до native launch.
Финальная snapshot — `final4-source`; ранее сохранённые parity CSV относятся
к соответствующему моменту проверки. Три дополнительные `.c` — test fixtures;
production parity проверяется отдельно от них.
`production-final4-parity.csv` подтверждает совпадение всех 171 production
файлов с окончательной snapshot. `preserved-files-after.json` подтверждает
шесть исходных SHA256; `additional-user-files-preserved.json` — неизменность
пользовательских diff в README, builder static audit и builder runtime fixture.

## Подтверждённые причины

1. Очередь восьми трудных путей прекращала генерацию новых площадок. Один
   кандидат мог расходовать 4096 path queries; запасные старты проходили почти
   одинаковый обход последовательно. Native baseline у госпиталя за шесть минут
   не разместил малые казармы США: 105 кандидатов, 10009 queries, 7913 path
   queries, две искусственные минутные паузы.
2. Каждый Halton-центр получал лишь один поворот. Исключались внутренние 8 м
   и внешняя полоса, полученная вычитанием circumradius. Это не соответствовало
   реальной проверке углов повернутого footprint.
3. `GetPositionAndRotation` выбирал случайный stock marker и мог выполнять
   скрытые `FindEmptyTerrainPosition`/`IsFree` queries. Он вызывался не только
   для path start, но и внутри access/obstacle guards.
4. До 16 goal raycasts на каждую расширенную вершину повторяли дальние
   отрицательные проверки. Отрицательные projections и проверенные рёбра
   между кандидатами не переиспользовались.
5. Первичная physics-проверка не включала измеренное terrain-расширение по Y.
   Commit впервые проверял более высокий объём и мог отклонить выбранную
   площадку. Диагностика повторного отказа терялась за первым событием категории.
6. Metadata chunks часто оставляли остаток общего CPU slice неиспользованным,
   откладывая следующий chunk на секунду. Native cold load остаётся
   неделимым вызовом: наблюдались update около 270–566 мс. Это отдельная
   измеренная граница, не скрытая в `max_slice_ms` поиска.
7. В промежуточном runtime navmesh-подтверждённый endpoint привёл рабочего
   лишь на расстояние около двух метров, без начала работы. Дополнительная
   физическая проверка места для тела исключила этот endpoint; последующий
   прогон той же площадки завершился с физическим подходом и `service_online=1`.
   Конкретный тип препятствия не был записан в той промежуточной версии,
   поэтому причина остановки не приписывается определённому объекту.

## Изменение алгоритма

- Восемь ориентаций на центр, четыре начальных центра около authored stock
  spawn и дальнейшая Halton-последовательность по полному provider disk.
  Bounds используют полный footprint с прежним margin; дорогие queries
  начинаются после дешёвого исключения выходящих за bounds кандидатов.
- Geometry-ready frontier до 32 checkpoints. Близкие к stock spawn площадки
  получают path search раньше далёких. Quantum пути — 128 queries, общий
  предел кандидата — 512, lifetime — 15 с; geometry checkpoint — 60 с.
- Один multi-source поиск от детерминированных authored spawn roots. Общий
  cursor projections принадлежит order и не восстанавливается из checkpoint.
- Order-local cache projections/edges с TTL 30 с и ограниченный граф
  достижимых точек. Cache только направляет поиск: выбранная цепочка целиком
  проходит свежий native `RayTrace`. Изменение пути инвалидирует cache epoch;
  смена или auto-null pathfinding context отменяет order на каждом tick,
  включая checkpoint и ожидание commit quota.
- Меньше дальних raycasts: два ближайших endpoint на root/вершину и редкие
  дальние shortcuts. Шаги 8/4/2 м, максимум 512 вершин и глубина цепочки 64.
- До path проверяется тот же terrain envelope, что перед completion. Endpoint
  проверяет свободный объём тела. Commit повторяет bounds, physical clearance,
  endpoint, identity, inventory и economy quote.
- `IdentityValid` получает base/provider owner в локальные переменные и
  явно проверяет null до stock `GetEntityFaction(notnull IEntity)`. Это
  закрывает исключение, обнаруженное native тестом удаления stock service.
- Незавершённый поиск продолжает работу в соседних ticks и до трёх окон без
  минутной паузы. Неудачная commit-геометрия освобождает только site reservation.
  Малые казармы сохраняют приоритет до трёх ограниченных попыток.
- Сохраняются общий предел четырёх новых кандидатов, 96 queries/window,
  cooperative slice 8 мс, round-robin active bases и один placement/tick.
  Цена, оплата, rollback, authority, footprint, ownership и физическая работа
  строителя не подменяются.

`search_status` различает `SITE_FOUND`, `COMPUTING`, `COMPUTE_LIMIT`,
`AREA_EXHAUSTED`, `TEMPORARY_OBSTACLE`, `CANCELLED`. Только аналитически
недостаточный диаметр provider даёт `SEARCH_AREA_EXHAUSTED`; предел времени,
узлов, transforms или queries не доказывает отсутствие места. Выборка конечна
и не гарантирует нахождение любого сколь угодно узкого допустимого участка.
Временный blocker определяется по character/vehicle, включая их дочерние
объекты; неизвестный объект не объявляется временным без данных.

Каждый отказ выбранной площадки записывается как
`CONSTRUCTION_SELECTED_REJECTED` с `phase`, `obstacle`, `candidate_index`.
Существующие события и их поля сохранены; новые поля добавлены.
`selected_rejections` считает эти события, включая неокончательный
`QUERY_BUDGET` при completion. Геометрические отказы и ожидания различаются
по `reason`; все попытки, а не только первая категория, остаются в log/index.

## Проверки

В таблицах «подход» означает placement → первый `BUILDER_WORK_STARTED`,
«работа» — первый start → `BUILDER_COMPLETED`, включая возможные приостановки.
Отдельный `work_to_service_ms` в `metrics/orders.json` и сводных CSV измеряет
первый start → `CONSTRUCTION_COMPLETED service_online=1`; stock service
обычно подтверждается на следующем tick после окончания работы. Значения
не смешиваются, оба timestamps и оба интервала сохранены.

Baseline static: ConstructionStatic, ConstructionContracts, BaseBuildersStatic,
Stage4Static, Stage35Static — PASS. AICommanderModeStatic — FAIL: прежние два
`AI_COMMANDER_UI_STATE` (waiting label и SYSTEM_HOLD/ATTACK markers).

После: те же verdict; ConstructionContracts — 16 log inputs и
positive/25 negative static inputs. Новых static failures нет.
Финальный production и `final4-source/` прошли terminal Workbench 1.8.0.13:
Arland, Everon, ArlandRHS, EveronRHS и два fixture-графа Arland/EveronRHS,
каждый exit 0 + `Script validation successful`.
Evidence: `final4-compile-verdicts.json`, `wb-final4-*-command.json`,
`wb-final4-*-launch.txt` и соответствующие полные Workbench logs.

Промежуточная проверяемая версия: `validation2-source/`. Workbench 1.8.0.13,
`-noThrow -wbsilent -wbModule=ScriptEditor -run -validate`: production Arland,
Everon, ArlandRHS, EveronRHS и fixture-граф EveronRHS — exit 0,
`Script validation successful`. Точные executable/arguments и полные logs:
`wb-production-validation2-*-command.json`, `wb-production-validation2-*/`,
`wb-validation2-EveronRHS/`.

Промежуточные воспроизведения:

| Case | Малые казармы США | Малые казармы СССР | Ограничение |
|---|---|---|---|
| baseline-hospital-2 | Нет placement за 360 с | Placement и completion | Исходный алгоритм, полный stopped log |
| iteration4-hospital | Placement 105 с, затем completion | Placement и completion | Промежуточный алгоритм, ориентир 60 с превышен |
| iteration6-hospital | Placement 58 с, подход без работы | Placement и completion | Endpoint не прошёл end-to-end criterion |
| iteration7-hospital | Placement 58,2 с; работа через 10 с; service online | Placement 12,9 с; service online | Endpoint physics добавлен; более поздние guards ещё не все включены |
| validation1-airfield | Авиабаза: placement 22,9 с, service online | Госпиталь: placement 58,1 с, service online | До финального исправления общего spawn cursor |

Функциональное прохождение RHS не означает зелёный полный runtime gate:
в baseline и новых RHS logs присутствуют четыре ошибки инициализации
`SCR_Faction` для ключей `US`/`USSR`. Они не удаляются из лога и не исключаются
из анализаторов. Дополнительные teardown errors, если есть, учитываются
по каждому case отдельно.

### Контролируемая пара baseline / после

`baseline-paired2-full` и `validation4-paired-full`: полный Everon RHS,
одинаковые US HQ `0x200000000000000F` и USSR HQ `0x2000000000000003`,
5500 supplies, 180 с, BOTH. Test-only fixture выбирает эти две штатные базы
до campaign initialization; production не содержит их IDs или координат.
Одинаковые player-free режим, refill, геометрические проверки и builder policy.
Workbench и другие тестовые серверы одновременно с этой парой не запускались.

| Версия / сторона | Decision → placement | Подход → работа | Работа → worker completion | Кандидаты | Queries до placement/остановки | Path queries | Search CPU / max slice |
|---|---:|---:|---:|---:|---:|---:|---:|
| Baseline US | Нет за 180 с | — | — | 82 | 4565 | 2859 | 203 / 16 мс |
| После US | 58,223 с | 11,007 с | 16,094 с | 121 | 2303 | 90 | 136 / 11 мс |
| Baseline USSR | 43,985 с | 15,045 с | 16,060 с | 16 | 1194 | 343 | 68 / 10 мс |
| После USSR | 9,884 с | 6,031 с | 15,060 с | 1 | 226 | 72 | 46 / 8 мс |

Обе новые постройки завершены настоящим tool use с `service_online=1`.
Для US число path queries уменьшилось на 96,9%, общая квота — на 49,6%,
при проверке большего числа transforms. CPU сравнивается с незавершённым
baseline, поэтому это не отношение стоимости двух одинаковых завершений.
US после: candidate 996, terrain 1188, path 90, commit 27, inventory 2;
completion отдельно 26. СССР после: 53 / 72 / 72 / 27 / 2, completion 26.
`queries` — единицы budget, не точное число всех native вызовов.

За весь case search CPU 317 → 578 мс: новая версия обрабатывает пять orders
вместо трёх и завершает три вместо одного. После scheduler CPU 885 мс,
максимальный полный update 275 мс; этот cold-load spike не входит в
максимальный search slice. Средний FPS по серверным samples 58,03 → 58,51,
максимальный engine frame 3727,7 → 2704,8 мс. Эти общие frames включают
загрузку мира; причинное улучшение frame time из них не выводится.
Поля `*_cpu_ms` измерены через `System.GetTickCount`: это накопленная
wall-clock длительность вызовов script scheduler/search, а не OS thread CPU
profiler. Работа движка и completion callbacks вне planner не включена в
эту сумму. OS CPU контролируемой baseline-пары отдельно не измерен;
отдельный замер финального heavy case приведён ниже.

Полный gate обоих cases **FAIL**: 48 одинаковых ошибок Stage3.5
meaningful-task/MOB-egress из-за стратегической связности этой выбранной пары,
четыре RHS faction initialization errors; после также два teardown errors
`SCR_BaseResupplySupportStationComponent needs entity catalog manager`.
Это не зелёный end-to-end gate всего мода.

Исторический пользовательский full Everon имел другую конфигурацию мира,
которую не удалось полностью восстановить из terminal evidence. Эта пара
воспроизводит госпиталь в полном штатном RHS-сценарии, но не объявляется
точной копией прежней GUI-сессии. `baseline-paired-full` с неудачной подготовкой
HQ исключён из сравнения; причина и полный log сохранены.

### Повторяемость и превышение 60 секунд

| Case | US: placement / подход / работа | USSR: placement / подход / работа | Результат строительства |
|---|---|---|---|
| validation4-hospital-1 | 61,055 / 11,025 / 15,073 с | 12,913 / 5,994 / 37,010 с | Обе стороны service online |
| validation4-airfield-1 | 16,861 / 7,013 / 24,048 с | 53,048 / 8,014 / 17,078 с | Обе стороны service online |
| validation2-full-everon, штатная случайная пара | 16,012 / 9,030 / 29,099 с | 77,237 / 7,033 / 17,062 с | Обе стороны service online |
| final3-hospital, до явного provider-owner guard | 76,254 / 10,058 / 16,093 с | 17,938 / 5,995 / 16,083 с | Обе стороны service online |
| final4-hospital, окончательный production | 74,200 / 10,014 / 18,063 с | 17,911 / 6,032 / 22,043 с | Обе стороны service online |

Вместе с `iteration7-hospital` и `validation1-airfield` это повторные
положительные прохождения обоих распределений HQ. Промежуточные source copies
сохранены отдельно: их не выдаём за байтово идентичную финальную версию.

61,055 с у госпиталя: metadata 10,045 с, поиск 51,006 с, commit 4 мс;
2303 budget units, search CPU 125 мс, max slice 10 мс. Превышение 1,055 с
обусловлено длительностью ticks/подготовки, не минутной паузой. Оно остаётся
невыполнением ориентира, а не округляется до PASS.

77,237 с СССР на случайной full-паре: metadata 8,004 с, поиск 69,230 с,
commit 3 мс; 3772 units (candidate 1745, terrain 1922, path 76, commit 27,
inventory 2), 35 ожиданий общей квоты, CPU 99 мс, max slice 8 мс. Основная
стоимость — геометрия и terrain. Этот case не является госпитальной парой.

Повтор final3 US hospital: metadata 19,046 с, поиск 57,202 с, commit 6 мс;
те же 121 transforms и 2303 units, что в быстрых повторах, включая path 90.
Search wall time 211 мс, max slice 9 мс. По сравнению с 61,055-секундным
повтором metadata заняла на 9,001 с больше, поиск — на 6,196 с больше.
Минутного cooldown, нового order или расширения budget не было; причина
роста стоимости metadata на уровне OS/native profiler не установлена.
**Ориентир 60 с не достигнут устойчиво**: успешный end-to-end результат
повторяется, но время US hospital меняется примерно от 58 до 76 с.
Последний final4 US hospital: metadata 17,080 с, поиск 57,114 с, commit
6 мс; 2303 units, 121 transforms, накопленная длительность search 193 мс,
max slice 10 мс. Проверка обеих построек/работы/completion проходит;
полный search gate FAIL из-за RHS/teardown ошибок и превышения 60 с США.

`placement-over-60s.csv` содержит все placements дольше 60 с из включённых
в сводку остановленных cases: этапы, queries, budget waits и категории отказов.
`all-case-orders.json`, `all-case-costs.json`, `physical-completions.csv`
сохраняют полный индекс, включая неподходящие базы и незавершённые orders.

Запущенный или частичный log не считается завершённой проверкой.

### Stock-матрица

`validation4-stock-matrix`: stock Arland, 18 минут, обычные базы подготовлены
владением ближайшей стороны и supplies; production проверки не обходятся.
Одна активная стройка на сторону, четыре минуты на фазу. Уже принятый order
может закончиться в следующей фазе; итог считается по completion, а не по
промежуточному флагу `MATRIX_PHASE`.

| Тип / сторона | Placement | Подход | Работа | Результат |
|---|---:|---:|---:|---|
| Small US | 56,917 с | 11,004 с | 18,000 с | service_online=1 |
| Small USSR | 83,083 с | 14,031 с | 15,056 с | service_online=1 |
| Light US | 52,237 с | 25,211 с | 23,102 с | service_online=1 после отказа другой базы |
| Light USSR | 147,585 с | 8,067 с | 16,075 с | service_online=1 |
| Large USSR | 210,079 с | 26,162 с | 18,139 с | service_online=1 |
| Large US | — | — | — | 242 transforms, COMPUTE_LIMIT |
| Heavy US/USSR | — | — | — | Тест завершился до placement |
| Armory US/USSR | — | — | — | Существующий service: проверена защита от дубля |

Small USSR: metadata 7,004 с, геометрия/поиск 76,076 с, commit 3 мс;
192 transforms, 1851 units, CPU 197 мс, max slice 13 мс. Это превышение
60 с вне проблемной HQ-пары также явно сохранено. Large USSR: metadata
36,129 с и поиск 173,945 с. Большие prefabs остаются существенно дороже.
На Large US наблюдался неделимый search slice 43 мс: cooperative budget
8 мс не является жёстким пределом длительности native geometry call.

Полный verdict **FAIL**: отсутствуют пять положительных faction/type пар,
в stopped log есть `ORDER_REPAIR_ACCOUNTING_INVARIANT_FAILED`, его
`CORE_ERROR_BRIDGE` и два stock teardown errors. Первую ошибку нельзя
объявлять сохранённым baseline failure без отдельного такого же длительного
контрольного прогона. Construction domain не меняет order repair accounting.
Суммарно search CPU 2746 мс, scheduler CPU 3172 мс, max update 271 мс,
109 frame samples: средний FPS 59,96, max engine frame 294,7 мс.

### Дополнительные stock cases окончательного production

`final4-armory`, stock Arland: существующие armory services перенесены за
provider sphere только в isolated runtime, чтобы возникла настоящая потребность
в новой постройке. Владение, providers, ресурсы и обычная coverage-проверка
сохранены; необходимость не имитируется результатом `Covered`.

| Тип / сторона | Placement | Подход | Работа | Кандидаты / queries | Результат |
|---|---:|---:|---:|---:|---|
| Armory US | 28,050 с | 10,994 с | 20,098 с | 57 / 1591 | tool use, service_online=1 |
| Armory USSR | 9,049 с | 6,995 с | 23,079 с | 17 / 269 | tool use, service_online=1 |

Это время от `CONSTRUCTION_DECISION`. США получили решение при следующем
обычном сканировании потребностей после обновления пространственного индекса
перенесённых stock services; время подготовки fixture не включено в поиск.
Оба orders без pause, compute exhaustion и selected rejection. Search wall
106 мс суммарно, max slice 8 мс; полный scheduler 672 мс с максимальным
update 524 мс. Этот cold/native spike сохраняется как ограничение.
Все три полных runtime-аудита **FAIL** только из-за двух stock teardown errors;
физическое строительство обеих сторон подтверждено полным stopped log.

`final4-large-north-stock`, stock North Everon, 480 с:

| Сторона | Placement | Подход | Работа | Кандидаты / queries | Результат |
|---|---:|---:|---:|---:|---|
| US | 380,380 с | 21,062 с | 24,167 с | 212 / 10695 | tool use, service_online=1 |
| USSR | — | — | — | 256 / 13476 | COMPUTE_LIMIT, без оплаты/placement |

US: metadata 77,109 с, поиск 302,265 с, выбранная площадка → commit 1,006 с.
Расходы: candidate 1511, terrain 5746, path 3408, commit 27, inventory 3;
completion отдельно 26. 123 ожидания общей квоты, 11 expired checkpoints,
1277 cache hits; search wall 758 мс, max slice 27 мс.
Основные отказы: дороги 80, физические объекты 55, bounds 44, проходы базы 16,
path query limit 4, candidate timeout 1. Это существенно медленный положительный
случай; он не выдаётся за достижение общего времени около 60 с.

USSR: metadata 66,179 с; 5470 terrain и 6606 path units, 143 ожидания квоты,
5869 cache hits. 11 маршрутов дошли до своего query limit; 256 transforms
исчерпали конечную выборку. Наличие пригодного места на этой базе не доказано
и не опровергнуто. Положительный large-USSR получен на Arland выше.

Полный stopped log без script errors. `Test-ConstructionLog`,
`Test-BaseBuildersLog -RequireToolUse` и `Test-ConstructionSearchLog` — **PASS**.
Эти вызовы подтверждают одно физическое завершение и lifecycle, но не требуют
двух large completions; матрица обеих сторон в этом отдельном case не закрыта.
За case: search wall 1889 мс, scheduler 2368 мс, max update 434 мс;
48 samples, средний FPS 59,86, max engine frame 463,9 мс.

`final4-heavy-north-stock`, stock North Everon, 480 с:

- USSR на второй базе: placement 184,771 с, подход 14,083 с, работа 33,196 с;
  первый tool use → service online 34,145 с. Metadata уже была в cache,
  поиск 184,766 с, commit 5 мс; 158 transforms, 7692 units: candidate 1518,
  terrain 5316, exits 744, path 81, commit 31, inventory 2. Completion отдельно
  30. Search wall 106 мс, max slice 7 мс; 73 ожидания квоты.
- Первая USSR-база: 256 transforms, 5376 units, `COMPUTE_LIMIT` без оплаты;
  path не начинался, варианты отвергались геометрией/выездами.
- US на первой базе: metadata 27,969 с, 225 transforms, 13344 units
  (candidate 1591, terrain 6776, exits 3546, path 1430), `COMPUTE_LIMIT`.
  Как минимум один geometry checkpoint истёк до path admission. Это потеря
  ранее выполненной геометрии из-за конечного lifetime, а не cache hit;
  не все длинные поиски устранены новой frontier policy.
- Вторая US-база также исчерпала 256 transforms; начатая позднее попытка
  первой базы остановлена вместе с тестом. Положительный US heavy здесь
  **не подтверждён**.

Один selected rejection относится к `QUERY_BUDGET` на completion, после
которого постройка СССР штатно завершилась. Физические guards не обходились.
Все три полных аудита **FAIL**: `Defend Waypoint not provided to the node!`
в stock `ActivityDefend.bt` и два teardown errors. Для первой ошибки нет
идентичного baseline-прогона; она не объявляется сохранённым baseline failure.
Search wall 1004 мс, scheduler 1474 мс, max update 379 мс; 48 frame samples,
средний FPS 59,85, max engine frame 404,8 мс.

Read-only OS CPU sampling этого точного process/profile: 101 sample,
447,234 CPU-секунды всего server process к 510,300 с от его старта
(user 315,000 с, kernel 132,234 с), в среднем 0,876 занятого ядра.
Последний sample за 2,174 с до выхода; хвост не измерен. Это включает startup,
AI, physics и другие домены, не является CPU только строительства или
контролируемым до/после сравнением. Identity, samples и расчёт сохранены в
`process-cpu-identity.json`, `process-cpu-samples.csv`, `process-cpu-summary.json`
в каталоге case; observer не менял процесс или игровые настройки.

`final4-heavy-arland-stock` — дополнительный последовательный прогон 480 с
после остановки North Everon, тот же окончательный production. Восемь orders,
1438 transforms, 21368 budget units; **ноль placement и completion**.
Пять orders исчерпали вычислительный предел, один отменён lifecycle guard,
два остановлены вместе с тестом. Отсутствие пригодного места не доказано.

US на второй базе: 256 transforms, 10065 units — candidate 1590,
terrain 5808, exits 2246, path 420, inventory 1; два expired checkpoints,
search wall 340 мс, max slice 46 мс. Ни footprint, ни выезды, ни обязательные
проходы не уменьшались ради положительного результата.
Полный log без script errors: ConstructionLog и ConstructionSearchLog PASS
для lifecycle/diagnostics, BaseBuildersLog FAIL (`BUILDERS_IDLE_COVERAGE`).
Эти два PASS не означают успешное физическое строительство.
За case search wall 1610 мс, scheduler 2245 мс, max update 566 мс;
48 frame samples, средний FPS 59,88, max engine frame 597,7 мс.

Сводное физическое покрытие (`type-coverage.json`):

| Тип | US | USSR | Основные cases |
|---|---|---|---|
| Small barracks | Подтверждено | Подтверждено | final4-hospital |
| Armory | Подтверждено | Подтверждено | final4-armory |
| Light depot | Подтверждено | Подтверждено | validation4-stock-matrix |
| Large barracks | Подтверждено | Подтверждено | final4-large-north-stock / validation4-stock-matrix |
| Heavy depot | **Не подтверждено** | Подтверждено | final4-heavy-north-stock; дополнительный Arland без placement |

Старые source versions в этой таблице названы явно. Финальные owner/context
guards дополнительно проверены fault cases; старые положительные logs не
переименованы в прогоны окончательной snapshot.

## Анализ и воспроизведение

### Отрицательные и identity cases финальной версии

| Case | Проверка по полному stopped log | ConstructionSearchLog |
|---|---|---|
| final2-no-site | Radius 1 меньше полного footprint; два `AREA_EXHAUSTED`, ноль placement/payment | PASS |
| final2-provider | Provider сдвинут после geometry checkpoint; cancelled=1, paid=0, reserved=0, checkpoints=0, path_present=0; старый token не размещён | PASS |
| final2-cancel | Явная отмена pending order; те же cleanup/invalidation assertions | FAIL: только два stock teardown errors |
| final2-context | Моделируется auto-null component reference; отмена в том же tick, cache/checkpoints очищены | FAIL: только два stock teardown errors |
| final3-owner | Обычная база сменена с USSR на US во время pending; старый order очищен без оплаты и не размещён позже | PASS |
| final3-dynamic | Реальный physics blocker отклонён до оплаты; тот же token размещён через 8 с на следующем кандидате и завершён инструментом, service_online=1 | FAIL: script errors полного log, все fault assertions PASS |
| final4-deleted-provider | Повтор удаления stock service после owner guard: исключения GetEntityFaction больше нет | FAIL: stock Wrong class of provided Waypoint и два teardown errors |

В no-site/provider/owner `Test-ConstructionLog` также PASS. Общий
`Test-BaseBuildersLog` требует `BUILDERS_IDLE_COVERAGE` даже при нуле
построек; отсутствие этого покрытия в коротком отрицательном тесте сохранено
как отдельный FAIL, критерий анализатора не ослаблен. Физическая работа и
completion проверяются положительными cases.

`final-owner` не активировал fault из-за выбора HQ; `final2-owner` подтвердил
проектную отмену, но тестовая передача null faction вызвала stock exception
в `EvaluateDefenders`. Оба не засчитываются как корректный полный owner test.
`final2-dynamic` не засчитывается: первая версия fixture удаляла blocker при
проверке другого order. Повтор `final3-dynamic` привязал его к token и проверил
соответствие описания препятствия, отказ `COMMIT_GEOMETRY`, отсутствие оплаты,
последующее placement и completion того же заказа.
`final3-armory` также исключён из положительной матрицы: удаление stock
service удалило связанные provider/resource entities, обнулило supplies и
вызвало исключение `GetEntityFaction`. Именно этот native случай привёл к
явному owner guard. Повтор удаления — только отрицательная проверка;
положительная подготовка арсенала переносит существующий service за sphere
provider, сохраняет связи и защищает base/provider ancestors.

### Воспроизведение

```powershell
pwsh -NoProfile -File tools/Test-ConstructionStatic.ps1
pwsh -NoProfile -File tools/Test-ConstructionContracts.ps1 -EvidenceRoot .codex-runtime/construction-search-20260920/contracts-final4
pwsh -NoProfile -File tools/Test-BaseBuildersStatic.ps1
pwsh -NoProfile -File tools/Test-Stage4Static.ps1
pwsh -NoProfile -File tools/Test-Stage35Static.ps1
pwsh -NoProfile -File tools/Test-AICommanderModeStatic.ps1
pwsh -NoProfile -File tools/Measure-ConstructionSearch.ps1 -LogPath <полный-console.log> -OutputDirectory <case>/metrics
pwsh -NoProfile -File tools/Test-ConstructionLog.ps1 -LogPath <полный-console.log> -ExpectedMode BOTH -RequireCompletion
pwsh -NoProfile -File tools/Test-BaseBuildersLog.ps1 -LogPath <полный-console.log> -MinimumCompleted 2 -RequireToolUse
pwsh -NoProfile -File tools/Test-ConstructionSearchLog.ps1 -LogPath <полный-console.log> -RequireBothSmall -MaxSmallPlacementMs 60000
```

Native launch воспроизводится из сохранённых `command.json` через
`tools/Start-AICFRuntime.ps1`; `run-case.ps1` в evidence служит точной оболочкой
для этих параметров, а не альтернативным launcher.
Версии обоих native executables подтверждены через `VersionInfo`:
`native-versions.json`, Workbench и Server `1.8.0.13`.

Сводные проверки evidence (рабочий каталог — корень репозитория):

```powershell
pwsh -NoProfile -File .codex-runtime/construction-search-20260920/summarize.ps1
pwsh -NoProfile -File .codex-runtime/construction-search-20260920/type-coverage.ps1
git -c safe.directory=C:/Users/retar/IdeaProjects/Arma-Reforger-AI-Conflict diff --check
```

`type-coverage.json` проверяет связь token/layout/faction с placement, настоящим
tool use, `BUILDER_COMPLETED` и `service_online=1` в полных stopped logs.
Это суммарное функциональное покрытие нескольких cases с указанными sources,
а не замена полного runtime verdict каждого case. `metrics-interval-check.json`
проверяет согласованность времён работы и подтверждения stock service.

## Изменённые файлы этой задачи

- Construction: `AICF_ConstructionPlanner.c`, `AICF_ConstructionOrder.c`,
  `AICF_ConstructionCandidate.c`, `AICF_ConstructionSiteSearch.c`,
  `AICF_ConstructionPath.c`, новый `AICF_ConstructionNavigation.c`.
- Инструменты: `Measure-ConstructionSearch.ps1`, `Test-ConstructionStatic.ps1`,
  `Test-ConstructionContracts.ps1`, новый `Test-ConstructionSearchLog.ps1`.
- Fixtures: `AICF_ConstructionRuntimeProbe.c`, `AICF_ConstructionBudgetProbe.c`,
  новый `AICF_ConstructionSearchProbe.c`. В production addon они не копируются.
- Документация: этот отчёт, `ARCHITECTURE.md`, `TESTING.md`,
  `CONSTRUCTION_VALIDATION.md`. Предыдущие пользовательские правки сохранены.

Client/JIP и визуальная ручная оценка — NOT RUN: задача проверяется server-only,
GUI/screenshot workflow не использовался. OS CPU baseline-пары и причинный
профиль cold/native spikes — NOT RUN; имеющихся samples недостаточно, чтобы
приписать изменения frame time только construction search.
