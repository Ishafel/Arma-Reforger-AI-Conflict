# Форма снабжения на карте

Кнопка **«Снабжение»** находится в верхней части полноэкранной игровой карты,
рядом с командным интерфейсом AICF. Выберите базу-источник, другую союзную базу
назначения, целое количество припасов и нажмите **«Отправить»**. Ниже ползунка
появятся ответ сервера, маршрут и результат перевозки. Карту можно закрыть:
положение и ход рейса отображаются союзным маркером **«Л»**.

После ответа сервера можно отправить следующую заявку, пока предыдущие рейсы
ещё выполняются. Число заявок игрока и всей службы отдельно не ограничено;
лимитов логистических машин на автопарк и фракцию нет. Сохраняется бюджет AI.
Если все машины заняты, следующая заявка выделяет отдельную машину того же
действующего автопарка; свободная подходящая машина переиспользуется. Форма показывает
маршрут, состояние и результат **последней заявки**, а каждый активный рейс
виден по своему маркеру «Л».

В активном статусе указано примерное прибытие к **текущей** цели: «До источника»
до погрузки, «До получателя» после неё. Это оценка по расстоянию по прямой и текущей
горизонтальной скорости, с коэффициентом 1,3 на повороты и округлением вверх до
минут; время меньше минуты выводится отдельно. Она пересчитывается на сервере,
а не отсчитывается на клиенте. Остановка, отсутствие данных и восстановление
движения заменяют число пояснением; на месте показывается погрузка/разгрузка.
Оценка может увеличиться при замедлении или объезде и не является сроком доставки.
До подготовки машины время неизвестно. Общий срок с ожиданием погрузки не вычисляется.
Валидация редактируемой заявки всегда видна отдельной строкой над результатом
последнего рейса. Недоступные данные склада, нехватка припасов или места у
получателя объясняют отключённую кнопку и во время другой перевозки.

Интерфейс заменяет автоматическую отправку логистики **обеих фракций**.
Новые рейсы возникают только по явной заявке игрока. Нужен действующий союзный
автопарк с совместимой грузовой машиной, местом для её появления и бюджетом AI. За один
рейс перевозится не больше вместимости машины; сервер сообщает доступный предел
при отказе. Обслуживание jobs, возврат остатка cargo на склад и cleanup сохранены.
Общие vehicle/economy subsystems работают постоянно.

## Исправления после ревью — 2026-09-14

Валидация следующей заявки всегда видна над результатом предыдущей перевозки.
Concurrent-probe ждёт результаты всех принятых tokens, даже при отказе второй
заявки или её завершении раньше первой; deadline не отключает игрока с активными
заявками. Учёт результатов находится только в тестовой fixture.

Evidence: `.codex-runtime/unlimited-supply-review-fixes-20260914/RESULT.md`.
`ManualSupplyStatic` — PASS / 0, 30 negative inputs; `SupplyMapUIStatic`,
`Stage35Static`, `Stage4Static` — PASS / 0. Прежний `AI_COMMANDER_UI_STATE`
в `AICommanderModeStatic` сохранён. Четыре production Workbench graphs и
изолированная Everon-копия с fixtures — PASS / 0. Enforce-проверка учёта
результатов на dedicated server — 14/14 PASS, exit 0, SCRIPT (E/F), ENGINE (F)
и VM/null errors — 0; прочие 48 resource/world/entity/material/pathfinding
error lines сохранены в полном остановленном логе. Это ограниченный gate
учёта результатов: client replication, повторный рейс с доставкой и ручной
визуальный/input gate после этих исправлений — NOT RUN.

## Проверка нескольких заявок — 2026-09-14

Проверка нескольких заявок от 2026-09-14 сохранена в
`.codex-runtime/unlimited-supply-20260913/` (baseline снят 2026-09-13).
`ManualSupplyStatic` — PASS / 0, 23 отрицательных входа; отдельные шесть
повреждённых source-копий отклонены. `SupplyMapUIStatic`, `LogisticsStatic`,
`LogisticsContracts` (142 cases), `LogisticsMapMarkersStatic`, `Stage3Static`,
`Stage35Static`, `Stage4Static` — PASS / 0. Сохранён прежний FAIL / 1
`AICommanderModeStatic`: только `AI_COMMANDER_UI_STATE`. Workbench Validate
Arland/Everon/ArlandRHS/EveronRHS — PASS / 0; initial sandbox launch не смог
подключиться к Steam, успешный terminal retry выполнен вне sandbox.

В остановленном Everon server log один player `2` имеет перекрывающиеся
`request=1/2`, разные slots `1000000/1000001`, две готовые пары driver/vehicle
и два независимых jobs. Повторный клик до ответа не создал лишнюю заявку.
Это ограниченный PASS одновременного приёма; полный runtime audit — FAIL / 1:
одна погрузка 100, выгрузок нет до остановки fixture. Cleanup удержал обе машины
из-за `PROTECTED_CLEARANCE_PLAYER_POSITION_UNKNOWN_GRACE_EXPIRED`; записаны также
stock shutdown/catalog/persistence errors. Всего server `SCRIPT (E)=14`,
VM/null errors=0. Финальный client `SCRIPT (E/F)=0`, VM/null errors=0; resource/world
errors сохранены. Server/client завершились с exit 0 через fixture `RequestClose`.
Первый клиент US остановлен терминально: fixture не подготовила для него source/depot;
повторный клиент проверял подготовленную сторону USSR.

Полные логи: `server-profile/logs/logs_2026-09-14_21-03-02/console.log` и
`client-ussr-profile/logs/logs_2026-09-14_21-06-29/console.log` внутри evidence.
Manifest, команды, source hashes и все verdicts перечислены в `RESULT.md` там же.
JIP/reconnect, разные маршруты, несколько игроков, больше двух заявок, RHS runtime,
полный delivery/return lifecycle, soak и ручной visual/input gate этим прогоном
не закрыты. Проверка UI без GUI automation остаётся NOT RUN.

## Данные и lifecycle

- `AICF_SupplyMapData` читает `SCR_MilitaryBaseSystem.GetBases()`, оставляет
  инициализированные `SCR_CampaignMilitaryBaseComponent` с текущей фракцией
  локального игрока. Проверяется фактический объект фракции: RHS не требует
  stock-ключей `US`/`USSR` в UI. Radio connectivity и роль AI-командира список
  не ограничивают.
- Названия берутся из `GetBaseName()`. Порядок — переведённое название,
  затем `EntityID` для одинаковых названий. Выбор хранит компонент и immutable
  `EntityID`, а не индекс или название базы.
- `GetSupplies()` возвращает физический запас stock consumer `DEFAULT/SUPPLIES`.
  Максимум ползунка — округление вниз до целого. Это показания склада, без
  обещания будущей брони: другие игроки, строительство и reinforcement могут расходовать
  припасы. Запрос доставки повторно проверяет обе базы, остаток после экономических
  обязательств и reservations, место у назначения и вместимость транспорта.
  Резерв создаёт общий ledger после готовности машины; списание происходит только
  при физической погрузке. Изменение stock в пути может дать частичную доставку,
  которая отражается в фактическом результате.
- Существующий tick `AICF_StrategicUIController` каждые 500 ms обновляет форму,
  включая состояние без локальной фракции. При движении ползунка и выборе базы
  ownership и запас повторно читаются непосредственно в обработчике.
- При переходе на другую базу количество ограничивается новым максимумом.
  Потеря выбранной базы сбрасывает количество в ноль и выбирает первую оставшуюся
  базу; смена фракции также сбрасывает выбор. Popup закрывается перед заменой
  индексов. Если меняется только запас, открытый список не пересоздаётся.
- Пустой список отключён. Нулевой запас отключает ползунок с диапазоном `0..0`.
  Отсутствующий consumer или отрицательный stock aggregate показываются как
  недоступные данные, а не как подтверждённый нулевой склад.
- Источник исключается из списка назначения. Назначение всегда выбирается явно;
  потеря ownership/identity сбрасывает его, не подменяя адрес другой базой.
  `GetSuppliesMax()` показывает место у назначения. При его нехватке «Отправить»
  отключается, диапазон ползунка остаётся `0..stock источника`.
- Пока запрос ожидает ответа сервера, повторная отправка и изменение полей
  отключены. После ответа они доступны и при активном рейсе. Сохраняются защита
  от повторного token и ограничения частоты: 2 секунды на игрока и 500 ms между
  admission службы. Старый рейс не может перезаписать ответ последней заявки,
  в том числе если новая заявка отклонена. Состояние хранится на player controller,
  поэтому закрытие карты его не теряет. Выход игрока/смена фракции отменяют все
  его заявки; reconnect создаёт новый controller, старые заявки не восстанавливаются.

`AICF_SupplyMapUI` принадлежит существующему strategic UI и использует его методы
построения фона/текста. Внутри панели создаются штатные
`UI/layouts/WidgetLibrary/ComboBox/WLib_ComboBox.layout` и
`UI/layouts/WidgetLibrary/WLib_Slider.layout`; собственных layouts, world,
prefabs не добавлено. Эти пути присутствуют в установленном
`1.8.0.13` resource database.

Встроенные labels скрываются через `GetLabelWidget().SetVisible(false)` и
очистку текста. `UseLabel(false)` после `CreateWidgets()` здесь запрещён: он
удаляет label и его animation component, тогда как уже инициализированный
`SCR_AutomaticScrollComponent` продолжает использовать его при focus/hover.
Скрытие сохраняет lifetime штатных компонентов до удаления всего control.

Снабжение и AICF command panel открываются по очереди; незавершённый выбор точки
карты отменяется. Активные exclusive stock tools закрываются через
`SCR_MapToolMenuUI.GetMenuEntries()` и их `OnDisableMapUIComponent()`.
Кнопка текстовая, программная, по существующему UI-паттерну проекта.
`RegisterToolMenuEntry()` в `1.8.0.13` регистрирует icon-only entry, а его
`PopulateToolMenu()` выполняется до используемого AICF `OnMapOpenComplete`.

Модальный фон перехватывает мышь. `SCR_MapCursorModule.HandleDialog()` блокирует
pan/drawing; узкое расширение `AICF_SupplyMapInput` дополнительно блокирует
zoom, selection, drag, rotate и radial/tool actions, которые stock `CS_DIALOG`
сам по себе не блокирует. Guard действует только при захваченном вводе данной
формой. Modded cursor сохраняет `[BaseContainerProps()]`: штатные Map*.conf
должны создавать его через config loader, а не только видеть script typename.
Кнопка «Закрыть», клик вне панели, действие Back и закрытие карты
очищают popup/focus; после клика ввод карты возвращается на следующем callqueue
tick, чтобы тот же mouse-up не выбрал объект под формой. `Detach`/`Stop` снимают
все собственные обработчики и отложенный callback. При повторном открытии карты
виджеты создаются заново.

Layout resource names содержат проверенные GUID: ComboBox
`{4B5AE6E64037FFB4}`, Slider `{4A41296C0E9A889F}`. Значения совпали в read-only
`resourceDatabase.rdb` установленных Game и Server 1.8.0.13; разбор RDB сверялся
с известным stock `WLib_OpenedComboRoot` (`B8C4345E3A833B05`). Запись одного пути
без GUID работала через fallback, но создавала `Wrong GUID/name` при открытии
формы. Source audit теперь проверяет обе resource identities.

## API и мультиплеер

Проверка исходников выполнена по закреплённому Script Diff **1.8.0.13**, commit
`3d77cc212d5cda9922daf5f45635c7300d2d4cce`:

- `Game/Systems/SCR_MilitaryBaseSystem.c`: клиентский реестр баз.
- `Game/Components/Locations/SCR_CampaignMilitaryBaseComponent.c`:
  `GetSupplies()` на proxy не обновляет authority resource grid.
- `Game/Sandbox/Resources/Consumer/SCR_ResourceConsumer.c`:
  `GetAggregatedResourceValue()` использует реплицированный aggregate при
  отсутствии локальной container queue; `Extract`/`Inject` сериализуют aggregate,
  включая начальное состояние, а не только локальные видимые containers.
- `SCR_ResourceComponent` реплицирует consumers; owner/faction читаются через
  штатный `SCR_FactionAffiliationComponent`. UI не обращается к server-only
  `AICF_MatchController` и не вызывает `Replication.BumpMe()`.
- `AICF_SupplyTransportRpc` отправляет только намерение через reliable server RPC
  на player-owned controller. Базы адресуются `RplComponent.Id()` и разрешаются
  на сервере через `Replication.FindItem`; локальный `EntityID` по сети не передаётся.
  Сервер повторно выводит player/faction из controller. Ответ — `RplProp(OwnerOnly)`
  со status, token и названиями маршрута последней заявки; Bump вызывается только при изменении.
  Такой snapshot доступен при stream/JIP controller, а закрытие карты его не удаляет.

Это подтверждение сетевого пути по исходникам. Проверки реальным удалённым
клиентом, JIP и визуального ввода остаются отдельными runtime gates.

Официальные справочники:
[UI и ввод](https://reforger.armaplatform.com/news/modding-boot-camp-4-user-interface-and-hud),
[SCR_MapToolMenuUI](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__MapToolMenuUI.html),
[stock overlay](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__MapSuppliesTransportSystemUI.html),
[базы](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__MilitaryBaseSystem.html),
[данные базы](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__CampaignMilitaryBaseComponent.html),
[ComboBox](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__ComboBoxComponent.html),
[Slider](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__SliderComponent.html).
Для реализации приоритет имеют закреплённые исходники версии проекта.

## Проверка в игре

Клиент запускается пользователем либо агентом после отдельного указания,
через canonical launcher из [DEVELOPMENT.md](DEVELOPMENT.md).

1. На Arland/Everon открыть карту обычным действием карты, нажать «Снабжение».
   Сверить список с контролируемыми базами, включая удалённую и изолированную
   от HQ. Проверить обе фракции; отдельно повторить с RHS.
2. Сверить припасы с stock UI склада. Выбрать количество у верхней границы;
   перейти на базу с меньшим запасом и проверить `amount <= maximum`.
3. Выбрать пустую базу: `0 / 0`, ползунок отключён. Проверить отсутствие баз
   и отсутствие назначенной игроку фракции: список и ползунок недоступны.
4. Пока форма открыта, расходовать/пополнять припасы на сервере. Показания
   должны следовать репликации и следующему UI tick. Повторить во время
   перетаскивания ползунка и при открытом списке.
5. Передать выбранную базу другой фракции, удалить её и сменить фракцию игрока.
   Убедиться, что старая база исчезла, количество сброшено, старый popup не
   выбирает базу с переиспользованным индексом.
6. Колесо, drag и клики внутри формы не должны менять карту/её selection.
   Проверить закрытие кнопкой, кликом снаружи, Back и закрытием карты при
   раскрытом списке. После закрытия проверить pan/zoom/selection, затем
   десять раз переоткрыть карту. Проверить переходы в command panel, выбор
   точки, stock exclusive tool и возврат из pause menu.
7. Подключить свежий удалённый/JIP клиент после изменения supplies/ownership.
   Проверить те же данные без предварительного посещения удалённой базы.
   Проверить, что ответ о заявке виден только её отправителю.
8. Создать спрос на базах при наличии источника и рабочего depot у обеих сторон.
   Убедиться, что AICF не создаёт новую логистическую машину/водителя и не назначает
   рейс автоматически. Открытие формы и изменение количества также не запускают
   транспорт и не списывают припасы. В server log ожидается
   `LOGISTICS_DISPATCH_POLICY mode=MANUAL automatic_dispatch=0`.
9. Выбрать другую союзную базу назначения, количество в пределах её свободного
   места и нажать «Отправить». Дождаться ответа сервера. При отсутствии автопарка
   построить его; при отказе по вместимости уменьшить количество. Двойное нажатие
   до ответа сервера не должно создавать второй рейс. Закрыть/открыть карту во
   время подготовки: маршрут и актуальный статус сохраняются. После ответа и
   интервала частоты отправить вторую заявку: при занятых машинах автопарк
   выделяет дополнительный worker, оба рейса
   продолжаются независимо, форма показывает последнюю заявку. Проверить другой
   маршрут, отказ новой заявки и завершение старой: старый статус не должен
   подменять последний ответ. Повторить при заполненном обычном fleet cap:
   логистическая заявка не должна отклоняться по числу машин. При исчерпании
   бюджета AI отказ сохраняется и не отменяет уже принятые заявки.
10. Проследить маркер «Л» до погрузки и выгрузки. Сверить фактическое списание и
    зачисление, итог «Доставлено». Повторить при расходе stock, заполнении назначения,
    смене владельца, уничтожении машины и выходе отправителя. Проверить возврат
    сохранившегося груза и отсутствие автоматической новой доставки.
11. Во время рейса сверить строку прибытия в форме и в подробностях «Л»:
    сначала «До источника», затем «До получателя». При остановке должно появиться
    пояснение, при возобновлении движения — новая оценка. Закрыть и открыть карту:
    время должно отражать текущий снимок сервера. Проверить, что длинный маршрут
    вместе с ETA помещается над кнопкой при 720p/1080p и UI scaling.

Для каждого runtime-прогона нужны полные остановленные server/client logs и
ручной verdict по виду/вводу. Автоматический static PASS не означает runtime PASS.

## Примерное прибытие — 2026-09-13

Изменены `UI/AICF_LogisticsArrivalEstimate.c` (новый общий read model),
`UI/AICF_LogisticsMapMarkers.c`, `UI/AICF_SupplyMapUI.c`,
`Economy/AICF_ManualSupplyDispatch.c`, `tools/Test-ManualSupplyStatic.ps1`,
`tools/fixtures/AICF_ManualSupplyClientProbe.c`, этот документ,
`LOGISTICS_MAP_MARKERS.md` и `TESTING.md`.
Read model не меняет worker, phase, job, ресурсы или waypoint. Новый сетевой
протокол не вводится: форма получает прежний `OwnerOnly` status, подробности
маркера — прежний faction/JIP snapshot. `Physics.GetVelocity`, `vector.DistanceXZ`
и `Math.Ceil` проверены в закреплённом Script Diff 1.8.0.13.

Evidence: `.codex-runtime/logistics-eta-20260913/`. Команда
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-<Name>.ps1`:
`SupplyMapUIStatic`, `ManualSupplyStatic`, `LogisticsStatic`, `LogisticsContracts`,
`LogisticsMapMarkersStatic`, `Stage4Static` — **PASS до и после**, exit `0`;
`LogisticsContracts` сохраняет все **142** positive/negative cases. Для
`ManualSupplyStatic` добавлен negative case, запрещающий Reserve до завершения
`SPAWN_PENDING`: теперь **17**, существующие не ослаблялись.
Baseline failures в этом наборе нет; отдельный известный
`AI_COMMANDER_UI_STATE` из предыдущего отчёта не относится к ETA и не исправлялся.

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File
.codex-runtime/logistics-eta-20260913/Validate-Final.ps1` запускает terminal Workbench
командой из `DEVELOPMENT.md`: **PASS Arland/Everon/ArlandRHS**, exit `0`,
`Script validation successful`, SCRIPT E/F и VM `0`. Полные логи — `workbench-final-*/`.
Штатные resource diagnostics совпали с предыдущим baseline: `25/25/26`, новых нет;
сравнение в `workbench-baseline-comparison.json`. Изолированный runtime-source
также скомпилирован: `Validate-Probe-Final.ps1`, exit `0`; production SHA-256 совпадают.

Runtime использует существующие `AICF_LogisticsRuntimeProbe.c` и
`AICF_ManualSupplyClientProbe.c`, только в изолированной копии. Через
`tools/Start-AICFRuntime.ps1` поднят Everon server с `AICommanderMode BOTH`,
`aicfLogisticsProbe=1`, `aicfLogisticsProbePrepare=1`, `aicfLogisticsProbePeace=1`,
`aicfLogisticsProbeDepotAtSource=1`, `aicfLogisticsProbeDurationMs=300000`;
в отдельной terminal session подключён client с `aicfManualSupplyProbe=1`,
`aicfManualSupplyProbeStartStep=3`, `aicfLogisticsClientSpawnFaction=US`.
Финальный прогон использует `-RepositoryRoot <evidence>/runtime-source-final`; клиент получил
`-ServerProfileRoot` из server manifest. Exact команды, свежие profiles и проверка
CLI/process/ROSTER_READY сохранены в `final-*-launch.log` и `final-*-manifest.json`.

В реальном рейсе **Levie → Main Operating Base** клиент получил через RPC/status
последовательно: остановку у источника, «До источника: ≈ меньше минуты»,
погрузку, остановку после погрузки, «До получателя: ≈ меньше минуты».
Числа и фазы в клиентском логе взяты из production status, без GUI automation.
Первый прогон доставил 100/100, но выявил гонку `WORK_BEFORE_EXACT_READY`:
`Ready()` становился true между vehicle polls, а ручная заявка уже назначала
job и меняла фазу до завершения acquisition. Заявка теперь ожидает выхода из
`SPAWN_PENDING`; штатные utility/site, readiness diagnostics и phase переход
остаются у transport controller. Изменение не задерживает повторное использование
уже подготовленной машины. Первые полные логи и FAIL сохранены в `probe-*`.
В первом прогоне клиент закрывался принудительно (`-1`); fixture теперь вызывает
`RequestClose()` после итогового ответа, чтобы завершить runtime штатно.

Финальный прогон: **Tiller's Find → Main Operating Base**, доставка **100/100**;
`LOGISTICS_DRIVER_READY` предшествует `LOGISTICS_JOB_RESERVED`. Клиент получил
смену «До источника» → «До получателя: ≈ 4 мин» → «≈ меньше минуты» → итог без ETA.
Server и client завершились с exit `0` и `Game destroyed`; клиентские SCRIPT/VM
errors — `0`. Полные остановленные логи — `final-server-logs/` и
`final-client-logs/`, сводка — `final-runtime-results.json`.

Полный server audit
`tools/Test-LogisticsLog.ps1 -LogPath <final-server-console> -RequireDelivery
-RequirePolicy -RequireLedger -RequireGraph -RequireSearch` — **FAIL**, exit `1`.
Нарушение готовности больше не обнаружено, но сервер записал **3 VM `Failed move`**
из stock `SCR_AIProcessFailedMovementResult.NodeErrorOnce` и **2 SCRIPT errors**
при Stop: `VEHICLE_CLEANUP_RETAINED` / `CORE_ERROR_BRIDGE` с
`PROTECTED_CLEARANCE_PLAYER_POSITION_UNKNOWN_GRACE_EXPIRED`. Последний случай
уже наблюдался в baseline ручных рейсов: у test-only игрока нет controlled entity.
Cleanup сохраняет машину и cap по существующему защитному контракту.
Ошибки и companion `error.log`/`crash.log` не исключались из аудита;
см. `final-logistics-audit.txt`. Нулевых указателей и ENGINE fatal нет.
Клиентский статус этой fixture проверен по полному client log; параметр
`-ClientLogPath` старого `Test-LogisticsLog` ожидает другой формат resource-probe
samples и не используется для доказательства нового RPC/status.

**NOT RUN:** ручная визуальная проверка расширенной строки, 720p/1080p/UI scaling,
hover ETA на клиенте, JIP/reconnect и отдельное воспроизведение recovery с ETA.
Они не заменяются успешной компиляцией или логом RPC. Порядок ручной проверки —
пункт 11 выше.

## Подключение ручных рейсов — 2026-09-13

Добавлены `UI/AICF_SupplyTransportRpc.c` и `Economy/AICF_ManualSupplyDispatch.c`.
Изменены форма/источник данных, `AICF_LogisticsJob`, `AICF_LogisticsPlanner`,
`AICF_LogisticsService` и узкий orchestration-метод `AICF_MatchController`.
Vehicle flows, transport controller, spawner и resource adapter используются
через существующие границы. Ручной повторный spawn после cleanup сохраняет
`ReplacementCooldownMs`. Изменение stock во время подготовки проверяется до Reserve,
потеря ownership/capture отменяет pending request. После погрузки судьба груза
остаётся у существующей логистики.

Evidence: `.codex-runtime/manual-shipping-20260913/`.

- До правки: `Test-SupplyMapUIStatic`, `Test-LogisticsStatic`,
  `Test-LogisticsContracts`, `Test-Stage4Static`, `Test-Stage3Static`,
  `Test-Stage35Static` — PASS / exit 0. Baseline failures этих команд отсутствуют.
- После правки: те же команды — PASS / exit 0, LogisticsContracts 142 cases.
  Новый `Test-ManualSupplyStatic.ps1` — PASS / exit 0, 16 negative inputs;
  существующий SupplyMapUI audit — PASS, 8 negative inputs.
- `Validate.ps1`: terminal Workbench 1.8.0.13 для Arland/Everon/ArlandRHS —
  PASS / exit 0. Полные логи: `workbench-final-pass-*/console.log`.
  Game module создан, validation successful, SCRIPT(E/F)/ENGINE(F)/VM = 0.
  25/25/26 прежних resource errors сохранены без новых сообщений относительно
  `compact-marker-cards-20260909`; сравнение записано в
  `workbench-baseline-comparison.json`.
- Дополнительно `Test-MapPointOrdersStatic` — PASS / 0;
  `Test-AICommanderModeStatic` — FAIL / 1 с единственным `AI_COMMANDER_UI_STATE`.
  Тот же FAIL воспроизведён на чистом архиве HEAD до правки
  (`head-Test-AICommanderModeStatic.txt`): ожидаемая аудитором строка ожидания
  player command не совпадает с существующей подписью. Этот несвязанный
  baseline не исправлялся в задаче снабжения.
- `git diff --check` — PASS. `final-source-hashes.json` фиксирует production
  sources; `probe3-production-delta.json` подтверждает совпадение тестовой копии.

Тестовая `AICF_ManualSupplyClientProbe.c` живёт только в `tools/fixtures` и
изолированных runtime-source копиях. Использует тот же публичный client facade,
что кнопка формы, без GUI automation. Native faction request сохраняет штатную
проверку; персонаж автоматически не создаётся. У старой respawn fixture обнаружен
NULL `loadout` в stock `SCR_BaseGameMode.CanPlayerSpawn_S` (probe2); эта сессия
прервана через проверенные manifests, её exit -1 не объявляется PASS.

Первый завершённый probe подтвердил клиентские ответы об отказе: та же база,
отрицательное количество, чужая база и отсутствующий depot. Во время этих
запросов SCRIPT/VM ошибок не было. Полные `probe-server-logs`/`probe-client-logs`
сохраняют также stock resource/world errors и три SCRIPT ошибки меню после
server shutdown (`SCR_FilterCategory` ×2, `REPLICATION_SHUTDOWN` preset ×1).
Поэтому полный runtime gate этого прогона не зелёный.

Третий probe на Everon подтвердил физическую ручную доставку `Coastal Base Morton`
→ `Main Operating Base`, request `1`, job `L1_S1000000_G1`:

- принята одна заявка, создана одна машина M998 с одним водителем, один reserve;
  повторное нажатие client facade не создало второй запрос;
- LOAD: источник `2185 → 2085`, машина `0 → 100`;
- UNLOAD: машина `100 → 0`, назначение `250 → 350`;
- `discrepancy=0`, `balance_delta=0`, `requested=100 delivered=100`;
- клиент получил busy-статусы и итог «Доставлено: 100 / 100» с `busy=0`.

Полные логи: `probe3-server-logs` и `probe3-client-logs`; manifests/CLI/exit
находятся рядом, числовые результаты — `probe3-runtime-results.json`.
Server завершился с exit 0; оставшийся после завершения мира client process
закрыт через exact manifest и имеет exit -1. VM exceptions — 0, client SCRIPT
errors — 0. Общий `Test-LogisticsLog -RequireDelivery -RequirePolicy -RequireLedger
-RequireGraph -RequireSearch` — FAIL / 1: при Stop cleanup удержал пустую машину
из-за `PROTECTED_CLEARANCE_PLAYER_POSITION_UNKNOWN_GRACE_EXPIRED` у тестового
игрока без персонажа. Это два связанных SCRIPT events: `VEHICLE_CLEANUP_RETAINED`
и `CORE_ERROR_BRIDGE`; те же два события есть в companion `error.log`.
Условия безопасного удаления не ослаблены. Physical delivery/RPC подтверждены,
полный runtime gate не объявлен PASS.

Анализатор логов обновлён для глобального startup event `LOGISTICS_DISPATCH_POLICY`:
у службы до создания worker нет faction/slot/vehicle identity. Проверяются её
четыре policy fields вместо worker identity. Положительный и три отрицательных
log fixtures добавлены в Contracts; реальные cleanup errors продолжают давать FAIL.

Оставлена обычная сессия Everon `127.0.0.1:2001`, без fixtures/подготовки баз:
`play-server-manifest.json`, `play-client-manifest.json`. Canonical launcher
подтвердил живой server process, exact CLI и `ROSTER_READY`. На снимке
`play-status-live.json` SCRIPT/VM errors — 0; живой log не заменяет stopped gate.
Server profile: `C:\Users\retar\AppData\Local\AICF\Server-Everon-20260913-134114-045`;
client profile: `C:\Users\retar\AppData\Local\AICF\Client-Everon-20260913-134141-686`.

Ручная visual/input матрица выше и отдельный JIP/два одновременно отправляющих
игрока — NOT RUN: терминальные RPC fixtures их не заменяют. Исторические записи
ниже описывают прежнюю форму только с источником, до подключения отправки.

## Выполненная проверка — 2026-09-09

Исходный commit `7a26f4e495c70b32b1f2e2c587fb61c6da1a7efe`. Изначально tracked
изменений не было; существующие untracked runtime evidence и документы других
задач сохранены. Новые файлы:

- `AIConflictCore/Scripts/Game/AIConflict/UI/AICF_SupplyMapData.c`;
- `AIConflictCore/Scripts/Game/AIConflict/UI/AICF_SupplyMapUI.c`;
- `AIConflictCore/Scripts/Game/AIConflict/UI/AICF_SupplyMapInput.c`;
- `tools/Test-SupplyMapUIStatic.ps1`, этот документ.

Изменены `UI/AICF_StrategicUI.c`, `tools/Test-Stage4Static.ps1`, `README.md`,
`docs/ARCHITECTURE.md`, `docs/TESTING.md`. Методы построения прямоугольников/текста
стали общими для двух панелей; Stage 4 extractor теперь допускает как public,
так и protected методы. Проверки цвета и отрицательный fixture сохранены.

Evidence находится в `.codex-runtime/supply-map-ui-20260909/`:
`status-before.txt`, `commit.txt`, `before-*.log`, `final-*.log`,
`static-results.json`, `source-sha256.json`, `workbench-version.txt`,
`workbench-final-*-arguments.json`, `workbench-final-*/console.log`,
`workbench-final-results.json`, `engine-errors-comparison.json`.
Это локальные generated artifacts, они не добавляются в Git.

| Команда / gate | До изменения | После изменения |
|---|---|---|
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage4Static.ps1` | PASS / 0 | PASS / 0 |
| Та же команда для `Test-Stage35Static.ps1` | PASS / 0 | PASS / 0 |
| Та же команда для `Test-MapPointOrdersStatic.ps1` | PASS / 0 | PASS / 0 |
| Та же команда для `Test-GroupMapMarkersStatic.ps1` | PASS / 0 | PASS / 0 |
| Та же команда для `Test-LogisticsStatic.ps1` | PASS / 0 | PASS / 0 |
| Та же команда для `Test-SupplyMapUIStatic.ps1` | новая проверка | PASS / 0, 4 negative inputs |
| Терминальный `ArmaReforgerWorkbenchSteamDiag.exe -noThrow -wbsilent -gproj ... -addonsDir ... -addons ... -logsDir ... -wbModule=ScriptEditor -run -validate` | не запускался | Arland, Everon, RHS: PASS / 0 |
| `git diff --check` | — | PASS / 0 |
| Server/client/JIP, визуальная проверка, мышь/Back/повторное открытие | — | NOT RUN |

Workbench **1.8.0.13**: во всех трёх финальных полных логах есть
`Game successfully created` и `Script validation successful`; `SCRIPT (E/F)`,
`ENGINE (F)` и VM/null errors отсутствуют. Exact CLI и exit codes сохранены
отдельно. Запуск `Validate.ps1` в evidence последовательно выполняет команды
из `DEVELOPMENT.md` для трёх root projects. Первая попытка в sandbox дошла до
создания Game, но не выполнила Validate из-за `SteamAPI_Init`; последующие
прогоны в окружении пользователя закрыли этот gate.

Baseline failures применённых статических аудиторов отсутствовали. Полные
Workbench logs сравнены с остановленными `workbench-release-*` из
`.codex-runtime/group-marker-cards-20260908-221457/`: наборы engine/resource
ошибок совпали (25 уникальных сообщений stock, 26 RHS). Сохранились штатные
shutdown resource leaks и RHS `StringTableSource` GUID/name mismatch.
Прежний warning `AICF_OrderPlanner.c:2864` об избыточном up-cast также сохранён;
предупреждений в новых supply UI исходниках нет.

Клиент не запускался по явному ограничению задачи. Сетевая совместимость
обоснована исходниками и compile, но фактические multiplayer/JIP показания,
layout и ввод требуют ручного прогона по матрице выше. Статус приёмки не выставлен.

## Замена автоматической отправки — 2026-09-09

После уточнения назначения формы в `Economy/AICF_LogisticsService.c` удалены
preflight/spawn нового worker, автоматический выбор следующей доставки и
replacement после cleanup. Оставлены исполнение принятого job, возврат cargo,
возврат пустой машины и безопасное завершение. Изменение действует для обеих
фракций независимо от AI commander mode. Новых RPC, CLI opt-out или economy
transactions форма не добавляет.

Дополнительно изменены `tools/Test-LogisticsStatic.ps1`,
`tools/Test-LogisticsContracts.ps1`, `README.md`, `docs/ARCHITECTURE.md`,
`docs/TESTING.md`, `docs/LOGISTICS_VALIDATION.md` и этот документ.
Устаревшая проверка ожидания cooldown перед автоматическим spawn заменена
пятью контрактами: запрет автоспавна/автодоставки и сохранение job service,
vehicle tick, cargo return. Каждый имеет отрицательный source fixture.
Прочие проверки geometry, cooldown, identity и conservation сохранены.

Evidence: `.codex-runtime/manual-supply-dispatch-20260909/`, включая
`status-before.txt`, `before-*.log`, `after-*.log`, `before-results.json`,
`after-results.json`, `source-sha256.json`, `Validate.ps1`,
`workbench-final-*-arguments.json`, `workbench-final-*/console.log`,
`workbench-final-results.json`, `engine-errors-comparison.json`,
`diff-check.log` и `diff-check-result.txt`.

Все статические команды запускались как
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<имя>.ps1`:

| Аудитор / команда | Baseline до замены | После замены |
|---|---|---|
| `Test-LogisticsStatic` | PASS / 0 | PASS / 0 |
| `Test-LogisticsContracts` | PASS / 0, 134 cases | PASS / 0, 138 cases |
| `Test-Stage4Static` | PASS / 0 | PASS / 0 |
| `Test-Stage35Static` | PASS / 0 | PASS / 0 |
| `Test-SupplyMapUIStatic` | PASS / 0 | PASS / 0 |
| Терминальный Workbench `-wbModule=ScriptEditor -run -validate` | результаты UI выше | Arland, Everon, RHS: PASS / 0 |
| `git diff --check` | — | PASS / 0 |
| Server runtime отсутствия автодоставки и возврата текущего груза | — | NOT RUN |
| Client/JIP/визуальный ввод | — | NOT RUN |

Workbench `1.8.0.13`: все три полных лога содержат `Game successfully created`
и `Script validation successful`, без `SCRIPT (E/F)`, `ENGINE (F)` или VM/null
errors. Наборы resource errors совпадают с предыдущими UI-прогонами: 25
уникальных сообщений stock и 26 RHS, новых нет. Сохранены прежние shutdown
resource leaks, RHS `StringTableSource` GUID/name mismatch, stock deprecation
warnings и warning `AICF_OrderPlanner.c:2864`. В применённом static baseline
failures нет. Клиент не запускался; runtime PASS из compile не выводится.

## Исправление регистрации cursor после клиентского запуска — 2026-09-09

Первый Everon server/client запуск после разрешения пользователя подключился
успешно, но выявил ошибку реализации при открытии карты. В полном client log
сначала появились три `Unknown class 'SCR_MapCursorModule'` при загрузке
`MapPlain.conf`, `MapSpawnMenu.conf` и полноэкранной карты, затем 11 VM exceptions
в штатных `SCR_MapToolInteractionUI.OnMapOpen`, markers и drawing. Server VM
exceptions отсутствовали; `LOGISTICS_DISPATCH_POLICY` подтверждал отключённую
автоматическую отправку. Static/compile выше этого дефекта не обнаруживали.

Причина: в `UI/AICF_SupplyMapInput.c` modded class не повторял
`[BaseContainerProps()]`, присутствующий у stock `SCR_MapCursorModule` в API
1.8.0.13. Добавлена регистрация config class. Vanilla файлы и методы не менялись;
null guards в падающих stock инструментах не заменяют создание cursor.
`Test-SupplyMapUIStatic.ps1` теперь проверяет регистрацию, включая отрицательную
мутацию без атрибута. Текущий тест содержит пять negative inputs.

Неуспешный запуск сохранён в `.codex-runtime/supply-ui-play-20260909-192457/`:
оба launcher manifests, исходные profile paths, полные
`server-stopped-console.log`/`client-stopped-console.log` и `startup-status.json`.
Оба процесса остановлены принудительно после проверки executable/profile для
перезапуска исправленной версии; launcher exit `-1` не означает graceful Stop.

Исправление проверено командами
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/<имя>.ps1`:
`Test-SupplyMapUIStatic`, `Test-Stage4Static`, `Test-MapPointOrdersStatic`,
`Test-GroupMapMarkersStatic` — baseline и после правки PASS / 0. Терминальный
Workbench `-wbModule=ScriptEditor -run -validate` для Arland/Everon/RHS — PASS / 0;
`SCRIPT (E/F)`, `ENGINE (F)` и VM/null errors отсутствуют. Прежние наборы resource
errors (25 stock, 26 RHS) сохранены без новых сообщений. `git diff --check` — PASS.
Evidence: `.codex-runtime/map-cursor-registration-20260909/`, полные `before-*`,
`after-*`, `workbench-final-*`, manifests команд и source SHA256.

Исправленная сессия запущена тем же `tools/Start-AICFRuntime.ps1` в отдельных
терминалах: server `-Role Server -Variant Everon -AICommanderMode BOTH`, client
`-Role Client -Variant Everon -ServerProfileRoot <новый профиль> -ServerPort 2001`.
Client launcher снова проверил exact CLI, живой server process и `ROSTER_READY`.
Её evidence: `.codex-runtime/supply-ui-play-fixed-20260909-193145/`.

Этот повторный запуск подтвердил устранение cursor failure: `Unknown class
'SCR_MapCursorModule'` — 0, карта трижды дошла до `STRATEGIC_UI_READY`.
Однако взаимодействие с формой выявило вторую ошибку: 27 VM exceptions в
`SCR_AutomaticScrollComponent.ResetScrolling` при focus/hover списка и ползунка.
Остановленные server/client logs и `startup-status.json` сохранены; server VM — 0.
Поэтому повторный запуск не считается успешным runtime gate.

Причина второй ошибки подтверждена исходниками 1.8.0.13:
`SCR_ChangeableComponentBase.UseLabel(false)` вызывает `ClearLabel()` с
`RemoveFromHierarchy()`, а `SCR_AutomaticScrollComponent` сохраняет ссылку на
animation component удалённой подписи. В `AICF_SupplyMapUI.CreatePanel()` оба
вызова заменены общим `HideNativeLabel`, который очищает текст и скрывает widget.
Регистрация cursor из первого исправления сохранена. Stock handlers не меняются.
Focused static audit проверяет обе точки вызова, отсутствие удаления label и
отрицательную мутацию; текущий итог — шесть negative inputs.

Baseline и проверки после второго исправления находятся в
`.codex-runtime/supply-label-lifetime-20260909/`. `Test-SupplyMapUIStatic`,
`Test-Stage4Static`, `Test-MapPointOrdersStatic`, `Test-GroupMapMarkersStatic` —
PASS / 0 до и после правки. `git diff --check` — PASS.

Workbench Validate после второго исправления: Arland/Everon/RHS — PASS / 0,
без `SCRIPT (E/F)`, `ENGINE (F)` и VM/null errors; наборы stock/RHS resource
errors не изменились. SHA256 production sources после compile совпал.
Сервер и клиент перезапущены через canonical launcher с новыми profiles.
Текущая сессия: `.codex-runtime/supply-ui-play-label-fixed-20260909-193726/`,
Everon, `127.0.0.1:2001`. `server-manifest.json`, `client-manifest.json`, полные
живые snapshots и `runtime-status.json` сохраняют CLI и наблюдаемый результат.
Подключение, `ROSTER_READY`, `MANUAL_PENDING` и открытие карты подтверждены
логами; на момент записи VM exceptions и cursor config errors отсутствуют.
Сессия оставлена работать для пользовательской проверки hover/list/slider:
живые snapshots не заменяют полный остановленный runtime gate или ручной
вердикт по виду и вводу. Полная multiplayer/JIP матрица остаётся NOT RUN.

Перед завершением проверки выявлены и исправлены две AICF resource errors с
нулевым GUID для `COMBO_LAYOUT`/`SLIDER_LAYOUT`. Последний прогон до исправления
GUID не имел VM exceptions после повторного открытия карты; его полные logs
сохранены перед перезапуском. Исправление resource references проверяется в
`.codex-runtime/supply-resource-ids-20260909/`; там же находятся
`resource-identities.json` с read-only сверкой Game/Server RDB, baseline и
последующий `Test-SupplyMapUIStatic` (PASS / 0, теперь восемь negative inputs),
source SHA256 и финальные Workbench logs/arguments/results.

Финальный Workbench Validate после GUID-правки: Arland/Everon/RHS — PASS / 0,
без script/VM/fatal errors. Прежние resource errors сохранены: 25 stock, 26 RHS,
delta пуст. Source hashes после compile совпали; `git diff --check` — PASS / 0.
Изменены два supply UI исходника, `Test-SupplyMapUIStatic.ps1`, этот документ
и `docs/TESTING.md`.

Финальная оставленная работающей сессия:
`.codex-runtime/supply-ui-play-final-20260909-194401/`, Everon, `127.0.0.1:2001`.
Canonical server/client launcher подтвердил exact CLI, живой server process,
`ROSTER_READY` и подключение. Сервер сообщает `MANUAL_PENDING automatic_dispatch=0`.
Profiles, manifests, живые полные snapshots и наблюдаемые счётчики ошибок записаны
в evidence и `runtime-status.json`. Итоговый stopped runtime gate, полная ручная
матрица UI и JIP не объявляются PASS: эта сессия остаётся доступной пользователю.
