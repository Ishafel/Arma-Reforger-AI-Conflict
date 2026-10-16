# Устаревший вход allocation в ActivityDefend

В живом Everon North WCS Hard на `8d199ec` дважды возникло
`Defend Waypoint not provided to the node!`. Native
`SCR_AIAllocateActionsForDefendActivity.EOnTaskSimulate` из Script Diff 1.8.0.13
читает `WaypointIn`, затем вызывает `NodeError`, если вход нельзя привести к
`SCR_DefendWaypoint`. `NodeError` вызывает `Debug.Error`: кроме VM diagnostic
появляется диалог. Исходная защита `SCR_AIGetDefendWaypointParameters` относится
к другому узлу и не закрывает это позднее чтение.

## Контракт исправления

`AICF_AINodeLifecycle.c` проверяет настоящий `WaypointIn` allocation node.
Отсутствующий или неверно типизированный вход даёт `ENodeResult.FAIL` до native
allocation только на authoritative server и только если native `m_groupOwner`
сейчас принадлежит managed infantry slot активного controller. Валидный
`SCR_DefendWaypoint`, чужие группы и client сохраняют native delegate.

Guard не подставляет current waypoint вместо старого входа, не меняет приказ,
не завершает новый activity, не вызывает Remove/Delete и не создаёт callbacks.
Planner сохраняет владение waypoint. Предупреждение
`[AICF][AI_DEFEND_ALLOCATION_CANCELLED] group=... input=... reason=INVALID_DEFEND_INPUT`
публикуется один раз на последовательность отказов данного node; валидный
вход снова разрешает следующую диагностику.

## Терминальный oracle

`tests/fixtures/AICF_DefendAllocationProbe.c` копируется только в isolated Core
stage. `-aicfDefendAllocationProbe 1` использует реальные US/USSR groups и native
BT nodes. После успешной stock allocation fixture заменяет hold waypoint через
`AssignPlayerPointOrder`. Она читает тот же native port после удаления старого
entity и повторяет по 64 вызова на том же node в его настоящем BT callstack.
Это детерминированная инъекция позднего вызова при настоящей замене waypoint,
а не утверждение, что естественная гонка повторится в каждом коротком матче.

Oracle проверяет native preconditions: group, utility, mailbox и cached defend
activity существуют. Cached `m_RelatedActivity.m_RelatedWaypoint` не обязана
совпадать с текущим входом: stock node кеширует activity в `OnInit`. Проверять
это равенство как обязательный native контракт было бы ошибкой самого теста.
Point order использует `SCR_DefendWaypoint` для MOVE_AND_HOLD; смена проверяется
по entity identity, а не по требованию другого класса.

Итого 22 readbacks: успешная native allocation обеих сторон, исчезновение старого
waypoint/input, 128 ожидаемых FAIL, сохранность новой waypoint queue и group,
новый current waypoint через 30 секунд. Во время focused oracle fixture
приостанавливает controller Update/CommanderTick/ReliabilityTick, сохраняя
native AI и BT. Полный gameplay/soak этим не проверяется. Timer удаляется в
Stop, собственный probe-server завершает себя через RequestClose.

Запуск — только canonical `tools/Start-AICFRuntime.ps1`, отдельный свободный
listener через `-addr 127.0.0.1:2003`, fresh profile, `-Difficulty Hard` и
`-noThrow -aicfDefendAllocationProbe 1 -aicfRequirePlayerForResult 0`.
`-noThrow` используется в обоих сравнимых runs и не исключает VM diagnostics
из полного лога. Текущая игровая пара на 2001 не останавливается.

`Test-DefendAllocationLog.ps1 -LogPath <stopped-console.log> -LauncherPath <launcher.txt>
-NativeExitPath <exit.txt>` требует native exit 0, полный stopped log, exact CLI,
manifest, 22 уникальных положительных readbacks, две попытки по 64 вызова,
два ограниченных cancellation warnings и ноль script/VM/fatal errors.
`-ExpectReproduction` отдельно подтверждает ожидаемый исходный дефект, печатая
`runtime_gate=FAIL`; это не runtime PASS исходного source.

Static contracts: `Test-DefendWaypointInputContracts.ps1`, 11 mutations, включая
native delegate, authority/ownership fences, отсутствие подмены входа и bounded
diagnostics. Synthetic log contracts: `Test-DefendAllocationLogContracts.ps1`.
Результаты и ограничения — [TESTING.md](TESTING.md).
