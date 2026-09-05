# Проверка набора пехоты — 2026-09-05

## Outcome

Реализован набор живой автономной пехоты через действующие казармы.
Старт/полная замена infantry group — один боец, цель набора по умолчанию — 10.
Supplies за бойца: обычный/командир/помощник 10, медик 15,
гранатомётчик/AT 20, пулемётчик/автоматчик 20. Предел поиска — 500 м.
Текущая или соседняя база, физический подход, продолжение боевого приказа,
ограниченные ожидания, отмена stale intent и очистка очереди входят в реализацию.
Политика и ограничения: [INFANTRY_RECRUITMENT.md](INFANTRY_RECRUITMENT.md).

Исходный `main`: `478e042c13bac31f6a64423ddf7b201be24c11bc`, рабочее дерево
до изменений чистое. Изменения подготовлены для Git tag `0.1.12`.
Game, Server и Workbench: `1.8.0.13`. API сверялся с закреплённым
`.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13`.
GUI automation и screenshots/video не использовались. Первичные проверки
выполнены без client/JIP; последующий запуск client описан ниже.

## Изменённые файлы

Все пути ниже относительно корня репозитория.

| Файлы | Изменение |
|---|---|
| `AIConflictCore/Scripts/Game/AIConflict/Config/AICF_InfantryRecruitmentConfig.c` | Цены и границы поиска/ожидания |
| `AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage1Config.c` | Уточнение комментария о capacity |
| `AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c` | Deployment size, identities позиций roster, временный визит |
| `AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryRecruitmentOrder.c` | Identity snapshot и live checks |
| `AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryRecruitmentService.c` | Выбор базы, визит, готовность и передача бойца |
| `AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantryRecruitSpawner.c` | Недостающая позиция состава, donor и cleanup |
| `AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_GroupSpawner.c` | Сохранение stock source roster для согласованного fallback |
| `AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_ReinforcementSystem.c` | Полная замена использует deployment size |
| `AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_InfantryRecruitmentEconomy.c` | Quote, точный debit, refund и commit |
| `AIConflictCore/Scripts/Game/AIConflict/Economy/AICF_EconomySystem.c` | Цена replacement соответствует deployment size |
| `AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_OrderPlanner.c` | Временный waypoint, сохранение/восстановление intent |
| `AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c` | Composition, Update/Stop, readiness, agent budget, исключение визита из обычного task recovery |
| `tools/Test-Stage35Static.ps1`, `tools/Test-Stage4Static.ps1` | Точные контракты deployment size |
| `tools/Test-InfantryRecruitmentStatic.ps1`, `tools/Test-InfantryRecruitmentLog.ps1` | Новые аудиторы |
| `tools/fixtures/AICF_InfantryRecruitmentRuntimeProbe.c` | Воспроизводимая runtime fixture |
| `README.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md` | Актуальные defaults и границы |
| `docs/INFANTRY_RECRUITMENT.md`, этот файл | Политика и evidence |

Временная копия fixture удалена из Core. Generated logs, profiles и
`resourceDatabase.rdb` не добавлялись в Git. GUID и установленные игровые
ресурсы не менялись.

## Static gates

Каждая команда: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<script>`.
Pre/post outputs сохранены в
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\play-infantry-recruitment\`
как `baseline-*.txt` и `final-*.txt`.

| Script | До, exit | После, exit |
|---|---|---|
| `Test-Stage3Static.ps1` | FAIL, 1: 3 issues | Тот же FAIL, 1: 3 issues |
| `Test-Stage35Static.ps1` | FAIL, 1: 2 issues | Тот же FAIL, 1: 2 issues |
| `Test-Stage35RecoveryPolicy.ps1` | PASS, 0 | PASS, 0 |
| `Test-Stage4Static.ps1` | PASS, 0 | PASS, 0 |
| `Test-AICommanderModeStatic.ps1` | PASS, 0 | PASS, 0 |
| `Test-MapPointOrdersStatic.ps1` | PASS, 0 | PASS, 0 |
| `Test-RHSIntegrationStatic.ps1` | PASS, 0 | PASS, 0 |
| `Test-InfantryRecruitmentStatic.ps1` | Новая проверка | PASS, 0 |
| `git diff --check` | — | PASS, 0 |

Сохранены без изменения output:

- `STAGE3_PROGRESS_EVIDENCE`;
- `STAGE3_BOUNDED_PROTECTED_CLEARANCE`;
- `STAGE3_MARKER_STATE`;
- `STAGE35_MEANINGFUL_TASK_PROOF`;
- `STAGE35_BOUNDED_PROTECTED_CLEARANCE`.

Этот список фиксирует исторический output прогона набора пехоты. После
исправления контрактов аудиторов все пять failures сняты; текущие Stage 3/3.5
дают PASS. Обоснование и отрицательные проверки — в [TESTING.md](TESTING.md).

Обновления двух regex сделаны по новому product contract, не для обхода
ошибок: `GetDeploymentSize()` определяет точную численность создаваемой
группы и её цену, `GetDesiredSize()` — цель последующего набора. Проверки
request-before-ready, exact alive faction roster и accounting сохранены.

Новый static auditor проверен на отдельной копии без generation fence:
ожидаемый exit 1, `STALE_REQUEST_FENCE`. Новый log auditor проверен на полном
положительном stock log: exit 0; удалённые debit events и превышение размера
отряда дали ожидаемые exit 1. Исходники и логи игры для этих отрицательных
inputs не менялись. Результаты: `negative-static-result.txt`,
`log-positive-check.txt`, `log-negative-unpaid-check.txt`,
`log-negative-oversized-check.txt` в evidence directory.

## Workbench

После удаления fixture выполнен terminal-only Validate:

```powershell
& $toolsExe -noThrow -wbsilent -gproj "$repoRoot/$project/addon.gproj" `
  -addonsDir $addonDirs -addons $addons -logsDir $logsDir `
  -wbModule=ScriptEditor -run -validate | Out-File $commandOutput
```

`$toolsExe` —
`C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe`.
`$repoRoot` — `C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict`.
Во всех graphs `addonsDir` включает game `addons` и repo; RHS дополнительно
`C:\Users\retar\OneDrive\Документы\My Games\ArmaReforger\addons`.

| Graph | Root project | Addons | Verdict/exit |
|---|---|---|---|
| Stock | `AIConflictArland` | `9178E5822AFE48EA,B52C5F6AEDBF423E` | PASS / 0 |
| Everon | `AIConflictEveron` | предыдущие + `A4B2E62595F645A4` | PASS / 0 |
| RHS | `AIConflictArlandRHS` | Stock + `1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C` | PASS / 0 |

Каждый log содержит `Game successfully created` и `Script validation successful`;
`SCRIPT (E/F)=0`, `ENGINE (F)=0`, VM/null errors `0`. Пять AICF up-cast
warnings и Workbench shutdown resource messages сохранены, не скрыты.
Полные `console.log`, `script.log`, `error.log` находятся в evidence
subdirectories `wb-verified-Stock`, `wb-verified-Everon`, `wb-verified-RHS`;
terminal output — `wb-verified-*-command.txt`.

Первый sandbox Workbench создал Game, но не смог инициализировать Steam.
Итоговые проверки выше выполнены под основной учётной записью через terminal pipeline,
который ожидает GUI executable и сохраняет действительный exit code.

## Runtime evidence

Команда положительных прогонов, с временной fixture:

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock -AICommanderMode BOTH `
  -AdditionalArguments @('-aicfRecruitProbe','1','-aicfRequirePlayerForResult','0')
```

Для RHS использован `-Variant RHS`; для отрицательного stock прогона добавлены
`'-aicfRecruitProbeFaults','1'`. Profile создавал canonical launcher вне repo.
Полные commands, exact `CLI Params` и `AICF_RUNTIME_MANIFEST_JSON` сохранены
в evidence `server-*-launch.txt`. Процессы закончились по `RequestClose()`
fixture, exit 0. Во всех трёх прогонах двадцать initial slots дали
`GROUP_ROSTER_READY initial_agents=1 expected_agents=1`.

| Прогон | Проверенное поведение | Полный log audit |
|---|---|---|
| Итоговый Stock, 22:21–22:22 | 18 spawn/debit/join; US и USSR по 10; прежние group identities; возвращены боевые назначения | FAIL / 1: 2 stock shutdown script errors |
| Итоговый RHS, 22:22–22:23 | 18 spawn/debit/join; USMC и MSV по 10; прежние group identities; возвращены боевые назначения | FAIL / 1: 6 RHS startup + 2 stock shutdown script errors |
| Отрицательный Stock, 22:16 | US: новый player point intent; USSR: supplies удалены при pending donor. По 1 бойцу, joins=0, debits=0, pending=0 | FAIL / 1: 2 stock shutdown script errors |

Для положительных final logs выполнялась команда
`Test-InfantryRecruitmentLog.ps1 -LogPath <полный console.log> -RequireFullRosters`.
На отрицательном log — без `-RequireFullRosters`. Аудитор намеренно не
игнорирует stock/RHS script errors; общий runtime не объявлен PASS.
Отрицательный gameplay smoke предшествовал последней правке auxiliary
diagnostics; повтор на финальном source — `NOT RUN`.

Полные остановленные logs:

- Stock: `C:\Users\retar\AppData\Local\AICF\Server-20260905-222111-334\logs\logs_2026-09-05_22-21-11\console.log`.
- RHS: `C:\Users\retar\AppData\Local\AICF\Server-RHS-20260905-222238-198\logs\logs_2026-09-05_22-22-38\console.log`.
- Отрицательный: `C:\Users\retar\AppData\Local\AICF\Server-20260905-221602-864\logs\logs_2026-09-05_22-16-02\console.log`.

Рядом сохранены остальные engine logs. Summary и полные verdict аудитора —
`server-*-summary.json`, `server-*-audit*.txt` в evidence directory.
`ENGINE (F)`, VM/null exceptions и AICF `[ERROR]` равны 0 во всех трёх.

Два сообщения `SCR_BaseResupplySupportStationComponent ... entity catalog manager`
появляются при shutdown. Тот же текст зафиксирован до этой задачи в
`.codex-runtime/Scenario-Stock-Direct-20260829-234106/logs/logs_2026-08-29_23-41-06/console.log`.
Шесть сообщений RHS о `US`, `USSR`, `RHS_ION` на init совпадают с
`.codex-runtime/Scenario-RHS-Direct-20260829-234237/logs/logs_2026-08-29_23-42-38/console.log`;
в этом старом логе есть и те же два shutdown сообщения.

Помимо script errors, Stock сохранил 15 `RESOURCES/WORLD/ENTITY (E)` строк,
RHS — 69. Они относятся к installed resources: старым GUID, Hierarchy,
`Parent`, `SlidingTrackMaterial`, `SCR_AIDangerReaction_UnsafeArea`; RHS также
к localization, `SCR_BaseTaskManager` и `m_fAILimitThreshold`, плюс shutdown
resource leaks. Эти сообщения не исправлялись и не исключались из полных logs.

Более ранний stock прогон `Server-20260905-221045-878` прошёл оба
`Test-InfantryRecruitmentLog.ps1 -RequireFullRosters` и
`Test-Stage4Log.ps1 -AllowActiveAtEnd` с exit 0: 3 визита, 18 бойцов.
Это evidence промежуточной версии, не замена final runtime verdict.
Два первых прогона остановлены адресно через `Stop-Process` после выявления
stock fallback и campaign randomizer; их logs сохранены как неуспешные
`server-stock-1/2-launch.txt`. Эти ошибки исправлены последующими изменениями.

## Наблюдения обычного Stock server/client прогона

После focused checks пользователь запросил обычный сервер и клиент.
Сервер запущен в 22:36:35 без fixture, клиент подключён в 23:00:49:

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock -AICommanderMode BOTH
& ./tools/Start-AICFRuntime.ps1 -Role Client -Variant Stock `
  -ServerProfileRoot 'C:\Users\retar\AppData\Local\AICF\Server-20260905-223635-155'
```

Отдельные terminal sessions сохранили `AICF_RUNTIME_MANIFEST_JSON` в
`.codex-runtime/play-20260905-223622/server-launch.txt` и `client-launch.txt`.
Client launcher подтвердил exact server CLI, живой process/UDP 2001 и
`ROSTER_READY`; server log содержит `Player connected`, client log —
полученное `STATE_REPLICATED`. Profiles: `Server-20260905-223635-155` и
`Client-20260905-230025-168` под `C:\Users\retar\AppData\Local\AICF`.

Снимок полного server console log сохранён в 23:08:03 как
`.codex-runtime/play-20260905-223622/server-snapshot-2307.log`.
Это промежуточное наблюдение работающего сервера: exit code и shutdown
ещё отсутствовали, общий runtime/client gate не объявляется PASS.

| Показатель на момент снимка | US | USSR |
|---|---:|---:|
| Покупки живой пехоты | 45 | 91 |
| Supplies за эти покупки | 625 | 1360 |
| Stable slots, совершившие покупку | 5 | 10 |
| Завершённые полные replacements | 2 | 11 |

Все 136 debit/join совпали по faction, slot, generation, token, group, role
и cost; разность supplies соответствует цене. Полные replacements отдельно
стоили по 50 supplies и одному ticket. В server snapshot нет SCRIPT (E/F),
ENGINE (F), VM/null или AICF ERROR; сохранены 15 resource/world/entity errors
и одна NETWORK (E) при подключении клиента.

Известные ограничения обычного прогона, не исправленные в `0.1.12`:

- У USSR D2 (`numeric_slot=8`, `generation=1`) после покупки в 23:05:38
  было `alive=10`, но SLOT_ACTIVITY в 23:06:46 и 23:07:46 показал
  `alive=11 roster=11`. Причина превышения лимита не установлена;
  успешные focused fixtures этот сценарий не покрыли.
- Поиск пополнения начинается даже при составе 9/10. USSR A0 уходил
  за одним бойцом с дистанции около 395 м, A5 — около 351 м.
  Повтор после отмены возможен через 60 секунд. Это вызывает дополнительные
  возвраты к казарме; пользователь сообщил о множестве бегающих рядом бойцов.
- На момент снимка US совершал покупки только на MOB; пять атакующих slots
  оставались одиночными. В строительстве передовых казарм встречались
  препятствия и отсутствие безопасного места. Зафиксировано 35 эпизодов
  GROUP_STUCK_DETECTED, восемь отмен визита GRAPH_CHANGED и одна
  APPROACH_INTERRUPTED_OR_TIMEOUT.

## NOT RUN

- Полный остановленный client/JIP gate, UI и визуальные критерии;
  screenshots не создавались. Подключение и log-side replication наблюдались выше.
- Everon runtime; его отдельный Workbench gate выполнен.
- Soak, многопользовательский contention и длительная автономная война.
- Runtime на точной границе 500 м и на соседнем radio node; контракт проверен static.
- Потеря медика в бою, захват/уничтожение казармы, stuck/deadline,
  полный replacement после уничтожения, player takeover donor и partial-debit
  injection как отдельные runtime fixtures.
- Изменение точности стрельбы/боевой эффективности не измерялось.

Статус `ACCEPTED` не присваивается: поведение подтверждено в указанных
сценариях, ограничения и исходные failures сохранены выше.
