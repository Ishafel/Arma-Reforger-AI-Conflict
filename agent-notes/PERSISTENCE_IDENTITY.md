# Persistence при direct-connect без backend identity

`AICF_PlayerPersistenceIdentity` защищает существующий `SCR_SpawnLogic` в
`OnPlayerAuditSuccess_S`. Только на server, при доступном native persistence,
положительном playerId и пустой backend identity пропускается штатный вызов
`SetId(controller, identity)`. Вместо назначения `NULL_UUID` вызывается публичный
`StopTracking(controller, false)`: anonymous controller остаётся nonpersistent,
как после штатного SetId, а config освобождается через обычный boundary. Записи
не удаляются, выдуманная backend identity не назначается. Остальные пути вызывают
`super` ровно один раз. Disconnect остаётся у native; сохранение мира, reconnect,
персонаж, библиотека и права игрока не отключаются. Идентификаторы аккаунтов
не выводятся в диагностику.

## Доказательство и границы

В закреплённом API1.8.0.13 `SCR_SpawnLogic.OnPlayerAuditSuccess_S` только получает
controller/identity и вызывает `PersistenceSystem.SetId`. На dedicated без BI
backend `SCR_PlayerIdentityUtils.GetPlayerIdentityId` возвращает пустой UUID.
Native-only trace показал у обоих клиентов `tracked=1` до callback и `tracked=0`
после него; disconnect уже не сериализовал controller. После двух подключений
и полного shutdown оставались8 leaked instances config/rule/serializers.

Изолированный A/B guard пропустил только назначение пустого identity. Controller
оставался tracked до disconnect, штатный serializer выполнялся, затем native
снимал регистрацию. Native exits0/0/0, полный shutdown save завершился; leaks0.
В этом control без production остались2 прежних resupply teardown errors:
его полный fatal gate не объявляется PASS.

Первый вариант guard на полном production graph проверен отдельно: Stock Workbench0,
client34/JIP8, server readbacks COMBAT/437, native0/0/0/fullstopped. Неизменённый
frozen `Test-VehiclePresetClientLog.ps1` вернул0; SCRIPT E/F, VM и все leaks0
во всех sibling logs. Строгая проверка всех native Error остаётся FAIL из-за
ресурсов/сети: raw sibling lines352, не352 независимых дефектов.

Evidence29.10.2026: `.codex-runtime/issue78-finalization-20261029/persistence/`
(`trace-v2`, `guard-v1`) и `backend-persistence-production-v1/verified-result.json`.
Второй isolated A/B `stop-null-v1` сохраняет исходное nontracked поведение:
`StopTracking(controller,false)` до назначения пустого UUID, оба controller
имеютtracked0послеaudit, serializer не вызывается на anonymousdisconnect,
native0/0/0/leaks0/успешныйSHUTDOWNsave. Он выбран для production вместо первого
варианта, чтобы не сохранять недоступные по backend anonymous player records.
Уточнённый вариант проверен на полном production graph в `backend-ui-current-v4`:
Stock Workbench0, client34/JIP8, два server readback, native0/0/0 и все три
остановленных процесса. Неизменённый frozen `Test-VehiclePresetClientLog.ps1`
вернул0; во всех sibling logs SCRIPT E/F, VM, assertions и persistence leaks0.
Строгий all-native Error gate остаётся FAIL:348 raw sibling lines
(server32/client158/JIP158); это не348 независимых дефектов. Cigarette parent
velocity errors в этом прогоне0 после отдельного исправления smoke adapter.
SHA исходников, auditor и полных логов сохранены в `verified-result.json`.
Первый fullPASS выше относится к предыдущему snapshot и не подменяет эту проверку.

Промежуточный `backend-persistence-smoke-final-v2` сохранил functional FAIL:
fixture не нашла безопасную площадку для server-spawned proxy. В `proxy-candidates-v3`
proxy создан, но первый клиент подключился лишь через9 минут: живой отряд уже
потерял бойца, Apply честно отклонён `MEMBER_INACTIVE`, JIP не получил assignment.
Его сервер остановлен адресно, native-1/ABORTED; это не полный stopped PASS.
Fixture sampler расширен с4 до24 безопасных кандидатов при прежних clearance и
дистанции10..20м; свежий v4 подключал клиента сразу после roster readiness.
Старые FAIL сохранены. Прежний вывод «безопасный script workaround не найден»
устарел: отсутствие production scripts доказывало воспроизведение native ошибки,
но не невозможность защитить её входные данные. Исправление не является
подтверждением устранения native `SetId(NULL_UUID)` в установленном движке.

`Test-PlayerPersistenceIdentityContracts.ps1` исполняет извлечённое production
тело callback в32 комбинациях authority/persistence/playerId/identity: пустая
backend identity снимает только tracking без удаления записей; непустая identity
и остальные ветки делегируются native. Настоящий authenticated account
persistence/restart этим не проверен; guard не выдаёт session identity за backend identity.

## Дополнительная диагностика

`tests/fixtures/AICF_PersistenceReconnectTrace.c` предназначена только для
isolated stages. Без fixture flag A/B она наблюдает native audit/disconnect и
serializer; не вызывает `GetConfig`, не удерживает config/serializer references.
`-aicfPersistenceReconnectTrace 1` включает boundary logs и boolean наличия
backend identity. `-aicfPersistenceGuardNullIdentity 1` — только экспериментальный
native-control guard; дополнительный `-aicfPersistenceStopNullTracking 1` выбирает
вариант с публичным StopTracking. Production эти параметры не читает. `BaseContainerProps`
обязателен у modded spawn logic: без него fixture ломает native config loading,
что отдельно сохранено как неудачный trace-v1, а не дефект production.
