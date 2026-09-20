# Экипировка RHS и сценарий осмотра — 2026-09-20

Последующая полная проверка обнаружила и исправила потерю gadget items при
смене одежды. Теперь у ЧВК сохраняются часы, фонари, лопатки, бинокли и RF-10
радиста. Патроны, медицина и дополнительное снаряжение проверены у всех 23:
[RHS_INVENTORY_AUDIT.md](RHS_INVENTORY_AUDIT.md).

## Результат

Российский roster сохраняет стиль MSV: шесть демисезонных и четыре летних
комплекта VKPO/EMR. Используются только существующие faction `CHARACTER`
entries; роли и десять мест в составе сохранены. Оружие берётся целиком из
штатных RHS loadouts, вместе с совместимыми магазинами и обвесом.

| № в ряду | Роль | Форма | Основное оружие в проверенном runtime |
|---|---|---|---|
| 1 | Командир | Demiseason | АК-74М, ГП-25, 1П87/1П90 |
| 2 | Медик | Summer | АК-74М |
| 3 | Пулемётчик | Demiseason | ПКП, ПМ |
| 4 | Гранатомётчик | Summer | АК-74М, РПГ-7 с ПГО-7 |
| 5 | Подствольный гранатомёт | Demiseason | АК-74М, ГП-25 |
| 6 | Стрелок с ручным пулемётом | Summer | РПК-74М |
| 7 | Старший стрелок | Demiseason | АК-74М, 1П78 |
| 8 | Помощник пулемётчика | Demiseason | АК-74М |
| 9 | Помощник гранатомётчика | Summer | АК-74М |
| 10 | Стрелок | Demiseason | АК-74М, 1П63 |

FIA в RHS получает три комплекта в стиле ЧВК: оливковая, песочная или тёмная
рубашка Crye, брюки G3 с наколенниками в Multicam/Multicam Black, тёмные
AVS/пояс/рюкзак, шлем OPSCORE AMP с гарнитурой, чёрная балаклава и очки ESS,
перчатки Mechanix и ботинки Salomon. Именные и фракционные patch items
не добавляются. Отсутствие заметных знаков, вшитых в текстуры RHS, требует
ручного осмотра. Основное оружие заменено комплектными RHS вариантами:

| Роли | Вооружение | Запас вне оружия |
|---|---|---|
| Стрелок, LAT, AAT, RTO, сапёр | M4A1 FSP, VCOG/RMR, AN/PEQ-15, глушитель | 6 × 30 |
| Командир, AT | AR15 UD145, BRAVO4, AN/PEQ-15, глушитель | 6 × 30 |
| Медик, помощник пулемётчика | Mk18, RMR, AN/PEQ-15, глушитель | 6 × 30 |
| Пулемётчик | ПКМ B51 с EOTech | 3 × 100 |
| Меткий стрелок | СВД, 1П21, ТГП-В | 5 × 10 |
| Разведчик | M4A1, ACOG/ReapIR, глушитель | 6 × 30 |
| Подносчик боеприпасов | M4A1 FSP, VCOG/RMR, AN/PEQ-15, глушитель | 26 × 30 |

Каждое оружие дополнительно имеет заряженный магазин/коробку. Помощник
пулемётчика несёт три полные коробки ПК на 100 патронов вместо UK59.
ПТ-оружие, его боезапас, медицина и специализация сохранены; фракция остаётся
FIA. В обычном stock Conflict этот adapter не работает.

Server-only adapter сначала собирает комплект в изолированном inventory
draft, сохраняет содержимое прежней одежды, проверяет вместимость. Только
затем переносит snapshot на живого AI и сверяет inventory signature.
Неудачный draft не меняет бойца; неудачный commit вызывает rollback.
Одноразовый callback имеет EntityID/prefab guards и снимается в destructor.
Повторяющихся campaign callbacks adapter не создаёт.

## Осмотр

Fixture находится вне gameplay addons и подключается только к отдельной копии
исходников. Она использует существующий северный RHS Everon, находит свободный
ровный участок возле US HQ и строит один ряд с шагом 2,5 м. Первые десять моделей
— российский roster, следующие тринадцать — все роли FIA в новых комплектах.
AI и damage моделей выключены, фракции дружественны, дальнейшее планирование
AICF в этой fixture приостановлено. После deployment игрок автоматически
переносится к началу ряда; можно обойти бойцов спереди и сзади. Перенос
выполняется owner RPC после завершения deployment, с проверкой replicated
identity персонажа. Сервер подтверждает расстояние до цели в двух следующих
тиках, клиент независимо проверяет фактическую позицию через две секунды.

После терминального Workbench Validate/Compile из `DEVELOPMENT.md` запустить
в двух отдельных PowerShell-сессиях из корня репозитория:

```powershell
# Используйте новый путь для каждого запуска.
./tools/Start-AICFRHSWardrobeShowcase.ps1 -Role Server `
  -SessionRoot "$PWD/.codex-runtime/rhs-wardrobe-inspection"
```

```powershell
./tools/Start-AICFRHSWardrobeShowcase.ps1 -Role Client `
  -SessionRoot "$PWD/.codex-runtime/rhs-wardrobe-inspection"
```

Helper копирует scripts, missions, language и локальные generated resource
indices, добавляет fixture и вызывает только `Start-AICFRuntime.ps1`.
Без resource indices fresh source-client не смог загрузить server world
(`DATA/WORLD_LOAD_ERROR`); проверка наличия индексов выполняется до запуска.
Установленные game/RHS assets не копируются и не меняются. Canonical launcher
проверяет CLI, живой процесс и `ROSTER_READY`; оба JSON manifests сохраняются
в `server-launch.txt` и `client-launch.txt`.

## Проверки

Окружение: Reforger `1.8.0.13`, установленные RHS packages `0.16.5208`.
Стабильный profile key `RHS_USMC_MSV_0_16_5150` сохранён. Выбранные prefabs
повторно подтверждены runtime catalog probe установленной версии.
Все evidence находятся в `.codex-runtime/rhs-wardrobe-20260920/`.

| Команда / gate | До | После |
|---|---|---|
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3Static.ps1` | PASS / 0 | PASS / 0 |
| `Test-Stage35Static.ps1`, `Test-Stage4Static.ps1` с тем же prefix | PASS / 0 | PASS / 0 |
| `Test-RHSIntegrationStatic.ps1` | PASS / 0 | PASS / 0 |
| `Test-AILoadoutStatic.ps1`, `Test-InfantryRecruitmentStatic.ps1` | PASS / 0 | PASS / 0 |
| `Test-AICommanderModeStatic.ps1` | FAIL / 1, два правила | Те же два правила |
| `Test-RuntimeLauncherStatic.ps1` | — | PASS / 0 |
| PowerShell parser нового helper | — | PASS |
| `git diff --check` | — | PASS / 0 |
| Workbench stock Arland | — | PASS / 0 |
| Workbench ArlandRHS, EveronRHS, RHS fixture | — | PASS / 0 |

Baseline failures `AI_COMMANDER_UI_STATE`: точный текст ожидания команды
игрока и ложный objective marker на собственном HQ для `SYSTEM_HOLD ATTACK`.
Они не исправлялись. RHS static получил узкое исключение только для
одноразового character callback и дополнительные authority/identity/cleanup
проверки. Негативные пробы с repeat=true и отсутствующим Remove отклонены
правилами `RHS_SINGLE_LIFECYCLE` и `RHS_PMC_CLEANUP` (exit 1).

Точные Workbench argument arrays сохранены в `wb-verified-*-args.json`;
команда — `ArmaReforgerWorkbenchSteamDiag.exe @arguments` с `-noThrow
-wbsilent -gproj ... -addonsDir ... -addons ... -logsDir ...
-wbModule=ScriptEditor -run -validate`. Полные compiler outputs находятся
в `wb-verified-*-launch.txt`, exits — в `wb-verified-*-exit.txt`.
Во всех трёх есть `Script validation successful.`, ошибок компиляции нет.
Stock evidence: `wb-final-Stock-*`.

Финальный завершённый функциональный smoke:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthRHS `
  -RepositoryRoot "$PWD/.codex-runtime/rhs-wardrobe-20260920/stage" `
  -ProfileRoot "$PWD/.codex-runtime/rhs-wardrobe-20260920/smoke-final-2" `
  -AdditionalArguments @('-noThrow', '-aicfWardrobeShowcase', '2')
```

Native exit **0**; полный остановленный `console.log` лежит в
`smoke-final-2/logs/logs_2026-09-20_17-45-37/`. Результат: **13/13**
`PMC_EQUIPMENT_APPLIED`, **23/23** `WARDROBE_CHECK` с `usable_weapon=1`,
`capability_preserved=1`, `ai_active=0`; ошибок draft/commit нет.
Сравнение capability включает prefab оружия, prefab и число патронов каждого
магазина, prefab каждого медицинского предмета до и после смены одежды.

Полный runtime не объявляется чистым PASS: сохранены четыре RHS faction-init
SCRIPT errors, четыре arsenal RPC errors, две stock resupply SCRIPT errors
при shutdown и native world/resource diagnostics. В этой ранней тестовой
копии также отсутствовал Language; упаковка исправлена перед успешным
client run. Дополнительные damage RPC ошибки промежуточного prototype
устранены и в финальном smoke отсутствуют. Все неудачные прогоны сохранены.

Первый подключённый осмотр: `.codex-runtime/rhs-wardrobe-inspection-20260920-1755/`.
Server подтвердил 23 модели, 13 замен одежды и все 23 capability checks;
client загрузил мир, server зарегистрировал одного игрока. По сообщению
пользователя перенос не произошёл: первоначальный вызов `TeleportPlayer`
на сервере не перемещал owning client. Прежний `WARDROBE_VISITOR` доказывал
только результат вызова, а не положение игрока. Этот запуск остановлен.

Исправление находится только в `tools/fixtures/AICF_RHSWardrobeShowcase.c`:
owner RPC, identity guard, задержка после deployment, не более трёх попыток,
проверка фактического положения обоими peers. Callback клиентской проверки
снимается при повторной попытке и в destructor. Новый session root —
`.codex-runtime/rhs-wardrobe-inspection-20260920-1802/`.
Workbench fixture (`wb-teleport-args.json`, `wb-teleport-launch.txt`) — PASS / 0.
`Test-RHSIntegrationStatic.ps1` и `Test-RuntimeLauncherStatic.ps1` до/после —
PASS / 0 (`teleport-before-*`, `teleport-after-*`). Production не менялся.

В 18:03:23 новый runtime подтвердил перенос на обоих peers для одного RplId:
server `WARDROBE_VISITOR_VERIFIED distance_m=0.106406`, client
`WARDROBE_VISITOR_CHECK distance_m=0.100445 passed=1`. Это измеренная позиция
через две секунды после owner RPC, а не результат самого вызова. Индекс:
`teleport-verified.txt`; полные логи находятся в новом session root.
После переноса клиент записал один SCRIPT E штатного
`SCR_SupportStationAreaMeshComponent`: несовпадение offset зоны FUEL при
streaming новой области. VM/ошибок переноса нет. Сервер и клиент этого запуска
остановлены перед обновлением вооружения. Визуальный verdict новых комплектов
по-прежнему NOT RUN.

### Усиленные комплекты ЧВК

Evidence обновления: `.codex-runtime/rhs-pmc-upgrade-20260920/`.
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File` для
`tools/Test-RHSIntegrationStatic.ps1`, `Test-AILoadoutStatic.ps1` и
`Test-Stage35Static.ps1` до и после — PASS / 0. `Test-AICommanderModeStatic.ps1`
сохраняет те же два `AI_COMMANDER_UI_STATE` failures / exit 1.
`git diff --check` — PASS / 0.

Терминальный Workbench Validate для ArlandRHS, EveronRHS и fixture — PASS / 0,
во всех трёх `Script validation successful.`. Точные аргументы и полные
outputs: `wb-final-*-args.json`, `wb-final-*-launch.txt`, `wb-final-*-exit.txt`.

Остановленный smoke использует предыдущую команду `Start-AICFRuntime.ps1`
с новым `-ProfileRoot "$PWD/.codex-runtime/rhs-pmc-upgrade-20260920/smoke-5"`
и теми же stage/variant/CLI. Native exit 0; полный log:
`smoke-5/logs/logs_2026-09-20_18-26-21/console.log`, 2596 строк.
Результат: 13/13 `PMC_EQUIPMENT_APPLIED`, 13/13 `PMC_ARMAMENT_CHECK passed=1`,
23/23 `WARDROBE_CHECK` с `usable_weapon=1 capability_preserved=1 ai_active=0`.
У 12 основных стволов `suppressed=1`; ПКМ — 100 заряженных патронов и три
запасные коробки. У подносчика 26 запасных магазинов; у помощника пулемётчика
проверены три полные коробки для ПКМ. Ошибок draft/commit нет.
`smoke-5-summary.json` и `verified-attachments.txt` — индексы полного лога.

Capability теперь сравнивает неизменяемую часть: медицину, ПТ и пистолеты.
Основной калибр и коробки помощника проверяются отдельно через фактическое
оружие, magazine well, количество патронов, совместимость и наличие глушителя.
Native RHS prefab обеспечивает обвес; его реальные дочерние entities записаны
в `WARDROBE_ITEM`. В первичных попытках несовместимые места для очков/подсумка
останавливали draft до commit. В окончательном комплекте порядок надевания
исправлен, используются штатные подсумки совместимого AVS/ronin.

Полный runtime по-прежнему не чистый PASS: четыре RHS faction-init SCRIPT
errors, четыре arsenal RPC errors, два stock resupply SCRIPT errors при
shutdown, native world/resource/pathfinding diagnostics и font resource leak.
Полный список — `smoke-5-errors.txt`; ошибок VM/нового adapter нет.

Новый подключённый осмотр запущен командами helper выше с
`-SessionRoot "$PWD/.codex-runtime/rhs-pmc-inspection-20260920-1832"`.
Сервер и клиент используют одну копию исходников; launcher сохраняет оба
`AICF_RUNTIME_MANIFEST_JSON` и проверяет живой сервер, точный CLI и
`ROSTER_READY` до client connect. Сервер и клиент оставлены для ручного осмотра.
Stopped client gate этого запуска — NOT RUN.

В 18:32:23 оба peers подтвердили новое фактическое положение одного RplId:
server `distance_m=0.0922821`, client `distance_m=0.0997976 passed=1`.
Client загрузил мир и подключился; в сохранённом полном live snapshot
нет SCRIPT/RPL errors или `WORLD_LOAD_ERROR`. Полные копии текущих логов:
`inspection-server-live-snapshot.log`, `inspection-client-live-snapshot.log`;
индекс переноса — `inspection-teleport-verified.txt`. SHA256 двух production
PMC файлов совпадают с копиями запущенного сценария. Это live evidence,
оно не заменяет stopped client gate или ручную оценку внешнего вида.

**NOT RUN:** ручная оценка нового внешнего вида и нашивок, client inventory/JIP audit,
длительный бой и soak, runtime ArlandRHS, ручное подтверждение результата.

## Изменённые файлы

- `AIConflictArlandRHS/.../Content/AICF_RHSContentProfile.c` — варианты MSV.
- `AIConflictArlandRHS/.../Content/AICF_RHSPMCEquipment.c` — комплекты FIA/ЧВК.
- `AIConflictArlandRHS/.../Content/AICF_RHSPMCArmament.c` — RHS оружие, магазины и проверка вооружения.
- `tools/fixtures/AICF_RHSWardrobeShowcase.c` — ряд и функциональные проверки.
- `tools/Start-AICFRHSWardrobeShowcase.ps1` — воспроизводимый запуск осмотра.
- `tools/Test-RHSIntegrationStatic.ps1` — точный контракт character callback.
- `README.md`, `docs/ARCHITECTURE.md`, `docs/TESTING.md`, этот документ.

Непрофильные пользовательские prompt-файлы не изменены. Generated caches,
profiles и logs не добавлены в Git.
