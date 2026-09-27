# Возрождение за ИИ своего отряда

После смерти живые ИИ своего отряда отображаются карточками в штатной галерее
экипировки рядом с сохранёнными пресетами. Выберите карточку бойца и нажмите
штатную кнопку развёртывания: для выбранного ИИ она называется «Принять
управление». Отдельной панели нет. Список обновляется каждые две секунды.
Выбор обычного пресета возвращает штатное возрождение на выбранной точке.
Штатный таймер возрождения продолжает действовать; при отказе из-за таймера
можно повторить выбор позднее.

Игрок получает управление существующим персонажем: его местоположение,
состояние и экипировка сохраняются. Новый боец не создаётся, лечение,
телепортация и выдача сохранённого player loadout не выполняются. Речь о
нативной player group, в которую входит игрок, а не о любом союзном AI-отряде
под стратегическим командованием. Initial join и живой игрок не имеют доступа
к такому способу возрождения.

Состав включает бойцов основной группы и её штатной подчинённой AI-группы
(`GetSlave()`): native recruitment хранит приглашённых ИИ именно там.
Проверяется обратная связь `GetMaster()` и faction. Способ попадания бойца
в отряд не должен требовать от игрока дополнительных действий.

Штатная игра при передаче управления может добавить предмет личных вещей
(`PersonalBelongings_US` в проверенном stock US сценарии). Исходные оружие,
одежда и содержимое инвентаря при этом сохраняются.

## Владельцы и проверки

- `Respawn/AICF_SquadRespawnPolicy.c`: серверная допустимость, native death/spawn
  callbacks и immutable identity попытки. `BaseContainerProps` сохраняет
  регистрацию модифицированного штатного `SCR_SpawnLogic`.
- `Respawn/AICF_SquadRespawnRpc.c`: player-owned RPC передаёт только `RplId`
  выбранного персонажа и revision смерти. Сервер повторно определяет игрока,
  faction, текущую player group, наличие живого AI-agent и отсутствие другого
  владельца персонажа. Snapshot и результат получает только owner.
- `Respawn/AICF_SquadRespawnHandler.c`: native `SCR_PossessSpawnData` проходит
  штатный respawn pipeline и его таймер. Перед окончательным `AssignEntity_S`
  повторно проверяются entity/group identity, membership и death revision.
  Identity непосредственной AI-группы также сохраняется в попытке: перенос
  бойца в другую группу или замена slave до callback приводит к отказу.
  Отказ не удаляет AI. Обычные native possess requests сохраняют своё поведение.
- `UI/AICF_SquadRespawnUI.c`: native `SCR_LoadoutButton` в `SCR_LoadoutGallery`,
  выбор по `RplId` и обычная `RequestRespawn` кнопка `SCR_DeployMenuMain`.
  Карточки ИИ не подменяют player loadout и не отправляют запрос пресета.
  Истёкшая death revision/исчезнувшая цель снимает выбор; cleanup удаляет
  собственные карточки и подписки. Пресеты остаются под управлением vanilla.
  Карточка получает native icon из prefab, выбранный боец показывается через
  `ItemPreviewManagerEntity.SetPreviewItem` с его текущей экипировкой. Пока
  entity не загружена клиентом, используется prefab preview; после streaming
  он заменяется live preview. На сервере персонаж/экипировка не изменяются.

Pending context сохраняется до native response, даже если наступила следующая
смерть: устаревший callback должен пройти проверку revision и получить отказ.
Повторный параллельный запрос не запускает ещё одну передачу управления.
Постоянное состояние отряда остаётся у native groups/possessing managers;
отдельная копия roster для JIP не создаётся.

События: `[AICF][STAGE4][INFO][SQUAD_RESPAWN_REQUEST]` содержит `player`,
`character`, `group`, `death_revision`, `authority`; `SQUAD_RESPAWN_RESULT`
содержит `player`, `character`, `result`, `authority`.
`SQUAD_RESPAWN_LIST` при смене death revision или числа доступных бойцов
содержит `player`, `death_revision`, `group`, `eligible`, `authority`.

## Проверки

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-SquadRespawnContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage3Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage35Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-Stage4Static.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-LocalizationStatic.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Test-SquadCommandsContracts.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/Build-AICFLocalization.ps1 -Check
```

Workbench Validate/Compile выполняется по `DEVELOPMENT.md`. Для runtime
скопируйте addons в отдельный staging, допишите содержимое
`tools/fixtures/AICF_SquadRespawnProbe.c` в конец staging-копии
`AIConflictCore/Scripts/Game/AIConflict/UI/AICF_SquadRespawnUI.c` (только в staging).
Порядок важен для наследования `modded` классов в Enforce. Скомпилируйте staging.
Запустите отдельные terminal server/client через
`Start-AICFRuntime.ps1`, `-Variant EveronNorth`, со свежими profiles,
`-RepositoryRoot <staging>` и `-AdditionalArguments @('-aicfSquadRespawnProbe','1')`.
Для сервера укажите также `-addr 127.0.0.1:<port>` и одинаковый `-ServerPort`
для обоих запусков; клиенту передайте точный `-ServerProfileRoot`.

Fixture добавляет экипированных AI штатным `AddAIToSlaveGroup` в player group,
проверяет отсутствие этих агентов в старом прямом `GetAgents`, убивает игрока и одного
из AI, затем вызывает production owner RPC. Проверяет недопустимость живого
игрока, мёртвой/чужой/вражеской цели, старой death revision и invalid RPC,
передачу того же персонажа, сохранение inventory entity identities и группы.
Клиентская часть проверяет наличие обычных пресетов и карточки ИИ в одной
native gallery, отсутствие старого overlay и вызывает production deploy action.
Результат оценивается по обоим полным остановленным логам и launcher manifests,
а не только по строкам probe. Fixture не входит в production addon.

Визуальная компоновка меню, ручной выбор мышью/контроллером, гонка двух реальных
игроков за одного AI, takeover бойца в технике и RHS runtime требуют отдельных
проверок; статический аудит и компиляция не означают их PASS.
