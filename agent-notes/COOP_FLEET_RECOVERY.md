# Пополнение, смешанный Co-op и восстановление машины — 01.11.2026

Рабочая ветка `codex/coop-fleet-recovery`, исходный commit `629cdfe`.
Это продолжение #78; issue остаётся открытой, PR79 — draft.

## Изменения

- Пресет техники можно назначить stable slot до готовности живого состава,
  в том числе во время платного пополнения. Сервер проверяет будущие numeric
  positions и обязательный экипаж. Подтверждённое сокращение выполняется только
  после полного набора: новый identity-safe CrewPlan возвращает лишних бойцов.
  Оплаченные пополнения не отменяются, выдача техники остаётся обычной покупкой.
- Co-op имеет по10 управляемых слотов у обеих сторон. Slots0..3 — грузовики,
  slot4 — вооружённая лёгкая машина, slots5..6 — БТР; WCS slots7..8 — танки.
  Остальные пешие. Начальные рецепты не перезаписывают ручную настройку.
  WCS loader у M1 считается экипажем, а не высаживаемым десантом.
  Для дорогих назначений planner учитывает реальную вместимость снабжения:
  при недостатке строится обычный зарегистрированный supply storage.
  Вместимость и деньги не подменяются; в human Co-op прежний запрет
  самостоятельной стройки за сторону игрока сохранён.
  Подсказка выбора Co-op и брифинг на русском/английском теперь описывают
  союзные автономные отряды и их пополнение; прежний текст отрицал их наличие.
- Stock local `SCR_DefendWaypoint` может не иметь `RplComponent`. Созданная
  authoritative factory точка ожидания теперь получает разрешение на основе
  exact factory/receipt/entity/world/model identity. Это не разрешает принимать
  произвольные локальные entities. Общий replicated-entity fence сохранён.
- Настоящий native group GetOut для полностью исправной машины и полного
  посаженного отряда вне боя сначала получает ограниченное восстановление
  маршрута. Оно использует прежние lease, deadline и recovery budget.
  Повреждённая машина, неполный состав, бой и сменившийся контекст не допускаются.
- Штатный GetOut перед открытием шлагбаума может уже быть `COMPLETED`, пока
  utility ещё не выбрала исходный Perform. Exact completion receipt и оба
  живых queued продолжения сохраняют принадлежность исходной цепочки.
  Это не продлевает её60s окно или trip deadline. Смена машины, priority,
  activity, группы либо потеря сознания по-прежнему отменяет разрешение.
- При рассогласовании двух собственных native movement handlers полностью
  живой текущий состав должен быть посажен в ту же машину. Оба handler и все
  их участники проверяются, чужие claims отклоняются, scope читается повторно.
  Восстановление вызывает штатный join водителя; не сбрасывает handlers,
  не перемещает физические entities и не меняет clocks или health.
- RenderTarget отвязывается от private world до его удаления/замены. Редактор
  пресета ИИ не закрывается только из-за смены управляемого персонажа игрока;
  personal editor сохраняет проверку exact avatar. Это защитные исправления;
  исходный отказ окна и причина исходного OOM ими пока не доказаны.

## Свежие native доказательства

- Deferred preset: server5/client6 checks, девять реальных платных пополнений,
  полный состав10 → подтверждённый экипаж1, сохранённый комплект, одна покупка
  УАЗа50 и exact Pilot. Отдельный последовательный reconnect/JIP сохранил binding;
  все три процесса завершились native0, SCRIPT/VM/persistence/OOM0.
- Co-op defaults: Stock22/22 и WCS22/22;14 и18 назначений соответственно,
  обе стороны и реальные compartments, включая loader M1. Это не доказательство
  одновременных оплаченных поездок всех7/9 машин каждой стороны.
- Отдельная RHS/WCS матрица трёх карт:6/6 Workbench и native запусков,
  132 native cases и275 postchecks, оба runtime faction и нужные7/9 назначений.
  Это frozen393 до SupplyDepot/Exit delta; строгие resource gates всех6 FAIL.
- WCS M1: actual debit3750, exact4 вместе с loader,619.11м/102velocity polls,
  прибытие и сохранение всего экипажа. Для этого раннего стенда capacity
  поднималась контролем fixture; это не proof штатного строительства склада.
  T72 v11: actual debit3500, exact3,593.277м/120polls, прибытие и5s сохранения
  экипажа. Native storage расширяет1000→4000 без прямой записи maximum;
  его controlled spawn также не заменяет production paid construction.
  Tank physical probes используют controlled truce и заполненный fixture
  запас припасов. Native quote/debit/quota сохранены; ordinary экономическая
  достижимость всех назначенных танков этим не доказана.
  Исходные Gate v6/v10 premature `CREW_UNAVAILABLE` сохранены как FAIL;
  обычный маршрут v11 без шлагбаума не подтверждает исправленный Exit bridge.
- V12 действительно исполнил completed Exit predicate: exact current
  `COMPLETED/removed` action, completion receipt и оба queued продолжения,
  `accepted=1` в прежнем60s окне. Затем native Perform получил `FAILED`
  через27.128s, поэтому полный Gate Return/arrival этого запуска — FAIL.
  После повторной посадки прежний код отказал на record2/pilot_handler1
  и бросил здоровую машину с3/3 экипажа. Это отдельная доказанная причина
  для нового narrow subgroup repair; исходный FAIL не переписан в PASS.
- V14-positive использует весь текущий393 production snapshot. Контролируемый
  native fault разделил только собственные handlers: record1/pilot2. Actual
  production scope и один штатный join дали readback1/1, сохранили lease,
  vehicle generation, deadline и mobility attempts0. Та же T72 прошла592.313м,
  142 velocity polls, DESTINATION_REACHED, исходные3crew на местах ещё5s,
  deployed0. Checks12/0FAIL, native0/full stopped. Усиленный public auditor
  `-RequirePositiveHandler`: functional/fatal PASS, strict resources208 FAIL.
  Это controlled fault/truce/fill proof, без physical/health mutation;
  отдельный combined Gate Return v12 по-прежнему FAIL.
- Склад Everon v2: обычная цена175, supplies1000→825, реальная работа
  строителя0→50, тот же token/layout ONLINE, capacity1000→4000, native0.
  После hint-first resolver отдельные свежие Everon/Stock подтверждают
  устранение лишней загрузки helipad: raw sibling errors468→466 и30→28.
  Повторная платная стройка на новой случайной coastal MOB не достигнута:
  256кандидатов отвергнуты штатной геометрией, остановка через profile flag.
  Это safe-site FAIL/paid NOT EXERCISED последнего запуска, без обхода площадки;
  успешный платный цикл относится к отдельному snapshot до смены порядка поиска.
- Canonical root-owned transport probe подтвердил local hold, одну реальную
  покупку, удаление owned hold с readback `absent=1`, exact boarding5/5,
  удалённый приказ, выбранный native GetOut и production recovery.
  Та же машина с прежними пятью бойцами после recovery проехала27.8693м;
  budget0→1/max2, route1→2, deadline не изменён.

## Память и ограничения

Одинаковый hidden native vehicle preview probe с корректными catalog GUID
прошёл60 переключений модели на старом и новом UI, оба native0/full stopped,
SCRIPT/VM/OOM0. Peak private bytes1.43/1.44GB соответственно. Этот короткий
контроль не воспроизвёл исходный OOM примерно через64s после shutdown матча
и не доказывает его устранение. Первый стенд с неполным ResourceName падал
native access violation и на старом, и на новом UI; эти FAIL сохранены отдельно.

Дополнительный `frontend-return/native-v3`: production Play прошёл4 transition
checks, обычное завершение gameplay и реальный возврат в главное меню;
ожидание120224ms, native0/full stopped, fatal/OOM0. Private memory снизилась
с peak3516MiB до2309MiB. Public `Test-ScenarioReturnLog` вернул0;
strict resource gate136 raw sibling Error — FAIL. Это отдельный короткий
shutdown trajectory proof; исходное падение памяти всё ещё не воспроизведено.

Строгий all-native-Error gate остаётся FAIL: resource/world/network ошибки
не скрываются. Установленные stock/RHS/WCS resources не изменяются.
Native-only persistence reproducer с8 leaks не объявляется исправленным:
свежий production reconnect подтверждает только защищённый project path.
Визуальное открытие окна вводом игрока и удобство нового окна — NOT RUN:
терминальные probes не воспроизводят физический ввод/визуальную оценку.
Игроку нужно в обычном матче открыть карту → отряд → пресеты пехоты/техники,
проверить выбор модели, AutoAssign, список мест и preview, затем повторить
это во время пополнения и после завершения матча. Полный маршрут и длительная
игра проверяются отдельно по [чек-листу](VEHICLE_VISUAL_CHECK.md).
Свежий длительный ordinary combat/memory soak, исходный pressure2→4→6 с полной
фоновой армией и authenticated account persistence/restart в этом продолжении
не запускались. Их прежние результаты и blockers не превращаются в новый PASS.

## Evidence

| Команда / проверка | Verdict |
|---|---|
| Профильный baseline до правок, `Invoke-AICFChecks.ps1 -Name` |4/4 PASS, `baseline-root/`|
| `Invoke-AICFChecks.ps1 -Suite All` + законченный tail и corrective reruns |284/284 unique PASS; исходный all-v1 имел FAIL, история сохранена|
| Terminal WB `-wbsilent -wbModule=ScriptEditor -run -validate`, Stock/RHS/ArlandWCSRHS |3/3 compile PASS; strict resource50/52/78 FAIL|
| Canonical `Start-AICFRuntime.ps1`, deferred server/client/JIP |Платное пополнение, binding и покупка PASS; resource FAIL|
| Canonical `Start-AICFRuntime.ps1`, RHS/WCS × Arland/Everon/North |6/6 defaults PASS на замороженном срезе; resource FAIL|
| `Test-HealthyNativeAbandonLog.ps1` |Same asset repair functional PASS; resource FAIL отдельно|
| `Test-TankPhysicalLog.ps1 -RequirePositiveHandler` |Current393 same T72/nativejoin/motion/crew PASS; native0/fatal0, resources208 FAIL|
| `Test-ScenarioReturnLog.ps1`, native120s после gameplay end |Functional/fatal PASS; resource FAIL; исходный OOM не воспроизведён|

Полный список изменённых файлов — `changed-files.txt` в общем evidence.
Gameplay изменения распределены по прежним владельцам: GroupSlot/DeferredPreset,
VehiclePresetService/UI, content profiles/CoopFleetDefaults, ConstructionPlanner,
acquisition hold/factory, TransitFlow/NativeGate/BehaviorFlow. Fixture/auditor
изменения лежат в `tests/`; запуск вариантов — `tools/AICF_Scenarios.ps1`.

- Общие команды, baseline, offline и итоговая компиляция:
  `.codex-runtime/coop-fleet-recovery-20261101/`.
- Пополнение и reconnect:
  `.codex-runtime/coop-fleet-recovery-20261101/reinforcement/REPORT.md`.
- Co-op defaults и crew policy: `.codex-runtime/coop-fleet-20261101/REPORT.md`.
- Native hold/recovery: `.codex-runtime/healthy-native-abandon-20261101/`.
- RHS/WCS Arland/Everon/North defaults matrix:
  `.codex-runtime/coop-fleet-matrix-20261101/REPORT.md`.
- Оплаченные WCS tanks, сохранение экипажа и native gate:
  `.codex-runtime/wcs-tank-physical-20261101/REPORT.md`.
- Supply storage: `supply-depot/` в общем evidence; actual-source contracts
  включают constructor/reset/cancel нового индекса и узкий own-side RHS alias.
- Preview A/B: `preview-comparison.json` в общем evidence. Полные logs,
  launcher manifests, exit codes, memory samples и проверенные source ZIP
  сохраняются; удалены только собственные исходные stage-копии.

Итоговый production checkout: `Final-Compile-Root.ps1` /
`final-compile-root-v3` в общем evidence, включая SupplyDepot, null-owner guards,
completed Exit receipt, hint-first resolver и native subgroup repair.
Штатный WB обновил generated databases в checkout.
Stock/RHS/ArlandWCSRHS3/3 PASS, exit0/validation/fatal0;393 production files,
source drift0. Начальная попытка dryrun root compile была отклонена launcher:
даже неиспользуемый ProfileRoot нельзя располагать внутри runtime repository;
следующая попытка использует внешний temporary path, реальный profile не создаёт.
Прежние397 stage entries отдельно архивированы с проверкой каждого SHA до
удаления stage. Строгие resource Error50/52/78 остаются FAIL.
Профильный `Invoke-AICFChecks -Name`
проверил семь actual-source contracts: CoopFleetDefaults, DeferredVehiclePreset,
HealthyNativeAbandonRecovery, PlayerPersistenceIdentity, PresetEditorOpening,
VehiclePresetCrew, VehicleStagingHold —7/7 PASS. Полный offline union:
284/284 уникальных имён PASS, `offline-union-284-reviewed-handler-auditor.json`
в evidence Co-op. Последний completed Exit delta проверен6/6,
включая138 actual cases,8 source и12 executable unsafe variants;
SupplyDepot54 cases/8 unsafe; positive subgroup40/8;
TankLog46 и SupplyLog13 parser controls, NativeSubgroup20 structural/9 actual.
Исходные9 failed names сохранены с повторными PASS.
Причины: устаревшие
предположения о Co-op/сигнатурах crew policy, синтаксические adapters к actual
Enforce bodies, неизменённый junction contract без прав и сначала не обновлённая
source string table при правке runtime перевода. Production gates и отрицательные
контроли не ослаблены. Это union завершённых проверок и corrective reruns,
а не утверждение, что исходный `all-v1` завершился без FAIL.
