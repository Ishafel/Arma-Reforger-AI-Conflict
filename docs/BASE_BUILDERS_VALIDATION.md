# Строители баз — проверка 2026-09-04

Реализована отдельная серверная служба: один одиночный faction bot на базу
обслуживает очередь размещённых stock layouts, возвращается к master provider
и исчезает после 30 секунд непрерывного простоя. Новая работа использует того
же бота. Смерть допускает замену через 60 секунд; смена владельца отменяет
работу прежней стороны. Character prefab берётся из active stock/RHS profile.

Исходная ревизия: `4dffba1762f9fca7991dbf852a42455a1b100bb1`, дерево до правки
чистое. Game, Server и Tools: `1.8.0.13`. Изменения не коммитились.

Изменённые файлы:

| Файл | Изменение |
|---|---|
| `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_BaseBuilder.c` | Стабильный service slot и runtime identity |
| `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_BaseBuilderSpawner.c` | Один faction-catalog rifleman, раздельные bind и asynchronous spawn |
| `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_BaseBuilderService.c` | Очередь, достройка, возврат, idle, cleanup и diagnostics |
| `AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_OrderPlanner.c` | Owned builder waypoint, navmesh endpoint и detach-before-delete |
| `AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c` | Только composition, Update после army readiness и Stop |
| `tools/Test-BaseBuildersStatic.ps1` | Структурные safety/lifecycle contracts |
| `tools/Test-BaseBuildersLog.ps1` | Проверка полного остановленного runtime log |
| `tools/fixtures/AICF_BaseBuilderRuntimeProbe.c` | Отдельная диагностическая fixture, не входящая в обычный addon |
| `README.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`, этот файл | Поведение, ownership и воспроизводимые проверки |
| `.gitignore` | Исключение локального builder evidence |

Static-команды запускались через
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<script>`:

| Script | До | После |
|---|---|---|
| `Test-Stage3Static.ps1` | exit 1, 3 failures | exit 1, те же 3 failures |
| `Test-Stage35Static.ps1` | exit 1, 2 failures | exit 1, те же 2 failures |
| `Test-Stage35RecoveryPolicy.ps1` | exit 0, PASS | exit 0, PASS |
| `Test-Stage4Static.ps1` | exit 0, PASS | exit 0, PASS |
| `Test-AICommanderModeStatic.ps1` | exit 0, PASS | exit 0, PASS |
| `Test-MapPointOrdersStatic.ps1` | exit 0, PASS | exit 0, PASS |
| `Test-RHSIntegrationStatic.ps1` | exit 0, PASS | exit 0, PASS |
| `Test-BaseBuildersStatic.ps1` | Новый auditor | exit 0, PASS; fixture отсутствует в Core |

Сохранены baseline rule IDs: `STAGE3_PROGRESS_EVIDENCE`,
`STAGE3_BOUNDED_PROTECTED_CLEARANCE`, `STAGE3_MARKER_STATE`,
`STAGE35_MEANINGFUL_TASK_PROOF`, `STAGE35_BOUNDED_PROTECTED_CLEARANCE`.
Вывод существующих аудиторов до/после совпал полностью.

Negative input нового static auditor — оставленная временная fixture в Core —
дал ожидаемый exit 1 (`BUILDERS_RUNTIME_FIXTURE_IN_PRODUCTION`). Log auditor
проверен на позитивной последовательности (exit 0) и негативных данных:
два member, преждевременный idle despawn и второй active builder одной базы
(exit 1 с соответствующими rule IDs).

Workbench запускался только терминальными командами из `DEVELOPMENT.md`:

| Root project | Native exit | Verdict |
|---|---:|---|
| `AIConflictArland/addon.gproj` | 0 | PASS |
| `AIConflictEveron/addon.gproj` | 0 | PASS |
| `AIConflictArlandRHS/addon.gproj` | 0 | PASS |

Все три logs содержат `Game successfully created` и
`Script validation successful`, `SCRIPT (E/F)=0`, `ENGINE (F)=0`, VM/null `0`.
Пять повторов прежнего AICF up-cast warning сохранены. В RHS Workbench также
есть resource GUID/name warning для `Language/rhs_localization.st`; shutdown
resource leaks записаны полностью и отдельно от compile verdict.

Команда пакетной валидации: `& ./.codex-runtime/builders-20260904/validate-final.ps1`.
Финальные exact arguments, exit codes и verdict каждого root project
сохранены в `.codex-runtime/builders-20260904/workbench-final-*.json`.
Первый sandbox run обнаружил несовместимое сравнение `EntityID`; оно исправлено.
После этого Game компилировался, но sandbox не видел SteamAPI и Validate
завершался exit -1. Повтор в пользовательском Steam-окружении дал exit 0 и
`Script validation successful`. Final validation выполнена после удаления
временной fixture из Core. `git diff --check` завершился exit 0.

Проверки runtime выполнялись canonical launcher:

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -AdditionalArguments @('-aicfBuilderProbe','2','-aicfRequirePlayerForResult','0')
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant RHS `
  -AdditionalArguments @('-aicfBuilderProbe','2','-aicfRequirePlayerForResult','0')
```

Fixture создаёт реальные layouts через stock registry; player placement UI и
его supply transaction она не имитирует. После прогона копия fixture из Core
удаляется. Engine завершает прогон через `GetGame().RequestClose()`.
`AICF_RUNTIME_MANIFEST_JSON`, exact CLI и native exit code сохранены в каждом
`*-launch.txt`; оценка ошибок выполнялась по полным остановленным logs.

Stock lifecycle run — **PASS**, native exit 0. Полный log: 1806 строк,
`SCRIPT (E/F)=0`, `ENGINE (F)=0`, VM/null errors `0`. Четыре проекта достроены;
одновременно было не более одного строителя каждой базы. Новая работа во
время idle сохранила group/slot/generation. После контролируемой смерти
replacement создан через 60335 ms с той же numeric slot и generation 2.
Смена владельца вызвала retirement старой стороны на следующем Update.
Другой строитель штатно исчез после возвращения и 30 секунд простоя.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-BaseBuildersLog.ps1 `
  -LogPath 'C:/Users/retar/AppData/Local/AICF/Server-20260904-223723-432/logs/logs_2026-09-04_22-37-23/console.log' `
  -RequireLifecycle
```

Verdict: exit 0, `PASS completed=4 idle_retirements=1 lifecycle=True`.
Двенадцать stock startup `RESOURCES/WORLD/ENTITY (E)` и shutdown resource-leak
messages сохранены в полном логе и отдельно перечислены в evidence.

Предыдущий basic stock run также завершился exit 0: 1448 строк, два builders,
три completed layouts и два idle retirements, без script/fatal/VM ошибок.
Первый экспериментальный probe не создал layouts из-за неверного предположения
fixture о faction composition catalog; он остановлен вручную с native exit -1
и не считается runtime PASS. Production использует stock building registry.

RHS probe завершился native exit 0: два одиночных workers, три completed layouts,
один idle retirement. Другой строитель погиб в обычном бою после завершения
очереди и был корректно очищен; при пустой очереди replacement не появился.
Полный log: 2309 строк, `ENGINE (F)=0`, VM/null `0`, но **6 `SCRIPT (E)`**.
`Test-BaseBuildersLog.ps1 -LogPath <RHS console.log>` дал exit 1, единственный
rule — `BUILDERS_RUNTIME_SCRIPT_ERROR`. Поэтому общий RHS runtime — **FAIL**,
хотя базовая последовательность строительства подтверждена.

Все шесть сообщений — `SCR_Faction ... not a valid SCR_Faction` для `US`,
`USSR`, `RHS_ION`. Для проверки использована отдельная `git archive HEAD`
копия исходной ревизии без службы строителей; единственная fixture в ней
запрашивает выход через 60 секунд и снимает callback в `OnGameEnd()`.
Canonical RHS server на этой копии также завершился exit 0 и воспроизвёл
**те же шесть сообщений**, без VM/null. Сравнение полных остановленных logs
сохранено в `rhs-baseline-comparison.json`; этот baseline не повышен до PASS.

```powershell
& ./.codex-runtime/builders-20260904/baseline-repo/tools/Start-AICFRuntime.ps1 `
  -Role Server -Variant RHS `
  -RepositoryRoot (Join-Path (Resolve-Path '.').Path '.codex-runtime/builders-20260904/baseline-repo') `
  -AdditionalArguments @('-aicfRequirePlayerForResult','0')
```

Все созданные тестовые servers завершены. Source SHA256 production-файлов
зафиксированы отдельно; временной runtime fixture в Core нет.

**NOT RUN:** Everon server runtime, player placement/UI, визуальная анимация,
client/JIP, отдельные cancellation/moved-layout/unreachable fixtures и soak.
Контролируемые reuse/death/owner-change этапы RHS probe не завершены из-за
естественной смерти бота до их начала; эти этапы доказаны отдельным stock run.
Server logs подтверждают наличие строительного инструмента у stock/RHS bots,
но не заменяют ручную проверку его визуальной анимации.

Полные evidence paths:

- Final stock lifecycle: `C:/Users/retar/AppData/Local/AICF/Server-20260904-223723-432/logs/logs_2026-09-04_22-37-23/`.
- Stock basic: `C:/Users/retar/AppData/Local/AICF/Server-20260904-223122-735/logs/logs_2026-09-04_22-31-22/`.
- RHS probe: `C:/Users/retar/AppData/Local/AICF/Server-RHS-20260904-224350-025/`.
- Исходный RHS baseline: `C:/Users/retar/AppData/Local/AICF/Server-RHS-20260904-225008-886/logs/logs_2026-09-04_22-50-08/`.
- Workbench, manifests, baseline/post static output, source SHA256 и summaries:
  `C:/Users/retar/IdeaProjects/Arma-Reforger-AI-Conflict/.codex-runtime/builders-20260904/`.

## Исправление после клиентской проверки: 2026-09-04, 23:00–23:20

Пользователь сообщил, что проекты достраиваются, но самого строителя и
анимацию не видно. Первоначальный server-only probe не проверял обязательный
equip инструмента: `TryUseItemOverrideParams` вызывался без ожидания gadget
в руке, а `AddBuildingValue` выполнялся даже при отказе анимации. Кроме того,
рабочая точка находилась за полным stock danger radius вместо края layout.
Это дефект первоначальной реализации; прежние server-only результаты его не
исключали.

Изменены `Construction/AICF_BaseBuilder.c`, `AICF_BaseBuilderService.c`,
добавлен `AICF_BaseBuilderDanger.c`. Теперь бот подходит к ближайшей стороне
layout, достаёт инструмент через штатный AI equip path, ждёт hand attachment
и `OnItemUseBegan`. Прогресс требует непрерывного `IsUsingItem` в течение
3 секунд. Без инструмента/анимации строительства нет. Очистка снимает оба
invoker и проверяет прежнюю character/group identity перед изменением gadget.
Исключение из собственного unsafe-area event сохраняет exact layout,
character и generation и действует только вне footprint. Прочие AI/danger
events используют stock reaction.

Обновлены оба `Test-BaseBuilders*.ps1`, fixture, `README.md`,
`ARCHITECTURE.md`, `TESTING.md` и этот отчёт. Новые log gates:
`-RequireToolUse`, `-ClientLogPath`; fixture mode `3` даёт на клиенте
независимые samples replicated character position, hand attachment и item use.

Команды проверки:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-BaseBuildersStatic.ps1
& ./.codex-runtime/builders-20260904/visibility-fix/final/validate-final.ps1
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant RHS `
  -AdditionalArguments @('-aicfBuilderProbe','3','-aicfRequirePlayerForResult','0')
& ./tools/Start-AICFRuntime.ps1 -Role Client -Variant RHS `
  -ServerProfileRoot 'C:\Users\retar\AppData\Local\AICF\Server-RHS-20260904-231023-972'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-BaseBuildersLog.ps1 `
  -LogPath 'C:\Users\retar\AppData\Local\AICF\Server-RHS-20260904-231023-972\logs\logs_2026-09-04_23-10-24\console.log' `
  -ClientLogPath 'C:\Users\retar\AppData\Local\AICF\Client-RHS-20260904-231051-646\logs\logs_2026-09-04_23-11-04\console.log' `
  -RequireToolUse
```

Focused static: exit 0, PASS. Stage 3/3.5/RecoveryPolicy/MapPointOrders/RHS
audits до/после совпали полностью; сохранены те же 3 + 2 baseline failures,
остальные PASS. Старый остановленный stock log с `-RequireToolUse` получил
ожидаемый exit 1 / `BUILDERS_PROGRESS_WITHOUT_ANIMATION`: новый gate выявляет
отсутствовавшее прежде evidence, а не повышает старый прогон до PASS.

Финальный Workbench Validate без fixture: Arland, Everon и RHS — native exit 0,
`Game successfully created`, `Script validation successful`, script/fatal/VM
errors 0, **PASS**. После runtime добавлены два защитных guard: null owner в
animation callback и полная `IsWorkerValid` перед уборкой gadget. Эти guards
прошли финальную компиляцию/static; игровой прогон выше предшествует им.
`git diff --check`: exit 0. Изменения не коммитились.

RHS server и client завершились native exit 0. Полные остановленные logs:
1958 и 1626 строк. Восемь completed projects (три из fixture и пять дополнительно
размещённых в сессии), 31 progress event с `tool_active=1 item_using=1`,
два одиночных workers разных баз, один подтверждённый idle retirement.
Клиент получил тот же RplId `-2147413754`, его перемещение более 2 метров и
92 samples `using=1 tool_attached=1 proxy=1`. Очередь использовала прежнюю
group/generation. Все behavioral gates анализатора, включая client movement
и tool animation, выполнены.

Общий log audit: **exit 1 / FAIL** — `BUILDERS_RUNTIME_SCRIPT_ERROR` и
`BUILDERS_CLIENT_SCRIPT_ERROR`. Engine fatal и VM/null: 0. На сервере остались
6 известных faction init errors; при shutdown добавились два
`SCR_BaseResupplySupportStationComponent needs a entity catalog manager`.
На клиенте при disconnect/shutdown: два `SCR_MenuLayoutEditorComponent`,
два `SCR_FilterCategory constructor is not public` и один отсутствующий preset
`REPLICATION_SHUTDOWN`. Для этих shutdown errors baseline сравнение не
выполнялось; они не объявлены доказанно прежними. Полный список resource,
world и shutdown ошибок сохранён в `Server-all-errors.txt`/`Client-all-errors.txt`.
Попытка штатного `taskkill /PID 24408` вернула отказ без `/F`; затем client
завершился сам с exit 0, что подтверждено canonical launcher и `Game destroyed`.

После runtime удалена временная fixture из Core. **NOT RUN** после исправления:
отдельные stock/Everon runtime, повторные death/owner-change/transfer fixtures,
поздний JIP после начала работы и визуальная проверка пользователем. Client
samples подтверждают сетевое состояние, но не заменяют визуальный verdict.

Evidence: `.codex-runtime/builders-20260904/visibility-fix/`, включая exact
server/client manifests в `rhs-probe-*-launch.txt`, полный список ошибок,
runtime summary, before/after audits, negative gate и final Workbench logs.

Обычная playable сессия после проверки запущена без fixture и probe аргументов:
`Start-AICFRuntime.ps1 -Role Server -Variant RHS -AICommanderMode BOTH`, затем
отдельный `-Role Client -Variant RHS -ServerProfileRoot <fresh profile>`.
Server profile: `C:/Users/retar/AppData/Local/AICF/Server-RHS-20260904-232012-276`;
client profile: `C:/Users/retar/AppData/Local/AICF/Client-RHS-20260904-232055-386`.
Exact manifests и readiness gate сохранены в `server-play-final-launch.txt` и
`client-play-final-launch.txt`. Эта сессия не используется как full runtime PASS.
После этого пользователь закрыл клиент; по его последующему запросу сервер
PID 26840 остановлен через `Stop-Process`, отсутствие процесса проверено.
