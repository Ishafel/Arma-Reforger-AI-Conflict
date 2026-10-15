# Штатная охрана при подходе ИИ — issue #54

`AICF_ManagedObservers` добавляет живые управляемые силы US/USSR в серверный
`ObserversSystem`. Штатные ambient patrol spawn points получают наблюдателя
при подходе ИИ даже без подключённого игрока. Это общий proximity-механизм:
наблюдатели видны штатным серверным потребителям, включая lifecycle групп.
Они не реплицируют игрока и не создают отдельный гарнизон.

## Владение и lifecycle

Сервис запускается и останавливается `AICF_MatchController`; его `Update`
вызывается существующим authority scheduler раз в секунду. Собственных
`CallLater` или подписок нет. Допуск требует server/master, работающий матч,
READY slot, master controller и живого лидера/участника через
`AICF_GroupRuntime.ResolveAliveLeader`. Фактическая faction бойца после
нормализации content profile должна соответствовать US/USSR state.

Локальный observer следует за entity живого бойца. Origin `SCR_AIGroup`
не используется. Ключ `-1513160000 + 2 * numeric slot + side` постоянен
при замене группы и переименовании роли; `side` равен 0 для US и 1 для USSR.
Это зарезервированный проектом диапазон ключей `InsertObserverSP`.
Каждое обновление читает текущую incarnation slot, переустанавливает observer
и удаляет отсутствующие ключи. Пустой, погибший, неготовый или чужой roster
не сохраняет observer. Удаление после потери допустимости занимает не более
одного шага scheduler. `Stop` удаляет все собственные ключи и допускает повтор.

Сервис не вызывает `SpawnPatrol`, `RequestSpawn`, `SpawnMembers`, не меняет
AI budget, lifecycle policy, eliminated/respawn state или faction базы.
Состав, исходные позиции, ограничения бюджета и очередь появления бойцов
остаются у native кода 1.8.0.13:

- `SCR_AmbientPatrolSystem.ProcessSpawnpoint` проверяет eliminated, respawn,
  budget и наличие observer, затем создаёт controller;
- `SCR_AmbientPatrolSpawnPointComponent` выбирает исходный prefab/waypoint
  и назначает `ProximityDriven`;
- `SCR_AIGroup.LifecycleTick` и native spawn queue отдельно материализуют
  бойцов, сохраняют их в despawn band и дематериализуют после ухода;
- native base faction callbacks и eliminated flags сохраняют свои правила.

Manager проходит точки с native интервалом 3 секунды, group lifecycle —
1 секунда; observer обновляется раз в секунду. Очередь, загрузка navmesh и
AI budget могут отложить появление: probe проверяет readback через 20 секунд
после подхода при доступном бюджете, а не устанавливает новый deadline игры.
Native very-near запрет pop-in и elimination у недоматериализованной позиции
не обходятся. Дальний отряд не включает всю карту.

Дополнительные машины, экипажи и десант остаются у
`AICF_FIAGarrisonService`/`AICF_FIAPatrolService`; RHS/WCS вооружение — у
профильных adapters. Сервис не меняет их план или identity.

## Проверки

`tests/contracts/Test-ManagedObserversContracts.ps1` входит в полный offline
runner. Проверяет authority/readiness, live entity, faction, numeric slot,
удаление устаревших observers, Stop и отсутствие обхода native владельцев;
отрицательные мутации должны отклоняться. Это отдельный static gate.

Runtime fixture добавляется только в `AIConflictCore/Scripts/Game/ZZTests/`
изолированного stage всех локальных addon выбранного graph:

- `tests/fixtures/AICF_AmbientPatrolProbe.c` — общий probe; исходный код
  запускается с `-aicfAmbientProbe baseline`;
- `tests/fixtures/AICF_ManagedObserversProbe.c` добавляется только после
  исправления; `-aicfAmbientProbe us` выполняет US, затем USSR cases.

Подготовь stage с production sources и generated resource databases,
выполни terminal Workbench по [DEVELOPMENT.md](DEVELOPMENT.md), затем запусти
`tools/Start-AICFRuntime.ps1 -Role Server -Variant <Variant> -RepositoryRoot
<stage> -ProfileRoot <fresh-profile> -AdditionalArguments <array>`.
Массив содержит `-aicfAmbientProbe`, режим, `-aicfRequirePlayerForResult`, `0`;
при отдельном listener добавь `-addr`, `127.0.0.1:<port>` и проверь фактический
`listening on address` в полном логе. Не подменяй это параметром `-ServerPort`.

Probe выбирает существующие FIA точки по world data, перемещает живой roster
на сушу и отдельно читает spawn point, controller, dormant counts, alive/active
members, prefab и физическую позицию. Он защищает тестовые силы от случайного
боевого урона; управляемый negative roster и целевые FIA затем убиваются явно.
В budget case на 10 секунд устанавливает native limit 1 и приостанавливает
только восстановление builder capacity floor. Native manager/lifecycle/queue
не подменяются. Ограничения fixture не входят в production.

После `RequestClose` анализируй полный остановленный `console.log`:
`tests/log-audits/Test-AmbientPatrolLog.ps1 -LogPath <log> [-Baseline]`.
Отсутствующий случай, failed readback, незавершённый log, script/VM error
или leaked AICF type дают FAIL. Native resource/shutdown diagnostics печатаются
отдельно; functional PASS не означает чистый content/engine log.

Фактические сценарии, команды и результаты текущей реализации:
[TESTING.md](TESTING.md). Внешний вид боя, client/JIP и длительный soak
не подтверждаются этим серверным probe.
