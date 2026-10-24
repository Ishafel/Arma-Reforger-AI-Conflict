# Универсальное меню AI Conflict

Один пункт **AI Conflict** в штатном меню открывает настройку перед Play или
Host. Это игровой экран, отдельного лаунчера нет.

## Путь игрока

1. Выбрать AI Conflict → Play либо Host с обычными сетевыми настройками.
2. Выбрать Арланд, Эверон или Север Эверона; Vanilla, RHS или WCS + RHS.
3. Выбрать Conflict или Co-op. В Co-op флаг определяет сторону всех игроков:
   US/СССР, USMC/МСВ РФ либо НАТО/РФ соответственно контенту. В Conflict
   обе стороны доступны позднее в штатном deployment.
4. Выбрать сложность гарнизонов, время суток и начальную погоду.
   Время: ползунок 00:00–23:59 с шагом 1 минута и немедленной подписью HH:MM.
   Случайного времени в меню нет: начало операции всегда задаётся точно.
   Погода: случайная, Clear, Cloudy, Overcast, Rainy. Defaults — 08:00 и random.
   Погода продолжает меняться; ускорение времени остаётся по сценарию.
   Слева — карточки карт с native preview, переключатели контента, режима,
   стороны и сложности, ниже — время и погода. Справа — название карты,
   многострочная сводка и пояснение варианта под курсором/фокусом.
   Выбранные пункты отмечены золотой рамкой и полосой; hover/focus подсвечиваются.
   Нижняя панель содержит статус, отмену и основную кнопку запуска.
   Начать операцию загружает её; Создать сессию
   использует прежние настройки сети/пароля/player limit. Отмена закрывает экран.
   При отказе запуска выбор сохраняется.

Vanilla показана всегда. RHS появляется при включённых зависимостях RHS,
WCS + RHS — при включённом полном наборе обоих модов. Скачанный, но отключённый
контент скрыт; неполный набор также скрыт. WCS-only профиля нет.
Включённые, но неготовые local revisions и несовместимость с принудительными
параметрами запуска оставляют видимый вариант серым. Экран ничего не скачивает
и не переключает моды. После изменения модов в Мастерской нужно переоткрыть
настройку. Флаги США/СССР используют stock texture GUID,
РФ — native tricolor widgets, НАТО — стилизованный компас на синем поле.

Подключающиеся игроки используют обычный браузер/direct connect и штатные
Welcome/faction/group UI. Новый экран не перенастраивает текущую сессию и не
показывается JIP-клиенту. Co-op — [COOP_SCENARIO.md](COOP_SCENARIO.md), найм —
[GAMEPLAY.md](GAMEPLAY.md). Автостройка своей стороны в Co-op отключена;
рабочие и заказы бойцов в казарме сохранены.

## Подсказка игроку

Кнопка «Как настроить» постоянно видна в верхней правой части setup, под
индикатором локальной/сетевой сессии. Она открывает отдельную modal-панель
в том же экране с тремя блоками: подготовка модов в Мастерской; карта/контент,
Conflict/Co-op, сложность и окружение; проверка видимости наборов и Play/Host.
Скачанный мод не объявляется включённым. Для RHS/WCS объясняется полный набор
зависимостей и повторное открытие setup после изменения модов. У серого варианта
предлагается проверить зависимости, загрузки и статус. Это инструкция, а не
автоматическая диагностика конкретного отсутствующего пакета.

`AICF_ScenarioSetupScreen.OpenGuide/CloseGuide` меняют только modal/focus.
Кнопка «Понятно» возвращает к настройкам без их сброса; повторный вызов
идемпотентен, `Close` снимает вложенную modal перед основной. Guide widgets
создаются один раз на открытие setup, участвуют в существующем font scaling
и уничтожаются вместе с root. Основные 20 control indices сохранены: guide
имеет отдельные input references. Обработчики root/guide снимаются при Close;
новых таймеров и сетевого состояния нет.
Тексты RU/EN принадлежат `AICF_Localization.st`; runtime `.conf` генерируются
`tools/Build-AICFLocalization.ps1`.

Guide frame, как основные `AddButton`, требует `SetZOrder(10)`: у полноэкранного
root есть прозрачный `AICF_RectInput` на слое 1. Frame со значением 0 остаётся
видимым, но его нажатия получает этот root input. Проверка одного `OpenGuide`
не выявляет перекрытие. Native probe отдельно вызывает read-only
`WidgetManager.TraceWidgets` в девяти точках каждой guide-кнопки, учитывает
IGNORE_CURSOR и требует, чтобы ближайший ButtonWidget совпадал с её input.
Мышь/клавиатура при этом не перемещаются и нажатия не генерируются.

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
`m_aContentAddons` генерируется из полного графа без GUID собственных
`localProjects`: frontend может загрузить integration при transition, поэтому
его предварительное включение не является условием видимости. Core не содержит
RHS/WCS GUID. `IsContentEnabled` требует `WorkshopItem.IsEnabled()` каждого
внешнего dependency. API учитывает enable state до scripts reload и всегда
возвращает true для уже загруженного item. Для source addon без WorkshopItem
требуется присутствие в `GameProject.GetLoadedAddons`; одной available-записи
недостаточно. Пустой content graph означает Vanilla. Неготовый scan закрывает
модифицированные варианты до повторного открытия меню.

Скрывается весь frame, включая input; оставшиеся кнопки сдвигаются влево без
изменения стабильных content indices. Выбор карты, `SelectVariant` и
`CanStart` независимо применяют enabled gate: скрытый вариант нельзя выбрать
программно или запустить по устаревшему выбору. Статус перечисляет только
видимые, но недоступные наборы. CLI launch не ограничивается frontend-фильтром.

`HasDependencies` требует каждый ID. `IsAvailable` использует native
`GameProject.GetAvailableAddons`; при наличии Workshop active revision требует
Downloaded и отсутствие corruption. Точная active version передаётся Host.
`IsReadyToRun` зависит также от online/selected metadata: у целого offline
WCS Attachments 8.4.1 он вернул false при Downloaded=true/Corrupted=false.
Этот selected-revision gate применяется только к source local pseudo-revision,
как прежде. Полный локальный граф независимо требует каждый ID из GameProject.
Source placeholder WorkshopItem без revision подтверждается GameProject.
Пустой scan не считается наличием модов.

`AICF_ScenarioSetupSelection` хранит карту/контент/режим/сложность и независимые
`m_iStartMinutes` (0..1439, sentinel 1440 = random) и weather index 0..4.
Sentinel остаётся в model/storage API для совместимости; frontend
`SelectEnvironment` принимает только 0..1439 и не содержит random control.
Кнопок теперь 20: map 0..2, content 3..5, mode 6..7, side 8..9, difficulty
10..12, launch 13, cancel 14, weather 15..19; slider — отдельный native widget.
Default — 480 минут. Смена карты/контента не сбрасывает их. Ошибочный
выбор сохраняет прошлый. Defaults: Арланд / Vanilla / Co-op USSR / Medium,
если запуск не закрепил несовместимый контент. При смене карты контент
сохраняется либо выбирается первый допустимый набор в стабильном порядке.

## Оформление экрана

Визуальные ориентиры: [штатный выбор сценария Reforger](https://reforger.armaplatform.com/news/boot-camp-host-your-own-game)
и [mission briefing Ghost Recon](https://interfaceingame.com/screenshots/tom-clancys-ghost-recon-breakpoint-new-mission/).
Разделение настроек и сводки, графитовые панели, приглушённые изображения и золотой
акцент — композиционное решение этого экрана. Внешние изображения не копируются:
каталог `previewHeaders` задаёт штатный header для каждой карты, generator
передаёт его как `m_sPreviewHeader`. Карточки читают его `m_sPreviewImage` либо
`m_sLoadingScreen`: Арланд использует preview, Эверон — отдельную stock иллюстрацию,
Север Эверона — preview без наложенной сетки и отметки N.
Каталог `mapCards` задаёт локализованные tag/description, RGB accent, artwork и
sectorGrid. Генератор проверяет подписи, три целых RGB 0..255 и boolean-флаги,
передаёт оформление descriptor каждого content-варианта. Core не содержит
имён карт, конкретных палитр или ресурсов. Арланд выделен бирюзовым, Эверон —
песочным, Север — голубым. Для текущих карт `sectorGrid=false`; прежняя
декоративная сетка удалена по запросу владельца. Название и короткое описание находятся под изображением,
метка операции — на контрастной подложке. Золото по-прежнему обозначает выбор.
Это не требует чтения header ещё не загруженного integration addon.
Размер текстуры задаёт масштаб заполнения с сохранением пропорций, parent
clipping ограничивает карточку. Native `OnUpdate` рассчитывает его после layout
pass и повторяет только при смене размера root. В этот же момент обновляется
размер текста; динамическая сводка/подсказки переносятся.
Смена времени меняет только виджеты и сводку, не читает headers/addons заново.
`AICF_StrategicUIController.CreateText` сохраняет packed Color до native
`CreateWidget`; тема возвращает managed `ref Color`, default text color
удерживается на время вызова. Это защищает transient значения палитры.

## Native launch boundary

`AICF_ScenarioSetupEntry` перехватывает только `SCR_MissionHeaderCampaign` с
`m_bAICFSetupEntry=1` в
`SCR_ScenarioUICommon.TryPlayScenario/TryHostScenario`; остальные сценарии идут
в super с исходными аргументами. Header читается по `scenario.Id()`:
`MissionWorkshopItem.GetHeader()` может быть null при наличии native metadata.

Первый frontend установленного Workshop мода может работать только с vanilla:
Core hooks и его StringTable ещё не загружены. Поэтому title и краткие
описания entry заданы literal; Play сначала штатно загружает addon graph и
setup header. `AICF_ScenarioBootstrap` на локальной authority открывает picker
после загрузки. AICF integration пропускает запуск своего match controller для
этого промежуточного header; stock world остаётся inherited Arland.
Явные CLI `server`/`client`/`world`, dedicated и remote client не перехватываются.

Picker допускает текущий preparation context только при точном совпадении
`entry.Id()` с активным flagged header. Metadata берётся через native
`WorkshopApi.GetCurrentMission`, затем точный catalog lookup. Отсутствующий
offline scan запускается один раз с ограниченным ожиданием. В мире нет
frontend `SCR_DownloadManager`: вместо него проверяется native `GetDownloads`,
`IsProcessed` и `GetPendingDownload`; main-menu gate сохраняет прежний manager.
Native CanHost применяется при создании новой сессии с config. Preparation
Listen уже создан штатным Host: выбор меняет его сценарий через CanPlay и
сохраняет platform gate. Полный graph, enabled/downloaded/corruption и
forced-CLI gates действуют в обоих контекстах. Отмена preparation вызывает
native GameplayEnd, обычное закрытие/успешный launch только снимает UI.
Callback снимается в OnGameEnd; UI освобождается до world cleanup.

Setup использует штатный тип header с явным opt-in flag. Самостоятельный
`AICF_ScenarioSetupHeader` больше не используется: контроль после native
Play → setup → leaf с этим subtype получал access violation при завершении;
тот же путь с `SCR_MissionHeaderCampaign` завершился native exit 0.
`AICF_ResolveSetupPreset` сохраняет bounds, mode/difficulty, world и запрет
рекурсивного setup. Core и Arland публикуются согласованной версией вместе.

`AICF_ScenarioLaunch.CanStart` заново проверяет main menu, отсутствие перехода,
native CanPlay/CanHost входного MissionWorkshopItem, hosting platform gate,
пустую download queue и зависимости выбранного descriptor. Неготовый набор
не запускается; download dialog не запускает по ошибке default entry.

Play передаёт leaf resource и **точный** addon CSV в штатный
`GameStateTransitions.RequestScenarioChangeTransition`, используемый native
`PlayGameConfig`. Host передаёт SCR_DSConfig в `RequestPublicServerTransition`:
меняет только scenarioId, hostedScenarioModId и mods с загруженными active версиями
(local pseudo-revision используется только как fallback для source),
при отказе возвращает прежние значения. Network/password/player limits и
gameProperties сохраняются. Setup и native menus закрываются после принятия.

В API 1.8.0.13 `RequestPublicServerTransition` не принимает addonList. Сразу после
принятия Host-запроса, в том же кадре, вызывается `GetGame().RequestReload`
с точным `variant.m_aAddons`. Это заполняет граф pending Host-перехода,
сохраняя Listen mode и его конфигурацию. Без этого source Host перезагружался
с двумя vanilla addons, терял Core/integrations и завершался WORLD_LOAD_ERROR.
`RequestReload` помечен obsolete («Pass addon GUIDs to the transition request
directly»); у Public API такого аргумента пока нет. Это локальное исключение
для Host, не замена современного Play-перехода. При отклонённом запросе reload
не вызывается. Workshop Enable и изменение установленных файлов не нужны.

Старый прототип вызывал leaf MissionWorkshopItem.Play/Host. Универсальному
варианту это не подходит: header незагруженной интеграции ещё не обязан быть
в native каталоге, а загруженные RHS/WCS меняют content profile factory.
Разрешения проверяются на реальном входном item, выбранные зависимости —
отдельно; загрузкой/перекомпиляцией занимается native transition. Gameplay,
roster и replicated state здесь не меняются.

## Передача времени и погоды

`AICF_ScenarioEnvironment` записывает одноразовый intent в native
`GameSessionStorage.s_Data` перед Play/Host transition. Это память текущего
executable, переживающая scripts/addons reload; файлы профиля и новые MissionHeader
варианты не создаются. Payload schema **2** содержит точный target resource,
issued tick, minutes и weather. Schema 1 с прежними time presets отклоняется,
чтобы старый индекс не стал минутами. При отказе native transition payload удаляется.
`ArmaReforgerScripted.OnMissionSet` потребляет его до валидации: принимает только
совпадающий leaf header, валидные bounds и возраст до 10 минут. Чужой сценарий,
повторный старт и malformed payload не наследуют параметры. Данные прикрепляются
к transient полю экземпляра header, без изменения исходного ресурса.

`AICF_ScenarioEnvironmentHandler` использует штатный
`SCR_TimeAndWeatherHandlerComponent.SetupDaytimeAndWeather` на authority.
Часы/минуты и native random daytime независимы от выбранного weather state;
готовые presets не должны подменять оба измерения одновременно. Имя погоды
проверяется в `GetWeatherStatesList` мира; недоступное состояние даёт WARNING
`ENVIRONMENT_FALLBACK` и native random. Native матрица покрывает три карты и все
четыре фиксированных состояния выбранными сочетаниями; полный перебор состояний
на каждой карте не запускался. `ENVIRONMENT_APPLIED` пишет
`time_minutes`, weather_mode и фактические часы/погоду. Старый `time_mode` относится
только к историческим preset logs. Повторный вызов не сбрасывает выбор; native save/load
имеет приоритет. JIP-клиент не меняет часы/погоду; сетевой синхронизацией,
дальнейшей сменой погоды и скоростью времени владеет native manager.

Host сохраняет network/password/player limits/gameProperties. Окружение передаётся
внутри того же executable; фактический public Host и JIP остаются отдельными
runtime gates. Dedicated CLI без frontend intent использует прежние настройки
MissionHeader: 08:00, random weather, acceleration 6/24.

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
81 legacy leaf ресурс наследует `m_bShowInScenarioMenu=0` от семи собственных
base headers. Setup явно задаёт `m_bShowInScenarioMenu=1` поверх скрытого parent.
Штатный `SCR_ContentBrowser_ScenarioSubMenu.CreateLine` в закреплённом
Script Diff 1.8.0.13 сам читает доступный header по ID и пропускает hidden resource.
Прежний script-only фильтр действовал только после загрузки Core. Чтение
headers ещё не загруженного опубликованного пакета остаётся отдельным gate:
source-only эксперимент без Core не смонтировал эти ресурсы и не подтверждает
видимость в настоящем cold Workshop frontend.
После загрузки Core сохраняется дополнительная фильтрация по точным IDs registry:
`SCR_ContentBrowser_ScenarioSubMenu.CreateLine` и оба списка
`SCR_ServerConfigListComponent`. Физические 81 headers и их GUID сохранены.
Сценарии других модов и сохранения не совпадают с этими IDs. Варианты доступны
по прежним GUID для picker и CLI; metadata скрывает строку, а не удаляет ресурс.
Обновить нужно все используемые сценарные addons: старый установленный пакет
сохраняет прежние visible headers. В native списке Host до загрузки Core
`GetModdedScenarios` не проверяет этот флаг; packaged cold Host list отдельно
не доказан. Script-фильтр Host действует после загрузки Core.

`AICF_WorkshopTagMetadata` сохраняет исправление SCR_FilterCategory:
BaseContainer adapter, callbacks Remove/Insert и override OnGameStart для
правильного static caller. Native category constructors не меняются.

## Метаданные публикации Workshop

Реальный installed 0.2.2 содержит 82 записи в `ServerData.json` →
`revision.scenarios`: Core 0, Arland 10, Everon 18, Arland RHS 9,
Everon RHS 18, Arland WCS RHS 9, Everon WCS RHS 18. Соответствующие native
publishing manifests также содержат их в `asset.scenarios`. Флаг hidden не
убирает эти записи. Первый frontend ещё не загрузил Core и StringTable:
он показывает raw names из metadata, а не результат script-фильтра.

`tools/Prepare-AICFWorkshopBundle.ps1` работает с готовым native bundle,
создаёт отдельную копию и оставляет в `asset.scenarios` только точный setup
Arland с literal name `AI Conflict`. В копиях остальных шести addons список пуст.
Все `.pak`, `.gproj`, `.rdb` и изображения копируются с проверкой SHA-256;
ресурсы внутри пакетов, GUID и version не меняются. Input bundle не редактируется.
Идентификаторы сверяются с каталогом; неизвестные/чужие/повторные IDs, отсутствие
setup, неправильный literal title, битые payload hashes и существующий output
отклоняются. `-Check` запрещает публикацию bundle с legacy entries.

Это подготовка metadata для отдельной публикации. Обычная повторная сборка
Workbench заново добавляет legacy entries; проверять нужно окончательный
манифест, который будет передан uploader. Приём backend и поведение native
`publishAddonDir` с подготовленной копией пока **NOT RUN**. Нельзя выдавать
проверенный JSON за успешную публикацию или редактировать installed ServerData.

`AICF_WorkshopMetadataProbe.c` — отдельный vanilla-dependent тестовый addon.
Он включает изолированные копии семи модов, но требует, чтобы их scripts
оставались незагруженными. Native GetPageScenarios воспроизводит 82/81 legacy
на исходных metadata и 1/0 на подготовленных; stock entries сохранены.
В test harness копируется только tag-map adapter для обхода отдельного дефекта
vanilla constructor, без сценарных фильтров Core. Изменённые ServerData/meta
в этом опыте — локальная модель ответа backend, а не новая публикация.

## Проверки

- `Test-ScenarioSetupStatic.ps1`: legacy default, atomic mode/difficulty,
  opt-in/passthrough, modal/cleanup; 15 отрицательных mutations.
- `Test-UniversalScenarioStatic.ps1`: generator/catalog agreement, полный граф,
  native/platform/download/forced gates, rollback, flags/help и leaf filters.
- `Test-WorkshopBootstrapStatic.ps1`: native тип/flag, literal metadata,
  authority/identity, scan/download gates, cancel и lifecycle.
- `AICF_WorkshopBootstrapProbe.c`: прямой native Play/Host без warm UI hook,
  фактический picker, выбор leaf, применение времени/погоды, cancel и stopped log.
- `AICF_ScenarioCatalogProbe.c`: настоящий GetPageScenarios/GetInGameScenario,
  один вход, девять скрытых default leaf targets, stock preservation,
  repeat/dedup, native type/manw tag maps и чтение hidden metadata всех 81
  presets; 16 cases. Для всей матрицы нужен полный Everon WCS RHS graph.
- `AICF_UniversalScenarioProbe.c`: каждый dependency ID, все 81 plan, ожидаемая
  доступность установленных вариантов, forced subset, host mods, настоящие
  widgets/modal/bounds, флаги, пояснения, native SliderWidget range/step/current,
  HH:MM, 7200 fixed minute/weather choices и 5 random choices, atomic invalid
  choices, schema-2 roundtrip/schema-1 rejection, одноразовый storage/чужой target,
  cleanup. Guide дополнительно проверяет кнопку, nested modal, размеры текста,
  повторное открытие, возврат focus/выбора и Close с открытой подсказкой.
  Обязательные UI_GUIDE_HIT_TARGET/UI_GUIDE_HIT_CLOSE проверяют порядок input
  в девяти точках; UI_CARD_IDENTITY требует отсутствие декоративной сетки.
  `-aicfProbeLanguage ru_ru|en_us` меняет язык только в isolated fixture и
  возвращает исходный при Stop. Программные readbacks не доказывают физический ввод.
  CLI `-aicfUniversalProbe 1 -aicfExpectedReadyVariants 9`.
- `AICF_ScenarioTransitionProbe.c`: production preflight/launch, active header,
  точный GetLoadedAddons без лишних неванильных addons, ROSTER_READY; четыре cases.
  CLI `-aicfTransitionProbe 1 -aicfProbeMap 0 -aicfProbeContent 2
  -aicfProbeMode 1 -aicfProbeDifficulty 0`.
  Дополнительные `-aicfProbeTime 817 -aicfProbeWeather 4` (13:37, дождь) проверяют перенос через
  reload, фактическое начальное окружение, повтор и native save/load priority.
  В этом fixture-only режиме Welcome pause снимается через world API.
  `-aicfProbeHost 1` запускает production Host вместо Play: требует Listen mode,
  реальный RPL socket на заданном нестандартном порту 24021 и точный loaded graph.
  Сохранение name, player limit, password/admin password, visibility, BattlEye
  и third-person проверяется на переданном request, не выдаётся за readback engine.
  `GetRunningDSConfig` в этом Listen возвращает false, `ServerLobbyApi.GetServerConfig`
  и `GetGame().GetServerInfo()` — null. Первые попытки fixture с такими oracle
  сохранены как FAIL; заменить ими проверку settings нельзя. Фактическая
  авторизация паролем, применение лимита и прочих settings — NOT RUN.
  Fixture задаёт частную сессию и закрывает её после readback.
- `Test-ScenarioEnvironmentStatic.ps1`: guarded mutations для authority,
  target/TTL/one-shot/schema, rejected transition cleanup, native owner,
  минутной точности, binding ползунка и независимого UI.
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
разрешениях, внешний public Host с другим игроком/JIP, packaged Workshop build, длительный soak.
Terminal-only правила запрещают GUI/Host UI automation. Игроку нужно открыть
AI Conflict, пройти выбор/отмену и запустить Host. Native widget readbacks и
local Play и программный private Host не доказывают эти действия. Свежие результаты и неуспешные
промежуточные прогоны — [TESTING.md](TESTING.md).
