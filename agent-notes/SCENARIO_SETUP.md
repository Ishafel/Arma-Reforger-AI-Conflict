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
Welcome/faction/group UI. Новый экран настраивает только локальный запуск и не
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

Каталог tools/config/AICF_Scenarios.json schema 2 задаёт девять сочетаний
карты/контента, семь dependency graphs и четыре штатных MissionHeader:

| Target | ResourceName | Workshop owner |
|---|---|---|
| StockArland | {C41618FD18E9D714}Missions/23_Campaign_Arland.conf | пустой |
| StockEveron | {ECC61978EDCC2B5A}Missions/23_Campaign.conf | пустой |
| RHSArland | {7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf | 595F2BF2F44836FB |
| RHSEveron | {AAD43C10045857C1}Missions/RHS_Conflict.conf | 595F2BF2F44836FB |

WCS использует соответствующий RHS target, северный сектор — Everon target.
Режим и сложность передаются отдельно: 81 логический план больше не означает
81 ресурс. Старые .conf/.meta и их GUID удалены; сохранения и server configs
с этими GUID, а также alias StockCoopUSSR больше не поддерживаются.
Update-AICFCoopScenarios.ps1 удалён. Старые локализованные имена также удалены.

Update-AICFScenarioRegistry.ps1 [-Check] генерирует integration descriptors
с m_sMission/m_sMissionOwner, addon CSV, content dependencies, метками сторон
и оформлением карт. Core не содержит конкретных карт или target GUID.
Для нового integration нужны native target, runtime map policy и запись
каталога; отдельные сценарии для режимов/сложностей создавать не нужно.
Resolve проверяет bounds и наличие owner в графе.

m_aContentAddons содержит внешние dependencies без семи localProjects.
IsContentEnabled требует enabled каждого внешнего WorkshopItem; source addon
без item подтверждается GameProject.GetLoadedAddons. Пустой граф — Vanilla.
HasDependencies проверяет каждый ID. IsAvailable проверяет available graph,
active revision Downloaded и отсутствие corruption; source placeholder без
revision подтверждается GameProject. Native IsReadyToRun дополнительно нужен
для local pseudo-revision. Host передаёт версии активных revisions.

Selection хранит карту/контент/режим/сложность, минуты и weather index 0..4.
Model/storage допускает sentinel 1440=random, экран — только 0..1439.
Defaults: Арланд/Vanilla/Co-op USSR/Medium, 08:00/random weather.
Неверный выбор сохраняет прежнее состояние; смена карты/контента сохраняет
время и погоду. 20 индексов кнопок и отдельный ползунок не меняются.

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

Единственный собственный ресурс:
{A1CF261019500001}Missions/AICF_Conflict_Arland_Setup.conf.
Он наследует native Arland напрямую, имеет literal title AI Conflict и
m_bAICFSetupEntry=1. В нём нет старого массива presets или собственной world.
Обычные native сценарии сохраняются; фильтры старых IDs в UI удалены.
Integration регистрирует только setup в GetDefaultGameConfigs с native dedup.

TryPlayScenario/TryHostScenario перехватывают только flagged header,
прочитанный по scenario.Id(); nullable catalog GetHeader не определяет тип.
При холодном запуске штатный Play/Host сначала загружает setup и scripts,
после чего AICF_ScenarioBootstrap открывает picker на локальной authority.
Матч AICF в preparation scene не запускается. Dedicated/direct client и
явные server/client/world не перехватываются. Отмена preparation вызывает
native GameplayEnd с явным RequestReload текущего non-vanilla graph;
callbacks и modal удаляются в OnGameEnd. Без сохранения graph GameplayEnd
возвращает vanilla-only menu, выгружает Core/tag adapter и ломает повторный Host
ошибками SCR_FilterCategory. Cancel сохраняет уже загруженный набор модов;
новые зависимости не выбираются и не включаются. До reload проверяется
ShouldIntercept, отсутствие pending transition и непустой addon list.

CanStart повторно проверяет entry identity, native CanPlay/CanHost, platform,
download queue, enabled/downloaded/corruption и forced CLI graph.
В preparation без SCR_DownloadManager проверяются native GetDownloads,
IsProcessed/GetPendingDownload. Неизвестный forced gproj или лишний forced
non-vanilla addon закрывают запуск.

Play передаёт native ResourceName и точный addon CSV через
RequestScenarioChangeTransition. Host меняет только scenarioId,
hostedScenarioModId и mods в SCR_DSConfig, откатывает их при отказе,
передаёт полный graph через RequestReload в том же кадре после принятия
RequestPublicServerTransition. Порт/пароль/лимиты принадлежат native config.

Если preparation Listen должен изменить scripts graph, HostTransition
сохраняет immutable intent (phase/issued/entry/selection/environment), завершает
preparation, загружает main menu с нужным graph, повторяет native Host с
сохранённой engine config, затем переключает native mission.
Границы проверяют TTL, graph, Listen context, entry и итоговый selection:
одного общего Everon target недостаточно для различения full/North и режимов.
События HOST_HANDOFF phase=RETURN/HOST/LEAF/READY сохранены. Stop снимает retry.

## Runtime selection и JIP

AICF_RuntimeScenario владеет одной атомарной записью GameSessionStorage
AICF_RuntimeScenario schema 1. Она содержит полную строку native target,
map/content/mode/difficulty/issued/minutes/weather. Строка сериализуется как
string: JSON не должен сокращать ResourceName до GUID до загрузки RHS addon.
Decode проверяет все поля, bounds, target из registry и TTL 10 минут.
Перед применением запись удаляется; повторное потребление невозможно.
Отказ перехода очищает pending, старые environment schemas не читаются.

OnWorldPostProcess до super применяет policy перед native weather setup;
OnGameStart до super служит идемпотентным fallback перед инициализацией баз.
Прототип только OnMissionSet не сработал при начальном direct server и не
используется. Native header и ранние caches campaign/time handler получают
GENERAL, отсутствие vehicle rank gate, save types 0, mode/difficulty,
начальное время/погоду и ускорение 6/24. Файлы игры не изменяются.
Snapshot восстанавливает все изменённые поля и исходный base-list в OnGameEnd.

Everon integration владеет whitelist северного сектора: семь штатных баз,
два HQ и пять control points, radio range 2000, establishing отключён.
Core не содержит их названий. World/layout/entities не копируются.

Server/master публикует variant/difficulty/mode/ready через RplProp уже
реплицируемого SCR_GameModeCampaign, с BumpMe только при фактическом изменении.
Клиент и JIP не читают authority intent: verified snapshot задаёт presentation
header и локальные native rank caches, не изменяя replicated state.
Если snapshot приходит после OnGameStart, readiness callback запускает только
отложенную клиентскую инициализацию. Время/погоду и дальнейшие переходы
реплицирует native manager; AICF_ScenarioEnvironmentHandler работает на authority.

Canonical launcher использует -aicfScenarioVariant, -aicfMatchMode 0..2 и
-aicfDifficulty 0..2 вместе с полным native -server/-MissionHeader.
AdditionalArguments не могут переопределить управляемые параметры. Client
проверяет свежие CLI Params, живой PID и ROSTER_READY перед подключением.
Старый mission GUID нельзя заменить одним переименованием в server config:
нужно заново разрешить полный план и graph через launcher.

## Метаданные публикации Workshop

В новом исходном наборе собственный mission ровно один. Все 81 старые
.conf/.meta физически удалены, поэтому обычная новая сборка должна содержать
только setup в Arland и ноль собственных missions в остальных шести addons.
Ранее опубликованная 0.2.3 остаётся прежней до отдельной публикации: одна Git
ветка не очищает installed ServerData или backend catalog.

Prepare-AICFWorkshopBundle.ps1 теперь отклоняет старые bundles, даже если
asset.scenarios уже очищен: проверяет список Missions/*.conf в native RDB.
Только точный setup допустим у Arland; у остальных owners список пуст.
Неизвестные/чужие/повторные IDs, неправильное имя, отсутствие entry и битые
payload hashes отклоняются до создания output. На выходе scenarios явно
записан как [] для addon без missions, даже если native manifest его опустил.
Готовые старые .pak с metadata workaround повторно использовать нельзя.

Prepare-AICFWorkshopCatalog.ps1 сначала проверяет все семь bundles одинаковой
версии, затем создаёт отдельные копии и catalog.json. Input payload/version
не меняются. Check повторно сверяет owners, manifests/hashes/index и один entry.
Tool не вызывает publisher; publication=NOT_RUN. Команды — DEVELOPMENT.md.
Workshop upload запрещён текущим запросом владельца. Native pack/backend и
поведение скачанного будущего патча — отдельные gates; JSON PASS их не заменяет.

## Проверки

Test-ScenarioHeadersStatic проверяет единственный setup, физическое отсутствие
81 старого ресурса и ссылок в production, project identities/platforms и
сохранение composition boundaries. Test-CoopScenarioMatrix проверяет 81 план,
36 server/client manifests, negative catalog и отклонение старого alias.
Test-RuntimeScenarioStatic защищает authority/JIP/TTL/identity/snapshot и
ранние native boundaries. Остальные setup/environment/bootstrap checks
сохраняют modal, cleanup, permissions, download и forced graph gates.

AICF_RuntimeScenarioProbe.c в isolated stage проверяет фактический active
target/mode/difficulty/rank/save, все 81 JSON roundtrip, invalid values,
опционально отказ чтения всех 81 retired GUID и состав North 7/2/5.
Expected CLI для probe имеет префикс aicfProbeExpected; это не authority input.
AICF_WorkshopBootstrapProbe проверяет cold Play/Host/cancel, native world,
точный graph, runtime policy, time/weather и остановленные полные логи.
Флаг aicfColdRetryHost проверяет отмену preparation Listen и повторный Host
в том же процессе. aicfColdLifecycleProbe проверяет восстановление всех полей
cached header и очистку runtime selection/pending handoff после завершения.
Readbacks клиента проверяются отдельно после canonical readiness gate.
Точные результаты, эксперименты и ограничения — TESTING.md и evidence report.
