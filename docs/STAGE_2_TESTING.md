# Stage 2 — надёжность и баланс

Stage 2 принимается отдельно от Stage 1. Его цель — доказать, что автономный цикл не деградирует при потере waypoint, задержках spawn, застревании AI и длительной работе без игрока.

Текущий Stage 2-кандидат добавляет:

- уникальную `generation` каждой попытки развёртывания слота;
- запрет повторной привязки группы и аудит lifecycle-инвариантов;
- ограничение числа одновременно создаваемых replacement-групп;
- автоматическое восстановление потерянного или завершившегося waypoint;
- stuck-watchdog по фактическому сокращению расстояния от живого лидера до waypoint;
- подавление recovery-churn у активной цели и штатную замену устойчиво застрявшей группы;
- компактную stock-формацию `Column` без телепортации бойцов;
- перестроение маршрута застрявшей группы и диагностику повторных попыток;
- общий параметр темпа войны и отдельные настройки надёжности через CLI;
- headless-режим результата без обязательного игрока;
- воспроизводимый test-hook потери приказа;
- `[AICF][STAGE2]` heartbeat и итоговую сводку.

## Обязательная подготовка

1. Зафиксировать commit и версии игры, Tools и Server.
2. Открыть `AIConflictArland` в Workbench.
3. Выполнить `Build → Validate Scripts` и `Compile and Reload Scripts`.
4. Любая ошибка компиляции означает `BLOCKED`.
5. Каждый прогон использует новый `-profile` и `-backendFreshSession`.

## Настройки Stage 2

| CLI-параметр | Значение по умолчанию | Назначение |
|---|---:|---|
| `aicfWarTempoPercent` | `100` | Масштаб темпа: `200` вдвое сокращает стандартные commander/reinforcement интервалы |
| `aicfRequirePlayerForResult` | `1` | `0` разрешает чистый headless-прогон |
| `aicfReliabilityIntervalMs` | `5000` | Частота lifecycle/order/stuck аудита |
| `aicfOrderRecoveryRetryMs` | `5000` | Минимальный интервал повторных попыток восстановления приказа |
| `aicfStuckWatchdog` | `1` | Включение stuck-watchdog |
| `aicfStuckTimeoutMs` | `120000` | Время без достаточного прогресса до признания группы застрявшей |
| `aicfStuckProgressMeters` | `25` | Минимальное сокращение дистанции, считающееся прогрессом |
| `aicfMaxStuckRecoveries` | `3` | Число реальных перестроений маршрута; при следующем stuck группа заменяется штатно |
| `aicfObjectiveHoldTimeoutMs` | `300000` | Grace-период у активной цели без повторной выдачи завершившегося waypoint |
| `aicfMaxConcurrentSpawns` | `1` | Максимум одновременных replacement-spawn |

Явные `aicfCommanderIntervalMs` и `aicfReinforcementDelayMs` применяются после `aicfWarTempoPercent` и имеют приоритет.

## Быстрый reliability-прогон на отдельном порту

Этот запуск можно выполнять одновременно с основным сервером. Порт `2002` и A2S-порт `2003` не должны использоваться другим процессом.

```powershell
$serverRoot = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server"
$repoRoot = "C:\Users\<имя>\IdeaProjects\Arma-Reforger-AI-Conflict"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$profileRoot = "$env:LOCALAPPDATA\AICF\Stage2-Reliability-$stamp"

Set-Location $serverRoot
& "$serverRoot\ArmaReforgerServerDiag.exe" `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -server "worlds/MP/CTI_Campaign_Arland.ent" `
  -MissionHeader "Missions/23_Campaign_Arland.conf" `
  -worldSystemsConfig "Configs/Systems/ConflictSystems.conf" `
  -addonsDir "$repoRoot,$serverRoot\addons" `
  -addons "9178E5822AFE48EA,B52C5F6AEDBF423E" `
  -profile "$profileRoot" `
  -bindPort 2002 `
  -a2sPort 2003 `
  -aicfInitialTickets 100 `
  -aicfRequirePlayerForResult 0 `
  -aicfCommanderIntervalMs 5000 `
  -aicfReliabilityIntervalMs 1000 `
  -aicfOrderRecoveryRetryMs 1000 `
  -aicfStuckTimeoutMs 30000 `
  -aicfStuckProgressMeters 500 `
  -aicfMaxStuckRecoveries 2 `
  -aicfMaxConcurrentSpawns 1 `
  -aicfTestDropOrderFaction US `
  -aicfTestDropOrderSlot 0 `
  -aicfTestDropOrderAtMs 15000 `
  -backendFreshSession `
  -maxFPS 60 `
  -logStats 10000
```

Test-hook должен использоваться только в этом воспроизводимом прогоне. Он отключён, если не заданы одновременно faction и slot.

## Проверка быстрого прогона

После `ROSTER_READY` обязательна последовательность:

1. Восемь уникальных `SPAWN_BOUND`, по одному для каждой пары faction/slot.
2. `TEST_ORDER_DROPPED faction=US slot=0` не раньше заданного времени.
3. `ORDER_RECOVERED faction=US slot=0 cause=WAYPOINT_REFERENCE_MISSING` не позднее двух reliability-интервалов.
4. После recovery группа снова имеет обычный приказ и продолжает движение.
5. При срабатывании watchdog расстояние считается от живого лидера: `GROUP_STUCK_DETECTED`, затем `GROUP_STUCK_RECOVERY action=REBUILD_ORDER`.
6. После исчерпания `aicfMaxStuckRecoveries` допустим только `GROUP_STUCK_PERSISTENT action=RECYCLE_GROUP`, затем `GROUP_RECYCLED` и штатный `REINFORCEMENT_SCHEDULED reason=PERSISTENT_STUCK`. Успешная замена списывает обычный replacement-билет; бесплатного recycle нет.
7. Завершённый waypoint рядом с ещё активной целью даёт одно `ORDER_RECOVERY_SUPPRESSED state=AT_OBJECTIVE`, а не серию `ORDER_RECOVERED`. По истечении grace-периода разрешено одно контролируемое перестроение.
8. Каждая `RELIABILITY_HEARTBEAT` увеличивает `audits`; число управляемых агентов остаётся ограниченным.
9. Нет `DUPLICATE_GROUP_BINDING`, `SPAWN_CONCURRENCY_INVARIANT_FAILED`, `[AICF][STAGE2][ERROR]` или server-side `SCRIPT (E)`.

Автоматическая первичная проверка:

```powershell
$log = Get-ChildItem "$profileRoot\logs" -Filter console.log -File -Recurse |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

& powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File "$repoRoot\tools\Test-Stage2Log.ps1" `
  -LogPath $log.FullName
```

## 30-минутная матрица

Запустить новый профиль без test-hook и с нормальными значениями:

```text
aicfWarTempoPercent=100
aicfRequirePlayerForResult=0
aicfReliabilityIntervalMs=5000
aicfStuckTimeoutMs=120000
aicfStuckProgressMeters=25
aicfMaxStuckRecoveries=3
aicfObjectiveHoldTimeoutMs=300000
aicfMaxConcurrentSpawns=1
```

За 30 минут подтвердить:

- обе стороны действуют без игрока;
- происходят естественные захваты и retarget;
- нет двух групп, привязанных к одному slot-generation;
- завершившиеся или потерянные waypoint восстанавливаются;
- отдельный route-rebuild не создаёт новую группу и не списывает билет;
- устойчиво застрявшая после трёх rebuild группа проходит штатную replacement-очередь и оплачивается обычным билетом только после успешного spawn;
- waypoint, завершившийся внутри зоны цели, не создаёт recovery-churn во время боя/захвата;
- прогресс watchdog и игровой групповой маркер используют одного и того же живого лидера;
- каждый AICF waypoint применяет компактную stock-формацию `Column` с нулевым formation displacement; временный боевой выход в укрытие допустим;
- replacement-spawn остаётся безопасным и списывает билет только после готовности;
- очередь `LOAD_LIMIT_BLOCKED` переносит due-time слота и завершается `LOAD_LIMIT_RELEASED`, не создавая ложный `REINFORCEMENT_TIMING_VIOLATION`;
- `managed_agents` не превышает конфигурационный предел;
- heartbeat продолжает поступать, server FPS не деградирует ступенчато.

## Двухчасовой soak

Использовать отдельный чистый профиль, `aicfRequirePlayerForResult 0` и достаточный запас билетов. Test-hook отключить. Сохранить:

- полный каталог серверных логов;
- начальные и конечные `Get-Process` (`WorkingSet64`, `PrivateMemorySize64`, `CPU`);
- первую и последнюю `RELIABILITY_HEARTBEAT`;
- выборку `logStats` по server FPS;
- количество spawn, recovery, capture, retarget и reinforcement;
- итог `Test-Stage2Log.ps1`.

`PASS` требует отсутствия бесконечного роста групп/waypoint, дублированных spawn, необслуженных групп без приказа и Stage 2 ошибок. Сам факт, что процесс прожил два часа, недостаточен.

## Статус результата

- `PASS`: Validate/Compile, быстрый fault-injection run, 30-минутная матрица и двухчасовой soak выполнены на одном commit.
- `FAIL`: Stage 2 загрузился, но нарушен lifecycle/order/load инвариант.
- `BLOCKED`: среда, порт, версия, addon loading или компиляция не позволяют начать проверку.
