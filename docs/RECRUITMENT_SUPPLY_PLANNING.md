# Планирование снабжения при наборе пехоты — issue #20

## Запас, доход и конкурирующие визиты (issue #20)

`AICF_InfantryRecruitSpawner.QuoteMissingRoster` суммирует цены отсутствующих
живых позиций до желаемой численности. Он использует тот же
`ResolveRecruitPrefab`, включая content profile и stock fallback, что и покупка.
Неразрешимый prefab отклоняет план; успешный transfer сразу уменьшает спрос,
поскольку позиция уже записана через `RecordRecruitedMember`.

Спрос других отрядов — сумма оставшихся цен всех действующих визитов к этой
базе, включая ручные и ещё идущие к казарме. Текущий slot исключается из суммы
при пересмотре своего плана. Источник спроса — сами визиты; отдельного ledger
или денежной reservation нет. Identity/generation, intent/assignment/graph,
безопасность и владелец базы проверяются при каждом чтении. Уничтоженный,
завершённый или отменённый визит не создаёт спрос, в том числе при задержанном
donor cleanup. Новые визиты регистрируются последовательно, поэтому следующий
отряд видит предыдущие намерения.

Read-only adapter `AICF_GetRecruitmentIncome` читает последнюю рассчитанную
stock сумму `GetSuppliesIncome`, интервал `GetSuppliesArrivalTimer` и ближайшую
дату `GetSuppliesArrivalTime`. Он повторяет gates stock `SupplyIncomeTimer`:
authority, spawn point, двустороннюю HQ-связь для обычной базы, capture и cooldown.
Сумма ограничена вместимостью и stock replenish threshold. Отдельного вызова
`CalculateSupplyRegenerationAmount` нет: этот API меняет реплицируемое состояние.
Ещё не рассчитанный либо остановленный доход считается нулевым. Обещанные
доставки логистики не включаются.

Формула `AICF_RecruitmentSupplyForecast`:

- `own_demand` — сумма цен недостающих позиций, `other_demand` — спрос других визитов;
- `deficit = max(0, own_demand + other_demand - stock)`;
- `travel_s = max(0, distance_m - 35) / 3`, где 3 м/с — плановая скорость;
- при дефиците `packages = ceil(deficit / income)` и
  `supply_s = next_income_s + (packages - 1) * income_interval_s`;
- `wait_s = max(0, supply_s - travel_s)`;
- `completion_s = max(travel_s, supply_s) + missing_members * 3`.

Нулевой доход при дефиците отклоняет кандидата. Допустимы ожидание после
подхода до 120 с и завершение в оставшийся срок визита. Выбирается минимум
`completion_s`, затем стабильный порядок graph/services. Например, при спросе
100 и девяти вакансиях близкая база с запасом 10 и пакетом 100 через 60 с даёт
87 с; база в 185 м с запасом 150 даёт 77 с и выигрывает. Если второй отряд уже
заявил на неё спрос 100, запас 150 больше не считается достаточным.

Это оценка, а не гарантия: она не предсказывает бой, точную длину navmesh-пути,
изменение quick bonus, заполнение склада между пакетами, внешние расходы и
задержки spawn. Фактическая оплата остаётся побойцовой и повторно проверяется
Economy непосредственно перед transfer.

Каждые 30 с свободный от donor и hidden recovery визит пересчитывает прогноз.
Сменить базу можно не чаще раза в 60 с, при выигрыше не менее
`max(30 с, 25% текущего completion_s)` либо недопустимости текущего прогноза.
Новый route сначала проверяет `AICF_OrderPlanner`; при отказе прежний визит
сохраняется. Успешная смена сохраняет исходный абсолютный deadline и начатое
ожидание supplies. Предполагаемый путь должен укладываться и в оставшийся
approach deadline. Между службами одной базы экономическое перепланирование
не выполняется.

Если кандидатов нет, поиск повторяется через 60 с без бесполезного визита;
первоначальный muster продолжает ждать полного состава. Если supplies
закончились уже после физического прибытия, новый donor не создаётся;
ожидание ограничено 120 с и абсолютным сроком визита. При наличии donor
сохраняется прежний fail-closed отказ и очистка. Без подходящей альтернативы
после timeout действует обычный retry.

## Issue #20: спрос и время пополнения пехоты — 2026-10-06

Core оценивает полную стоимость недостающих позиций и время снабжения с
учётом остальных действующих визитов. Спрос пересчитывается из живых identities,
не списывает supplies и освобождается до donor cleanup. Есть периодический
пересмотр с hysteresis, ограниченное ожидание и диагностируемый retry.
Формула и границы описаны выше.

База исходников: `77b7ffbfd279a11248a44528b8a0de8c6e017fa2` + локальный diff.
Изначальные несвязанные изменения WCS README, `AICF_WCSItemResources.c`,
WCS `addon.gproj`, `Start-AICFRuntime.ps1`, `Test-WCSIntegrationStatic.ps1`
сохранены. Game, Tools и Server: `1.8.0.13`.

Изменённые файлы задачи:

- Core: `Config/AICF_InfantryRecruitmentConfig.c`,
  `Forces/AICF_InfantryRecruitSpawner.c`, `Forces/AICF_InfantryRecruitmentOrder.c`,
  `Forces/AICF_InfantryRecruitmentService.c`,
  новый `Economy/AICF_RecruitmentSupplyForecast.c`;
- `tools/Test-RecruitmentSupplyContracts.ps1`,
  `tools/Test-InfantryRecruitmentLog.ps1`,
  `tools/fixtures/AICF_RecruitmentSupplyProbe.c`, `.gitignore`;
- `README.md`, `docs/RECRUITMENT_SUPPLY_PLANNING.md`. Локальные игнорируемые справочники `docs/ARCHITECTURE.md`, `docs/INFANTRY_RECRUITMENT.md` и `docs/TESTING.md` также обновлены; в Git они не добавляются.

Evidence находится в `.codex-runtime/issue20/`. Полные baseline/after выводы
сохранены отдельно и совпадают для всех шести прежних аудиторов. Каждый
PowerShell-аудит запускался как
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<script>.ps1`.

| Команда / gate | До | После |
|---|---|---|
| `Test-InfantryRecruitmentStatic.ps1` | PASS, 0 | PASS, 0 |
| `Test-Stage35Static.ps1` | PASS, 0 | PASS, 0 |
| `Test-Stage4Static.ps1` | FAIL, 1: `STAGE4_ATTACKED_BASES` | тот же FAIL, 1 |
| `Test-BarracksCombatContracts.ps1` | PASS, 0 | PASS, 0 |
| `Test-DefendWaypointInputContracts.ps1` | PASS, 0 | PASS, 0 |
| `Test-TemporaryHoldContracts.ps1` | PASS, 0 | PASS, 0 |
| `Test-RecruitmentSupplyContracts.ps1` | новый | PASS, 0; 10 отрицательных мутаций |
| `git diff --check` | — | PASS, 0 |
| Terminal Workbench, production stock graph | — | PASS compile, exit 0 |
| Stock server, шесть конкурирующих roster | — | PASS focused scenario, exit 0 |

Production Workbench запущен точной командой из `DEVELOPMENT.md`:
`ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent -gproj <repo>/AIConflictArland/addon.gproj
-addonsDir <game>/addons,<repo> -addons 9178E5822AFE48EA,B52C5F6AEDBF423E
-logsDir <repo>/.codex-runtime/issue20/workbench-final -wbModule=ScriptEditor -run -validate`.
Invocation направлен в `Out-Null`, чтобы дождаться GUI-subsystem executable
и получить native exit code. В `workbench-final/console.log` есть
`Game successfully created`; нет `SCRIPT (E/F)`, `ENGINE (F)`, VM/null errors.
Есть obsolete API warnings и 24 resource leak entries при shutdown.
Начальные sandbox Workbench попытки компилировали Game, но получили
`SteamAPI_Init failed`; fixture compile exit `-1`. Финальная проверка выполнена
вне sandbox без этой ошибки.

Runtime выполнен из отдельного stage, содержащего копии Core/Arland и fixture;
fixture в production не добавлялась. Команда:

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot "$issueRoot/stage" -ProfileRoot "$issueRoot/server-2" `
  -ServerPort 2021 `
  -AdditionalArguments @('-aicfRecruitmentSupplyProbe','1','-aicfRequirePlayerForResult','0')
```

`$issueRoot` — абсолютный путь `<repo>/.codex-runtime/issue20`.
`server-2-launcher.txt` сохраняет `AICF_RUNTIME_MANIFEST_JSON`, native CLI
и exit 0. Полный остановленный лог:
`server-2/logs/logs_2026-10-06_13-24-36/console.log` (2061 строк).
Проверка `Test-InfantryRecruitmentLog.ps1 -LogPath <этот-log> -RequireSupplyPlanning`
дала `PASS visits=7 joined=54 full_factions=2`, exit 0. Fixture зафиксировала
`passed=15 total=15`, `finished=1 full=6 stable=1 demand_checks=1`.
Новые visits видели `other_demand=125/250` у US и `135/270` у USSR;
identity/intent/graph/release/cancel исключали спрос, успешный набор уменьшал его.
Девять численных проверок внутри Enforce покрыли бедную/богатую альтернативу,
общий запас, низкий/нулевой доход, дискретные пакеты, горизонт и повторяемость.

Анализатор проверен отдельно: позитивный synthetic log — exit 0;
удалённое диагностическое поле, неполный итог roster и stale token — каждый
exit 1 (`analyzer-*-result.txt`). Эти synthetic логи не являются gameplay evidence.

Полный runtime log не имеет `SCRIPT (E/F)`, `ENGINE (F)`, VM/null errors.
При загрузке штатных ресурсов присутствуют Wrong GUID/name для `Gizmo3D/sphere.xob`,
unknown `SlidingTrackMaterial`/`Parent`, четыре сообщения о дублированном Hierarchy;
при shutdown — один font resource leak. Эти сообщения сохранены, targeted
recruitment PASS не означает полностью чистый engine log. Первая sandbox
попытка `server-1` не дошла до мира из-за SSL/GameConfig; процесс остановлен,
его полный лог и exit `-1` сохранены. Процесс `server-2` завершила fixture сама.

`NOT RUN`: реальный переход между двумя физическими базами, полный runtime
capture/death/replacement matrix, WCS/RHS, client/JIP и ручная визуальная проверка.
Проверки identity в fixture меняют сохранённые stamps визита и не подменяют
эти отсутствующие игровые сценарии. Результат не объявляется `ACCEPTED`.
## Проверка отдельной PR-ветки от main

Для публикации создана `codex/issue-20-recruitment-supply` от
`743ddfb3de2d386067d0ba64f8576b9663dffe45`. В неё перенесён только diff issue #20:
несвязанные WCS, suppression и temporary hold изменения прежней ветки исключены.

Пять доступных релевантных аудиторов повторены до/после на новой базе:
InfantryRecruitmentStatic, Stage35Static, Stage4Static, BarracksCombatContracts,
DefendWaypointInputContracts. Выводы попарно совпали; единственный failure —
прежний `STAGE4_ATTACKED_BASES`. Supply contracts — PASS, 10 мутаций.

Production Workbench на PR-исходниках: exit 0, `Game successfully created`,
нет script/VM errors; полные логи — `.codex-runtime/issue20/pr-workbench/`.
Runtime повторён на копии PR-исходников `pr-stage`, через тот же canonical
launcher, свежий profile `pr-server` и порт 2022. Полный остановленный лог:
`.codex-runtime/issue20/pr-server/logs/logs_2026-10-06_13-37-57/console.log`
(2178 строк, 581017 байт). Server exit 0; анализатор с `-RequireSupplyPlanning`:
`PASS visits=7 joined=54 full_factions=2`, exit 0. Fixture: 15/15 проверок,
шесть полных roster, stable identities и уменьшение спроса подтверждены.
Manifest сохранён в `pr-server-launcher.txt`. Классы остаточных asset/shutdown
сообщений совпадают с первым прогоном; script/VM errors нет.

Критерии 1, 2, 3, 6, 7 подтверждены в указанном объёме. Пункты 4 и 5 оставлены
открытыми: нужны полный capture/death/replacement matrix и проверка отсутствия
метаний между двумя физическими базами при колебаниях supplies. Численные
forecast checks и проверки stamps не объявляются заменой этих сценариев.

## Дополнительная приёмочная матрица для критериев 4 и 5

Первая публикация не проверяла эти критерии полностью: изменение сохранённых
stamps не доказывает игровой lifecycle, а повторный расчёт формулы не доказывает
устойчивость действующего визита. Для закрытия пробела добавлены отдельные
terminal fixtures и анализатор `tools/Test-RecruitmentAcceptanceLog.ps1`.
Production gameplay-код в этом дополнении не меняется.

`AICF_RecruitmentLifecycleProbe.c` проверяет настоящие события: отмену визита,
смену владельца через native `SetFaction` с последующим production update,
`Kill()` единственного бойца, штатную replacement deployment в тот же slot,
новый визит replacement-группы и набор до десяти бойцов. Смерть проверяется
на первом наблюдаемом tick с пустым живым roster: `Kill()` не синхронен с
обновлением списка живых агентов. Fixture не меняет stamps заказа.

`AICF_RecruitmentStabilityProbe.c` выбирает две настоящие базы из stock graph,
создаёт зарегистрированные ONLINE казармы и вызывает production manual visit.
Ручной путь позволяет изолировать критерий выбора от ограничения 500 m;
forecast, demand, `ReconsiderBarracks`, cooldown и порог выигрыша общие с AI.
На каждом sample выбор повторяется десять раз с теми же входами. Запас второй
базы колеблется на ±1 supply; минимум два production пересмотра после 60 s
должны встретить более выгодную альтернативу и сохранить прежний token.
После 125 s запас альтернативы увеличивается существенно: ожидается ровно
одно переключение и перенос спроса со старой базы на новую.

Управляемые условия stability: income snapshot зафиксирован fixture на
0.1 supply/s с дискретными пакетами раз в секунду. Это даёт воспроизводимые
10 s изменения ETA на 1 supply. Supplies читаются с настоящих баз, решение
и смена waypoint выполняются production кодом; fixture не заменяет selector
или `ReconsiderBarracks`. Боец физически перемещается fixture, чтобы исключить
помехи от stuck recovery. Эти проверки не являются evidence навигации,
естественной периодичности native income, клиентского интерфейса или WCS.

В отдельном `stability-stage` находятся копии PR Core/Arland и обе fixtures
в `AIConflictCore/Scripts/Game/AIConflict/Diagnostics/`. Серверы запускаются
последовательно: параметр launcher `ServerPort` используется для client readiness
и сам по себе не меняет stock server listener 2001.

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot "$issueRoot/stability-stage" `
  -ProfileRoot "$issueRoot/lifecycle-server-final" `
  -AdditionalArguments @('-aicfRecruitmentLifecycleProbe','1','-aicfRequirePlayerForResult','0')

& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot "$issueRoot/stability-stage" `
  -ProfileRoot "$issueRoot/stability-server-final" `
  -AdditionalArguments @('-aicfRecruitmentStabilityProbe','1','-aicfRequirePlayerForResult','0')

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-RecruitmentAcceptanceLog.ps1 `
  -Mode Lifecycle -LogPath <полный-остановленный-lifecycle-console.log>
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-RecruitmentAcceptanceLog.ps1 `
  -Mode Stability -LogPath <полный-остановленный-stability-console.log>
```

Результаты 06.10.2026 на PR-коде:

| Gate / команда | Verdict |
|---|---|
| Шесть `Test-{InfantryRecruitmentStatic,Stage35Static,Stage4Static,BarracksCombatContracts,DefendWaypointInputContracts,RecruitmentSupplyContracts}.ps1` | Выводы до/после полностью совпадают; пять PASS, сохранён `STAGE4_ATTACKED_BASES` FAIL |
| Terminal Workbench Validate/Compile с обеими fixtures | PASS, exit 0; `acceptance-workbench-verified/` |
| `Start-AICFRuntime.ps1`, lifecycle | exit 0; 10/10 случаев |
| `Test-RecruitmentAcceptanceLog.ps1 -Mode Lifecycle` | PASS, exit 0 |
| `Start-AICFRuntime.ps1`, stability | exit 0; 124 samples × 10 повторов; 60 samples с лучшей альтернативой |
| `Test-RecruitmentAcceptanceLog.ps1 -Mode Stability` | PASS, exit 0; 5 случаев |
| `git diff --cached --check` | PASS |

Lifecycle log: `lifecycle-server-final/logs/logs_2026-10-06_13-59-26/console.log`
(1431 строк, 342382 байт). Stable slot `0` получил replacement generation `2`
и новую group entity; смерть сняла спрос на первом наблюдаемом tick, старый
визит завершился, новый набрал девять бойцов, завершение освободило спрос.

Stability log: `stability-server-final/logs/logs_2026-10-06_14-01-47/console.log`
(1751 строка, 456239 байт). На 60.200 и 90.250 s кандидат выигрывал 10 s,
но production пересмотры сохранили token `258`. После увеличения запаса
альтернативы до 135 supplies на 150.350 s выполнен единственный replan:
ETA 157 → 103.837 s, выигрыш 53.163 s при пороге 39.25 s. Спрос 125 перенесён
с base `34` на base `54`; старый спрос равен нулю. Это реальные runtime IDs
данного прогона, а не привязки production к карте.

Все пути evidence относятся к `.codex-runtime/issue20/`. Manifest и native CLI:
`lifecycle-launcher-final.txt`, `stability-launcher-final.txt`; результаты
анализаторов — `acceptance-{lifecycle,stability}-result.txt`.
Проверены полные остановленные логи: script/VM errors отсутствуют. Сохранились
прежние stock resource сообщения (`Gizmo3D/sphere.xob`, `SlidingTrackMaterial`,
`Parent`, duplicate Hierarchy) и один font leak при shutdown в каждом runtime.
Это не полностью чистый engine log.

Подготовительные прогоны сохранены и не считаются PASS: первые lifecycle
попытки прерваны из-за разрыва между тестовыми действиями; `lifecycle-server-3`
дал 9/10 из-за слишком ранней синхронной проверки после `Kill()`. Первый
stability server не дошёл до roster из-за занятого stock listener, второй
остановлен для усиления проверки совпадения колебаний с моментами replan.
Отрицательные проверки анализатора должны отвергать неполную матрицу,
проваленную identity, незавершённый лог, нестабильный sample и отсутствие
выгодного кандидата в моменты production reevaluation.

Критерии 4 и 5 подтверждены в описанном server-side объёме. WCS/RHS,
client/JIP, ручной visual gate, естественное движение между базами и
естественный income timing этим дополнением не проверялись (`NOT RUN`).
Итоговый статус `ACCEPTED` остаётся решением пользователя.
