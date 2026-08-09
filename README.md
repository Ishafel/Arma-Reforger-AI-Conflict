# Arma Reforger AI Conflict

Scripts-only прототип автономного AI Conflict для Arma Reforger. Проект расширяет штатный Conflict на Arland, не копирует и не изменяет vanilla-мир и выполняет игровую логику только на authoritative server/master.

## Текущий статус

| Этап | Статус | Что это означает |
|---|---|---|
| Stage 0 — исследовательский прототип | **PASS** | Ручной direct Diag dedicated-тест на Arland завершился `[AICF][STAGE0][RESULT][PASS]`: обнаружены штатные базы и граф, созданы US/USSR-группы, обе получили цели и waypoint |
| Stage 1 — пехотный вертикальный срез | **PASS** | Подтверждены 4+4 группы, роли `2/1/1`, AI-захват и retarget, безопасные подкрепления, билеты, групповые маркеры и клиентский runtime-прогон |
| Stage 2 — надёжность и баланс | **кандидат реализован** | Lifecycle-аудит, восстановление приказов, stuck-watchdog, spawn/load guard, внешний CLI-конфиг и headless soak; runtime-матрица учитывается отдельно |
| Stage 3 — наземная техника | **условно завершён / принят заочно** | Владелец проекта зафиксировал текущую реализацию в `main` как завершение этапа с продолжением анализа. Исторические Transport T1–T9 и Armed A1 остаются `FAIL`, известные дефекты не закрываются административно; post-T9-патч сохраняет stuck-группу в поле, разделяет crew/mobility budget и выполняет bounded safe vehicle-unstuck с обязательным post-recovery motion/progress evidence. Static audit и Workbench validation пройдены |
| Stage 3.5 — Active Motorized Forces | **запланирован** | Четыре группы по пять бойцов на сторону; `3 ATTACK / 1 forward DEFEND-QRF`; одна вместительная машина на каждый managed slot; controlled matrix и soak до экономики |
| Полный MVP | **не готов к приёмке** | После Stage 2 остаются стандартная MVP-матрица, клиентская синхронизация и полный 30-минутный прогон |
| Двухчасовой soak | **не запускался** | Выполняется отдельно только после успешной полной MVP-матрицы |

Подтверждённые Stage 0 и Stage 1 относятся к своим проверенным commit. Stage 2 продолжает отдельную runtime-проверку. Stage 3 принят владельцем проекта заочно на текущем snapshot: это управленческая фиксация этапа, а не отмена исторических `FAIL` или замена оставшихся Transport/Armed/soak-проверок.

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

Stage 1 реализует серверные модели конфигурации, ролей, слотов и билетов, live-выбор целей, role-aware приказы, безопасный spawn, ticket reservation, репликацию билетов, билетную победу и групповые маркеры на карте.

## Что входит в Stage 2

- spawn-generation и защита от повторной привязки группы;
- периодический lifecycle-аудит всех устойчивых слотов;
- автоматическое восстановление потерянного waypoint без recovery-churn у активной цели;
- измерение прогресса по живому лидеру, перестроение маршрута и штатная платная замена устойчиво застрявшей группы;
- компактная stock-формация `Column` для управляемых групп без телепортации бойцов;
- ограничение одновременных replacement-spawn и общей численности AI;
- настройка темпа войны и reliability-порогов через CLI;
- отдельный контракт `[AICF][STAGE2]` и расширенное состояние групповых map-маркеров;
- воспроизводимый fault-injection прогон и длительный headless soak.

## Что входит в Stage 3

- отдельная конфигурация `AICF_Stage3Config`, выключенная по умолчанию через `aicfVehiclesEnabled=0`;
- одна транспортная ATTACK-группа на сторону в первом срезе и отдельное опциональное лёгкое вооружённое отделение во втором;
- серверное создание техники только после детерминированного обхода всех безопасных friendly-баз: contested/enemy state, допустимая дистанция, пустой участок и расстояние каждого живого бойца до точной позиции проверяются до `SpawnEntityPrefabEx`;
- перед посадкой измеряется дистанция каждого живого бойца: при `farthest_m > aicfVehicleMaximumReuseDistanceMeters` группа продолжает пешком, а при `farthest_m > 75 м` каждый удалённый живой участник получает собственный MOVE-only `SCR_AIMoveIndividuallyBehavior` к реальной машине с action radius не больше 70 м; в `APPROACH` нет group Move-waypoint, vehicle utility и GetIn, progress отслеживается отдельно по каждому бойцу, stalled action через 15 секунд переиздаётся не более одного раза, а завершение требует двух settled poll всей группы в радиусе 75 м;
- перед `DRIVER` и повторно после `APPROACH` синхронно нормализуется уже занятое неверное место; затем водитель и опциональный стрелок садятся через точные `SCR_AIGetInVehicle` action на заранее зарезервированные Pilot/Turret compartment, а машина подключается к group utility и получает `CARGO_ONLY` только после обязательных crew-фаз; после высадки или fallback `NormalizeAfterVehicle` очищает временные vehicle movement handlers и восстанавливает штатную компактную пехотную формацию;
- план фаз фиксируется один раз для текущей попытки и включает только действительно нужные `APPROACH`, `DRIVER`, опциональный `GUNNER` и обязательный `PASSENGERS`: максимум 3 фазы для transport и 4 для armed-light; у каждой фазы есть soft timeout, на всю попытку допускается ровно одна target-scoped grace длительностью 10 секунд при подтверждённом физическом прогрессе/входе, а phase и total hard cap не продлеваются повторно;
- `BOARDING_COMPLETE` требует двух последовательных poll, в которых каждый живой участник физически settled именно в этой машине;
- высадка на настраиваемой дистанции от цели и восстановление прежнего пехотного приказа без изменения правил `CaptureRelay`;
- отдельный vehicle watchdog для boarding timeout, отсутствия водителя/стрелка, отсутствия прогресса, переворота, повреждения, разделения группы и физической очистки кузова после высадки;
- ограниченная последовательность восстановления: обязательный экипаж считается восстановленным только после повторной проверки полного набора ролей; далее возможны перестроение маршрута, высадка, abandoned/cleanup и продолжение пешком;
- атомарный лимит машин на фракцию, поколения группы и машины, защита от двойного spawn и stale runtime;
- retryable spawn-request делает не более четырёх попыток с экспоненциальным backoff до 60 секунд; затем переходит в `WAITING_FOR_SITE`, сохраняет активный пехотный приказ и освобождает AI vehicle cap. Периодический site probe и смена target/base revision пробуждают запрос с новой полной диагностикой без 10-секундного state/log churn; recoverable `APPROACH_LOST/STALL/TIMEOUT_APPROACH`, `VEHICLE_TOO_FAR` и `GROUP_COHESION` также возвращают запрос в ожидание без assignment suppression;
- исправная abandoned-машина снимается с управления Stage 3 и active AI cap, но остаётся в мире доступной игрокам в отдельном faction world pool с safety-first soft target 4; при переполнении координатор пишет `VEHICLE_WORLD_POOL_SOFT_OVERFLOW`, ищет oldest-safe retirement, pool может временно превысить target и никогда не уменьшается небезопасным удалением при player/interaction/proximity gate;
- нормализация `SAFE_REUSE`: старые vehicle actions/compartment reservations очищаются до новой role-ordered посадки; occupant без живого водителя проходит ограниченный `BOARDING_ROLE_RESET → BOARDING_ROLE_RETRY`, а не продолжает driver-фазу как пассажир;
- dismount-clearance объединяет logical compartment link, get-in/get-out transition и положение персонажа внутри bounds машины; completion требует два последовательных чистых poll, а логически вышедший, но физически застрявший боец получает ограниченное relocation наружу;
- одноразовый `DISEMBARK_REISSUED` на половине deadline и отдельный `DISEMBARK_TIMEOUT` на полном deadline;
- удаление непригодной или явно выбранной для retirement машины разрешено только после отсутствия protected occupants, target-scoped player transitions и живых игроков в радиусе 15 м в течение непрерывных 5 секунд; непосредственно перед destructive call выполняется повторный scan. Cleanup сначала пишет `VEHICLE_DELETE_REQUESTED` с `EntityID`/`RplId`, подтверждает исчезновение entity на authority и только затем пишет `VEHICLE_CLEANUP_CONFIRMED`; визуальный client despawn всё ещё требует корреляции тех же ID в Armed A2;
- при остановке координатора cleanup не обходит safety-contract: one-shot poll раз в секунду до 60 секунд пытается получить те же 15 м/5 с stable-clear и проверяет `EntityID`/`RplId`; identity mismatch, неподтверждённое удаление или недоступный clear заканчиваются fail-closed `VEHICLE_STOP_CLEANUP_RETAINED`, а не принудительным delete;
- пехотный recovery считается выданным через `ORDER_RECOVERY_ISSUED`, а `ORDER_RECOVERED` появляется лишь после трёх наблюдений одного и того же waypoint одновременно как exact current и элемента очереди, причём стабильность длится не меньше `max(10 с, 2 × reliability interval)`; после исчерпания bounded stuck budget та же полевая группа получает локальный hold на достигнутой позиции и позже повторяет текущую операцию без удаления, MOB-респавна или списания билета;
- `RESULT_CANDIDATE status=READY final=0` вместо mid-run PASS; последующий boarding/dismount/role defect защёлкивает `ACCEPTANCE_FAILURE_LATCHED` и инвалидирует кандидата до финальной проверки полного журнала;
- состояние `VEH <state>` в игровом групповом маркере и отдельный лог-контракт `[AICF][STAGE3]`.

Stage 3 условно завершён и принят владельцем проекта заочно на текущем snapshot; дальнейший анализ продолжается. Ручные Transport T1–T9 и Armed A1 сохраняют исторический статус `FAIL`, а подробные T8/T9-отчёты — в `docs/STAGE_3_TESTING.md`: известные дефекты не считаются исчезнувшими. Post-T9-патч сохраняет persistent-stuck группу и её target на достигнутой позиции, возобновляет операцию после bounded hold или изменения карты, пишет агрегированный результат проверки каждой невражеской spawn-базы и фиксирует current AI action каждого бойца во время посадки. Crew-role recovery больше не расходует и не сбрасывает mobility watchdog; исправная неподвижная машина получает bounded authority-only reposition с obstacle/water/mine/character/player guards, а успех подтверждается только последующим самостоятельным motion либо route progress. Ранее добавленные bounded `WAITING_FOR_SITE`, functional world pool и 15-метровый/5-секундный delete gate сохранены. Штатные мины Arland не удалялись и не менялись. Текущий snapshot прошёл `tools/Test-Stage3Static.ps1` и Workbench 1.7 `Validate Scripts` по пяти конфигурациям: `.cache/stage3-post-t9-vehicle-unstuck-final2-20260809/console.log`, Game CRC32 `946e5a78`, `Script validation successful`, `SCRIPT (E/F)=0`. Transport T10, Armed A2, controlled no-combat boarding repeat, focused timeout/order/cleanup/cap fault-срезы и 30-минутный прогон остаются post-acceptance проверками.

## Что планируется в Stage 3.5

- увеличить initial и replacement-группы с трёх до пяти faction-correct бойцов;
- сохранить четыре slot на фракцию, но использовать их как `3 ATTACK / 1 forward DEFEND-QRF`, без постоянно простаивающего резерва на MOB;
- выдать каждой группе собственный грузовик или вместительный лёгкий транспорт, выбираемый по числу доступных мест для всей живой группы;
- поднять безопасную нижнюю границу managed AI budget, проверить 40 AI и восемь active vehicles;
- сохранить действующие bounded boarding/recovery/fallback, `SAFE_REUSE`, world-pool и player-safe cleanup-инварианты;
- завершить controlled 30-минутную матрицу и двухчасовой soak до Stage 4.

Полный план и критерии: [Stage 3.5 — Active Motorized Forces](docs/STAGE_3_5_ACTIVE_FORCES.md).

## Что ещё не заявлено готовым

- полная runtime-приёмка Stage 2: fault-injection, 30-минутная матрица и двухчасовой soak;
- стандартный 30-минутный MVP-прогон;
- синхронизация билетов и состояния матча на нескольких клиентах;
- проверка смерти и повторного развёртывания игрока;
- окончательная настройка баланса и темпа войны;
- runtime-приёмка наземной техники, расширенная логистика, сохранение состояния и пользовательский интерфейс;
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
│       ├── State/
│       ├── UI/
│       └── Vehicles/
├── AIConflictArland/
│   ├── addon.gproj
│   └── Scripts/Game/AIConflictArland/Integration/
├── docs/
│   ├── API_REFERENCE.md
│   ├── STAGE_0_TESTING.md
│   ├── STAGE_1_TESTING.md
│   ├── STAGE_2_TESTING.md
│   └── STAGE_3_TESTING.md
├── tools/
│   ├── Test-Stage2Log.ps1
│   ├── Test-Stage3Static.ps1
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
git switch codex/stage-3-ground-vehicles
git rev-parse --short HEAD
```

Текущая локальная ветка разработки — `codex/stage-3-ground-vehicles`. Если она ещё не опубликована, используйте переданную рабочую копию или конкретный commit от разработчика. Commit обязательно записывается отдельно для каждого теста.

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

Групповые маркеры являются частью текущего игрового процесса и не требуют CLI-флага. На карте всегда отображаются восемь глобальных маркеров, следующих за живыми лидерами managed AI-групп, с цветом стороны, устойчивым слотом, полной ролью, текущей задачей, целью, числом живых бойцов, route recovery и состоянием техники. После гибели лидера маркер автоматически перепривязывается к новому лидеру. На текущем этапе все клиенты видят US и USSR; следующий этап видимости должен оставить игроку только союзную фракцию.

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

Stage 2 и Stage 3 дополняют тот же `run` отдельными префиксами:

```text
[AICF][STAGE2]
[AICF][STAGE3]
```

Отфильтрованный вывод не заменяет полный журнал. К отчёту прикладывается вся папка `$profileRoot\logs`, чтобы сохранить соседние `SCRIPT`, `RESOURCES`, `RPL` и `VME`-ошибки.

## Руководства по тестированию

- [Stage 0: инструкция приёмочного тестирования](docs/STAGE_0_TESTING.md) — контракт исследовательского прототипа и расшифровка подтверждённого `[AICF][STAGE0][RESULT][PASS]`.
- [Stage 1: пехотный вертикальный срез](docs/STAGE_1_TESTING.md) — direct Diag-команды, профиль `4 × 4`, временные и причинные инварианты, два ускоренных запуска и итоговая матрица.
- [Stage 2: надёжность и баланс](docs/STAGE_2_TESTING.md) — fault injection, lifecycle/order/load инварианты, 30-минутная headless-матрица и двухчасовой soak.
- [Stage 3: наземная техника](docs/STAGE_3_TESTING.md) — транспортный и вооружённый срезы, crew/boarding, safe spawn, watchdog, fallback, лимиты и регрессия Stage 2.
- [Stage 3.5: Active Motorized Forces](docs/STAGE_3_5_ACTIVE_FORCES.md) — группы `4 × 5`, активные роли `3/1`, транспорт каждого slot, capacity policy и нагрузочная приёмка.

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

## Порядок приёмки Stage 3

1. Выполнить Workbench Validate/Compile для `AIConflictCore` и `AIConflictArland`, затем `tools/Test-Stage3Static.ps1`.
2. Доказать транспортный срез с `aicfVehiclesEnabled=1`, но без вооружённых машин: safe spawn, полный экипаж, посадка, движение, высадка и пехотный захват.
3. Отдельно воспроизвести boarding timeout, потерю водителя, отсутствие прогресса, переворот/повреждение и убедиться в конечном пешем fallback без дублей.
4. После PASS транспорта включить одну вооружённую лёгкую машину на сторону и проверить стрелка, потерю экипажа и тот же fallback-контракт.
5. Проверить лимит техники, cleanup, отсутствие роста сущностей и отдельный регрессионный запуск с `aicfVehiclesEnabled=0`.

Строка `[AICF][STAGE3][RESULT_CANDIDATE] ... status=READY final=0` означает только достижение автоматизируемых инвариантов первых configured-поездок. Она не является автоматическим PASS и может быть позднее инвалидирована `ACCEPTANCE_FAILURE_LATCHED`. Текущий Stage 3 принят владельцем проекта заочно; техническая матрица по-прежнему требует полного server/client log, ручной проверки и видео/скриншотов и используется для дальнейшего анализа регрессий.

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
