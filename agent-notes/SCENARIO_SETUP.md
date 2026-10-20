# Универсальное меню AI Conflict

Один пункт **AI Conflict** в штатном меню открывает настройку перед Play или
Host. Это игровой экран, отдельного лаунчера нет.

## Путь игрока

1. Выбрать AI Conflict → Play либо Host с обычными сетевыми настройками.
2. Выбрать Арланд, Эверон или Север Эверона; Vanilla, RHS или WCS + RHS.
3. Выбрать Conflict или Co-op. В Co-op флаг определяет сторону всех игроков:
   US/СССР, USMC/МСВ РФ либо НАТО/РФ соответственно контенту. В Conflict
   обе стороны доступны позднее в штатном deployment.
4. Выбрать сложность гарнизонов. Справа — пояснение варианта под курсором/фокусом,
   внизу — сводка параметров. Начать операцию загружает её; Создать сессию
   использует прежние настройки сети/пароля/player limit. Отмена закрывает экран.
   При отказе запуска выбор сохраняется.

WCS-only профиля нет: интеграция WCS требует RHS. Серые варианты означают
отсутствие зависимостей, неготовый local revision или несовместимость с
принудительными параметрами запуска. Экран ничего не скачивает. После установки
модов нужно переоткрыть настройку. Флаги США/СССР используют stock texture GUID,
РФ — native tricolor widgets, НАТО — стилизованный компас на синем поле.

Подключающиеся игроки используют обычный браузер/direct connect и штатные
Welcome/faction/group UI. Новый экран не перенастраивает текущую сессию и не
показывается JIP-клиенту. Co-op — [COOP_SCENARIO.md](COOP_SCENARIO.md), найм —
[GAMEPLAY.md](GAMEPLAY.md). Автостройка своей стороны в Co-op отключена;
рабочие и заказы бойцов в казарме сохранены.

## Каталог и расширение

`tools/config/AICF_Scenarios.json` — единый источник CLI и frontend targets:
девять сочетаний карты/контента, семь dependency graphs, 81 leaf header.
`mapIndex`/`contentIndex` задают текущую матрицу 3×3.
`tools/Update-AICFScenarioRegistry.ps1 [-Check]` генерирует
`AIConflictArland/.../Integration/AICF_ScenarioVariants.c`: точный addon CSV,
owner, метки сторон, девять ресурсов Standard/CoopUS/CoopUSSR × Easy/Medium/Hard.
Generated `.c` отслеживается; ручная правка будет обнаружена `-Check`.

Core `AICF_ScenarioRegistry` хранит generic descriptors. Integration расширяет
виртуальный instance `Build`, без static dispatch ловушки и второго controller.
Карты, world, GUID targets и dependency IDs не переносятся в Core. Новому
integration нужны профиль, thin headers и запись каталога; добавление нового
измерения меню требует расширить три текущих колонки/строки, generator validation
и тестовую матрицу. Неизвестный Workshop мод сам по названию профилем не становится.

`AICF_ScenarioVariant.Resolve` проверяет bounds, девять targets и owner в графе.
`HasDependencies` требует каждый ID. `IsAvailable` использует native
`GameProject.GetAvailableAddons`; при наличии Workshop local revision требует
Downloaded, отсутствие corruption и IsReadyToRun. Source addon может иметь
placeholder WorkshopItem без revision: тогда native GameProject подтверждает
локальное наличие. Пустой scan не считается наличием модов.

`AICF_ScenarioSetupSelection` хранит карту/контент/режим/сложность. Ошибочный
выбор сохраняет прошлый. Defaults: Арланд / Vanilla / Co-op USSR / Medium,
если запуск не закрепил несовместимый контент. При смене карты контент
сохраняется либо выбирается первый допустимый набор в стабильном порядке.

## Native launch boundary

`AICF_ScenarioSetupEntry` перехватывает только собственный header subtype в
`SCR_ScenarioUICommon.TryPlayScenario/TryHostScenario`; остальные сценарии идут
в super с исходными аргументами. Header читается по `scenario.Id()`:
`MissionWorkshopItem.GetHeader()` может быть null при наличии native metadata.

`AICF_ScenarioLaunch.CanStart` заново проверяет main menu, отсутствие перехода,
native CanPlay/CanHost входного MissionWorkshopItem, hosting platform gate,
пустую download queue и зависимости выбранного descriptor. Неготовый набор
не запускается; download dialog не запускает по ошибке default entry.

Play передаёт leaf resource и **точный** addon CSV в штатный
`GameStateTransitions.RequestScenarioChangeTransition`, используемый native
`PlayGameConfig`. Host передаёт SCR_DSConfig в `RequestPublicServerTransition`:
меняет только scenarioId, hostedScenarioModId и mods с локальными версиями,
при отказе возвращает прежние значения. Network/password/player limits и
gameProperties сохраняются. Setup и native menus закрываются после принятия.

Старый прототип вызывал leaf MissionWorkshopItem.Play/Host. Универсальному
варианту это не подходит: header незагруженной интеграции ещё не обязан быть
в native каталоге, а загруженные RHS/WCS меняют content profile factory.
Разрешения проверяются на реальном входном item, выбранные зависимости —
отдельно; загрузкой/перекомпиляцией занимается native transition. Gameplay,
authority, roster и replicated state здесь не меняются.

Обнаруженный при native local Play startup defect исправлен отдельно от меню:
Welcome останавливает SP simulation, а прежний timeout группы считал 30 секунд
через System.GetTickCount. `AICF_GroupSlot` теперь сохраняет WorldTimestamp при
BeginInitialSpawn/BeginReplacementSpawn; controller сравнивает
GetSpawnSimulationAgeMs с прежними 30000 ms. Пауза не считается зависанием,
wall-clock getters и существующие поля audit сохранены. Identity/readiness,
native RequestSpawn и длительность timeout активной симуляции не меняются.

Принудительные `-gproj` и `-addons` нельзя считать выгружаемыми. Проверяется
включение их GUID в выбранный граф; GUID source-проекта разрешается по известному suffix пути в integration registry;
неизвестный source-проект закрывает запуск. Обязательный неванильный
addon вне выбранного графа блокирует выбор с отдельным пояснением.
Для универсального source frontend нужны базовый `AIConflictArland/addon.gproj`,
`-addons 9178E5822AFE48EA,B52C5F6AEDBF423E` и `-addonsDir` с репозиторием,
game resources и установленными RHS/WCS. Смена только `-gproj` при сохранении
принудительного полного WCS CSV недостаточна. Installed frontend не требует
отдельного запускающего приложения.

## Видимость и совместимость

Стабильный вход: `{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf`.
Он остаётся thin inherited vanilla Arland Co-op USSR Medium: CLI запуск ID
без меню использует этот default. Dedicated server использует canonical
launcher с Variant/MatchMode/Difficulty и существующие leaf GUID.

`AICF_ArlandScenarioCatalog` дополняет GetDefaultGameConfigs только входом,
сохраняя stock entries и native InsertNewScenario dedup. Source addon не имеет
installed WorkshopItem; одного `.rdb` недостаточно для меню. В packaged списке
legacy leaf ресурсы фильтруются только по точным IDs registry:
`SCR_ContentBrowser_ScenarioSubMenu.CreateLine` и оба списка
`SCR_ServerConfigListComponent`. Физические 81 headers и их GUID сохранены.
Сценарии других модов и сохранения не совпадают с этими IDs.

`AICF_WorkshopTagMetadata` сохраняет исправление SCR_FilterCategory:
BaseContainer adapter, callbacks Remove/Insert и override OnGameStart для
правильного static caller. Native category constructors не меняются.

## Проверки

- `Test-ScenarioSetupStatic.ps1`: legacy default, atomic mode/difficulty,
  opt-in/passthrough, modal/cleanup; 15 отрицательных mutations.
- `Test-UniversalScenarioStatic.ps1`: generator/catalog agreement, полный граф,
  native/platform/download/forced gates, rollback, flags/help и leaf filters.
- `AICF_ScenarioCatalogProbe.c`: настоящий GetPageScenarios/GetInGameScenario,
  один вход, девять скрытых default leaf targets, stock preservation,
  repeat/dedup, native type/manw tag maps; 15 cases.
- `AICF_UniversalScenarioProbe.c`: каждый dependency ID, все 81 plan, ожидаемая
  доступность установленных вариантов, forced subset, host mods, настоящие
  widgets/modal/bounds, флаги, 15 пояснений, выбор/cleanup; 43 cases.
  CLI `-aicfUniversalProbe 1 -aicfExpectedReadyVariants 9`.
- `AICF_ScenarioTransitionProbe.c`: production preflight/launch, active header,
  точный GetLoadedAddons без лишних неванильных addons, ROSTER_READY; четыре cases.
  CLI `-aicfTransitionProbe 1 -aicfProbeMap 0 -aicfProbeContent 2
  -aicfProbeMode 1 -aicfProbeDifficulty 0`.
- `Test-UniversalScenarioLog.ps1 [-Transition]`: полный stopped log, native exit,
  exact CLI/UTF-8 manifest, уникальные cases/completion, SCRIPT/VM/fatal и
  собственные resource errors. Transition сверяет реальный ресурс и загруженные
  IDs с общим каталогом. Contract mutations проверяют отказы.

`-aicfProbeResumeAfter 35` в transition fixture удерживает штатную Welcome pause
35 секунд после RUNNING, проверяет разницу real/simulation времени и снимает
native simulation pause. Это terminal runtime probe, не GUI input и не
подтверждение ручного Welcome. При этом флаге analyzer требует один успешный
TRANSITION_PAUSE, real_ms >= 35000 и simulation_ms < 2000, затем обычный
ROSTER_READY и четыре cases. Без флага fixture не снимает паузу.
`-Transition -Dedicated` — отдельный reference gate: exact canonical server/
MissionHeader CLI, два header/loaded-graph cases и ROSTER_READY; frontend Play
он не проверяет. Нельзя выдавать его за четыре transition cases.

Probes находятся только в isolated stage Core/UI как AICF_Z*.c. Полные логи
сохраняются; счётчик readbacks не отменяет script/resource ошибки всего лога.

**NOT RUN:** ручные mouse/keyboard/controller input, внешний вид на разных
разрешениях, фактический public Host, packaged Workshop build, длительный soak.
Terminal-only правила запрещают GUI/Host UI automation. Игроку нужно открыть
AI Conflict, пройти выбор/отмену и запустить Host. Native widget readbacks и
local Play не доказывают эти действия. Свежие результаты и неуспешные
промежуточные прогоны — [TESTING.md](TESTING.md).
