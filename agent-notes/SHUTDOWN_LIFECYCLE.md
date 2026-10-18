# Завершение матча и private equipment worlds

Issue [#63](https://github.com/Ishafel/Arma-Reforger-AI-Conflict/issues/63).
`AICF_EquipmentService` удерживает read-only `AICF_EquipmentPlan` с собственным
`AICF_EquipmentDraft`. Эти модели нужны для совместимости и оценки боезапаса;
они не являются бойцами кампании или источником выдаваемых предметов.

## Причина

`EquipmentDraft.Clear()` снимал native storage attachments корневых предметов,
затем удалял character. Отсоединённые roots уже не входили в его children.
Native oracle после возврата Clear находил **71 и 63 уникальных живых IEntity**
двух моделей. Ранний raw GetItems давал 75/67 записей с четырьмя повторами в
каждом kit; финальный oracle удаляет дубликаты. Обнуление draft/world references
не подтверждало удаление inventory.
В полном shutdown log оставались 355 строк классов / 5170 экземпляров и три
`Total Leaks`: всего 358 `SCRIPT (E)`. Отсутствие имён `AICF` не означало
внешнюю причину: удерживаемые native objects принадлежали проектным моделям.

Отключение только loadout fixture не исключало фоновые equipment worlds.
Control без обновления equipment service устранял leaks; подключение клиента
и preview редактора не были необходимыми условиями. Более ранняя очистка
кеша меню, полное исключение этого кеша и пауза после disconnect не устраняли
ошибки. Эти контрольные source stages и полные логи хранятся в evidence.

## Владельцы и порядок

- Bootstrap вызывает `MatchController.Shutdown()` до штатного `OnGameEnd` и
  освобождает свою ссылку. Shutdown использует существующий идемпотентный
  `Stop(false)`: службы снимают callbacks и освобождают plans, native teardown
  сохраняет владение удалением gameplay entities.
- `EquipmentDraft.Clear()` работает только с character вне мира кампании.
  Для каждого непосредственного storage root он снимает attachment, повторно
  проверяет отсутствие parent и принадлежность собственному world, затем
  удаляет root вместе с children. После этого удаляет character и освобождает
  world. Другие preview и authoritative inventory transactions сохраняют
  своих владельцев.
- `EquipmentPlan` продолжает разрывать hand/weapon связь перед освобождением
  модели. Само исчезновение script draft не заменяет native entity readback.

## Lifetime muzzle в EquipmentPlan

В `EquipmentPlan.Build` inline-вызов `AICF_EquipmentInventory.Muzzle(modelPrimary)`
в условии `for` оставлял одну native reference после выхода из цикла. Ранний
`Stop(true)` давал `Can't delete instance (of class: 'MuzzleComponent') with 1 references`
и VM exception при удалении модели. Порядок удаления roots и поздний unarm
этого удержания не устраняли. Muzzle теперь берётся в локальную переменную,
цикл обращается к ней, после чтения ссылка явно обнуляется. Данные боезапаса,
владельцы world и существующий cleanup не меняются.

Предыдущий PASS issue #63 проверял обычный OnGameEnd и пакеты операций над
Draft. Он не подтверждал ранний Stop и полный цикл Build/destructor именно
EquipmentPlan внутри каждого пакета. Теперь эти циклы входят в native oracle;
контроль с возвратом одного inline-вызова проверяет, что gate ловит дефект.
Свежие verdict и evidence — в [TESTING.md](TESTING.md).

## Терминальная проверка

`tests/fixtures/AICF_ShutdownProbe.c` добавляется только в isolated Core stage.
Он поддерживает `-aicfShutdownMode idle|batches`, native `MemoryStatsSnapshot`,
constructor/destructor counters и weak entity readback всех inventory items
после EquipmentDraft.Clear. Размер пакета — 8 операций (4 итерации по обеим
сторонам), пакетов — 3; каждый выполняет Build/Signature/Capture/Restore,
сравнение signature, navigation Build/Clear и Draft.Clear. Флаг
`-aicfShutdownEquipmentBatch 1` выбирает direct EquipmentDraft вместо UI draft.
В этом режиме каждая операция также строит EquipmentPlan, освобождает его
и проверяет weak character readback: 24 уникальных `SHUTDOWN_PLAN_CLEAR`
с `passed=1` обязательны для `Test-ShutdownLog.ps1 -Batches`.
Snapshots снимаются до/после пакетов и после не менее 15 секунд покоя.

Для control без постоянных models используется только fixture-флаг
`-aicfShutdownNoEquipment 1`; это диагностическое исключение в isolated stage,
а не production opt-out. Для standalone добавляется
`-aicfShutdownStandalone 1`. Connected client получает тот же ShutdownMode;
его callback закрывает клиент и снимается в OnGameEnd. Сервер ждёт disconnect
либо 180 секунд. Опциональный `-aicfShutdownGraceMs` служит контролем времени
после disconnect. Обычный reproducer issue #61 использует прежние Contents
Inventory + Clothing fixtures без замены.

Запуск — только `tools/Start-AICFRuntime.ps1` с fresh profiles, одинаковыми
Variant/Difficulty/source graph и штатным readiness gate. Compile извлекает
gproj/addonsDir/addons из DryRun manifest по [DEVELOPMENT.md](DEVELOPMENT.md).
Пример параметров measurement server: `-Variant EveronWCSRHS -Difficulty Hard`
и AdditionalArguments `-addr 127.0.0.1:2367 -aicfShutdownMode batches
-aicfShutdownEquipmentBatch 1 -aicfShutdownStandalone 1
-aicfRequirePlayerForResult 0`.

`Test-ShutdownLog.ps1 -LogPath <stopped-console.log> -LauncherPath <launcher.txt>
-NativeExitPath <exit.txt> [-Batches] [-RequireStop] [-Client]` требует exit 0,
полный log, manifest, соответствующие readbacks и отсутствие script/fatal/VM
ошибок, включая любые leaked instances. `-RequireStop` требует ноль живых draft,
равенство created/destroyed и ноль native items после Clear. Его synthetic
positive/negative contracts проверяют строгость анализатора отдельно от runtime.
При `-Batches` manifest обязан содержать точные пары `-aicfShutdownMode batches`
и `-aicfShutdownEquipmentBatch 1`, каждую ровно один раз; `SHUTDOWN_DONE` также
должен иметь `mode=batches`. Отсутствующее, неверное, непарное или повторное
значение отклоняется. Сами 24 operation records и readback фоновых service
models не доказывают, что пакеты выполнялись через `AICF_EquipmentDraft`:
без equipment-флага fixture использует обычный `AICF_LoadoutDraft`.
Для server audit без `-Batches` обязательна ровно одна точная пара
`-aicfShutdownMode idle` в manifest и единственный `SHUTDOWN_DONE mode=idle`.
Batch manifest либо batch completion отклоняются независимо друг от друга:
пакеты операций не должны попадать в idle envelope. Это ограничение не
распространяется на `-Client`, который может сопровождать оба server mode.
Weak `array<IEntity>` не удерживает world; snapshots удаляют повторы по native
pointer identity и проверяют liveness через следующие кадры и период покоя.
На baseline после каждого пакета остаются ещё 536 уникальных entities;
фикс должен давать ноль как немедленно, так и на отложенных snapshots.

Native memory содержит обычные прогревы resources и активный gameplay. До/после
каждого синхронного пакета число живых draft должно совпадать; две постоянно
используемые service models допустимы до Stop, после Stop — ноль. Для сравнения
памяти фиксируются все native stats и idle envelope до финального batch run.
Working set не является самостоятельным доказательством. Полный DumpInstances
с `-checkInstance` оказался слишком объёмным и задерживал simulation; такая
диагностическая попытка не входит в PASS измерений.

Свежие verdict, ограничения и пути evidence — в [TESTING.md](TESTING.md).
