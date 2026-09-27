# Сбор пехоты перед первым выходом

Автономная `INFANTRY` по-прежнему создаётся с одним бойцом. До набора заданной
численности (по умолчанию 10) `AICF_OrderPlanner` удерживает её у фактического
живого лидера с posture `INFANTRY_MUSTER`, сохраняя боевое strategic intent.
Готовая безопасная казарма запускает обычный подход и платное пополнение.
Только полный состав снимает первоначальное ожидание; фиксированного таймера
выхода нет. Отряд с явно заданной численностью 1 ждать не должен.

Если казарма ещё строится, недоступна из-за боя, закончились supplies или
истёк timeout визита, неполный отряд остаётся на месте и повторяет поиск по
существующему интервалу 60 секунд. Он не продолжает атаку одиночным бойцом.
Это может задержать выход до доставки supplies или восстановления казармы.
Цены, managed-agent budget, combat safety и identity fences набора сохранены.

`AICF_GroupSlot` хранит завершение первоначального набора для текущей группы.
После выхода последующие потери не включают первоначальное ожидание заново:
работает обычное пополнение. Полная замена группы сбрасывает готовность.
Явный приказ игрока имеет приоритет; motorized группы и сторона без
AI-командира этим правилом не задерживаются.

Ожидание считается частью `IsRecruitingInfantry()`: commander, stuck recovery
и обычное продвижение не подменяют его атакой. Waypoint создаёт, восстанавливает
и удаляет только planner. `AICF_InfantryRecruitmentService.Update()` обслуживает
ожидание без новых callbacks. `INFANTRY_MUSTER_WAITING` содержит faction, slot,
generation, group, alive, desired и waypoint; существующие events не изменены.

## Проверка

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryRecruitmentStatic.ps1`
проверяет новые gates и семь отрицательных мутаций. Остальные применимые gates:
Stage3, Stage35, RecoveryPolicy, Stage4, AICommanderMode, MapPointOrders и
BarracksCombatContracts; Workbench Validate проверяется отдельно.

Fixture `tools/fixtures/AICF_InfantryRecruitmentRuntimeProbe.c` в изолированной
копии Core с `-aicfRecruitProbe 1 -aicfRecruitProbeMuster 1` задерживает создание
казарм на 60 секунд. Проверяет удержание двух выбранных slots в пределах 30 м,
отсутствие выхода до полного набора и восстановление боевого приказа для двух
групп по 10. Остальные slots получают desired=1. Fixture готовит здания и
supplies, поэтому не доказывает скорость обычного автономного строительства.
Запуск — только `tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock|RHS`
с `-RepositoryRoot <изолированная копия>` и отдельным свежим profile.
Полный остановленный log проверяется `Test-InfantryRecruitmentLog.ps1
-LogPath <console.log> -RequireFullRosters -RequireMuster`; ошибки полного лога
остаются failures, даже если focused muster evidence положительно.

Дополнительные сценарии: отсутствие supplies, combat lock, уничтожение службы,
замена группы, player order во время ожидания, изменение desired size и Stop.
Client/JIP, длительный бой и визуальная проверка требуют отдельных verdict.

## Результат проверки 2026-09-27

Восемь перечисленных static gates до/после — PASS / 0, baseline failures нет.
Семь source mutations обнаружены. Workbench Arland, Everon, ArlandRHS,
EveronRHS и изолированной fixture — PASS / 0 на версии `1.8.0.13`.

Stock dedicated fixture: `waited_ms=60217 held=1 us=1 ussr=1`, затем два
полных roster по 10 с прежними group identities. `held_until_full=1`,
18 пополнений, два закрытых визита; атака восстановлена после полного набора.
Server завершён самой fixture, exit 0; полный log проходит
`Test-InfantryRecruitmentLog.ps1 -RequireFullRosters -RequireMuster` (PASS / 0).
Две мутации log evidence дают ожидаемый FAIL / 1.

В полном log нет SCRIPT E/F, ENGINE F или VM errors, но есть 14 native E
сообщений: resource GUID, stock world keywords, повтор Hierarchy и shutdown
resource leak. Они сохранены без исключения; чистый native runtime PASS и
сравнение этих сообщений с новым pre-change runtime не заявляются.
RHS runtime, client/JIP, soak, визуальная проверка, снижение потерь tickets
в длительном бою и дополнительные fault cases — NOT RUN.
Команды, полные logs, manifest и подробности находятся в локальном evidence
`.codex-runtime/infantry-muster-20260927/`.
