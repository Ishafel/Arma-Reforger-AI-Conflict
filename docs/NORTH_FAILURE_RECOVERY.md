# Отказы движения и строительства на севере Эверона

Изменения относятся к шести проблемам из передачи
`handoff-rhs-north-20260926-220216/FINDINGS.md`.

## Failed movement

Stock `SCR_AIProcessFailedMovementResult` для `UNKNOWN` и существующего
waypoint вызывает `Debug.Error` и сохраняет `RUNNING` для отладки. AICF
перехватывает только waypoint-related результат infantry handler
актуальной управляемой группы. Кроме default `0`, native BT передаёт `-1`.
Для `-1` отдельно исключаются назначенная машина и физическая посадка,
высадка или нахождение живых бойцов в машине. Missing-leader guard сохранён. Vehicle handler,
`WAITING_ON_NAVLINK` и посторонние группы продолжают штатную обработку.

Узел передаёт stock `OnMoveFailed`, завершает action с отказом и записывает
отказ в slot. Он не завершает waypoint и не создаёт новый. Запись проверяет
group, generation, assignment revision и конкретный owned waypoint. Повтор
того же результата не увеличивает счётчик. Существующий reliability loop
видит `WAYPOINT_MOVE_FAILED`, а OrderPlanner выбирает recovery endpoint.
Для base destination три отказа без подтверждения движения переводят
recovery в существующий temporary hold. Replacement/suspension снимает
ссылки на отказавший waypoint.
После `OnMoveFailed` повторно проверяются slot identity и executed action:
синхронный callback не должен привести к отказу уже нового action.
`AI_MOVE_CONTEXT` сохраняет входы и waypoint identity отказов, оставленных stock.

## Повторный выбор отвергнутой цели

Temporary route hold сохраняет в stable slot отвергнутую base на 180 секунд.
Память содержит несколько целей и переживает full replan. TargetSelector
учитывает её также при fallback, снимающем обычный `excludedTarget`.
OrderPlanner повторно проверяет cooldown перед созданием waypoint; fallback
контроллера не восстанавливает отвергнутый маршрут раньше срока.

Истечение cooldown разрешает новую попытку. Новая group generation не
наследует маршрутный отказ прежней группы; player intent имеет приоритет.
Cooldown не является доказательством недостижимости базы и не подтверждает
успешное восстановление. Физические проверки движения, ALL и 30-секундное
ожидание промежуточной точки сохранены.

Если recruitment/vehicle lifecycle снял waypoint, восстановление AI intent
во время cooldown создаёт локальный temporary hold. Оно не продлевает
deadline cooldown и не оставляет группу без задачи. Уже действующий hold
восстанавливается идемпотентно; после expiry снова разрешён маршрут.

## Metadata и повторный поиск площадок

`AICF_ResolveOutline` теперь повторяет lookup из Script Diff 1.8.0.13:
explicit composition mapping, три flat slot, три road slot, stock fallback.
Прежний adapter обрывался после flat slots. Runtime probe воспроизвёл
`OUTLINE_MISSING` для RHS AFRF heavy depot; после изменения этот prefab
проходит полную проверку geometry, service component и spawn slots.
Lookup не обходит последующие physical clearance, access и paid placement.

Metadata сообщает точную причину отказа. Закешированный unsupported prefab
публикует `CONSTRUCTION_DEFERRED` однократно за кампанию. `SEARCH_BUDGET_EXHAUSTED`
и `SEARCH_AREA_EXHAUSTED` увеличивают интервал новых поисков отдельно по
base/type: 60, 120, 240, 480 секунд при стандартной конфигурации. Более длинный
явный cooldown не сокращается. Остальные типы могут планироваться; cursor
поиска продолжает прежнюю последовательность. Owner/provider change снимает
прежнюю память, completion сбрасывает счётчик соответствующего типа.
15-секундная пауза builder completion сохранена.

## Random radius и radio contact

Четыре stock boundary с динамическими радиусами проверяют пустой диапазон.
Combat destination без длины и look без разброса сохраняют заданный центр;
point search с пустым допустимым диапазоном возвращает отказ. Положительные
диапазоны обрабатывает stock. `AI_RANDOM_RANGE_REJECTED` содержит caller;
исходное единичное событие без стека нельзя приписать одному из этих узлов
по старому логу. Глобальный `RandomGenerator` не изменён.
В новом естественном 35-минутном soak зафиксирован пустой диапазон именно
в `SCR_AIGetRandomLookPosition` при `tangent=0.7`: нулевое расстояние до
точки взгляда. Guard вернул исходную позицию без native ошибки диапазона.

`REPORT_CONTACT` без entity либо position отклоняется с `FAILED` в очереди
и при ожидании канала. Уже начатая передача, другие типы и валидные запросы
остаются у stock. `CONTACT_REQUEST_REJECTED` обозначает отказ, а не успешную
передачу или восстановление исчезнувшей цели.
Естественный soak также зафиксировал отклонение пустого request в WAITING.

## Проверки

`tools/Test-NorthFailureContracts.ps1` проверяет границы перехвата, fencing,
cooldown, lookup и radio state; отрицательные мутации должны отклоняться.
Это static gate, отдельно от Workbench и runtime.

`tools/fixtures/AICF_NorthFailureProbe.c` добавляется только в isolated stage.
Canonical launcher запускает stock/RHS server с `-aicfNorthFailureProbe 1`.
Fixture вводит `UNKNOWN` через тот же helper, что использует native BT input
boundary, проверяет соседние ветки, stale identity, cooldown, radio и radius.
Полная metadata geometry проверяется последовательно по ticks; через 270 секунд
измеряется физическое смещение группы. Это не полное воспроизведение stock
pathfinding ошибки. Проверка native BT port wiring остаётся статической.
Окно превышает 180-секундный route cooldown: прежние 90 секунд могли
закончиться во время законного hold, когда альтернативной базы нет. Порог
физического смещения остаётся 30 м; такой hold сам по себе не считается recovery.
Fixture измеряет максимум расстояния исходного живого лидера от начальной
позиции и требует два раздельных samples за порогом. Возврат за recruitment
не должен обнулять наблюдавшееся движение; конечное расстояние также пишется
отдельно. Полный log дополнительно проверяется на hidden relocation этой группы.

`tools/fixtures/AICF_NativeMoveFailureProbe.c` воспроизводит исходную позицию
из полного RHS North лога и задаёт законный point order к исходному endpoint.
На старте эта база ещё не достижима по radio graph; проверки base orders
не обходятся. Однократный перенос
персонажей служит только подготовкой теста; production recovery его не
использует. Результат pathfinding и входы BT не подменяются. Эта fixture
запускается отдельно с `-aicfNativeMoveProbe 1`, заканчивается через 190 секунд
после ROSTER_READY и не заменяет естественный soak.

Для end-to-end heavy depot используется отдельная существующая fixture
`AICF_ConstructionRuntimeProbe` с matrix type 4: production search, paid layout,
builder и online service. Она пополняет supplies только внутри тестового stage.
Нужны отдельные естественный RHS soak и полные остановленные логи. Client/JIP
и визуальный результат не подтверждаются server-only fixture.

Команды, исходные failures и актуальные verdict сохранены в
`.codex-runtime/rhs-north-fixes-20260926/result.md`.

## Несовпадение типа waypoint в defend activity — 27.09.2026

На сервере №5 зафиксированы две VM: ActivityDefend.bt →
SCR_AIGetDefendWaypointParameters.c:21, Wrong class of provided Waypoint.
Пауза около 189 секунд между ними объяснена пользователем: окно ошибки
оставалось без реакции. Это не доказательство самостоятельного зависания сервера.

В AICF_AINodeLifecycle.c добавлен локальный guard входа defend node:
явный WaypointIn имеет приоритет, включая явный null; при отсутствующем
порте используется GetCurrentWaypoint(), как в stock 1.8.0.13. Если результат
не SCR_DefendWaypoint, узел возвращает FAIL до stock NodeError. Корректный
Defend обрабатывается через super.EOnTaskSimulate с родными preset и outputs.
Guard не меняет очередь, intent, action, slot и не удаляет entities.

Соседство VM со сменой приказа после recruitment — гипотеза причины,
а не доказанное сопоставление группы из стека со stable slot.
Проверки: tools/Test-DefendWaypointInputContracts.ps1 и isolated fixture
tools/fixtures/AICF_DefendWaypointInputProbe.c. Fixture выполняет реальную
смену Defend → Move через planner, проверяет native SUCCESS на Defend,
64 повторных FAIL старого defend node на Move и неизменность нового приказа.
Полный текущий verdict и evidence:
.codex-runtime/defend-waypoint-fix-20260927/RESULT.md.
