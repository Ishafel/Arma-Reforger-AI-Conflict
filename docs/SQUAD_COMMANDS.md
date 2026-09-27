# Пополнение по приказу и возврат ИИ

На карте откройте командование и выберите отряд. Внизу панели доступны:

- **«Пополнить отряд в ближайшей казарме»** — пехота идёт в ближайшую
  безопасную союзную действующую казарму, где есть припасы на следующего бойца.
  После набора выбранной численности отряд продолжает прежний приказ.
- **«Вернуть отряд ИИ-командиру»** — отменяет пополнение, снимает ручной
  приказ и передаёт выбор задачи ИИ этой фракции. Если ИИ отключён параметром
  сценария, сервер отказывает с пояснением.

Ответ сервера показывается внизу панели. Полный состав, motorized unit type,
активная поездка, недоступная группа или незавершённый recovery не позволяют
начать ручное пополнение. При отсутствии безопасной казармы либо недоступном
navmesh endpoint прежний приказ остаётся в силе. Запрос можно повторить после
загрузки navmesh. Под «ближайшей» понимается расстояние X/Z от живого участника
до службы; порядок graph nodes/services разрешает равенство детерминированно.

Ручной поиск охватывает союзные базы графа без автономного ограничения 500 м
и одного radio edge. Подход ограничен 15 минутами, весь визит — 20 минутами.
Покупка выполняется только после физического подхода на 35 м. Бой рядом,
потеря службы/базы, исчерпание припасов, новый приказ и смена identity отменяют
визит через существующие cleanup и refund. Цены, общий бюджет AI, roster
readiness и ограничения одновременного spawn сохраняются.

## Владельцы состояния

`AICF_SquadCommandRpc` передаёт только numeric slot и enum команды на
player-owned controller. Сервер заново разрешает игрока, faction, stable slot,
readiness, rate limit и разрешение ИИ. Результат — reliable owner RPC;
постоянное состояние остаётся в существующем campaign snapshot для JIP.

`AICF_MatchController.RequestPlayerSquadCommand` выполняет orchestration.
`AICF_InfantryRecruitmentService` выбирает казарму и владеет визитом;
`AICF_OrderPlanner` владеет временным waypoint, освобождением ручного приказа
и восстановлением intent. Только `AICF_AICommander` выбирает новую
автономную задачу. При активном транспорте обновлённое назначение передаётся
через `AICF_VehicleCoordinator.AdoptCurrentStrategicAssignment`.

Manual visit хранит `m_bPlayerRequested`, сохраняя прежний durable intent.
Новый assignment revision отменяет отложенный map-point request; дальнейшие
callbacks проверяют group identity, generation, assignment/intent и graph.
Возврат ИИ очищает intent и ручную authority, увеличивает assignment revision.
При отсутствии доступной задачи возвращается отдельный ответ ожидания ИИ.

Существующие recruitment events и поля сохранены. Ручной
`INFANTRY_RECRUITMENT_STARTED` добавляет `player_requested=1` и задаёт
`max_distance_m=-1`. `Test-InfantryRecruitmentLog.ps1` разрешает расстояние
свыше 500 м только для этой формы события. `PLAYER_SQUAD_COMMAND_RESULT`
содержит `player`, `slot`, `command`, `accepted`, `reason`, `authority`.

## Воспроизведение проверок

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-SquadCommandsContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryRecruitmentStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-InfantryRecruitmentLog.ps1 -LogPath '<полный остановленный console.log>'
```

Fixture `tools/fixtures/AICF_SquadCommandsProbe.c` копируется только в
изолированный Core `Forces`, после чего его graph проходит терминальный
Workbench Validate. `Start-AICFRuntime.ps1 -Role Server -Variant Stock
-RepositoryRoot <копия> -AdditionalArguments @('-aicfSquadProbe','1',
'-aicfRequirePlayerForResult','0')` проверяет production planner/recruitment.
Fixture готовит бесплатную службу, но покупка бойца идёт через обычную экономику.
Все `[SQUAD_PROBE] check=... passed=1` и `finished` обязательны;
полный лог проверяется отдельно на engine/script errors.

Ручная/client/JIP матрица: оба действия для US и USSR, POINT и BASE intents,
неполная/полная пехота, активный транспорт, выключенный командир, захват казармы,
новый приказ при pending spawn, повторный клик, смена faction и поздний JIP.
Без отдельного прогона соответствующие gates остаются `NOT RUN`.

## Проверка 2026-09-27

Исходный `main`: `49e6bcd4118ec7fb88d4b26405126ccee0d6c288`, до правки рабочее
дерево чистое. Game/Server/Tools — `1.8.0.13`. Изменения оставлены в рабочем
дереве. Evidence root:
`C:\Users\retar\IdeaProjects\Arma-Reforger-AI-Conflict\.codex-runtime\squad-commands`.

| Команда / gate | Результат |
|---|---|
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>.ps1`: `AICommanderModeStatic`, `InfantryRecruitmentStatic`, `Stage4Static`, `MapPointOrdersStatic`, `LocalizationStatic`, `GroupMapMarkersStatic`, `Stage3Static`, `Stage35Static`, `Stage35RecoveryPolicy` | До/после: все девять **PASS / 0**, baseline failures отсутствуют. Полные результаты `before-*.txt`, `after-*.txt` |
| `tools/Test-SquadCommandsContracts.ps1` через тот же PowerShell | **PASS / 0**: 13 source contracts с отрицательными мутациями, четыре положительных/отрицательных log inputs |
| `tools/Build-AICFLocalization.ps1` и `-Check` через localization audit | **PASS / 0**, 407 записей |
| Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent ... -wbModule=ScriptEditor -run -validate` | **PASS / 0**: Arland, Everon, ArlandRHS, EveronRHS; `Game successfully created`, `Script validation successful`, без SCRIPT E/F, ENGINE F и VM/null errors |
| Workbench изолированной fixture | **PASS / 0**, обе версии fixture; первый sandbox-запуск production не дошёл до validation из-за окружения Steam, заменён успешным запуском в пользовательском окружении |
| `Start-AICFRuntime.ps1 -Role Server -Variant Stock -RepositoryRoot <stage> -ProfileRoot <server-movement> -AdditionalArguments @('-port','2387','-aicfSquadProbe','1','-aicfRequirePlayerForResult','0')` | Native **exit 0**, fixture сама завершила сервер; функциональные проверки **11/11** |
| `tools/Test-InfantryRecruitmentLog.ps1 -LogPath <полный остановленный server-movement console.log>` | **PASS / 0**, два визита, один оплаченный recruit |
| `git diff --check` | **PASS / 0** |

Финальный server run: `11:29:24–11:30:27` MSK, полный лог
`server-movement/logs/logs_2026-09-27_11-29-24/console.log`. Начальная дистанция
до казармы `61.0865 м`; physical arrival подтверждён через 10 секунд. Медик
стоил `15 supplies`, tickets не списывались. Восстановлены исходные POINT
и intent revision; повторный визит отменён при pending donor без его оплаты.
ИИ получил тот же group/entity и новый assignment. Первичный прогон
`server-stock` проверил тот же цикл из зоны прибытия.

Полный runtime содержит 14 строк E по stock resources/world/entity и shutdown
resource leaks: Gizmo sphere GUID, `SlidingTrackMaterial`, `Parent`, дублирующий
`Hierarchy`, font leak. SCRIPT E/F, ENGINE F и VM/null errors отсутствуют.
Функциональный PASS не объявляется общим чистым runtime PASS; ошибки сохранены
в `runtime-all-errors.txt`, полный лог остаётся основным evidence.
Workbench также сохраняет shutdown resource-leak diagnostics.

Полные команды и manifest находятся в `server-*-launcher.txt`,
`server-*-manifest.json`, `wb-*-arguments.json` и `validate-remaining.ps1`.
Workbench logs: `wb-stock-validate`, `wb-Everon`, `wb-ArlandRHS`,
`wb-EveronRHS`, `wb-fixture`, `wb-fixture-movement`.

**NOT RUN:** реальный player-owned RPC с подключённого клиента, client/JIP,
визуальная/input проверка панели, RHS runtime, дальний переход свыше 500 м,
возврат во время transport trip, вся негативная multiplayer-матрица и soak.
Режим выключенного командира проверен source contract, не отдельным runtime.

Изменённые файлы: `README.md`, `.gitignore`, эта инструкция;
`AICF_MatchController.c`, `AICF_InfantryRecruitmentConfig.c`,
`AICF_InfantryRecruitmentOrder.c`, `AICF_InfantryRecruitmentService.c`,
`AICF_OrderPlanner.c`, `AICF_StrategicUI.c`, новый `AICF_SquadCommandRpc.c`;
`AICF_Localization.st` и обе runtime `.conf`;
`Test-InfantryRecruitmentLog.ps1`, новый `Test-SquadCommandsContracts.ps1`
и test-only `AICF_SquadCommandsProbe.c`.
