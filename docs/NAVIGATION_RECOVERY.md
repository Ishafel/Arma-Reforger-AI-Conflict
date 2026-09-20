# Восстановление навигации — 2026-09-19

Исходная проблема: повторные `Failed move` у `US slot=0 generation=3`
после Девичьей бухты к Мо и `USSR slot=0 generation=1` к Кермовану.
Snapshot и первоначальный разбор:
`.codex-runtime/server-errors-20260919/`. Это снимок живой сессии, не полный
остановленный runtime gate.

В отдельном RHS server удалось воспроизвести `Failed move` в точке США из
snapshot. Успешные navmesh probes оставались в нескольких метрах от origin;
дальнего связанного участка там не оказалось. Перевыдача того же дальнего
waypoint не исправляла эту ситуацию. Индивидуальная navmesh бойца показала
то же ограничение. Короткий связанный маршрут в районе Кермована позволил
продолжить движение с сохранением identity и strategic intent.

## Поведение

С 2026-09-20 по запросу владельца общий player fence скрытого восстановления
пехоты и армейского транспорта использует радиус запрета 50 м по умолчанию
и проверку прямой видимости до 100 м. Проверяются исходная и конечная точки,
controlled entity и main entity игрока. CLI `aicfHiddenRecoveryPlayerRadiusMeters`
допускает 50..1000 м. Значения применяются при новом запуске; текущая сессия
сохраняет загруженный код и конфигурацию.

Аварийный выход со штаба ищет свободную точку сначала около 50 м вперёд
от каждого бойца к текущей цели. Если свободного участка нет, поиск расширяется
с шагом 50 м: 100, 150, 200 м. Выбирается первый подходящий участок.
Дальность по горизонтали ограничена текущим шагом, а перед переносом повторно
проверяется общий предел 200 м. Перенос к удалённой базе больше не используется.
Точка должна приближать бойца к цели, быть вне 100-метровой зоны штаба и
не пересекаться с местами других бойцов. После четырёх неудачных этапов
перенос отклоняется; дальнего fallback нет. Player/LOS и combat запреты
не обходятся увеличением дальности.
Обычный маршрут, одноразовый budget, identity и player/combat guards сохранены.

Последующий stock матч выявил отдельный случай появления бойца внутри
контейнера по наблюдению пользователя: локальная коррекция безопасного выхода
не нашла. Исправление проверки места до появления бойца и последующие runtime
результаты описаны в [INFANTRY_SPAWN_PLACEMENT.md](INFANTRY_SPAWN_PLACEMENT.md).
Восстановление маршрута само по себе не предотвращает небезопасный spawn.

- Обычный stuck-rebuild `ATTACK BASE`, кроме relay, выбирает промежуточный
  navmesh endpoint. `POSITION`, relay и system hold сохраняют прежнюю семантику.
- Временный участок хранит identity своего waypoint. Возврат к исходной базе
  требует stock `GROUP_CALLBACK_COMPLETED` и физического прибытия leader.
- Stuck watchdog использует радиус промежуточного waypoint, чтобы не считать
  отряд прибывшим лишь из-за общего 100-метрового допуска баз.
- `STUCK_ROUTE_ENDPOINT_SELECTED`/`STUCK_ROUTE_ENDPOINT_UNAVAILABLE` описывают
  выбор и отказ, включая состояние tile и результаты локальных probes.
- Если восемь успешных probes из navmesh origin не выходят за 5 м,
  `AICF_IsolatedNavmeshRecovery` проверяет ближайшие кольца 3/5/7 м. Конечная
  точка должна быть не дальше 8 м, иметь связанный выход минимум на 25 м,
  допустимую высоту, не быть водой и пройти body sweep/occupancy checks.
- Коррекция разрешена только единственному живому authoritative AI без
  possession, вне транспорта и переходов, при отсутствии combat threat и
  после существующих player-distance/LOS guards. Она отключается через
  существующий `aicfHiddenRecoveryEnabled`. Неизвестное состояние — отказ.
- Одна физическая мутация за generation, с повторной проверкой identity и
  clearance непосредственно перед `Teleport`. Группа, roster, target, tickets
  и supplies не заменяются. После этого planner отдельно перестраивает маршрут;
  telemetry сохраняет `movement_confirmation=PENDING` до реального движения.

## Проверка

Evidence находится в `.codex-runtime/navigation-fix-20260919/`.
`before-*` и `after-*` сохраняют полные статические результаты.
До правки Stage3Static, Stage35Static, Stage35RecoveryPolicy, Stage4Static и
MapPointOrdersStatic — PASS. AICommanderModeStatic — FAIL: две прежние проверки
`AI_COMMANDER_UI_STATE` (waiting label и HQ attack marker).

`tools/fixtures/AICF_NavigationRecoveryProbe.c` применяется только к отдельной
копии addons. Fixture перемещает исходных одиночных бойцов в записанные точки
и задаёт две цели, обходя только radio eligibility этих целей. Это проверка
навигации, а не реплика всей старой кампании. Сам route rebuild, watchdog,
ограничения коррекции и дальнейшее движение выполняет production code.
Дополнительные cases проверяют отказ player fence без мутации и запрет
повторного применения после расходования generation budget.

Запуск stage выполняется через `tools/Start-AICFRuntime.ps1 -Role Server
-Variant EveronNorthRHS -RepositoryRoot <stage> -ProfileRoot <fresh-profile>`
с `-AdditionalArguments @('-aicfNavigationProbe','1',
'-aicfRequirePlayerForResult','0','-aicfConstructionDecisionMs','3600000')`.
Manifest и полные остановленные logs обязательны. Fixture сама завершает
server после результата или 180 секунд наблюдения; досрочные диагностические
остановки фиксируются отдельно и не считаются PASS.

Проверка исходного живого матча после обновления, client/JIP и визуальная
проверка не подменяются этим тестом. Уже запущенный server использует старые
скрипты до перезапуска.

## Итог проверки

Game/Server/Tools: `1.8.0.13`. Исходный commit
`05f3c8ebca12eba3a69e19abe84ea74020404d0f`, branch `main`, dirty workspace;
пользовательские изменения сохранены. Новые файлы не закоммичены.

| Команда / gate | Verdict |
|---|---|
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3Static.ps1` | PASS / 0 до/после |
| Аналогично `Test-Stage35Static.ps1` | PASS / 0 до/после |
| Аналогично `Test-Stage35RecoveryPolicy.ps1` | PASS / 0 до/после, новые route assertions |
| Аналогично `Test-Stage4Static.ps1` | PASS / 0 до/после |
| Аналогично `Test-MapPointOrdersStatic.ps1` | PASS / 0 до/после |
| Аналогично `Test-AICommanderModeStatic.ps1` | Сохранён FAIL / 1, две `AI_COMMANDER_UI_STATE` |
| `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent ... -wbModule=ScriptEditor -run -validate` | Arland/Everon/ArlandRHS/EveronRHS PASS / 0; exact args в `wb-final-*-args.json` |
| Canonical RHS server, fixture `runtime-rhs-6` | Целевые assertions PASS / exit 0; штатный `RequestClose` |
| `git diff --check` | PASS / 0 |
| Client/JIP, visual, soak, live match после обновления | NOT RUN |

Финальный server работал 18:55:03–18:58:15 МСК. US сохранил entity,
group/generation/intent, получил одну локальную коррекцию примерно на 5 м,
затем самостоятельно прошёл маршрут: displacement от исходной точки 92.1914 м.
USSR прошёл 406.209 м. `NAVIGATION_PROBE_FINISHED passed=2 total=2
player_fence=1 single_use=1`. `Teleport` применяется асинхронно: immediate
`after` может ещё совпадать с `before`; этот снимок не является movement proof.
В данном прогоне обычный маршрут США восстановился на следующем stuck tick.

Полные logs находятся в
`.codex-runtime/navigation-fix-20260919/runtime-rhs-6/logs/logs_2026-09-19_18-55-03/`.
Одна `Failed move` воспроизведена до recovery; повторов после него нет.
Отдельно сохранены native diagnostics, совпавшие по числу с исходным snapshot:
159 `m_fAILimitThreshold`, 6 `Parent`, 10 `Incorrect tile position`,
4 `RPC_OnArsenalUpdated`, 4 faction init errors. Поэтому общий server gate не
является «чистым PASS»; `runtime-final-verdict.json` отделяет focused navigation
от `NOT_CLEAN_NATIVE_DIAGNOSTICS`. Ранние диагностические прогоны 1–5 и
промежуточные compile failures сохранены; они не выдаются за финальные gates.

Изменённые файлы этой задачи: `AICF_OrderPlanner.c`, `AICF_GroupSlot.c`,
`AICF_MatchController.c`, новый `AICF_IsolatedNavmeshRecovery.c`,
`tools/Test-Stage35RecoveryPolicy.ps1`, новая
`tools/fixtures/AICF_NavigationRecoveryProbe.c`, `docs/ARCHITECTURE.md`,
`docs/TESTING.md` и этот документ. Production sources в stage и рабочем дереве
сверены SHA-256; fixture в production addons отсутствует.
