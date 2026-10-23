# Контекст смерти и INSTIGATOR_OTHER

`AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_InstigatorContext.c` расширяет
`SCR_InstigatorContextData.CreateInstigatorData` для штатного `INSTIGATOR_OTHER`.
Механика общая для Standard/Co-op, vanilla/RHS/WCS и всех integrations.

## Причина и поведение

В Script Diff **1.8.0.13** enum `InstigatorType` содержит OTHER, а
`Game/Damage/SCR_InstigatorContextData.c` умеет классифицировать только AI,
PLAYER и GM. При OTHER native code пишет SCRIPT(E), затем продолжает с
UNKNOWN либо оставшимся от предыдущего вызова killer control type. Источник
без character/faction ошибочно попадает в KILLED_BY_NEUTRAL_OR_FACTIONLESS;
повторно используемый context может сохранить прежний AI control type.

Старый WCS log доказывал эту ошибку во время Physics simulation, но не
устанавливал конкретный столкнувшийся объект. Новый deterministic probe
создаёт настоящий GenericEntity → Instigator OTHER, строит contexts и вызывает
native Kill реального ИИ с тем же Instigator. Исходный production дал четыре
одинаковых SCRIPT(E) и четыре ошибочных readbacks. Исправление классификации
не доказывает устранение физического повреждения из исторического WCS run.

| Вход | Обработка |
|---|---|
| AI, PLAYER, GM, NONE, остальные значения | Штатный `super.CreateInstigatorData` с исходными аргументами |
| Удаление либо жертва не является character, включая null | Штатный ранний путь, в том числе при OTHER |
| OTHER с распознанным character source | Native `Instigator.CreateInstigator(source)` определяет AI/PLAYER; штатный context вычисляет suicide, factions, disguise, player ID и penalty rules; исходный Instigator возвращается в context |
| OTHER без распознанного управляющего character | Исходные victim/killer entities и Instigator сохраняются; killer control UNKNOWN, relation OTHER_DEATH; victim ID/control и perceived faction вычисляются штатными helpers |

Caller `killerEntity` имеет приоритет над `GetInstigatorEntity`, как в native.
Переклассификация character разрешена только при native результате AI/PLAYER;
OTHER не объявляется AI без такой идентичности. Сам исходный Instigator остаётся
неизменным и доступным native consumers. Environmental branch сбрасывает
производные control/disguise поля перед расчётом, включая повторное использование.
Native friendly/player attribution и правила штрафов остаются у super для
распознанного character. У OTHER без управляющего нет выдуманного игрока-убийцы.

Изменяется только transient context. Damage, здоровье, стратегия, spawn/delete,
фракции, ownership, score transactions и репликация остаются у своих владельцев.
Новых callback/subscription или lifecycle в production adapter нет.

## Проверки

`Test-InstigatorContextStatic.ps1` защищает passthrough, caller priority,
native character resolution, исходные metadata, сброс полей и OTHER_DEATH;
14 negative mutations. `Test-InstigatorContextContracts.ps1` проверяет строгий
log gate, включая обязательный отказ при native SCRIPT(E), live/неполном логе,
отсутствии case/CLI/death callback и неправильном completion.

`tests/fixtures/AICF_InstigatorContextProbe.c` копируется только в UI изолированного
stage как `AICF_ZInstigatorContextProbe.c`, после основного Co-op probe.
`-aicfInstigatorProbe 1` выполняет настоящие native constructor/readback cases,
убивает одного managed ИИ через `Kill(OTHER)` и проверяет context из настоящего
`GetOnControllableDestroyed` invoker. `Stop` снимает Insert и удаляет только
созданный fixture GenericEntity. Боевая группа восстанавливается обычным путём.

В Standard проверяется **18** случаев, включая native enemy AI; в Co-op с
`-aicfInstigatorPlayer 1` probe ждёт настоящего controlled player и проверяет
также native player attribution, OTHER с caller player override и player victim
context — **20** случаев. Это constructor/readback player cases, а не реальная
смерть игрока или ручная проверка экрана смерти.

`Test-InstigatorContextLog.ps1 -LogPath ... -LauncherPath ... -NativeExitPath ...
[-Player]` сначала выполняет полный `Test-CoopScenarioLog`, затем требует exact
cases/CLI и успешное завершение. SCRIPT(E/F)/VM/fatal остаются FAIL.
Client log отдельно проходит `Test-CoopScenarioLog -Client -RequirePlayer -RequireWelcome`.
Свежие результаты и ограничения — [TESTING.md](TESTING.md).
