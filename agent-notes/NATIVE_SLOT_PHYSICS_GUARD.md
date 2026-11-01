# Native slot occupancy: отсутствующий Physics

`Integration/AICF_NativeSlotPhysicsGuard.c` устраняет конкретное разыменование
null в штатной проверке занятости площадки. В `SCR_EntitySpawnerSlotComponent`
из Script Diff1.8.0.13 метод `TraceCallback` вызывает
`ent.GetPhysics().GetSimulationState()` без проверки Physics. Это не изменение
квот, цены, регистрации площадок, разрешений покупателя или factory lifecycle.

## Исходный native FAIL

Catalog v27 восстановил корректную сериализацию SpawnerData: исходные три
USSR producer получили по13 assets. Их child/near arrays были пусты. Затем
обычный builder завершил гараж:05:05:20.155, parent20A0. В05:05:20.706 новый
producer23FB нашёл реальный NEAR slot23F8/type8, native compatible1,
distance_sq_xz27.3584 при radius10. Следующий штатный `IsOccupied` упал:

```text
05:05:20.707 SCRIPT (E): Virtual Machine Exception
Reason: NULL pointer to instance. Variable '#return'
Class: 'SCR_EntitySpawnerSlotComponent'
Function: 'TraceCallback'
SCR_EntitySpawnerSlotComponent.c:306 TraceCallback
SCR_EntitySpawnerSlotComponent.c:158 IsOccupied
AICF_ZZQuotaCatalogProbe.c:1578 IsOccupied
SCR_CatalogEntitySpawnerComponent.c:507 GetFreeSlotFromArray
```

Полный stack включает fixture observer и обычный native GetFreeSlot/Tick.
Observer вызывает super ровно один раз. На строке306 единственное
разыменование возвращённого instance — `GetPhysics().GetSimulationState()`;
`IsLoaded()` уже вызван на существующей entity. **Категория и identity
конкретной collision entity не записаны и остаются NOT PROVEN.**

VM остановил выполнение до исходного300s deadline. Server/Client
40559/14403 принудительно завершены только после exact PID/profile proof,
actual CLOSED1/native-1. Elevated CIM05:07:03.6842620+03:00 подтвердил
allArma/WB0. Это FAIL, не штатное окончание300s/25s tail. Public auditor:
Server fatal8/resource10/other24 (4полных sibling logs, включая crash),
Client fatal0/resource2/other24 (3siblings). Собственные четыре path-only
metadata Resource.Load дают8 из10 Server resource errors; они не объявлены
внешним baseline. Исправление этих ResourceName — отдельная работа стенда.

Полные журналы/SHA/CLI/manifest/exits/forced-closure evidence находятся в
ignored `.codex-runtime/issue78-finalization-20261030/
catalog-native-v27-serialized-data-registration/`. Исходный FAIL неизменяем.

## Конечный синхронный frame

Каждый `IsOccupied` создаёт managed frame, сохраняет предыдущий frame в
локальной сильной ссылке и вызывает **ровно один** `super.IsOccupied()`.
Callback сохраняет локально именно свой frame. При null entity или Physics
он помечает только этот frame как unknown и принимает collision hit.
Итог `nativeOccupied || frame.unknown` не позволяет null `TraceEnt`
превратить неизвестный hit в свободную площадку.

Вложенный вызов получает отдельный frame и восстанавливает предыдущий
только пока сам остаётся текущим. Потеря текущего frame даёт occupied;
устаревший вызов не очищает successor. Pure frame veto выполняется перед
штатным удалением wreck и перед native false-ветками. Новых async callbacks,
events, static/global anchors, native getter logging или дополнительных
TracePosition/IsOccupied нет. Frame не удерживает component, entity или мир.

Пустой protected hook `AICF_ObserveUnknownOccupancyHit(ent, reason)` вызывается
только для NULL_ENTITY/NULL_PHYSICS после установки captured frame unknown.
Core не читает метаданные и не пишет логи. Opt-in Catalog fixture наблюдает
точный активный slot/frame, использует прежний bounded512 Write/Lost и читает
EntityID/class/cached EntityPrefabData.GetPrefabName только после решения.
Context и последний slot проверяются после каждого native getter; nonnull
entity проверяется после самой native context validation. Snapshot явно
помечен after-decision/non-atomic. Наблюдатель не вызывает Physics, occupancy
или trace повторно и не может изменить неизвестный результат на свободно.

Для известного Physics сохранён исходный порядок: loaded до Physics;
simulation0/cloth/weapon, затем proxy, character и destroyed vehicle.
Единственный исходный `DeleteEntityAndChildren` остаётся в прежней
authority wreck-ветке; новое разрешение на delete не добавлено. Physics
читается один раз и используется локально. Загрузка, создание машин,
списание supplies и родительская slot registry остаются штатными.

## Производные классы и граница доказательства

В полном pinned `scripts/Game` единственный наследник slot —
`SCR_EntitySpawnerCharacterSlotComponent`: он добавляет waypoint metadata и
getters, не переопределяет IsOccupied/TraceCallback. Единственный slot
TracePosition с этим callback находится в базовом IsOccupied:158.

Для прямого unframed вызова известного nonnull Physics сохраняется исходная
policy (`null == null` frame остаётся текущим). Неизвестный callback hit
возвращает accepted, но полноценное доказательство occupied при null
TraceEnt относится к базовому IsOccupied wrapper. Неизвестный внешний mod,
который самостоятельно выполняет trace через inherited callback без super
IsOccupied, должен сохранять свой occupancy context; его поведение здесь
не объявлено проверенным. Другие native exceptions не перехватываются.

## Проверки и статус

Baseline: замороженный r30 All265/265 PASS предшествует этому production
изменению. Он не проверяет новый guard. Новый обязательный контракт:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-NativeSlotPhysicsGuardContracts.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File tests/contracts/Test-NativeSlotPhysicsGuardContracts.ps1
```

Контракт извлекает реальные production методы и выполняет их в C#5 replay
с явно моделируемыми native границами. Literal прежнего pinned callback
должен воспроизводить nullPhysics exception, новый — occupied. Проверяются
все1024 комбинации исходных filters/authority и unframed варианты, порядок
native чтений/delete, null TraceEnt, вложенные вызовы, scope loss на native
getters, запрет stale delete/false, сохранение successor. Inverse projection
новой known-Physics policy должна совпасть с прежним pinned body; source SHA
`4E0FCC958A1DD15951689F4038D7AA0692352846E7BE5B05F83F19EAC2CAB24F`.
Canonical checkout не обязан содержать ignored API cache.

После явного Root CPU release05:24:36 новый контракт прошёл под Windows
PowerShell5.1 и pwsh7: **2078 actual-source checks, source unsafe8, inverse
projection PASS** в каждой оболочке. Три выполняемых mutants в обеих
оболочках дали ожидаемый FAIL: удалённый unknown result, безусловное
восстановление поверх successor и снятый pre-delete frame fence.
Первые два WinPS запуска обнаружили только недостатки Enforce→C# адаптера
(не добавлены implicit public и super→base); оба FAIL и последующие
исправленные запуски сохранены. Production40E8B06D при этом не менялся.

После добавления empty after-unknown hook новый replay обеих оболочек дал
**2083 checks/source unsafe10/projection PASS**; три прежних выполняемых
mutants вновь дали ожидаемый FAIL в обеих оболочках. Catalog сохраняет старые
39/2old/6request и4registration controls и добавляет13 actual-source
unknown-hit lifecycle cases. Небезопасный порядок `!ent` до native context
validation воспроизводит getter после удаления hit; unsafe mutant обоих
shell даёт ожидаемый FAIL. Первые PS harness failures (EArray.Count binding и
двойная closure capture) сохранены и исправлены без изменения допуска.

Evidence: ignored `.codex-runtime/issue78-finalization-20261030/
native-slot-physics-guard-fix/`, `actual-WindowsPowerShell-v1/v2/v3.txt`,
`pwsh-actual-v1.txt`, шесть mutant logs и `dual-mutants-wall.json`.
Новый production387 прошёл терминальный Workbench Stock/RHS/ArlandWCSRHS:
actual14391 CLOSED0/native0/validation/full3fatal0/raw387SHA0; прочие Error
50/52/78 сохранены как strict FAIL. Expanded receipt:
`r31-production-expanded-verdict.json` под finalization20261030.
Combined Catalog388 на guard5E8C49C3/fixture70AF7372 отдельно compiled:
actual60357 CLOSED0/native0/full3fatal0/other0/resource50FAIL/source388SHA0,
`catalog-native-v28-physics-guard/expanded-compile-verdict.json`.
Runtime guard пока **NOT RUN**. Source replay и compile не заменяют настоящую
покупку. Для native
закрытия нужны свежий immutable stage, WB full sibling audit и separate
Server/Client Catalog run с настоящими factory/physical vehicle/debit,
исходным300s/25s tail и полными stopped logs. Новый PASS не заявлен.
