# Arma Reforger AI Conflict

Исследовательский прототип автономного AI Conflict для Arma Reforger. Текущая версия реализует только этап 0 из [PROJECT_VISION.md](PROJECT_VISION.md): она проверяет интеграцию со штатным Conflict на Arland, строит граф баз и создаёт по одной тестовой группе США и СССР.

Статус: код этапа 0 подготовлен для Arma Reforger/Reforger Tools 1.7.0.54. В этой среде нет Reforger Tools, поэтому открытие проектов, компиляция Enforce Script и запуск миссии ещё должны быть подтверждены владельцем продукта по [чек-листу тестирования](docs/STAGE_0_TESTING.md). До такого теста этап 0 нельзя считать принятым.

## Что уже реализовано

- два scripts-only аддона: переиспользуемый `AIConflictCore` и тонкая интеграция `AIConflictArland`;
- запуск ядра только на authoritative server/master;
- ожидание запуска штатного `SCR_GameModeCampaign` и завершения инициализации Conflict-баз;
- обнаружение штатных `BASE`, `SOURCE_BASE` и `RELAY` без координат и названий конкретных баз Arland;
- ориентированный граф штатной радиодостижимости баз;
- подробный вывод узлов, рёбер и итогов графа в журнал;
- получение штатных фракций BLUFOR/OPFOR с обязательной проверкой ключей `US`/`USSR`;
- выбор для каждой стороны ближайшей допустимой цели, достижимой по графу от её штаба;
- создание одной группы из штатного `GetDefendersGroupPrefab()` каждой стороны;
- создание и назначение штатного Move-waypoint к выбранной базе;
- идемпотентная инициализация в рамках одного экземпляра Conflict: повторный запрос не создаёт ещё две группы;
- явные диагностические ошибки для отсутствующих API, баз, фракций, prefab, spawn point, групп и waypoint;
- локальный, не входящий в мод кэш официального Script Diff 1.7.0.54.

Интеграция не копирует и не изменяет штатный мир Arland. Когда аддон загружен, `modded SCR_GameModeCampaign` расширяет штатный Conflict после вызова его оригинального `OnGameStart()`.

## Что пока не реализовано

Этап 1 и последующие этапы намеренно не начаты. В прототипе нет:

- полноценного AI-командующего;
- повторного выбора целей после захвата или потери базы;
- проверки прибытия и захвата базы;
- ролей атаки/обороны, резервов и балансировки;
- билетов, подкреплений, восстановления групп и условий победы;
- техники, логистики, сохранения состояния и пользовательского интерфейса;
- отдельного скопированного или изменённого сценария Arland.

Move-waypoint подтверждает выдачу приказа, но этап 0 не обещает завершение маршрута или захват цели. Граф отражает радиосвязь Conflict, а не доказанную проходимость navmesh.

## Состав проекта

```text
Arma-Reforger-AI-Conflict/
├── AIConflictCore/
│   ├── addon.gproj
│   └── Scripts/Game/AIConflict/
│       ├── Bootstrap/
│       ├── Diagnostics/
│       ├── Forces/
│       ├── Integration/
│       ├── Objectives/
│       └── Orders/
├── AIConflictArland/
│   ├── addon.gproj
│   └── Scripts/Game/AIConflictArland/Integration/
├── docs/
│   ├── API_REFERENCE.md
│   └── STAGE_0_TESTING.md
├── tools/fetch_reforger_api_reference.sh
└── PROJECT_VISION.md
```

Идентификаторы проектов не следует менять:

- `AIConflictCore`: `9178E5822AFE48EA`;
- `AIConflictArland`: `B52C5F6AEDBF423E`;
- штатная зависимость Arma Reforger: `58D0FB3206B6F859`.

## Какие программы нужны

Для тестировщика:

1. Windows 10/11 x64.
2. Steam и установленная Arma Reforger версии 1.7.0.54.
3. Установленные через Steam Arma Reforger Tools той же версии и ветки, что и игра.
4. Git for Windows — только для клонирования и обновления проекта.
5. Для dedicated-варианта, который не обязателен для первого теста, — Arma Reforger Server.

Если Steam уже предлагает версию новее 1.7.0.54, сначала зафиксируйте её номер в отчёте. После любого обновления игры API и ресурс Move-waypoint должны быть проверены заново.

## Клонирование и размещение

Рекомендуемый каталог:

```text
%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\Arma-Reforger-AI-Conflict
```

Не используйте папку Workshop, `OneDrive` или другой каталог только для чтения. Откройте PowerShell и выполните:

```powershell
cd "$env:USERPROFILE\Documents\My Games\ArmaReforgerWorkbench\addons"
git clone https://github.com/Ishafel/Arma-Reforger-AI-Conflict.git
cd Arma-Reforger-AI-Conflict
git switch agent/stage-0-prototype
```

Для текущей локальной поставки рабочая ветка называется `agent/stage-0-prototype`. Если эта ветка не опубликована на сервере Git, используйте переданную владельцем продукта рабочую копию; инструкция не требует публикации мода в Workshop.

## Открытие `.gproj` в Reforger Tools

1. Запустите `Arma Reforger Tools` из Steam. Откроется Workbench Launcher.
2. Если штатный проект ещё не добавлен, нажмите `Add Existing` и выберите `<папка игры>\addons\data\ArmaReforger.gproj`.
3. Выберите `Add Project` → `Scan for Projects`.
4. Укажите корень клонированного репозитория `Arma-Reforger-AI-Conflict`, а не один из двух вложенных каталогов.
5. Убедитесь, что Launcher показывает три проекта: `ArmaReforger`, `AIConflictCore`, `AIConflictArland`.
6. Дважды щёлкните проект `AIConflictArland` в Launcher либо выберите для него `Open`. Его `addon.gproj` должен автоматически подключить `AIConflictCore` и штатный `ArmaReforger`.
7. Дождитесь окончания построения/обновления Resource Database. Не закрывайте Workbench, пока индикатор обработки ресурсов активен.

Если Launcher показывает только vanilla-проект или ошибку dependency, миссию запускать рано: повторите сканирование корня репозитория и приложите снимок окна к дефекту.

## Обновление зависимостей, ресурсов и скриптов

После первого открытия или получения новой версии:

1. Закройте активную игровую сессию и Workbench.
2. В PowerShell перейдите в каталог репозитория и выполните `git pull` для выданной вам ветки.
3. Убедитесь, что Arma Reforger и Reforger Tools находятся на одинаковой стабильной версии.
4. Повторите `Scan for Projects`, только если `.gproj` были перемещены или добавлены.
5. Откройте `AIConflictArland` и дождитесь автоматического обновления `resourceDatabase.rdb`.
6. В Workbench откройте `Editors` → `Script Editor`.
7. Выполните `Build` → `Validate Scripts`.
8. Если ошибок нет, выполните `Build` → `Compile and Reload Scripts` (`Shift+F7`).
9. Сохраните или сфотографируйте результат обеих операций для отчёта.

`resourceDatabase.rdb` создаёт Workbench; этот бинарный файл нельзя добавлять в Git. Если вы меняли зависимости через `Workbench` → `Options` → `Game Project`, перезапустите Workbench до компиляции.

## Штатный сценарий Arland

Проверяется официальный mission header:

```text
{C41618FD18E9D714}Missions/23_Campaign_Arland.conf
```

Отдельный сценарий, копия мира или пользовательский компонент не нужны. Не открывайте raw-файл `Arland.ent` и не считайте запуск `F5` проверкой Conflict: такой запуск может не загрузить штатный mission header и его слои.

Если Workbench или инструкция требует изменить vanilla Arland, сохранить его копию в проект либо вручную добавить компонент этапа 0, остановите тест и заведите дефект. В текущей реализации bootstrap является scripts-only расширением `SCR_GameModeCampaign` и не требует компонента в World Editor.

## Запуск listen server — рекомендуемый тест

Сначала успешно выполните `Validate Scripts` и `Compile and Reload Scripts`.

1. Закройте уже запущенную Arma Reforger.
2. Откройте PowerShell.
3. Подставьте фактические пути и запустите одной строкой:

```powershell
& "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\ArmaReforgerSteam.exe" -addonsDir "C:\Users\<имя>\Documents\My Games\ArmaReforgerWorkbench\addons\Arma-Reforger-AI-Conflict" -addons B52C5F6AEDBF423E
```

4. В игре выберите `Multiplayer` → `Host` → `Host new server`.
5. Выберите штатный `Conflict - Arland`.
6. Проверьте в списке модов наличие `AI Conflict Arland`. Если его нет, не продолжайте: vanilla Conflict может запуститься без нашего кода.
7. Не отключайте AI. Для эталонного теста используйте неограниченный AI limit; иначе лимита должно хватать на штатный AI и всех бойцов двух тестовых групп.
8. Запустите сервер и дождитесь появления мира. Играть или выбирать фракцию для запуска Stage 0 не требуется.
9. Подождите не менее 35 секунд, затем проверьте журнал.

`-addonsDir` указывает именно на корень репозитория, потому что внутри него соседствуют оба addon-проекта. `-addons` использует GUID тонкого Arland-аддона; Core загружается как зависимость.

## Dedicated server — дополнительный вариант

В уже работающем server JSON обязательно должен быть указан штатный сценарий и разрешён AI:

```json
{
  "game": {
    "scenarioId": "{C41618FD18E9D714}Missions/23_Campaign_Arland.conf"
  },
  "operating": {
    "disableAI": false,
    "aiLimit": -1
  }
}
```

Это фрагмент, а не полный сетевой конфиг: адреса, порты, имя и пароль сохраните из своего уже проверенного server JSON. Запуск:

```powershell
& "<путь>\ArmaReforgerServer.exe" -config "<путь>\stage-0-server.json" -addonsDir "<абсолютный путь>\Arma-Reforger-AI-Conflict" -addons B52C5F6AEDBF423E -backendFreshSession -maxFPS 60
```

Не используйте `-server "...Arland.ent"`: raw world не равен Conflict mission header и при таком параметре server config может быть проигнорирован. Клиент dedicated-теста запускайте с теми же `-addonsDir` и `-addons`, поскольку локальный неопубликованный аддон не скачивается из Workshop.

## Что искать в журнале

Фильтр всех сообщений проекта:

```text
[AICF][STAGE0]
```

Нормальная последовательность содержит:

```text
[AICF][STAGE0][INFO][BOOTSTRAP_SERVER]
[AICF][STAGE0][INFO][CONFLICT_READY]
[AICF][STAGE0][INFO][BASE_DISCOVERY]
[AICF][STAGE0][INFO][GRAPH_EDGE]
[AICF][STAGE0][INFO][GRAPH_NODE]
[AICF][STAGE0][INFO][GRAPH_SUMMARY]
[AICF][STAGE0][INFO][FACTION_READY] faction=US
[AICF][STAGE0][INFO][FACTION_READY] faction=USSR
[AICF][STAGE0][INFO][TARGET_SELECTED] faction=US
[AICF][STAGE0][INFO][TARGET_SELECTED] faction=USSR
[AICF][STAGE0][INFO][GROUP_CREATED] faction=US
[AICF][STAGE0][INFO][WAYPOINT_ASSIGNED] faction=US
[AICF][STAGE0][INFO][GROUP_CREATED] faction=USSR
[AICF][STAGE0][INFO][WAYPOINT_ASSIGNED] faction=USSR
[AICF][STAGE0][INFO][GROUP_READY]
[AICF][STAGE0][RESULT][PASS]
```

Имена баз, номера узлов, число рёбер, prefab и число бойцов определяются штатным сценарием во время запуска. Любая строка `[ERROR]` или `[RESULT][FAIL]` означает неуспешный тест, даже если сама миссия продолжает работать.

## Критерии успешного этапа 0

Тест успешен только одновременно при выполнении всех условий:

1. Оба `.gproj` видны Launcher; `Validate Scripts` и `Compile and Reload Scripts` завершились без ошибок.
2. Запущен именно штатный `Conflict - Arland` с активным `AI Conflict Arland`.
3. В server/listen-server журнале есть один `BOOTSTRAP_SERVER`, один `BASE_DISCOVERY` и `GRAPH_SUMMARY` с ненулевым числом узлов.
4. Выведены все `GRAPH_NODE`; в штатной конфигурации ожидаются также `GRAPH_EDGE`.
5. Есть ровно по одному `GROUP_CREATED` для `US` и `USSR`.
6. Для обеих сторон есть `TARGET_SELECTED` и `WAYPOINT_ASSIGNED`.
7. Есть `GROUP_READY` с числом бойцов больше нуля для обеих сторон.
8. Финальная строка — `[AICF][STAGE0][RESULT][PASS]`; строк `[ERROR]` и `[RESULT][FAIL]` нет.
9. Повторный внутренний запрос, если он возникает, даёт `BOOTSTRAP_DUPLICATE_SKIPPED`, а не дополнительные `GROUP_CREATED`.

Наличие `PASS` не проверяет захват базы: это уже этап 1.

## Где находятся логи

Стандартные каталоги Windows:

```text
%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\logs
%USERPROFILE%\Documents\My Games\ArmaReforger\logs
```

В Workbench Log Console открывается клавишей `F1`. Для dedicated server используйте папку профиля/логов, заданную вашей серверной установкой; если путь неоднозначен, добавьте к запуску отдельный `-profile` и приложите этот путь к отчёту.

Не присылайте только одну отфильтрованную строку: нужна последняя целая папка логов, чтобы видеть соседние ошибки `SCRIPT`, `RESOURCES`, `RPL` и `VME`.

## Что приложить к отчёту об ошибке

- заполненную форму из [docs/STAGE_0_TESTING.md](docs/STAGE_0_TESTING.md);
- версию Arma Reforger и Reforger Tools;
- ветку и вывод `git rev-parse --short HEAD`;
- полный command line либо server JSON без паролей;
- скриншот Launcher со всеми зависимостями;
- результат `Validate Scripts` и `Compile and Reload Scripts`;
- последнюю папку логов Workbench;
- последнюю папку логов игры/server;
- все сообщения `[AICF][STAGE0]` и 20–30 строк до первой ошибки;
- время запуска и, по возможности, скриншот/видео двух появившихся групп.

## После получения новой версии проекта

Каждый раз повторяйте полный короткий цикл:

1. Сохранить логи предыдущего теста.
2. Закрыть игру и Workbench.
3. Выполнить `git pull` в выданной ветке и записать новый commit.
4. Сверить версии игры и Tools.
5. При изменении структуры повторить `Scan for Projects`.
6. Открыть `AIConflictArland`, дождаться Resource Database.
7. Повторить `Validate Scripts` и `Compile and Reload Scripts`.
8. Снова запустить штатный `Conflict - Arland` с тем же addon GUID.
9. Заполнить новую таблицу фактического результата; старый `PASS` к новой версии не переносится.

## Локальная копия официальных API для разработчика

Чтобы не скачивать Script Diff при каждом анализе, выполните из Git Bash/macOS/Linux:

```bash
./tools/fetch_reforger_api_reference.sh
```

Скрипт один раз загружает официальный snapshot 1.7.0.54 с проверкой SHA-256 в `.cache/reforger-api/`. `.cache` исключён из Git и не является зависимостью мода. Проверенные сигнатуры и ссылки перечислены в [docs/API_REFERENCE.md](docs/API_REFERENCE.md).

## Официальные материалы

- [Scripting Modding](https://community.bistudio.com/wiki/Arma_Reforger%3AScripting_Modding)
- [Mod Project Setup](https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Project_Setup)
- [Script Editor](https://community.bistudio.com/wiki/Arma_Reforger%3AScript_Editor)
- [Server Config](https://community.bistudio.com/wiki/Arma_Reforger%3AServer_Config)
- [Server Hosting](https://community.bistudio.com/wiki/Arma_Reforger%3AServer_Hosting)
- [Startup Parameters](https://community.bistudio.com/wiki/Arma_Reforger%3AStartup_Parameters)
- [Arma Reforger Script Diff 1.7.0.54](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/v1.7.0.54)
- [Official Arma Reforger Samples](https://github.com/BohemiaInteractive/Arma-Reforger-Samples)
