# Навык управляемых боевых групп — 2026-09-05

Результат: `AICF_ManagedAICombatPolicy` задаёт `EAISkill.VETERAN` живым
участникам initial/replacement roster обеих сторон после readiness gates.
Применение server-only, синхронное, без новых subscriptions. Stock подавление,
perception и weapon handling сохраняются. Изменение действует на новые
deployment после загрузки обновлённых scripts; запущенный сервер нужно перезапустить.

В закреплённом Script Diff `1.8.0.13` метод
`SCR_AIGetAimErrorOffset.GetRandomFactor()` использует sigma `1` для `REGULAR`
и `0.5` для `VETERAN`. Это уменьшение случайной ошибки прицеливания,
а не обещание удвоить долю попаданий. Фактическая стрельба здесь не измерялась.

Исходная ревизия: `2c450cd26856e0805e2f671a5bc71ee62e708c16`, рабочее дерево до
правки чистое. Game/Tools/Server в прогонах — `1.8.0.13`.
Все локальные evidence находятся в игнорируемом каталоге
`logs/ai-skill-veteran-20260905/` от корня репозитория.

## Static и compile

Команды запускались до и после правки через
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<script>`:

| Script | До / после | Verdict |
|---|---|---|
| `Test-Stage35Static.ps1` | exit 1 / 1 | Сохранены ровно два baseline failures |
| `Test-Stage35RecoveryPolicy.ps1` | exit 0 / 0 | PASS |
| `Test-RHSIntegrationStatic.ps1` | exit 0 / 0 | PASS |
| `git diff --check` | exit 0 после | PASS |

Сохранённые rule IDs: `STAGE35_MEANINGFUL_TASK_PROOF` и
`STAGE35_BOUNDED_PROTECTED_CLEARANCE`. Полный вывод: `baseline-*.log` и
`after-*.log`; правила не изменялись.

Workbench запускался командами stock/RHS из [DEVELOPMENT.md](DEVELOPMENT.md)
через `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent ...
-wbModule=ScriptEditor -run -validate`, с соответствующими `gproj`,
`addonsDir`, `addons` и свежими `logsDir` в пользовательском TEMP.
Для ожидания GUI executable из терминала stdout направлялся в pipeline.

Оба итоговых запуска: exit 0, `Game successfully created`,
`Script validation successful`, `SCRIPT (E/F)=0`, `ENGINE (F)=0`, VM/null `0`.
Полные logs: `final-workbench-stock/`, `validated-workbench-rhs/`.
Есть obsolete/up-cast warnings и shutdown resource leaks; RHS также сообщает
о GUID/name `Language/rhs_localization.st`. Эти resource сообщения не являются
ошибками компиляции. Первая sandbox-попытка в `workbench-stock/` остановилась
на Steam initialization; она не считается успешной validation.

## Server smoke

Каждый server запускался отдельной терминальной сессией:

```powershell
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -ProfileRoot "$env:TEMP\AICF-Skill-20260905-Server-Stock" `
  -AdditionalArguments @('-aicfRequirePlayerForResult','0')
& ./tools/Start-AICFRuntime.ps1 -Role Server -Variant RHS `
  -ProfileRoot "$env:TEMP\AICF-Skill-20260905-Server-RHS" `
  -AdditionalArguments @('-aicfRequirePlayerForResult','0')
```

Exact `AICF_RUNTIME_MANIFEST_JSON`, CLI и launcher exit сохранены в
`runtime-stock-launch.log` и `runtime-rhs-launch.log`. Полные остановленные
server logs: `server-stock/logs_2026-09-05_21-30-52/` и
`server-rhs/logs_2026-09-05_21-32-05/`.

В каждом прогоне проверены 20 уникальных `faction + slot`, для каждого
`skill=VETERAN configured_agents=10 expected_agents=10`, затем `ROSTER_READY`.
Это 200 бойцов в stock и 200 в RHS. `GROUP_COMBAT_POLICY_INCOMPLETE` и
AICF errors отсутствуют. Машиночитаемый итог: `runtime-audit.json`.
Focused gate применения навыка — PASS в обоих вариантах.

Stock работал 21:30:52–21:31:55 MSK, RHS — 21:32:05–21:33:23 MSK.
После достижения smoke-критерия каждый был остановлен через `Stop-Process`
только после проверки exact profile в CLI соответствующего PID
(stock `7720`, RHS `24772`). Native exit `-1`, shell exit `1` — следствие этой
принудительной остановки; graceful shutdown не проверен.

Stock: `SCRIPT (E/F)=0`, `ENGINE (F)=0`; resource/world/entity errors относятся
к sphere GUID, `SCR_AIDangerReaction_UnsafeArea`, `SlidingTrackMaterial`,
`Parent` и повторным Hierarchy components. RHS: шесть `SCRIPT (E)` о faction
`US`/`USSR`/`RHS_ION`, совпадающих с ранее установленным baseline в
[BASE_BUILDERS_VALIDATION.md](BASE_BUILDERS_VALIDATION.md). Общий RHS runtime
остаётся FAIL, хотя применение навыка доказано. Дополнительно присутствуют
RHS resource/world compatibility messages (`m_fAILimitThreshold`, task manager,
localization) и четыре `RPL (E)` для `SCR_ArsenalComponent.RPC_OnArsenalUpdated`.
Они сохранены в полном evidence и не объявляются исправленными этой правкой.

## Не выполнено

`NOT RUN`: измерение попаданий/расхода боеприпасов в бою, replacement runtime,
Everon runtime/отдельный Workbench, client/JIP, soak и ручная визуальная проверка.
Replacement использует тот же проверенный по исходникам deployment hook, но
его отдельный runtime-сценарий в этот короткий smoke не входил.
