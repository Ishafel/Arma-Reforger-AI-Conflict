# Runtime fixture: каталог и применение

## Вооружение FIA RHS/WCS — issue #52

`AICF_FIAArmamentProbe.c` копируется в stage Core `Diagnostics/`.
`AICF_RHSFIAArmamentProbe.c` получает имя `ZZ_AICF_RHSFIAArmamentProbe.c`
в stage RHS `Content/`, после production PMC adapter. Для WCS добавляется
`ZZ_AICF_WCSFIAArmamentProbe.c` после WCS content profile. Stock использует
только общий Core probe. Ни один probe не размещается в production addon.

`-aicfFIAArmamentProbe 1`, Medium: 13 catalog roles, три AC prefabs, все
достижимые варианты, обычный цикл riflemen, живой inventory readback и
сохранение secondary. Negative cases вызывают production transaction:
invalid token, отсутствующий ресурс, реальный отказ native insertion
несовместимого prefab в weapon slot, rollback и retry. После успешной
выдачи расходуется патрон; повторный `Apply` сохраняет изменённый snapshot.
Наблюдатели реальных patrol/garrison `Board` hooks дополнительно проверяют
pending gate до посадки и подготовку экипажа/десанта. Stock сравнивает
фактические weapon/ammo inventories без RHS classes.

`Test-FIAArmamentLog.ps1 -LogPath <console.log> -Profile Stock|RHS|WCS
-LauncherPath <launcher.txt> -NativeExitPath <exit.txt>` требует полный
остановленный log, native exit 0, CLI/manifest, точные размеры 16/61/54
бойцов и 45/381/340 cases. Все 16 source prefab и каждый вариант обязательны.
Scoped negative errors сохраняются; VM/compile/fatal ошибки запрещены.
Таблицы и ограничения: [FIA_ARMAMENT.md](FIA_ARMAMENT.md), verdict и матрица
Arland/Everon/North: [TESTING.md](TESTING.md).

## Пополнение экипировки

`AICF_EquipmentProbe.c`, `AICF_EquipmentSafetyProbe.c`,
`AICF_EquipmentLifecycleProbe.c`, `AICF_EquipmentPlayerProbe.c` и
`AICF_EquipmentSearchProbe.c` копируются только в isolated stage
`AIConflictCore/Scripts/Game/AIConflict/Loadouts/`. Последние три при копировании
получают имена `AICF_ZEquipmentLifecycleProbe.c`, `AICF_ZZEquipmentPlayerProbe.c`,
`AICF_ZZZEquipmentSearchProbe.c`: общий modded class должен загрузиться первым.
В production addon этих файлов нет.

`-aicfEquipmentProbe 1|inventory|safety|counts|lifecycle|player|search` выбирает один
probe; `-aicfRequirePlayerForResult 0` сохраняет server-only сценарий активным.
Общий fixture вызывает production service/planner/applicator, создаёт настоящие
async rosters, corpses и stock/profile armory compositions. `1` проверяет
физический подход, частичный/достаточный loot, revisions, принудительный отказ
RestoreNew, тот же inventory после rollback, успешный retry и несколько
периодов без нового визита. `inventory` изолирует выдачу свежему живому actor
у реального арсенала; этот mode сам по себе не доказывает физический подход.

`counts` запускает короткую часть safety: native частичные магазины, loaded
magazine/chambers и расходники. В production counters передаются перечисления
с повторными ссылками и null: ожидается один вклад каждой entity, отдельный
вклад разных entities одного prefab, правильный low threshold и неизменность
исходного массива/боезапаса. Полный `safety` дополнительно повторяет одноразовые
launcher entities. Для отрицательного runtime regression в отдельном stage
отключается только identity filter: тот же `counts` обязан дать FAIL. Это
инъекция повторного перечисления, а не заявление, что getter всегда даёт дубли.

`safety` перечисляет enabled catalog launchers, классифицирует реальным
`EWeaponType`/muzzle/projectile/well и проверяет по одному представителю каждого
семейства: реальные transfers, вместимость, occupied slots, одноразовые трубы,
конкуренцию, threat до/во время визита, enemy/FIA corpses и удаление источника.
Fixture подготавливает native предметы у источника до production transfer;
production не создаёт их копии. Preview worlds живут до завершения использующих
их проверок. Faction/clock/threat/failure inputs контролируются fixture.

`lifecycle` использует реальные ARMORY services: nearest selection, owner loss,
delete, offline/no alternative, retry, новый manual intent, native смерть,
replacement и Stop. Recruitment/transport fences проверяются действующими
production identity records на настоящем roster; отдельная поездка не запускается.
`search` блокирует native movement получателя к телу в 25 м, проверяет реальный
45-секундный timeout без подмены arrival, radius и стабильный identity tie-break.
`player` требует direct-connect client из canonical launcher; server передаёт
реальные entities подключённому PlayerController через pinned native API.

`Test-EquipmentLog.ps1 -Mode <mode> -LogPath <полный остановленный console.log>
-OutputDirectory <evidence> [-ExpectedFamilies <count>]` проверяет обязательные
cases, количество checks, завершение server, исключения и неполный rollback;
сохраняет всю E/F-диагностику и native matrix. Static/compile/runtime/client
результаты отдельно перечислены в [TESTING.md](TESTING.md).

`AICF_AdaptiveBaselineProbe.c` + `AICF_AdaptiveProbeWorld.c` копируются в
isolated stock integration stage исходного commit. `-aicfAdaptiveBaseline 1`
сохраняет native graph/slots и controlled три допустимые цели; baseline code
не заменяется новым selector. Probe закрывается через RequestClose.

`AICF_AdaptiveCommanderProbe.c` + `AICF_AdaptiveProbeWorld.c` — stock Arland
или полный Everon stage, `-aicfAdaptiveProbe 1 -aicfRequirePlayerForResult 0`.
Вызывает production score и настоящие commander/planner, создаёт четыре
full 10/10 async rosters, проверяет callback + exact faction/alive + observation age.
Radio eligibility/threat и парные strategic inputs — synthetic; назначения,
replicated waypoint entities, native base ownership, slot readiness и fences —
реальные. Gates/expiry используют ускоренные часы; отдельный vehicle trip не
создаётся. Проверки и ограничения: [ADAPTIVE_COMMANDER.md](ADAPTIVE_COMMANDER.md).
`Test-AdaptiveCommanderLog.ps1 -LogPath <полный остановленный log> [-Baseline]`
не заменяет runtime статическим PASS. Fixture callbacks снимаются при Stop;
файлы не копируются в production.

`AICF_SocialProbe.c` и `AICF_SocialClientProbe.c` копируются вместе только в
изолированный `AIConflictArland/Scripts/Game/AIConflictArland/Integration/`.
`-aicfSocialProbe 1` проверяет production gate с управляемыми часами, четыре
native emote и policy/lifecycle на реальных initial groups. Для replacement
fixture создаёт временную казарму и пополняет supplies; это постановка теста,
не production поведение. Probe завершается через `RequestClose`.

С `-aicfSocialClientProbe 1` на server и client запускается отдельная матрица
четырёх реплик без прерывания idle, отдельного SALUTE свободного бойца, общего
cooldown и настоящего respawn игрока.
Client сам вызывает штатные faction/group/spawn APIs. Позиции тестового игрока
задаются fixture относительно бота; естественная навигация игрока не проверяется.
`-aicfSocialFaction US|USSR` выбирает сторону (default US). После завершения
первого client подключается новый профиль с `-aicfSocialJIP 1`, наблюдает 15 s
без старого audio token и закрывается; затем закрывается server.
Частота probe совпадает с production `TICK_MS=250`; callbacks снимаются при Stop.
Каждый процесс запускается отдельной terminal-сессией через canonical launcher,
свежими profile, client `-noFocus -forceUpdate` (штатные параметры PeerTool) и проверкой `ROSTER_READY`/manifest. Verdict берётся из полных
остановленных logs, а не из одного успешного вызова API.

Для регрессии client idle exit дополнительно копируется
`AICF_SocialProbeExit.c` (имя обеспечивает загрузку после `AICF_SocialProbe.c`).
Live matrix отменяет каждый из четырёх emotes через strategic assignment и
проверяет сохранение waypoint. Дополнительный случай отменяет вход в Pushups
через 500 ms. Клиент наблюдает `IsLoitering` перед отменой и через 1500 ms
проверяет отсутствие tag и scripted command; `Test-SocialLog.ps1 -ClientExit`
требует все пять наблюдений и server order cases. Предварительная server matrix
с немедленным запуском следующего emote не входит в эти client observations.
Animation tag — терминальная проверка графа, не визуальная оценка blending.

`AICF_SocialProbeWeapon.c` добавляется к двум основным social fixtures;
`-aicfSocialProbe 1 -aicfSocialWeaponProbe 1` включает отдельную матрицу оружия.
После выбора допустимого бойца fixture приостанавливает его AI agent и движение,
вооружает штатной винтовкой/пулемётом и вновь проверяет production `Available`.
Так последующий AI weapon task не может замаскировать дефект восстановления.
Проверяются четыре idle deadline (часы deadline ускорены), strategic assignment,
угроза, отмена сразу после запроса, уже безоружный боец и Stop. Через 4000 ms
требуется тот же entity в руках и завершённый item change; AI затем возобновляется.
С `-aicfSocialClientProbe 1` probe ждёт штатный spawn клиента и дополнительно
сравнивает replicated weapon identity для девяти случаев. Оба процесса закрываются
автоматически. `Test-SocialWeaponLog.ps1 -LogPath ... [-ClientLogPath ...]` требует
34 server assertions, полные остановленные logs и все девять client observations.

`AICF_MenuPriceProbe.c` вместе с `AICF_MenuTransactionProbe.c` копируются только
в изолированный RHS stage `Scripts/Game/AIConflictArlandRHS/Integration/`.
`-aicfMenuPriceProbe 1` после ROSTER_READY проверяет 67 native BUILDING групп
и 48/122 vehicle spawner entries RHS/WCS обеих сторон; сохраняет реальные
prefab/index/roster, старые native и новые цены. Проверяет UI total, own/children,
group debit budget, spawner, повторные и накопленные чтения, CopyFrom и возврат
цены после переключения active profile. Смена profile — локальная проверка
getters, не отдельный runtime stock. Закрывает сервер через RequestClose.

`-aicfMenuVehicleMeasure 1` дополнительно создаёт локальный экземпляр каждой
машины, рекурсивно снимает реальные compartments и weapon entities, сразу
удаляет их. Измерение включает inventory weapons, не только mounted armament;
source resource traversal отдельно сохраняет ammo/ERA. Не измеряет бронепробитие.
`-aicfMenuTransactions 1` использует реальные HQ providers обеих сторон:
native preflight при supplies `price-1` отказывает; при точном запасе разрешает,
очистка accumulated budget отменяет preflight без расхода. Затем создаёт одну
группу и одну машину каждой стороны с temporary provider и наблюдает изменение
физического supply pool непосредственно внутри native entity-core callback.
Наблюдение между кадрами недостаточно: HQ может начислить доход. Каждый случай
требует ровно одно списание полной цены; fixture удаляет свои entities/agents
и восстанавливает исходные supplies, cleanup повторяется при Stop.

Для client/JIP серверу передать `-aicfMenuPriceProbe client` (окно 60 с после
проверки), клиенту — `-aicfMenuPriceProbe 1`. Клиент запускается отдельной
терминальной сессией через canonical launcher с живым ServerProfileRoot;
после выбора profile читает тот же native каталог и закрывается. Повторный
CallLater снимается в OnGameEnd. Не запускать вместе с другой закрывающей
сервер fixture. Перед применением подтвердить Workbench compile.

`Test-MenuPriceLog.ps1` описан в [TOOLS.md](TOOLS.md). Native budget/debit probe
не подтверждает пользовательские menu/RPC, vehicle-factory PerformSpawn,
ошибку создания после подтверждения, интерактивную отмену или refund action.
Визуальные цены/доступность, эти транзакции и боевой баланс требуют отдельных
ручных проверок; терминальная каталогизация их не заменяет.

`AICF_MatchWeatherProbe.c`, `-aicfWeatherProbe baseline|random`: отдельный Core
stage любого Variant, `-aicfWeatherHeaders '<resource>;<resource>;...'` передаёт
доступные в его addon graph headers. Наблюдает штатные SetupDaytimeAndWeather
и ForceWeatherTo, не подменяя погоду, PRNG или часы. Проверяет время после
инициализации против header, дату, day duration и auto advance; один выбор,
фактическое применение погоды, сохранение естественной смены состояний,
наследование headers и отсутствие повторного выбора при дублирующем Start.
После ROSTER_READY наблюдает ещё 10 секунд clock самого weather manager и
проверяет скорость часов, затем завершает сервер через RequestClose.
`baseline` — 12 проверок исходного кода; `random` — 13, включая выбранное состояние.
`tests/log-audits/Test-MatchWeatherLog.ps1 -LogPath <полные остановленные логи>`
проверяет каждый case; `-Baseline` выбирает старый контракт, `-RequireVariation`
требует минимум две различные погоды среди новых матчей. Повторы случайной
погоды допустимы; для доказательства вариативности может понадобиться ещё запуск.
Не подтверждает client/JIP, визуальный вид погоды, ночную смену ускорения или
длительный погодный цикл. Если bootstrap/roster не готов, probe не завершится:
launcher/process необходимо контролировать отдельным терминальным watchdog.

`AICF_AntiTankProbe.c`, `-aicfAntiTankProbe 1`: отдельный Core stage, северный
Everon, Hard. Создаёт одного ПТ-бойца USSR, переносит его и готовый FIA БТР
на выбранную линию 350 м, назначает defend waypoint стрелку, отключает damage
стрелка и AI экипажа мишени; физика БТР остаётся в `COLLISION`, чтобы он не
скатывался после переноса. Штатный selector и perception остаются включены.
Фиксирует распознавание, native/effective range и реальное событие
`OnWeaponFired` с projectile и текущей целью. Завершается после выстрела либо
через 180 тиков после ROSTER_READY; успешное раннее завершение также требует
проверок aim/move. `AICF_ATProbeAim` сравнивает допуски на одних и тех же
реальных observer/target с подключённой и отключённой policy dependency;
винтовочный допуск должен совпасть. `AICF_ATProbeMove` вызывает production task
на отдельном `SCR_AICombatMoveState`: ожидание до 10 с и запрос 2 с с остановкой
для прицеливания. Запрос не отправляется живому AI и не доказывает проходимость
маршрута. Callback и weapon invoker снимаются при
завершении/Stop. Сам расход магазина не считается выстрелом.
`Test-AntiTankLog.ps1 -LogPath ...` требует фактический projectile на 300–400 м
и полный остановленный лог, а также оба runtime contract markers. Попадания,
движущаяся цель, client/JIP — отдельные
проверки; этот probe их не подтверждает. Политика: [ANTI_TANK_TACTICS.md](ANTI_TANK_TACTICS.md).

`AICF_FIARandomGarrisonProbe.c`, `-aicfRandomGarrisonProbe full|plan`: текущий probe
issue #25 для любого Variant/сложности. В отдельном stage проверяет production
plan, все три composition rules/fallback, детерминизм PRNG и контрольную
последовательность, synthetic directed graph (HQ, оба направления ребра, тупик,
изолированный узел, отсутствие кандидата), повторный Start и immutable snapshot.
Регрессии PR #26 проверяют граф с пригодным кандидатом, но изолированной целью,
HQ только с входящими рёбрами и HQ с выходом только к другому HQ. Положительный
контроль — связный граф с обоими HQ exits; `LIVE_GRAPH_READY` проверяет весь
реальный Hard graph после integration setup.
`full` ждёт actual READY/посадки всех выбранных мест (deadline 360 с), `plan`
проверяет план сразу, но закрывает сервер только на первом Update после
`ROSTER_READY`, до spawn гарнизонов. Закрытие прямо из `Start` воспроизводило
shutdown leak `BridgeRoad` во время незавершённой инициализации. Отключает damage готовых
машин/бойцов; не доказывает бой, движение, client/JIP или soak.
Анализ полного остановленного лога: `Test-FIARandomGarrisonLog.ps1 -LogPath ...`
(для `plan` добавить `-PlanOnly`). Целевой probe и общий SCRIPT/fatal gate
выводятся отдельно, общий exit сохраняет любые ошибки.

Дополнения для regression issue #25, применяются только вместе с этим probe:

- `AICF_MagazineGuardProbe.c` в Core `Diagnostics/`: вызывает production
  magazine task с отсутствующим access и на фактических занятых местах без
  weapon manager; ожидает FAIL без VM exception и сообщает число проверенных мест.
- `AICF_RHSPMCGarrisonProbe.c` в RHS `Content/`, после `AICF_RHSPMCEquipment.c`
  по порядку имён: проверяет armament всех готовых бойцов, отсутствие новых
  посадок при pending, а также принудительный pending у одного готового бойца.
  Fault injection восстанавливает исходный флаг сразу после вызова; не меняет
  inventory, identity и seats. Требует full с непустым гарнизоном для покрытия.
- `AICF_BridgeLifecycleProbe.c` отдельно в Arland `Integration/`:
  `-aicfBridgeProbe native|native-settled|native-ended` отключает AICF bootstrap,
  сравнивает закрытие после первого кадра, после 10 с и после `EndGameMode`.
  Это диагностическая изоляция lifecycle; остальные scripts addon остаются
  загруженными, поэтому результат не называется запуском без модов.

Следующие три старых probes (`DifficultyRollout`, `FIAGarrison`,
`FIAGarrisonRecovery`) и `Test-FIAGarrisonLog.ps1` описывают исторический
фиксированный состав 0.1.26 (5/10 машин, 7 пассажиров). Для случайного состава
issue #25 они не являются актуальными gates; их сохранённые результаты ниже
нельзя переносить на новую реализацию. Для новых запусков использовать probe выше.

`AICF_DifficultyRolloutProbe.c`: отдельный stage любого Variant,
`-aicfDifficultyProbe 0|1|2 -aicfDifficultyHeaders '<Easy>;<Medium>;<Hard>'`.
Загружает три native MissionHeader, проверяет inherited difficulty и flags,
число запланированных гарнизонов по фактическому objective graph, default роли
US/USSR и каждый готовый экипаж (три essential, нет десанта при выключенном
флаге). После общей готовности наблюдает 60 с, требует хотя бы одно прибытие
патруля. Deadline 300 с; ожидание spawn не считается успехом. Отключает damage
готовых машин/экипажей. Не доказывает движение каждого БТР, бой, UI или soak.

`AICF_DifficultyInfantryProbe.c`, `-aicfATProbe 1`: аналог пехотного probe ниже
для любого content profile, без ссылок на RHS/WCS классы. Проверяет 24 бойца,
включая donors 8/9, фактические prefab/роли и заряженные ракетные установки.
Не запускать одновременно с `AICF_InfantryATProbe` или другим probe, который
сам закрывает сервер. Параметр роли определяется заголовком текущего сценария.

`AICF_InfantryATProbe.c`, `-aicfATProbe 1`: отдельный WCS+RHS stage,
North Easy/Medium/Hard. После штатного ROSTER_READY создаёт по одному полному
отряду каждой стороны, по два one-member donor (индексы 8/9) и US donors 6/7 через production
resolver и async spawn. Проверяет все 26 prefab/роли/WCS-комплекта, 1/3/3 ПТ
в каждом полном отряде, заряженное оружие `WT_ROCKETLAUNCHER` у каждого ПТ,
точный M2 Carl Gustaf у всех US LAT/TL/AMG с одним зарядом без запасных,
неизменность inventory при повторном ApplyWCS у US 6/7,
соответствие stock/RHS mappings выбранной сложности и отказ для FIA. Пишет фактические prefab,
loaded/spare и завершает сервер через RequestClose. Не моделирует оплату,
переход donor в боевой отряд, стрельбу, client/JIP или пользовательский шаблон.

`AICF_USExtraLauncherProbe.c`, `-aicfExtraLATProbe 1`: отдельный WCS+RHS stage.
После ROSTER_READY строит 20 стандартных draft, проверяет комплекты всех ролей,
точный M2 Carl Gustaf с одним зарядом без запасных во втором слоте US LAT/TL/AMG,
прежний пустой слот РФ 6/7 и полную signature после Capture/Restore всех
комплектов US с гранатомётом (32 checks на Medium/Hard, 28 на Easy).
Печатает слоты всех ролей и завершает сервер. Не включать одновременно с другими
самозавершающимися probes; клиент, стрельба и file Save не проверяются.

`AICF_FIAGarrisonDamageProbe.c`, `-aicfDamageRecoveryProbe damage|identity`:
только отдельный stage последнего WCS addon, Hard North WCS+RHS. После production
plan выбирает детерминированные танк/БТР из FIA catalog, сохраняя production
spawn/boarding. После READY приостанавливает scheduler и фоновые damage handlers;
для тестового воздействия включает damage и вызывает настоящий HandleDamage
(TRUE, 5% выбранного hit zone), Kill и DeleteRplEntity. Результат recovery
не подменяет. Режим damage проверяет корпус/двигатель обеих машин, ранение,
лечение/ремонт между ticks, отдельную гибель driver/gunner/commander/passenger,
retry/leg, обычную высадку и positive recovery. Режим identity проверяет
missing/stale/replaced members, unknown observer/roster, stale vehicle/generation,
synthetic player veto, 49/50/51 м, отсутствие кандидата и настоящий урон,
инъецированный после Candidate/на player fence перед forced boarding.
Завершает сервер RequestClose, deadline 420 с. Анализ полного остановленного
лога: `Test-FIAGarrisonRecoveryLog.ps1 -LogPath <console.log> -DamageMode damage|identity`.
Наблюдения после первого ban сверяются с numeric_slot/generation/vehicle;
никаких последующих vehicle/crew teleport events быть не должно. Не проверяет
реального connected player, client/JIP и исходный игровой эпизод пользователя.

`AICF_FIAGarrisonRecoveryProbe.c` — историческая матрица,
`-aicfRecoveryProbe 1`: только отдельный stage,
Hard North WCS+RHS. Проверяет metadata трёх мест экипажа, геометрию границы
50 м, synthetic player veto без мутаций, перенос исходной машины,
возврат на каждое из трёх мест БТР и танка с последующим переносом,
высадку семи пассажиров после искусственного threat, возобновление патруля,
recovery после искусственного failed move, запрет при stale member identity
и отсутствие возврата/замены убитого водителя.
Отключает damage проверяемых гарнизонов; завершает сервер через RequestClose,
deadline 480 с. Synthetic veto не доказывает discovery настоящего connected
player и репликацию клиенту; искусственный threat не доказывает огневую
эффективность. Результаты запусков и ограничения — в TESTING.md.

`AICF_FIAGarrisonProbe.c` — новый probe сложности: параметр
`-aicfGarrisonProbe 0|1|2` задаёт ожидаемый уровень из header. На северном WCS+RHS
проверяет 0/5/10 машин, полную посадку, 240 с движения каждого гарнизона минимум на 25 м и минимум два прибытия.
Допускается до 250 м при развороте/возврате после порога 200 м; в конце
интервала машина обязана находиться внутри 200 м. Затем проверяет сохранение
гарнизона после программного захвата и одной боевой потери. Сам завершает сервер
через `RequestClose()`, deadline 420 с. Запускается только в отдельном stage.
В этой изолированной проверке отключает damage машин и экипажей после READY,
чтобы бой кампании не подменял тест навигации. Затем изменяет владельца первой
базы, включает damage одного fixture-owned бойца и убивает его.
Анализ: `tests/log-audits/Test-FIAGarrisonLog.ps1 -LogPath <console.log> -Difficulty 0|1|2`.
Это targeted lifecycle probe, не доказательство стрельбы, client/JIP или полного боя.

Все отслеживаемые probes перенесены в `tests/fixtures/` без изменения содержимого.
Статус при реорганизации: **DIAGNOSTIC / runtime NOT RUN**. Таблица описывает
назначение, но не обещает актуальный compile или прохождение сценария. Старое
evidence смотри в профильных заметках; новый запуск требует собственных логов.

Перед применением прочитай весь файл, его `modded class`, CLI guards и связанные
probes. Не копируй каталог целиком: модификации могут конфликтовать, использовать
одинаковые CLI flags или завершать игру. Отсутствие CLI флага в таблице не означает,
что fixture безопасна без него: некоторые hooks действуют с момента загрузки.

Порядок: отдельная копия нужного addon graph в `.codex-runtime/<задача>/source`,
только нужные probes в соответствующем `Scripts/Game`, terminal Workbench compile
для stage, затем canonical `tools/Start-AICFRuntime.ps1 -RepositoryRoot <stage>`.
Server и client используют согласованный stage. Для RHS/WCS hooks нужен именно
их integration addon. `PresetRespawnProbe` требует добавления в указанный файл,
а не произвольного копирования отдельного `.c`. Generated databases и evidence
не коммитятся. После остановки сохрани полные логи и убери свою source copy.

Параметры ниже извлечены из `GetCLIParam`: это имена, **не готовый argv**.
Значения/зависимости определяются исходником: не всем параметрам подходит `1`,
а динамические суффиксы (`Position`, `Angles`, `Work`) требуют номера case.
Для recruitment есть [точные сценарии и границы](RECRUITMENT_SUPPLY_PLANNING.md),
для WCS — [integration guide](../AIConflictArlandWCSRHS/README.md).

| Fixture | Назначение и ограничения | CLI параметры в исходнике |
|---|---|---|
| [AICF_AICombatInputProbe.c](../tests/fixtures/AICF_AICombatInputProbe.c) | Пустые weapon inputs; временно снимает selected-weapon reference | `aicfCombatInputProbe` |
| [AICF_ApproachRecoveryProbe.c](../tests/fixtures/AICF_ApproachRecoveryProbe.c) | Полные отряды, approach recovery и bounded shutdown | `aicfRecoveryProbe` |
| [AICF_BarracksCombatProbe.c](../tests/fixtures/AICF_BarracksCombatProbe.c) | Подготовка казарм/supplies и размещение лидеров; не доказывает подход | `aicfBarracksCombatProbe` |
| [AICF_BaseBuilderCompletionProbe.c](../tests/fixtures/AICF_BaseBuilderCompletionProbe.c) | Отказ completion на 45 секунд с последующей native clearance | `aicfBuilderCompletionProbe` |
| [AICF_BaseBuilderRuntimeProbe.c](../tests/fixtures/AICF_BaseBuilderRuntimeProbe.c) | Worker lifecycle; подготовка layouts обходит оплату/player placement | `aicfBuilderProbe` |
| [AICF_BuilderMovementProbe.c](../tests/fixtures/AICF_BuilderMovementProbe.c) | Инъекция BT result в группу строителя; проверка fencing и возврата | `aicfBuilderMovementProbe` |
| [AICF_ConstructionAllBasesProbe.c](../tests/fixtures/AICF_ConstructionAllBasesProbe.c) | Все точки одной стороне и полный supply pool; production стройка | `aicfAllBasesProbe`, `aicfAllBasesProbeMs` |
| [AICF_ConstructionBudgetProbe.c](../tests/fixtures/AICF_ConstructionBudgetProbe.c) | Общая квота без geometry calls; дополнительные параметры баз/radius | `aicfConstructionBudgetProbe`, `aicfConstructionProbeRadius`, `aicfConstructionProbeUSBase`, `aicfConstructionProbeUSSRBase` |
| [AICF_ConstructionCatalogProbe.c](../tests/fixtures/AICF_ConstructionCatalogProbe.c) | Registry, labels и budgets; не полная геометрия | `aicfConstructionCatalog` |
| [AICF_ConstructionManualProbe.c](../tests/fixtures/AICF_ConstructionManualProbe.c) | Диагностика ручного проекта без отдельного CLI guard | Нет собственного GetCLIParam; читай hooks |
| [AICF_ConstructionReplayProbe.c](../tests/fixtures/AICF_ConstructionReplayProbe.c) | Повтор geometry guards на переданных transforms, без gameplay spawn | `aicfConstructionReplay`, `aicfConstructionReplayAngles`, `aicfConstructionReplayPosition`, `aicfConstructionReplayProvider`, `aicfConstructionReplayValidOnly`, `aicfConstructionReplayWork` |
| [AICF_ConstructionRuntimeProbe.c](../tests/fixtures/AICF_ConstructionRuntimeProbe.c) | Подготовка supplies/матрицы; production поиск, оплата и завершение | `aicfConstructionClientProbe`, `aicfConstructionProbe`, `aicfConstructionProbeFault`, `aicfConstructionProbeMatrix`, `aicfConstructionProbeMatrixPhaseMs`, `aicfConstructionProbeMatrixType`, `aicfConstructionProbeMs`, `aicfConstructionProbeRefill`, `aicfConstructionProbeRepeatType`, `aicfConstructionProbeSupplies`, `aicfConstructionProbeTrace`, `aicfConstructionProbeType` |
| [AICF_ConstructionSearchProbe.c](../tests/fixtures/AICF_ConstructionSearchProbe.c) | Trace и fault injection поиска; применяется с runtime preparation | `aicfConstructionProbeMoveArmories`, `aicfConstructionProbeRemoveArmories`, `aicfConstructionSearchFault`, `aicfConstructionSearchTrace` |
| [AICF_DefendArrivalProbe.c](../tests/fixtures/AICF_DefendArrivalProbe.c) | Late move result при US system hold; commander USSR | `aicfDefendArrivalProbe` |
| [AICF_DefendWaypointInputProbe.c](../tests/fixtures/AICF_DefendWaypointInputProbe.c) | Реальные group/waypoint, смена приказа через planner | `aicfDefendInputProbe` |
| [AICF_EndgameProbe.c](../tests/fixtures/AICF_EndgameProbe.c) | Контролируемые territory/presence/hold time; настоящее завершение матча | `aicfEndgameProbe` |
| [AICF_EveronNorthProbe.c](../tests/fixtures/AICF_EveronNorthProbe.c) | Северный Everon: 7 активных баз, 2 HQ, 5 механизмов захвата, связность radio graph; Кермован исключён | `aicfNorthProbe` |
| [AICF_ForcedSmallBarracksProbe.c](../tests/fixtures/AICF_ForcedSmallBarracksProbe.c) | Отказ обычного коридора поиска, forced-small fallback | `aicfForcedSmallProbe` |
| [AICF_GroupCallsignProbe.c](../tests/fixtures/AICF_GroupCallsignProbe.c) | Три поколения групп в двух factions; group spawn и callsign uniqueness | `aicfCallsignProbe` |
| [AICF_InfantryAdvanceProbe.c](../tests/fixtures/AICF_InfantryAdvanceProbe.c) | Production infantry advance и подтверждение перемещения | `aicfInfantryAdvanceProbe` |
| [AICF_InfantryApproachProbe.c](../tests/fixtures/AICF_InfantryApproachProbe.c) | Approach navigation и bounded recovery | `aicfApproachProbe` |
| [AICF_InfantryRecruitmentRuntimeProbe.c](../tests/fixtures/AICF_InfantryRecruitmentRuntimeProbe.c) | Recruitment подготовка, fault/muster режимы; основа связанных probes | `aicfRecoveryEpisodeProbe`, `aicfRecruitHiddenContinue`, `aicfRecruitProbe`, `aicfRecruitProbeFaults`, `aicfRecruitProbeMuster` |
| [AICF_InfantrySpawnPlacementProbe.c](../tests/fixtures/AICF_InfantrySpawnPlacementProbe.c) | Native spawn placement и границы inputs | `aicfSpawnPlacementProbe` |
| [AICF_LoadoutClientProbe.c](../tests/fixtures/AICF_LoadoutClientProbe.c) | Клиентские loadout preview/validation; визуальный verdict требует пользователя | `aicfLoadoutClientProbe`, `aicfLoadoutClientProbeKeepOpen`, `aicfLoadoutPreviewOnly` |
| [AICF_LoadoutClothingProbe.c](../tests/fixtures/AICF_LoadoutClothingProbe.c) | Клиентские/серверные проверки одежды и inventory; stage обоих peers | `aicfClothingProbe` |
| [AICF_LoadoutMagazineProbe.c](../tests/fixtures/AICF_LoadoutMagazineProbe.c) | Magazines, ammo и runtime inventory | `aicfMagazineProbe` |
| [AICF_LoadoutNavigationProbe.c](../tests/fixtures/AICF_LoadoutNavigationProbe.c) | Loadout navigation regression; одноимённый CLI есть у другого probe | `aicfNavigationProbe` |
| [AICF_LoadoutContentsProbe.c](../tests/fixtures/AICF_LoadoutContentsProbe.c) | Общий список, точные адреса, remove/Undo, restore/validate и save/readback; требует ClothingProbe в том же stage, завершает сервер | `aicfContentsProbe` |
| [AICF_LoadoutWeaponCategoriesProbe.c](../tests/fixtures/AICF_LoadoutWeaponCategoriesProbe.c) | Реальные stock/WCS primary weapons, два native slots, полное разбиение без потерь и перестановок, контрольные prefab и пересечение с source; после ROSTER_READY завершает сервер | `aicfWeaponCategoriesProbe` |
| [AICF_LoadoutRuntimeProbe.c](../tests/fixtures/AICF_LoadoutRuntimeProbe.c) | Runtime выдача/проверка loadout; optional recruit/hold | `aicfLoadoutProbe`, `aicfLoadoutProbeHoldSeconds`, `aicfLoadoutRecruitProbe` |
| [AICF_LocalizationProbe.c](../tests/fixtures/AICF_LocalizationProbe.c) | Runtime локализация и форматирование строк | `aicfLocalizationProbe` |
| [AICF_LogisticsFallbackProbe.c](../tests/fixtures/AICF_LogisticsFallbackProbe.c) | Инъекция logistics fallback; используется с logistics runtime setup | `aicfLogisticsProbeFallback` |
| [AICF_LogisticsMapMarkerProbe.c](../tests/fixtures/AICF_LogisticsMapMarkerProbe.c) | Отображаемое состояние logistics markers без ручного visual PASS | `aicfLogisticsMarkerProbe` |
| [AICF_LogisticsRecoveryProbe.c](../tests/fixtures/AICF_LogisticsRecoveryProbe.c) | Runtime recovery logistics; совместно с LogisticsRuntimeProbe | `aicfLogisticsRecoveryProbe` |
| [AICF_LogisticsRuntimeProbe.c](../tests/fixtures/AICF_LogisticsRuntimeProbe.c) | Маршрут/спрос/supplies, доставка и client evidence; подготовка меняет мир | `aicfLogisticsClientProbe`, `aicfLogisticsClientSpawnFaction`, `aicfLogisticsProbe`, `aicfLogisticsProbeDepotAtSource`, `aicfLogisticsProbeDurationMs`, `aicfLogisticsProbeMaintainDemand`, `aicfLogisticsProbeMinSourceCapacity`, `aicfLogisticsProbePeace`, `aicfLogisticsProbePrepare`, `aicfLogisticsProbeRepeatSource`, `aicfLogisticsProbeReturn`, `aicfLogisticsProbeRouteMatrix`, `aicfLogisticsProbeStopAfterUralDelivery`, `aicfLogisticsRecoveryProbe` |
| [AICF_LogisticsSpawnClearanceProbe.c](../tests/fixtures/AICF_LogisticsSpawnClearanceProbe.c) | Clearance, slot selection/reservations вместе с LogisticsRuntimeProbe | Нет собственного GetCLIParam; читай hooks |
| [AICF_LogisticsSpawnParityProbe.c](../tests/fixtures/AICF_LogisticsSpawnParityProbe.c) | Depot parity вместе с LogisticsRuntimeProbe; direct setup обходит часть construction guards | `aicfParityDirectDepotSetup` |
| [AICF_LogisticsVehicleLimitProbe.c](../tests/fixtures/AICF_LogisticsVehicleLimitProbe.c) | Vehicle cap и leases | `aicfVehicleLimitProbe` |
| [AICF_ManualSupplyClientProbe.c](../tests/fixtures/AICF_ManualSupplyClientProbe.c) | Manual supply RPC/конкурентные intents на клиенте | `aicfLogisticsClientSpawnFaction`, `aicfManualSupplyProbe`, `aicfManualSupplyProbeConcurrent`, `aicfManualSupplyProbeStartStep` |
| [AICF_ManualSupplyProbeContracts.c](../tests/fixtures/AICF_ManualSupplyProbeContracts.c) | Enforce контракты manual supply helper, не самостоятельный PowerShell test | `aicfManualSupplyProbeContracts` |
| [AICF_MovementRecoveryProbe.c](../tests/fixtures/AICF_MovementRecoveryProbe.c) | Movement recovery и identity fencing | `aicfMovementRecoveryProbe` |
| [AICF_MusterTurretRuntimeProbe.c](../tests/fixtures/AICF_MusterTurretRuntimeProbe.c) | Muster при занятой turret; runtime подготовка | `aicfMusterTurretProbe` |
| [AICF_NativeMoveFailureProbe.c](../tests/fixtures/AICF_NativeMoveFailureProbe.c) | Сбор native movement failure; перемещение бойцов — подготовка | `aicfNativeMoveProbe` |
| [AICF_NavigationRecoveryProbe.c](../tests/fixtures/AICF_NavigationRecoveryProbe.c) | Координатный snapshot 2026-09-19; не универсальный тест карты | `aicfNavigationProbe` |
| [AICF_NorthFailureProbe.c](../tests/fixtures/AICF_NorthFailureProbe.c) | Прямая подача failed movement inputs; wiring портов проверяется отдельно статикой | `aicfNorthFailureProbe` |
| [AICF_InfantryMoveHandlerProbe.c](../tests/fixtures/AICF_InfantryMoveHandlerProbe.c) | Девять живых бойцов, реальная свободная машина с subgroup `-1`, посадка/выход и новый Move/waypoint/assignment. Поздний registered sentinel не должен менять новый приказ/action или вызывать failure callback; positive control без registry entry и stale failure fences; инъекция `UNKNOWN` через production helper, native BT wiring отдельно; 25 проверок, завершает сервер | `aicfInfantryMoveHandlerProbe` |
| [AICF_PersonalLoadoutProbe.c](../tests/fixtures/AICF_PersonalLoadoutProbe.c) | Personal catalog/recipe validation; не имитирует UI/player/JIP | `aicfPersonalLoadoutProbe` |
| [AICF_LoadoutPerformanceProbe.c](../tests/fixtures/AICF_LoadoutPerformanceProbe.c) | Сравнение Find/List со старой реализацией и замеры каталога без UI; после ROSTER_READY завершает сервер | `aicfLoadoutPerformanceProbe` |
| [AICF_PersonalNetworkProbe.c](../tests/fixtures/AICF_PersonalNetworkProbe.c) | WCS native owner RPC без управления UI | `aicfPersonalNetworkProbe`, `aicfProbeFaction`, `aicfProbePersonal` |
| [AICF_PresetRespawnProbe.c](../tests/fixtures/AICF_PresetRespawnProbe.c) | Добавляется в staging AICF_SquadRespawnUI.c; создаёт recruit и убивает test characters | `aicfPresetProbeFaction`, `aicfPresetRespawnProbe` |
| [AICF_RecoveryEpisodeProbe.c](../tests/fixtures/AICF_RecoveryEpisodeProbe.c) | Episode, hidden continuation и player release; требует чтения связанных recruitment fixtures | `aicfPlayerReleaseProbeOnly`, `aicfRecoveryEpisodeProbe`, `aicfRecruitHiddenContinue`, `aicfRecruitHiddenProbe`, `aicfRecruitMultiMemberProbe` |
| [AICF_RecoverySoakProbe.c](../tests/fixtures/AICF_RecoverySoakProbe.c) | Production gameplay с ограничением длительности и проверкой terminal hold | `aicfRecoverySoakMs` |
| [AICF_RecruitmentLifecycleProbe.c](../tests/fixtures/AICF_RecruitmentLifecycleProbe.c) | Native ownership/death/replacement; перемещает seed к казармам, не проверяет навигацию | `aicfRecruitmentLifecycleProbe` |
| [AICF_RecruitmentMapProbe.c](../tests/fixtures/AICF_RecruitmentMapProbe.c) | Synthetic slot с настоящими faction/base identities | `aicfRecruitmentMapProbe` |
| [AICF_RecruitmentStabilityProbe.c](../tests/fixtures/AICF_RecruitmentStabilityProbe.c) | Вместе с LifecycleProbe; управляемый income snapshot и стабильность replan | `aicfRecruitmentStabilityProbe` |
| [AICF_RecruitmentSupplyProbe.c](../tests/fixtures/AICF_RecruitmentSupplyProbe.c) | Шесть конкурирующих отрядов; stock казармы/transactions, test supplies | `aicfRecruitmentSupplyProbe` |
| [AICF_RecruitmentPriorityProbe.c](../tests/fixtures/AICF_RecruitmentPriorityProbe.c) | Три настоящих отряда на одной базе: очередь, ручное повышение, preemption и оплата; задаёт роли/запас/доход/позиции и приостанавливает стратегический loop; не проверяет UI/RPC игрока или навигацию | `aicfRecruitmentPriorityProbe` |
| [AICF_RecruitmentSequenceProbe.c](../tests/fixtures/AICF_RecruitmentSequenceProbe.c) | Две US базы и USSR: последовательные реальные покупки, 1→10, FIFO, manual, timeout/cancel/stale; управляемые часы и production forecast. Копировать в integration addon после Core. Задаёт admission/позиции/запас/доход, не доказывает естественную навигацию | `aicfRecruitmentSequenceProbe` |
| [AICF_RemainingErrorsProbe.c](../tests/fixtures/AICF_RemainingErrorsProbe.c) | Диагностика identity/parent/physics; отсутствие CLI не гарантирует бездействие | `aicfRemainingProbeSeconds` |
| [AICF_RHSPatchesProbe.c](../tests/fixtures/AICF_RHSPatchesProbe.c) | RHS роли, inventory, повторная выдача и разнообразие patches | `aicfPatchProbe` |
| [AICF_RHSPMCScenarioProbe.c](../tests/fixtures/AICF_RHSPMCScenarioProbe.c) | RHS AC охрана без исходного primary и FIA стрелок; без отдельного CLI guard | Нет собственного GetCLIParam; читай hooks |
| [AICF_RHSWardrobeShowcase.c](../tests/fixtures/AICF_RHSWardrobeShowcase.c) | RHS экипировка: режим 1 — ручной осмотр, 2 — smoke с остановкой | `aicfWardrobeShowcase` |
| [AICF_SquadCommandsProbe.c](../tests/fixtures/AICF_SquadCommandsProbe.c) | Barracks/движение/покупка/planner; не доказывает client RPC/UI | `aicfSquadProbe` |
| [AICF_SquadRespawnProbe.c](../tests/fixtures/AICF_SquadRespawnProbe.c) | Server/client possession pipeline; подготовка AI и смерть игрока | `aicfSquadRespawnProbe` |
| [AICF_StuckRouteWaitProbe.c](../tests/fixtures/AICF_StuckRouteWaitProbe.c) | Зависшее ожидание stock ALL; planner/watchdog/timer остаются production | `aicfStuckWaitProbe` |
| [AICF_VictoryRespawnProbe.c](../tests/fixtures/AICF_VictoryRespawnProbe.c) | Казарма/смерть/баланс задаются fixture; production replacement/victory | `aicfVictoryRespawnProbe` |
| [AICF_WCSAccessProbe.c](../tests/fixtures/AICF_WCSAccessProbe.c) | Native filters арсенала/heavy provider; общий trigger с WCSInfantry | `aicfWCSInfantryProbe` |
| [AICF_WCSCatalogProbe.c](../tests/fixtures/AICF_WCSCatalogProbe.c) | Merged catalogs WCS без клиента | `aicfWCSInfantryProbe`, `aicfWCSProbe` |
| [AICF_WCSInfantryProbe.c](../tests/fixtures/AICF_WCSInfantryProbe.c) | Два полных roster через production async spawn | `aicfWCSInfantryProbe` |
| [AICF_WCSLoadoutCatalogProbe.c](../tests/fixtures/AICF_WCSLoadoutCatalogProbe.c) | Каталог и источники WCS, не visual UI | `aicfWCSLoadoutCatalogProbe` |
| [AICF_WCSPlayerDeployProbe.c](../tests/fixtures/AICF_WCSPlayerDeployProbe.c) | Fault injection Restore на synchronous case, WCS deploy/rollback | `aicfWCSPlayerDeployProbe` |

## Synthetic logs

[physical-delivery.fixture](../tests/fixtures/logistics/physical-delivery.fixture) и
[client-loaded.fixture](../tests/fixtures/logistics/client-loaded.fixture) — входы
[Test-LogisticsContracts.ps1](../tests/contracts/Test-LogisticsContracts.ps1).
Это искусственные server/client events для положительных и отрицательных
проверок анализатора; они не являются evidence реального runtime.
