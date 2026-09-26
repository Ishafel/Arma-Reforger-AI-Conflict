# Нашивки РФ/США — 2026-09-26

`AICF_RHSDefaultPatches.c` добавляет нашивки всем штатным RHS персонажам
`RHS_USAF`/`RHS_AFRF` при создании, включая пополнение. Специального действия
игрока не требуется. Существующие одежда, оружие, боезапас, медицина и нашивки
не заменяются. FIA/ЧВК сохраняет прежние нейтральные комплекты.

США получает обычные/обратные флаги и четыре именных варианта USMC,
выбранные по роли. РФ — камуфлированные и светоотражающие флаги, включая
оранжевый вариант для ПТ/пулемётных ролей; для доступного нагрудного крепления
предусмотрена надпись «Россия». Медик получает обозначение A+ на подходящем
левом креплении. Это декоративные предметы RHS, без изменения игровой медицины.

Обходятся все зарегистрированные inventory storages и все Velcro-крепления,
в том числе вложенные детали жилета и рюкзаки. Выдача заполняет каждый
свободный совместимый слот. Важное ограничение самих ресурсов RHS: название
Velcro не гарантирует доступность. У стандартных 6Б47 оба таких места
отклоняют нашивки, у ECH отключено верхнее, некоторые панели жилетов перекрыты
подсумками, крепления FILBE также недоступны. Эти места не заполняются в обход
native проверки. На российской форме оба рукава и на совместимом Wartech
нашивки устанавливаются; у США работают боковые/задние места ECH и доступные
панели жилета. Фактический результат для каждого из 20 штатных комплектов:

| Роль в roster | США | РФ |
|---|---:|---:|
| Командир | 4 | 2 |
| Медик | 5 | 2 |
| Пулемётчик | 6 | 2 |
| ПТ | 6 | 2 |
| Подствольный гранатомёт | 6 | 2 |
| Автоматчик | 6 | 2 |
| Командир звена / старший стрелок | 4 | 2 |
| Помощник пулемётчика | 5 | 4 |
| Помощник / подносчик боеприпасов | 5 | 2 |
| Стрелок | 4 | 2 |

Это одноразовое оформление при создании персонажа. Ручная смена экипировки
позже не вызывает постоянного автозаполнения. Штатный игрок проходит тот же
server path; его deployment и JIP пока отдельно не проверены.

Evidence: `.codex-runtime/rhs-patches-20260926/`, исходный commit сохранён
в `commit.txt`, рабочее дерево перед изменениями чистое. API: Script Diff
`1.8.0.13`; установленные game/server/tools `1.8.0.13`, RHS `0.16.5208`.
Точные prefab GUID получены из установленного RHS `resourceDatabase.rdb`
и подтверждены runtime admission; установленные файлы не изменялись.

Статические команды до и после:

```powershell
# Для каждого Name из списка ниже:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>.ps1
```

`Stage3Static`, `Stage35Static`, `Stage35RecoveryPolicy`, `Stage4Static`,
`AICommanderModeStatic`, `RHSIntegrationStatic`, `AILoadoutStatic`,
`InfantryRecruitmentStatic`: **PASS / 0** до и после; сохранённых static failures
в этом срезе нет. Пять отрицательных копий с нарушением authority, identity,
cleanup, occupied-slot guard и repeat=false: ожидаемый **FAIL / 1**.
Полные результаты — `before-*`, `after-*`, `negative-*`.

Workbench выполняется прямым терминальным вызовом
`ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent -gproj ... -addonsDir ...
-addons ... -logsDir ... -wbModule=ScriptEditor -run -validate`.
Аргументы сохранены в `wb-*-args.json`, полный stdout — `wb-*-launch.txt`,
native logs — в одноимённых каталогах, exit — в `wb-*-exit.txt`.
Stock Arland и ArlandRHS: **PASS / 0**, `Script validation successful.`,
без SCRIPT/VM/fatal errors. Shutdown resource leaks сохранены отдельно.
Production EveronRHS: `Script validation successful.`, SCRIPT/VM/fatal = 0;
затем native shutdown hang после `Game destroyed`. Процесс остановлен
адресно по PID 35364 и полному CLI, native exit **-1**. Compile подтверждён,
полный Workbench process gate **FAIL**, не PASS / 0. Evidence:
`wb-everon-rhs/console.log`, `wb-everon-rhs-hung-process.txt`,
`wb-everon-rhs-stop.txt`, `wb-everon-rhs-exit.txt`.
Изолированный probe graph также скомпилирован успешно; процесс затем завис
при shutdown и остановлен адресно по PID/CLI, exit -1. Это не PASS завершения
Workbench. Первый sandbox-запуск discovery остановился на SteamAPI_Init,
exit 2; дальнейшие проверки выполнялись в пользовательской Steam-сессии.

Runtime fixture находится в `tools/fixtures/AICF_RHSPatchesProbe.c`;
она копируется в `AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/`
только изолированного source graph с пятью AICF addons и локальными resource
indices. Для копии сначала выполняется терминальный Workbench по команде из
`../DEVELOPMENT.md`, затем:

```powershell
./tools/Start-AICFRuntime.ps1 -Role Server -Variant EveronNorthRHS `
  -RepositoryRoot "$PWD/.codex-runtime/rhs-patches-20260926/source" `
  -ProfileRoot "$PWD/.codex-runtime/rhs-patches-20260926/probe-server" `
  -AdditionalArguments @('-noThrow', '-aicfPatchProbe', '1', '-aicfRequirePlayerForResult', '0')
```

Повторный запуск требует нового `ProfileRoot`. `probe-server-launch.txt`
сохраняет `AICF_RUNTIME_MANIFEST_JSON`; native exit **0**. Полный остановленный
log: `probe-server/logs/logs_2026-09-26_14-03-49/console.log`, 1333 строки.
Результат: **20/20** `PATCH_PROBE_CHECK` с `missing=0 preserved=1 idempotent=1`,
73 установленные нашивки, у production callback `failed=0`. Повторный Apply
сохранил прежние patch entity identities. Исходные вещи проверены по entity
identity, prefab, parent slot и числу патронов. Ошибок SCRIPT/VM нового adapter
нет; полный error gate не чистый: сохранены world/resource/pathfinding
диагностика и shutdown font leak, индекс — `probe-error-index.txt`.
До/после production изменения — по 198 строк `(E/F)`, SCRIPT/VM/fatal = 0;
нормализованные множества сообщений совпали (`error-pattern-diff.txt` пуст).
Baseline полного лога — `discovery-server/logs/.../console.log`, 2837 строк;
сводка обоих stopped logs — `full-log-summary.json`.

Первичная изолированная проба с неполным ResourceName завершилась native crash
(`fit2-server`, exit -1073741819). В production используются только полные
GUID-ссылки. Исправленная проверка совместимости `fit3-server` и итоговый
`probe-server` завершились штатно. Неудачная проба и dump сохранены.

**NOT RUN:** client/JIP, deployment игрока, ручная оценка размера/ориентации
и видимости нашивок, длительный бой/soak, отдельный runtime ArlandRHS,
установка в Workshop и ручная приёмка. Визуальная проверка выполняется
пользователем; screenshots и GUI automation не применялись.
`git diff --check`: **PASS / 0**. Все созданные серверы и Workbench процессы
остановлены; запуск игровой сессии пользователю не оставлен.

Изменённые файлы:

- `AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Content/AICF_RHSDefaultPatches.c` — выдача и lifecycle.
- `tools/Test-RHSIntegrationStatic.ps1` — guards нашивок и точное исключение одноразового callback.
- `tools/fixtures/AICF_RHSPatchesProbe.c` — изолированная runtime проверка.
- `.gitignore` — разрешён только этот новый fixture.
- `README.md`, этот отчёт — описание функции и evidence.
- Локальные `docs/ARCHITECTURE.md`, `docs/TESTING.md`, `docs/RHS_WARDROBE.md` — обновлены существующие разделы; эти файлы уже исключены текущим `.gitignore`.
