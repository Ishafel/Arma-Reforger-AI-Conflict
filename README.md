# Arma Reforger AI Conflict

Scripts-only прототип автономного AI Conflict для Arma Reforger. Проект расширяет штатный Conflict на Arland, не копирует и не изменяет vanilla-мир и выполняет игровую логику только на authoritative server/master.

## Текущий статус

| Этап | Статус | Что это означает |
|---|---|---|
| Stage 0 — исследовательский прототип | **PASS** | Ручной direct Diag dedicated-тест на Arland завершился `[AICF][STAGE0][RESULT][PASS]`: обнаружены штатные базы и граф, созданы US/USSR-группы, обе получили цели и waypoint |
| Stage 1 — пехотный вертикальный срез | **PASS** | Подтверждены 4+4 группы, роли `2/1/1`, AI-захват и retarget, безопасные подкрепления, билеты, диагностические маркеры и клиентский runtime-прогон |
| Stage 2 — надёжность и баланс | **в разработке** | Ветка `codex/stage-2-reliability`: lifecycle-аудит, восстановление приказов, stuck-watchdog, spawn/load guard, внешний CLI-конфиг и headless soak |
| Полный MVP | **не готов к приёмке** | После Stage 2 остаются стандартная MVP-матрица, клиентская синхронизация и полный 30-минутный прогон |
| Двухчасовой soak | **не запускался** | Выполняется отдельно только после успешной полной MVP-матрицы |

Подтверждённые Stage 0 и Stage 1 относятся к своим проверенным commit. Stage 2-кандидат компилируется и тестируется отдельно и не считается принятым до выполнения матрицы из `docs/STAGE_2_TESTING.md`.

## Что подтверждено в Stage 0

- загрузка `AIConflictCore` и `AIConflictArland` вместе со штатным Arma Reforger;
- запуск ядра только на authoritative server/master;
- ожидание готовности `SCR_GameModeCampaign` и Conflict base manager;
- обнаружение штатных `BASE`, `SOURCE_BASE` и `RELAY` без координат или имён конкретных баз Arland в коде;
- построение ориентированного графа штатной радиодостижимости;
- получение штатных фракций `US` и `USSR`;
- выбор graph-reachable цели для обеих сторон;
- создание по одной штатной пехотной группе и Move-waypoint;
- защита от повторной инициализации в одной сессии;
- стабильные диагностические сообщения `[AICF][STAGE0]`.

Stage 0 не проверял фактический захват базы, повторные приказы, подкрепления, билеты или завершение матча.

## Что входит в Stage 1

Целевой вертикальный срез должен обеспечить:

- по четыре управляемые пехотные группы на сторону;
- точное распределение каждой стороны `2 ATTACK / 1 DEFEND / 1 RESERVE`;
- реальные смены владельца штатных баз действиями AI;
- повторное назначение цели не позднее двух интервалов решений командующего;
- устойчивые слоты групп и обработку штатного `OnEmpty`;
- replacement через 30 секунд;
- списание билета только после успешного появления replacement-группы;
- запрет появления подкреплений на вражеской или contested-базе;
- ровно одно завершение матча;
- подключение игрока к US и USSR;
- стабильный диагностический контракт `[AICF][STAGE1]`.

Stage 1 реализует серверные модели конфигурации, ролей, слотов и билетов, live-выбор целей, role-aware приказы, безопасный spawn, ticket reservation, репликацию билетов, билетную победу и диагностические маркеры.

## Что входит в Stage 2

- spawn-generation и защита от повторной привязки группы;
- периодический lifecycle-аудит всех устойчивых слотов;
- автоматическое восстановление потерянного waypoint без recovery-churn у активной цели;
- измерение прогресса по живому лидеру, перестроение маршрута и штатная платная замена устойчиво застрявшей группы;
- компактная stock-формация `Column` для управляемых групп без телепортации бойцов;
- ограничение одновременных replacement-spawn и общей численности AI;
- настройка темпа войны и reliability-порогов через CLI;
- отдельный контракт `[AICF][STAGE2]` и расширенные map-маркеры;
- воспроизводимый fault-injection прогон и длительный headless soak.

## Что ещё не заявлено готовым

- полная runtime-приёмка Stage 2: fault-injection, 30-минутная матрица и двухчасовой soak;
- стандартный 30-минутный MVP-прогон;
- синхронизация билетов и состояния матча на нескольких клиентах;
- проверка смерти и повторного развёртывания игрока;
- окончательная настройка баланса и темпа войны;
- техника, логистика, сохранение состояния и пользовательский интерфейс;
- двухчасовой soak с контролем сущностей, групп, waypoint, памяти и server FPS.

Полные цели и границы продукта описаны в [PROJECT_VISION.md](PROJECT_VISION.md).

## Состав проекта

```text
Arma-Reforger-AI-Conflict/
├── AIConflictCore/
│   ├── addon.gproj
│   └── Scripts/Game/AIConflict/
│       ├── Bootstrap/
│       ├── Config/
│       ├── Diagnostics/
│       ├── Forces/
│       ├── Integration/
│       ├── Objectives/
│       ├── Orders/
│       └── State/
├── AIConflictArland/
│   ├── addon.gproj
│   └── Scripts/Game/AIConflictArland/Integration/
├── docs/
│   ├── API_REFERENCE.md
│   ├── STAGE_0_TESTING.md
│   ├── STAGE_1_TESTING.md
│   └── STAGE_2_TESTING.md
├── tools/
│   ├── Test-Stage2Log.ps1
│   └── fetch_reforger_api_reference.sh
└── PROJECT_VISION.md
```

Идентификаторы проектов менять нельзя:

- `AIConflictCore`: `9178E5822AFE48EA`;
- `AIConflictArland`: `B52C5F6AEDBF423E`;
- штатная зависимость Arma Reforger: `58D0FB3206B6F859`.

## Необходимые программы

Для разработки и Stage 1-приёмки нужны:

1. Windows 10/11 x64.
2. Arma Reforger.
3. Arma Reforger Tools той же версии и ветки, что игра.
4. Arma Reforger Server той же версии; нужен `ArmaReforgerServerDiag.exe`.
5. Steam и Git for Windows.

Исходный Stage 0 ориентировался на API 1.7.0.54, и именно на этой версии был получен runtime PASS. После обновления игры необходимо записать новую версию в отчёте и повторить компиляцию и весь runtime-тест.

## Получение рабочей ветки

```powershell
git clone https://github.com/Ishafel/Arma-Reforger-AI-Conflict.git
Set-Location Arma-Reforger-AI-Conflict
git fetch origin
git switch codex/stage-2-reliability
git rev-parse --short HEAD
```

Текущая локальная ветка разработки — `codex/stage-2-reliability`. Если она ещё не опубликована, используйте переданную рабочую копию или конкретный commit от разработчика. Commit обязательно записывается отдельно для каждого теста.

Репозиторий следует хранить в локальном доступном для записи каталоге. Не помещайте исходный проект в Workshop, OneDrive или каталог только для чтения.

## Подключение проектов в Reforger Tools

1. Запустите `Arma Reforger Tools` из Steam.
2. При необходимости добавьте штатный `<папка игры>\addons\data\ArmaReforger.gproj`.
3. Выберите `Add Project → Scan for Projects` и укажите корень репозитория.
4. Убедитесь, что Launcher показывает `ArmaReforger`, `AIConflictCore` и `AIConflictArland`.
5. Откройте `AIConflictArland` и дождитесь завершения Resource Database.
6. В `Editors → Script Editor` выполните `Build → Validate Scripts`.
7. При отсутствии ошибок выполните `Build → Compile and Reload Scripts` (`Shift+F7`).
8. Сохраните полный результат обеих операций.

Любая script/dependency/resource error блокирует runtime-тест текущего commit. `resourceDatabase.rdb` создаётся Workbench и не добавляется в Git.

## Эталонный direct Diag-запуск Arland

Для локального исходного аддона не используйте `Multiplayer → Host`: при пересоздании GameProject Host UI может выгрузить мод, которого нет в правой колонке Mods. Эталонная Stage 1-приёмка выполняется напрямую через Diag dedicated server.

Откройте PowerShell и подставьте фактический путь к репозиторию:

```powershell
$serverRoot = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server"
$repoRoot = "C:\Users\<имя>\IdeaProjects\Arma-Reforger-AI-Conflict"
$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$profileRoot = "$env:LOCALAPPDATA\AICF\Stage1-$runStamp"

Set-Location $serverRoot

& "$serverRoot\ArmaReforgerServerDiag.exe" `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -server "worlds/MP/CTI_Campaign_Arland.ent" `
  -MissionHeader "Missions/23_Campaign_Arland.conf" `
  -worldSystemsConfig "Configs/Systems/ConflictSystems.conf" `
  -addonsDir "$repoRoot,$serverRoot\addons" `
  -addons "9178E5822AFE48EA,B52C5F6AEDBF423E" `
  -profile "$profileRoot" `
  -backendFreshSession `
  -maxFPS 60 `
  -logStats 10000
```

Параметры `-MissionHeader` и `-worldSystemsConfig` обязательны: raw world без них не равен штатной Conflict-миссии. Для каждого приёмочного запуска создаётся новый `$profileRoot`; persistent-состояние предыдущего матча не переиспользуется.

## Подключение локального клиента

Клиент запускается с теми же локальными аддонами:

```powershell
$gameRoot = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger"
$repoRoot = "C:\Users\<имя>\IdeaProjects\Arma-Reforger-AI-Conflict"

Set-Location $gameRoot
& "$gameRoot\ArmaReforgerSteamDiag.exe" `
  -gproj "$repoRoot\AIConflictArland\addon.gproj" `
  -client 127.0.0.1 `
  -addonsDir "$repoRoot,$gameRoot\addons" `
  -addons "9178E5822AFE48EA,B52C5F6AEDBF423E"
```

Diag-клиент загружает тот же working project и автоматически подключается к dedicated server по `127.0.0.1` (стандартный локальный порт — `2001`). Если сервер запущен на другом порту, добавьте его после адреса в `-client`. Для Stage 1 требуются две новые сессии: запуск A с игроком за US и запуск B с игроком за USSR.

Для локальной отладки позиций managed AI-групп можно добавить `-aicfDebugMapMarkers 1` одновременно в команды сервера и клиента. На карте появятся восемь глобальных маркеров, следующих за живыми лидерами групп, с цветом стороны, устойчивым слотом, полной ролью, текущей задачей, целью, числом живых бойцов и состоянием route recovery. После гибели лидера маркер автоматически перепривязывается к новому лидеру. Режим выключен по умолчанию, раскрывает обе стороны и не предназначен для публичной игры или балансового прогона.

## Проверка журнала

Последний `console.log` из заданного профиля находится и фильтруется так:

```powershell
$log = Get-ChildItem "$profileRoot\logs" -Filter console.log -File -Recurse |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

$log.FullName
Select-String -LiteralPath $log.FullName -SimpleMatch "[AICF][STAGE1]" |
  ForEach-Object Line
```

Stage 0 использует префикс:

```text
[AICF][STAGE0]
```

Stage 1 использует отдельный префикс:

```text
[AICF][STAGE1]
```

Отфильтрованный вывод не заменяет полный журнал. К отчёту прикладывается вся папка `$profileRoot\logs`, чтобы сохранить соседние `SCRIPT`, `RESOURCES`, `RPL` и `VME`-ошибки.

## Руководства по тестированию

- [Stage 0: инструкция приёмочного тестирования](docs/STAGE_0_TESTING.md) — контракт исследовательского прототипа и расшифровка подтверждённого `[AICF][STAGE0][RESULT][PASS]`.
- [Stage 1: пехотный вертикальный срез](docs/STAGE_1_TESTING.md) — direct Diag-команды, профиль `4 × 4`, временные и причинные инварианты, два ускоренных запуска и итоговая матрица.
- [Stage 2: надёжность и баланс](docs/STAGE_2_TESTING.md) — fault injection, lifecycle/order/load инварианты, 30-минутная headless-матрица и двухчасовой soak.

Stage 1 принимается только если одновременно выполнены:

1. `Validate Scripts` и `Compile and Reload Scripts` без ошибок.
2. В каждом запуске готовы четыре US и четыре USSR-группы с ролями `2/1/1`.
3. AI реально меняет владельца базы без принудительного тестового вызова.
4. После захвата новая цель назначается не позднее двух commander-интервалов.
5. `OnEmpty` приводит к replacement не раньше 30 секунд.
6. Билет списывается только после успешного spawn.
7. Enemy/contested-база отклоняется как spawn site.
8. Матч завершается ровно один раз.
9. Запуск A проходит с игроком US, запуск B — с игроком USSR.
10. В логах нет `[AICF][STAGE1][ERROR]` и `[RESULT][FAIL]` и есть итоговый `[RESULT][PASS]`.

Даже два успешных Stage 1-запуска означают только готовность перейти к полной MVP-матрице. Они не являются результатом 30-минутного стандартного прогона или двухчасового soak.

## Порядок приёмки Stage 2

1. Выполнить Workbench Validate/Compile.
2. Запустить быстрый fault-injection прогон восстановления приказа.
3. Выполнить 30-минутную headless-матрицу на стандартном темпе.
4. Исправить дефекты и повторить матрицу на одном commit.
5. Выполнить отдельный двухчасовой soak и сравнить начало/конец по группам, waypoint, памяти и server FPS.

## Локальная копия официального API

Проверенные сигнатуры 1.7.0.54 перечислены в [docs/API_REFERENCE.md](docs/API_REFERENCE.md). Чтобы загрузить официальный Script Diff в локальный, исключённый из Git кэш:

```bash
./tools/fetch_reforger_api_reference.sh
```

## Официальные материалы

- [Scripting Modding](https://community.bistudio.com/wiki/Arma_Reforger%3AScripting_Modding)
- [Mod Project Setup](https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Project_Setup)
- [Script Editor](https://community.bistudio.com/wiki/Arma_Reforger%3AScript_Editor)
- [Server Hosting](https://community.bistudio.com/wiki/Arma_Reforger%3AServer_Hosting)
- [Startup Parameters](https://community.bistudio.com/wiki/Arma_Reforger%3AStartup_Parameters)
- [Arma Reforger Script Diff 1.7.0.54](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/v1.7.0.54)
- [Official Arma Reforger Samples](https://github.com/BohemiaInteractive/Arma-Reforger-Samples)
