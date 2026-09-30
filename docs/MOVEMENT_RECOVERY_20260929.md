# Исправление движения FIA и ожидания navmesh — 29.09.2026

Исходный отчёт: `.codex-runtime/all-variants-20260929-215110/HANDOFF.md`.
Почти 12 минут между двумя записями Failed move — ожидание закрытия diagnostic
окна пользователем. Это не измерение engine hang или длительности pathfinding.

## Выводы из остановленных исходных логов

Срез `20260929-223951` сохранён после проверки процессов: семь исходных
серверов уже отсутствовали, client processes=0. Их native exit codes неизвестны.
Новые серверы запускаются отдельно, только canonical launcher, без клиента.

FIA slot 0 stock Everon остался на первом leg 1→8. `UNKNOWN` с существующим
related waypoint попадал в stock `NodeErrorOnce`, который намеренно возвращает
RUNNING для отладки. Группа не является US/USSR infantry slot; применять к ней
пехотный recovery нельзя. Отдельно служба бесконечно обновляла waypoint каждые
две минуты без доказательства полезного прогресса.

US A4 Everon North нарушил hard deadline, но позднее вышел с базы в той же
generation: в 22:07 расстояние 836 м, в 22:14 — 1982 м. Это не постоянное
зависание. В 21:58:34 был назначен временный waypoint `tile_wait=1`, после чего
обычное ожидание ALL держало колонну ещё 30 секунд. Несколько таких ожиданий
увеличивают задержку выхода. Позиции heartbeat leader и полного MOB envelope
имеют разную семантику; один `at_mob=0` в heartbeat не доказывает выход всех.

Предупреждения движения сгруппированы по faction/numeric slot/generation/
assignment/waypoint. Повторения разных assignments нельзя считать одним
непрерывным эпизодом. В старом RHS log US slot 2 также повторял отказ цели 31
после трёхминутного запрета; `FALSE_COMPLETION` не означает прибытие. Такие
отказы остаются видимыми. Исправления ниже не доказывают достижимость всех
маршрутов stock/RHS/WCS или устранение ошибок ресурсов движка.

## Изменения

- `AICF_FIAPatrolMovementPolicy` обрабатывает только server-side UNKNOWN,
  waypoint-related событие зарегистрированного FIA patrol. Проверяются текущий
  waypoint, graph revision, crew, vehicle и native handler. Чужие группы и
  остальные stock ошибки сохраняют штатную обработку.
- `OnMoveFailed` вызывается один раз с настоящим vehicle handler. Перед Fail
  повторно проверяются token и exact executed action: синхронный callback не
  должен завершить новый приказ.
- Снимок ошибки сохраняет group/vehicle/crew identity, generation, leg, graph
  revision, target и waypoint. Recovery применяется существующим Update, без
  нового таймера. Устаревший снимок отменяется.
- Старый waypoint снимается через `AICF_VehicleTaskHandoff`; новый выдаётся на
  следующем tick. Максимум две повторные попытки без сокращения расстояния до
  endpoint на 15 м. Третья ошибка или исчерпание обычных двухминутных проверок
  завершает lifecycle с `ROUTE_RECOVERY_EXHAUSTED`, сохраняя entities.
- Временная пехотная точка ожидания tile проверяет именно запрошенный tile
  на существующем task audit. После загрузки маршрут продолжается без ожидания
  сбора ALL. Сохраняются group/assignment/waypoint guards, прежний target/intent,
  30-секундный предел загрузки и обычная семантика физического прибытия.
- Новые события: `FIA_PATROL_MOVE_FAILED`, `FIA_PATROL_ROUTE_RETRY`;
  новая причина rebuild: `INFANTRY_APPROACH_TILE_READY`.

## Воспроизведение

`tools/Test-MovementRecoveryContracts.ps1` проверяет 17 отрицательных мутаций
identity, ownership, callback guards, retry budget и порядка ожидания tile.
Обычные FIA, InfantryApproach и Stage audits остаются обязательными.

`tools/fixtures/AICF_MovementRecoveryProbe.c` копируется только в отдельный
stage. `-aicfMovementRecoveryProbe 1` запускает шестиминутный Everon с инъекциями
UNKNOWN в существующий production Move activity, stale token tests, проверкой
двух retries и сохранения entities после retirement. `... 2` наблюдает восемь
минут обычного EveronNorth без инъекции FIA и пишет `TILE_RESUME_PROBE`.
Оба режима завершаются через RequestClose. Fixture не входит в production.

Пример: `tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorth
-RepositoryRoot <stage> -ProfileRoot <fresh-profile> -ServerPort <port>
-AdditionalArguments @('-addr','127.0.0.1:<port>','-aicfMovementRecoveryProbe','2',
'-aicfRequirePlayerForResult','0')`.

При VM exception внешний терминальный watchdog завершает только exact
server process данного profile. Такая остановка означает FAIL, не PASS.
Клиент и GUI automation не используются.

`tools/Watch-AICFRuntimeDiagnostics.ps1 -Profile <тот же абсолютный fresh-profile>
-DeadlineSeconds 550` запускается во второй терминальной сессии. Он сравнивает
целый аргумент `-profile`, а не префикс пути, и повторно проверяет process перед
остановкой. Полные native logs сохраняются в profile; forced stop дополнительно
создаёт `watchdog-stop.txt`. Exit 2 означает diagnostic stop, exit 3 — timeout,
exit 0 — наблюдаемое завершение сервера; сам по себе exit 0 не является verdict
исправности gameplay. Клиента этот инструмент не создаёт и не останавливает.

## Gates

Результаты конкретных запусков и пути к полным logs сохраняются в итоговом
отчёте. Static, Workbench и runtime оцениваются отдельно. Baseline Stage4
`STAGE4_ATTACKED_BASES` существовал до исправлений; UI этой задачей не менялся.
Client/JIP/visual, семивариантный soak и достижимость каждого radio leg
этими тестами не покрыты. Самостоятельный статус ACCEPTED не присваивается.

## Фактические результаты

- Workbench production Everon, EveronRHS, ArlandWCSRHS: PASS, native exit 0.
- Everon 22:48:48–22:55:15: 17/17 assertions; native exit 0; 15 FIA arrivals;
  SCRIPT E/F=0, VM=0, MOB deadline=0. Lifecycle slot 0 корректно завершён после
  двух retries; entities сохранены. 55 native engine/resource E-строк остаются.
- Everon North 22:57:12–23:05:39: native exit 0; 23/23 resumes за 82–1231 мс,
  group/intent сохранены. US A4 вышел из MOB с 10 бойцами в generation 1:
  23:05:10 full-envelope minimum 101,426 м, 23:05:31 heartbeat 133,198 м.
  Ранее COMBAT_THREAT штатно блокировал скрытую коррекцию. SCRIPT E/F=0, VM=0,
  MOB deadline=0; 47 native E-строк, 2 FALSE_COMPLETION, 1 GROUP_STUCK остаются.
- Test-FIAPatrolLog для обоих полных stopped logs: PASS / 0. Не все патрули
  прибыли: Everon 9/18, North 2/3; RequireEveryPatrolArrival не применялся.
- Все релевантные static audits PASS, кроме сохранённого STAGE4_ATTACKED_BASES.
  MovementRecoveryContracts: 17 отрицательных мутаций; git diff --check PASS.
- Клиент, JIP, visual, семь вариантов runtime и длительный soak: NOT RUN.

Полный отчёт и архив:
`C:/Users/retar/Documents/Codex/2026-09-29/new-chat/outputs/`.
Baseline/after, exact CLI, source SHA-256 и native logs сохранены в `work/`
той же рабочей папки. Шесть production sources совпадают с runtime stage.
