# Строитель продолжает работу при боевой угрозе

## Причина и изменение

В `AICF_BaseBuilderService.Build()` был явный запрет работы при
`GetThreatMeasure() > 0.01`. Он останавливал инструмент и сбрасывал таймер
прогресса. Кроме того, штатные индивидуальные combat/danger behaviors имеют
приоритет выше движения к проекту. Это подтверждает возможность прерывания
строительства боем, но не устанавливает единственную причину задержки
строителя СССР в ранее сохранённой пользовательской сессии.

Проверка угрозы удалена. Отдельная группа строителя получает `HOLD_FIRE` до
асинхронного запроса roster. Штатный group target cluster processor в этом
режиме не выбирает атакующие или исследовательские задания.
`AICF_BaseBuilderWorkPolicy` снижает итоговую оценку индивидуальных behaviors
с причиной `DANGER_LOW` и выше: наблюдение угрозы, бой, укрытие, бегство и
самолечение не вытесняют подход, строительство и возврат к палатке.

Исключение read-only: служба должна работать на authority, worker должен
совпадать по character/group identity, текущей faction базы и принадлежности
агента группе. Retirement, Stop, смерть, потеря identity и управление игроком
снимают исключение. Общие настройки AI и очереди stock actions не меняются.
Армейские отряды и логистические водители этим lookup не распознаются.

Строитель остаётся уязвимым: смерть, бессознательное состояние, потеря базы,
нахождение в машине и недоступный проект по-прежнему препятствуют работе.
Сохраняются физический подход, нахождение вне footprint, инструмент в руке,
подтверждённая item-use анимация и stock `AddBuildingValue`. Телепортации,
бессмертия, обхода оплаты и проверок размещения в production нет.

API проверен по локальному Script Diff `1.8.0.13`: `SCR_AIActionBase`,
`SCR_AIBehaviorBase`, `SCR_AIGroupUtilityComponent`,
`SCR_AIGroupTargetClusterProcessor`, `SCR_AIThreatSystem`.

## Изменённые файлы

- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_BaseBuilderService.c`;
- `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_BaseBuilderSpawner.c`;
- новый `AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_BaseBuilderWorkPolicy.c`;
- `tools/Test-BaseBuildersStatic.ps1`;
- `tools/fixtures/AICF_BaseBuilderRuntimeProbe.c`;
- `README.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md` и этот отчёт.

Несвязанные `docs/AI_LOGISTICS_IMPLEMENTATION_PROMPT.md` и
`docs/VEHICLE_SPAWN_PARITY_PROMPT.md` сохранены без изменений.

## Проверки

Evidence: `.codex-runtime/builder-combat-20260919/`.

Команда аудитов:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>.ps1`.

| Name | До | После |
|---|---|---|
| BaseBuildersStatic | PASS / 0 | PASS / 0 |
| ConstructionStatic | PASS / 0 | PASS / 0 |
| ConstructionContracts | PASS / 0 | PASS / 0 |
| Stage35Static | PASS / 0 | PASS / 0 |
| Stage4Static | PASS / 0 | PASS / 0 |
| AICommanderModeStatic | FAIL / 1 | FAIL / 1, те же две `AI_COMMANDER_UI_STATE` |

Три отдельные отрицательные копии исходников подтвердили обнаружение
утраты work priority, retirement fence и `HOLD_FIRE`:
`static-negative-verdicts.txt`. Это проверка аудитора, не runtime.

Терминальный Workbench:
`ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent -gproj <root>
-addonsDir <dirs> -addons <graph> -logsDir <logs> -wbModule=ScriptEditor
-run -validate` — **PASS / 0** для Arland, Everon, ArlandRHS, EveronRHS
и изолированной stock fixture. Точные аргументы и полные логи: `wb-*`.
Первый запуск production graphs в sandbox скомпилировал Game, но завершился
с `-1` из-за `SteamAPI_Init failed`; повтор вне sandbox дал указанные PASS.

Fixture mode `4` перед каждым обновлением живого worker вызывает
`ThreatBulletImpact(20)` и обновление threat system без decay. В том же
tick она оценивает stock `SCR_AIMoveFromDangerBehavior` с уровнем приоритета
Game Master: у worker score `-998840`, при временно несовпадающей character
identity — штатные `3160`. Binding восстанавливается синхронно до production
update. Это тестовая инъекция угрозы; полноценная перестрелка не моделируется.
Fixture размещает stock sandbags без player placement/оплаты, поэтому её
завершения проверяют службу строителей, а не весь construction pipeline.

Оба dedicated server запущены только через canonical launcher, на отдельных
profiles и портах, без клиента:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant Stock `
  -RepositoryRoot '<repo>/.codex-runtime/builder-combat-20260919/stage' `
  -ProfileRoot '<evidence>/stock-threat' -ServerPort 22319 `
  -AdditionalArguments @('-aicfBuilderProbe','4','-aicfRequirePlayerForResult','0',
    '-addr','127.0.0.1:22319','-noThrow')

./tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronRHS `
  -RepositoryRoot '<repo>/.codex-runtime/builder-combat-20260919/stage' `
  -ProfileRoot '<evidence>/rhs-threat' -ServerPort 22320 `
  -AdditionalArguments @('-aicfBuilderProbe','4','-aicfRequirePlayerForResult','0',
    '-addr','127.0.0.1:22320','-noThrow')
```

Первый stock smoke (`profiles/`, `runtime-stock.txt`) не инъецировал угрозу:
fixture ошибочно искала utility на character вместо `AIAgent`. Остановлен
только этот тестовый процесс (`stopped-initial-probe.json`); его завершения
не считаются доказательством combat policy. Исправленная fixture повторно
скомпилирована, дальнейшие runs используют новые profiles.

Окончательные server runs штатно закрылись через `RequestClose()` fixture,
оба с **native exit 0**. Game/Server/Tools — `1.8.0.13`.

| Gate | Stock Arland | Полный Everon RHS |
|---|---|---|
| Начало / остановка, Москва | 23:42:41 / 23:47:58 | 23:42:54 / 23:48:24 |
| Завершения builder | 4, обе стороны | 5, обе стороны |
| Combat samples / samples с активным инструментом | 22 / 5 | 28 / 8 |
| Неверные priority samples | 0 | 0 |
| `Failed move` / VM / null | 0 / 0 / 0 | 0 / 0 / 0 |
| Целевые assertions угрозы и completion | PASS | PASS |
| Полный `Test-BaseBuildersLog -RequireToolUse` | FAIL / 1 | FAIL / 1 |

Команда полного анализа:
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-BaseBuildersLog.ps1
-LogPath <полный остановленный console.log> -RequireToolUse`.

Stock сохраняет две `SCR_BaseResupplySupportStationComponent` SCRIPT ошибки
при shutdown, известные из предыдущего baseline. RHS содержит четыре
известные `SCR_Faction` init ошибки, те же две resupply ошибки и две
`SCR_InstigatorContextData: INSTIGATOR_OTHER is not supported` в physics
simulation; последние не доказаны как сохранённый baseline этой правкой.
Native resource/world/pathfinding/Hierarchy diagnostics также сохранены,
категории собраны в `*-all-error-categories.txt`. Общий runtime — **FAIL**.

RHS анализатор дополнительно сообщает `BUILDERS_INVALID_LIVE_COUNT:35`:
fixture записала `agents=0` в tick `BUILDER_SPAWN_REQUESTED` второй generation
(145424 мс), до асинхронного `BUILDER_READY agents=1` (146436 мс).
Это не второй живой worker и не обход readiness; правило анализатора в этой
задаче не ослаблялось, исходный FAIL сохранён.

Полные остановленные логи:

- `.codex-runtime/builder-combat-20260919/stock-threat/logs/logs_2026-09-19_23-42-41/console.log`;
- `.codex-runtime/builder-combat-20260919/rhs-threat/logs/logs_2026-09-19_23-42-54/console.log`.

Остальные файлы тех же log directories, native manifests, source SHA/status,
версии и проверка совпадения production/stage hashes сохранены рядом.
`analyze.ps1` читает полные логи, `runtime-summary.json` отделяет целевые
assertions от полного verdict. `git diff --check` — **PASS / 0**.

**NOT RUN:** client/JIP и визуальная проверка по прямому ограничению
пользователя; настоящий обстрел/ранение, полная матрица построек и всех баз,
длительный soak, Workshop packaging/publication и изменение текущего
пользовательского матча. Правка применяется при следующей загрузке новых
исходников; уже опубликованный пакет сам не обновляется.
