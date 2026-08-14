# Arma Reforger AI Conflict

Scripts-only прототип автономного AI Conflict для Arma Reforger. Проект расширяет штатный Conflict на Arland, не копирует и не изменяет vanilla-мир и выполняет игровую логику только на authoritative server/master.

## Текущий статус

| Этап | Статус | Что это означает |
|---|---|---|
| Stage 0 — исследовательский прототип | **PASS** | Ручной direct Diag dedicated-тест на Arland завершился `[AICF][STAGE0][RESULT][PASS]`: обнаружены штатные базы и граф, созданы US/USSR-группы, обе получили цели и waypoint |
| Stage 1 — пехотный вертикальный срез | **PASS** | Подтверждены 4+4 группы, роли `2/1/1`, AI-захват и retarget, безопасные подкрепления, билеты, групповые маркеры и клиентский runtime-прогон |
| Stage 2 — надёжность и баланс | **кандидат реализован** | Lifecycle-аудит, восстановление приказов, stuck-watchdog, spawn/load guard, внешний CLI-конфиг и headless soak; runtime-матрица учитывается отдельно |
| Stage 3 — наземная техника | **FAILED — rewrite implemented, runtime acceptance not passed** | Vehicle-domain rewrite и atomic cutover завершены, legacy runtime/coordinator fragments удалены. Это development implementation status, не PASS: Transport T1–T9 и Armed A1 остаются историческими `FAIL`, Transport T10 и Armed A2 — `NOT RUN` |
| Stage 3.5 — Active Motorized Forces | **FAILED — rewrite implemented, runtime acceptance not passed** | Rewrite завершён вместе со Stage 3, но gameplay не дошёл до runtime-приёмки. Оба Transport-прогона `Stage35-T-20260810-210932` и `Stage35-T-20260811-190311` остаются `FAIL`; preliminary repeat smoke остаётся `BLOCKED`; Repeat T, Repeat-T2 и B/P/A/R/L/M30/S120 остаются `NOT RUN` |
| Полный MVP | **не готов к приёмке** | После Stage 2 остаются стандартная MVP-матрица, клиентская синхронизация и полный 30-минутный прогон |
| Двухчасовой soak | **не запускался** | Выполняется отдельно только после успешной полной MVP-матрицы |

Подтверждённые Stage 0 и Stage 1 относятся к своим проверенным commit. Stage 2 продолжает отдельную runtime-проверку. Новое решение владельца отменяет прежнюю заочную фиксацию Stage 3: implementation/cutover vehicle-domain rewrite завершены, однако Stage 3/3.5 сохраняют статус `FAILED`, потому что runtime acceptance не пройдена. Это не отменяет требования этапов, не обесценивает уже полученное положительное evidence и не переклассифицирует ни один исторический `PASS`, `FAIL`, `BLOCKED` или `NOT RUN`.

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
- перед `DRIVER` и повторно после `APPROACH` синхронно нормализуется уже занятое неверное место; водитель и опциональный стрелок садятся через точные `SCR_AIGetInVehicle` action на заранее зарезервированные Pilot/Turret compartment. После settled обязательного crew машина может подключиться к group utility, но пассажирского group waypoint нет: до первой passenger action BoardingFlow атомарно сопоставляет каждому живому пассажиру exact `CargoCompartmentSlot`, reservation и owner-token, затем выдаёт отдельный `SCR_AIGetInVehicle`; после высадки или fallback handoff очищает owned vehicle handlers и восстанавливает штатную компактную пехотную формацию;
- план фаз фиксируется один раз для текущей попытки и включает только действительно нужные `APPROACH`, `DRIVER`, опциональный `GUNNER` и обязательный `PASSENGERS`: максимум 3 фазы для transport и 4 для armed-light; у каждой фазы есть soft timeout, на всю попытку допускается ровно одна target-scoped grace длительностью 10 секунд при подтверждённом физическом прогрессе/входе, а phase и total hard cap не продлеваются повторно;
- `BOARDING_COMPLETE` требует двух последовательных poll, в которых каждый живой участник физически settled именно в этой машине;
- высадка на настраиваемой дистанции от цели и восстановление прежнего пехотного приказа без изменения правил `CaptureRelay`;
- отдельный vehicle watchdog для boarding timeout, отсутствия водителя/стрелка, отсутствия прогресса, переворота, повреждения, разделения группы и физической очистки кузова после высадки;
- ограниченная последовательность восстановления: обязательный экипаж считается восстановленным только после повторной проверки полного набора ролей; далее возможны перестроение маршрута, высадка, abandoned/cleanup и продолжение пешком;
- атомарный лимит машин на фракцию, identity/generation Trip и Lease/vehicle, fencing каждого callback/action/reservation и защита от double spawn, stale callback и ABA;
- retryable spawn-request делает не более четырёх попыток с экспоненциальным backoff до 60 секунд; затем переходит в `WAITING_FOR_SITE`, сохраняет активный пехотный приказ и освобождает AI vehicle cap. Периодический site probe и смена target/base revision пробуждают запрос с новой полной диагностикой без 10-секундного state/log churn; recoverable `APPROACH_LOST/STALL/TIMEOUT_APPROACH`, `VEHICLE_TOO_FAR` и `GROUP_COHESION` также возвращают запрос в ожидание без assignment suppression;
- исправная abandoned-машина через `FactionFleet` освобождает lease/active AI cap и остаётся доступной игрокам в отдельном faction world pool с safety-first soft target 4; `VehicleCleanupManager` диагностирует overflow и обрабатывает oldest-safe retirement, но pool может временно превысить target и никогда не уменьшается небезопасным удалением при player/interaction/proximity gate;
- нормализация `SAFE_REUSE`: старые vehicle actions/compartment reservations очищаются до новой role-ordered посадки; occupant без живого водителя проходит ограниченный `BOARDING_ROLE_RESET → BOARDING_ROLE_RETRY`, а не продолжает driver-фазу как пассажир;
- normal dismount проверяет logical compartment link, get-in/get-out transition и положение персонажа внутри bounds машины; физически застрявший боец до timeout получает только bounded per-member movement guidance, а exact eject/relocation разрешён исключительно в bounded terminal/fallback fail-closed recovery;
- одноразовый `DISEMBARK_REISSUED` на половине deadline и отдельный `DISEMBARK_TIMEOUT` на полном deadline; прекращение vehicle control немедленно запускает восстановление meaningful infantry order. `order_restored` и `clearance_safe` независимы: pending occupant/transition/bounds/player clearance блокирует только release/delete машины и не задерживает продолжение живой current-группы пешком;
- удаление непригодной или явно выбранной для retirement машины разрешено только после отсутствия protected occupants, target-scoped player transitions и живых игроков в радиусе 15 м в течение непрерывных 5 секунд; непосредственно перед destructive call выполняется повторный scan. Cleanup сначала пишет `VEHICLE_DELETE_REQUESTED` с `EntityID`/`RplId`, подтверждает исчезновение entity на authority и только затем пишет `VEHICLE_CLEANUP_CONFIRMED`; визуальный client despawn всё ещё требует корреляции тех же ID в Armed A2;
- при остановке facade `VehicleCleanupManager` не обходит safety-contract: fenced one-shot job раз в секунду до 60 секунд пытается получить те же 15 м/5 с stable-clear и проверяет полную Trip/Lease/vehicle/`EntityID`/`RplId` identity; identity mismatch, неподтверждённое удаление или недоступный clear заканчиваются fail-closed `VEHICLE_STOP_CLEANUP_RETAINED`, а не принудительным delete;
- пехотный recovery считается выданным через `ORDER_RECOVERY_ISSUED`, а `ORDER_RECOVERED` появляется лишь после трёх наблюдений одного и того же waypoint одновременно как exact current и элемента очереди, причём стабильность длится не меньше `max(10 с, 2 × reliability interval)`; после исчерпания bounded stuck budget та же полевая группа получает локальный hold на достигнутой позиции и позже повторяет текущую операцию без удаления, MOB-респавна или списания билета;
- `RESULT_CANDIDATE status=READY final=0` вместо mid-run PASS; последующий boarding/dismount/role defect защёлкивает `ACCEPTANCE_FAILURE_LATCHED` и инвалидирует кандидата до финальной проверки полного журнала;
- состояние `VEH <state>` в игровом групповом маркере и отдельный лог-контракт `[AICF][STAGE3]`.

Ранее заочно зафиксированный Stage 3 snapshot теперь признан владельцем `FAILED`; его архитектурная замена реализована и atomic cutover завершён. Ручные Transport T1–T9 и Armed A1 сохраняют исторический статус `FAIL`, а подробные T8/T9-отчёты — в `docs/STAGE_3_TESTING.md`: известные дефекты не считаются исчезнувшими. Post-T9-патч сохраняет persistent-stuck группу и её target на достигнутой позиции, возобновляет операцию после bounded hold или изменения карты, пишет агрегированный результат проверки каждой невражеской spawn-базы и фиксирует current AI action каждого бойца во время посадки. Crew-role recovery больше не расходует и не сбрасывает mobility watchdog; исправная неподвижная машина получает bounded authority-only reposition с obstacle/water/mine/character/player guards, а успех подтверждается только последующим самостоятельным motion либо route progress. Ранее добавленные bounded `WAITING_FOR_SITE`, functional world pool и 15-метровый/5-секундный delete gate сохранены как обязательные контракты rewrite. Штатные мины Arland не удалялись и не менялись. Прежний snapshot прошёл `tools/Test-Stage3Static.ps1` и Workbench 1.7 `Validate Scripts` по пяти конфигурациям: `.cache/stage3-post-t9-vehicle-unstuck-final2-20260809/console.log`, Game CRC32 `946e5a78`, `Script validation successful`, `SCRIPT (E/F)=0`; это историческое development evidence, не runtime PASS новой архитектуры. Transport T10, Armed A2, controlled no-combat boarding repeat, focused timeout/order/cleanup/cap fault-срезы и 30-минутный прогон остаются `NOT RUN` и должны быть выполнены для runtime acceptance rewrite.

Финальный dirty-working-tree rewrite snapshot прошёл обе обязательные static-команды с negative-fixture self-check и полный Workbench validate: `.cache/vehicle-rewrite-final-validate-20260812-r2/console.log`, Game `5692` files / `11109` classes, CRC32 `7f2cbec0`, `Game successfully created`, `SCRIPT(E/F)=0`, `ENGINE(F)=0`. Полный Workbench log также сохраняет два harness `PLATFORM(E)` (`SteamAPI_Init failed`/platform services) и 25 `RESOURCES(E)` строк shutdown resource-leak list, всего 27 generic `(E)/(F)` matches; это platform/shutdown-resource caveat, не AICF script compile error и не runtime gameplay evidence. Final-tree headless smoke `.cache/Stage35-Rewrite-FinalSmoke-20260812-002210` был `BLOCKED` внешним backend до AICF bootstrap (`BACKEND(E)=12`, SSL peer certificate/`BAD_REQUEST`; `AICF=0`), поэтому не дал roster/vehicle evidence и не является Repeat T, Repeat-T2 или M30.

Первый post-cutover runtime на Reforger 1.8 выявил отдельную Stage 1 regression: старый direct `SCR_AIGroup.SpawnUnits()` потерял все initial member attempts до первого world frame. Production path перенесён на 1.8 `SCR_AIWorld` queue через bind/subscription → `RequestSpawn(5)`, а timeout получил pre-cleanup per-group/per-member snapshot. Vehicles-off dedicated smoke `.cache/stage1-roster-request-smoke-20260813-r3` подтвердил восемь `GROUP_ROSTER_READY` по `5/5` и общий `ROSTER_READY` за 5.986 с без AICF/SCRIPT/ENGINE failures. Это снимает initial roster blocker, но не тестирует vehicle phases и не меняет статус Stage 3/3.5 `FAILED / NOT ACCEPTED`. Полная форензика: [Stage 1 spawn regression после Reforger 1.8 cutover](docs/STAGE_1_SPAWN_CUTOVER_2026-08-13.md).

## Что реализовано в Stage 3.5 rewrite

Ниже зафиксированы перенесённые требования и положительные результаты прежней реализации. Их реализация в новом vehicle-domain не означает runtime-приёмку и не меняет исторические результаты.

- initial и replacement-группы формируются из stock faction roster ровно по пять бойцов и переходят в `READY` только после проверки `5/5` и faction каждого участника;
- четыре stable numeric slot отображаются как `A0/A1/A2/D0`; штатная схема — `3 ATTACK / 1 forward DEFEND-QRF`, а CLI baseline сохраняет прежние роли `2/1/1` при том же размере группы;
- ATTACK-цели ранжируются детерминированно как primary/secondary/support, а D0 выбирает безопасную передовую friendly-базу и переходит в QRF при contested/HQ threat/потере соседней базы;
- QRF escalation выполняется сразу, возврат и смена передовой позиции ограничены stable-candidate hysteresis и minimum dwell;
- все четыре slot vehicle-eligible; A0/A1 предпочитают faction truck, A2/D0 — unarmed light M998/UAZ-452 с обязательным capacity preflight и truck fallback;
- active/reserved cap равен четырём на фракцию, не более одного lease на slot; functional world pool учитывается отдельно;
- сохранены bounded boarding/recovery/fallback, `SAFE_REUSE`, generation/identity guards и player-safe cleanup-инварианты Stage 3;
- нижняя граница `aicfMaxManagedAgents` поднята до 48 при standard 64; heartbeat показывает фактические agents, managed waypoint/entity, per-slot activity и раздельные vehicle/world-pool counters.

Полный контракт: [Stage 3.5 — Active Motorized Forces](docs/STAGE_3_5_ACTIVE_FORCES.md). Незаполненная runtime-матрица: [Stage 3.5 — приёмочное тестирование](docs/STAGE_3_5_TESTING.md).

## Что ещё не заявлено готовым

- полная runtime-приёмка Stage 2: fault-injection, 30-минутная матрица и двухчасовой soak;
- стандартный 30-минутный MVP-прогон;
- синхронизация билетов и состояния матча на нескольких клиентах;
- проверка смерти и повторного развёртывания игрока;
- окончательная настройка баланса и темпа войны;
- runtime-приёмка наземной техники, расширенная логистика, сохранение состояния и пользовательский интерфейс;
- controlled runtime-матрица Stage 3.5, 30-минутный headless-прогон и двухчасовой soak;
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

Stage 2, Stage 3 и Stage 3.5 дополняют тот же `run` отдельными префиксами:

```text
[AICF][STAGE2]
[AICF][STAGE3]
[AICF][STAGE3.5]
```

Отфильтрованный вывод не заменяет полный журнал. К отчёту прикладывается вся папка `$profileRoot\logs`, чтобы сохранить соседние `SCRIPT`, `RESOURCES`, `RPL` и `VME`-ошибки.

## Руководства по тестированию

- [Stage 0: инструкция приёмочного тестирования](docs/STAGE_0_TESTING.md) — контракт исследовательского прототипа и расшифровка подтверждённого `[AICF][STAGE0][RESULT][PASS]`.
- [Stage 1: пехотный вертикальный срез](docs/STAGE_1_TESTING.md) — direct Diag-команды, профиль `4 × 4`, временные и причинные инварианты, два ускоренных запуска и итоговая матрица.
- [Stage 2: надёжность и баланс](docs/STAGE_2_TESTING.md) — fault injection, lifecycle/order/load инварианты, 30-минутная headless-матрица и двухчасовой soak.
- [Stage 3: наземная техника](docs/STAGE_3_TESTING.md) — транспортный и вооружённый срезы, crew/boarding, safe spawn, watchdog, fallback, лимиты и регрессия Stage 2.
- [Stage 3.5: Active Motorized Forces](docs/STAGE_3_5_ACTIVE_FORCES.md) — нормативный контракт групп `4 × 5`, активных ролей `3/1`, транспорта каждого slot и capacity policy; rewrite implementation/cutover завершены, но статус этапа остаётся `FAILED` до runtime acceptance.
- [Stage 3.5: приёмочное тестирование](docs/STAGE_3_5_TESTING.md) — незаполненные срезы B/P/T/A/R/L, M30 и S120, команды, evidence-таблицы и правила результата.

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

Строка `[AICF][STAGE3][RESULT_CANDIDATE] ... status=READY final=0` означает только достижение автоматизируемых инвариантов первых configured-поездок. Она не является автоматическим PASS и может быть позднее инвалидирована `ACCEPTANCE_FAILURE_LATCHED`. Прежняя заочная фиксация Stage 3 отменена новым решением владельца; rewrite implementation/cutover завершены, но этап остаётся `FAILED`, а техническая матрица по-прежнему требует полного server/client log, ручной проверки и видео/скриншотов.

## Локальная копия официального API

Текущие production-сигнатуры 1.8.0.10 и отдельно помеченное историческое evidence 1.7.0.54 перечислены в [docs/API_REFERENCE.md](docs/API_REFERENCE.md). Чтобы загрузить официальный Script Diff в локальный, исключённый из Git кэш:

```bash
./tools/fetch_reforger_api_reference.sh
```

## Официальные материалы

- [Scripting Modding](https://community.bistudio.com/wiki/Arma_Reforger%3AScripting_Modding)
- [Mod Project Setup](https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Project_Setup)
- [Script Editor](https://community.bistudio.com/wiki/Arma_Reforger%3AScript_Editor)
- [Server Hosting](https://community.bistudio.com/wiki/Arma_Reforger%3AServer_Hosting)
- [Startup Parameters](https://community.bistudio.com/wiki/Arma_Reforger%3AStartup_Parameters)
- [Arma Reforger Script Diff 1.8.0.10](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/tree/v1.8.0.10)
- [Official Arma Reforger Samples](https://github.com/BohemiaInteractive/Arma-Reforger-Samples)
