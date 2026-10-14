# Начальные emotes и приветствие игрока

Общий server-owned `AICF_SocialService` обслуживает только готовые живые
пехотные slots US/USSR. `AICF_MatchController` создаёт и останавливает сервис.
Сервис не назначает waypoint, не меняет roster, очередь, цены или темп набора.
Faction mapping берётся из `AICF_ContentProfile`; обязательных RHS/WCS ресурсов
в Core нет.

## Допуск и отмена

Idle разрешён только при первой generation и начальном `INFANTRY_MUSTER`,
начальном наборе либо `IsAwaitingPlayerCommand`. Первый выход закрывает этот
допуск на оставшийся матч. Replacement и последующее восстановление потерь
не возвращают idle. FIA, строители и vehicle slots не входят в roster сервиса.

Проверяются group/member identity, generation, readiness, живой AI, authority,
текущий waypoint, отсутствие игрока, compartments, лечения/использования
предмета, ADS, падения/плавания/лазания и движения. Native behavior допускает
idle или остановившуюся оборону своего waypoint; другие behavior не вытесняются.
Threat state должен быть SAFE, threat measure не выше 0.01. Проверяются как
фактическая скорость, так и запрос движения AI.

Сид матча и индекс бойца дают разные варианты и задержки 750–4999 ms, не
затрагивая планировщик набора. Активная анимация не перезапускается на tick.
Перед началом из crouch/prone запрашивается штатная стойка STAND, затем native
`StartLoitering(..., holsterWeapon: true, ...)` временно убирает оружие через
`EEquipTypeUnarmedContextual`. Native переход владеет возвратом прежнего оружия
после loiter; постоянный `SelectWeapon(null)` и отдельная подготовка рук удалены.
Каждый этап подготовки ограничен 4000 ms. Неудача освобождает состояние без
изменения приказа; отмена очищает также ещё ожидающую holster заявку.
При отмене до входа в loiter команда holster может ещё ожидать следующий frame,
даже при `IsChangingItem == false`. Controller временно сохраняет исходное оружие
и actor identity; штатный `OnApplyControls` ждёт завершения unequip и возвращает
оружие через `TryEquipRightHandItem`. Ожидание ограничено 4000 ms, не использует
CallLater и не допускает новый social idle. Проверяются group/generation/member,
weapon entity ID и наличие в weapon manager; смерть, player control, новый loiter,
использование предмета или другой предмет в руках прекращают восстановление.
Уже безоружный боец остаётся таким. Обычный завершившийся idle использует native
contextual recovery без этого дополнительного пути.

`GetOnPresentationInvalidated` у slot уведомляет сервис при смене assignment,
снятии waypoint и передаче waypoint технике. Анимация отменяется в этом же
вызове. Остальная допустимость перепроверяется каждые 250 ms. Stop снимает
подписки и таймер. Старый member/generation не получает нового запуска.
Native `StopLoitering(true)` в 1.8 всё ещё делает мягкий выход, а `FinishLoiter`
снимает scripted command без гарантированного выхода client graph. Это давало
скольжение с продолжающимися отжиманиями после приказа. `AICF_SocialLoiterExit`
для собственного idle переводит command в DONE и вызывает `CMD_Gestures=-2`
(переход `Gestures -> Locomotion` vanilla 1.8.0.13), затем `FinishLoiter`.
DONE не позволяет `OnDeactivate` перезаписать hard exit мягким `-1`; штатное
удаление локального предмета и снятие attachment сохраняются.

Отмена доставляется reliable RPC на том же character controller, что native
start: порядок start/stop сохраняется. Authority проверяет ownership и точный
объект `m_CustomAnimData`; receiver проверяет type/config текущего context.
Ожидающая holster/align заявка очищается вместе с context; повтор без context
ничего не меняет. Отдельные таймеры и delayed callback на бойца не используются. Обычные
чужие loiter не получают нового поведения через глобальный override остановки.

## Четыре варианта vanilla 1.8.0.13

| Вариант | Штатный config | Native type / preset |
|---|---|---|
| Smoke | `Emote_Smoke.conf` | SMOKING / CIGAR |
| Pushups | `Emote_Pushups.conf` | PUSHUPS |
| Sit | `Emote_Sit_chair.conf`, inherited `Emote_Sit.conf` | CUSTOM / CHAIR |
| Sit on the ground | `Emote_SitGround.conf` | SIT |

Все configs лежат в `Configs/Commanding/Emotes/`. Обычный `Emote_Sit.conf`
сам не создаёт опору. Выбран его штатный вариант CHAIR: vanilla custom data
создаёт локальный временный стул и удаляет его после выхода. Мир и установленные
ресурсы не меняются. Нужна допустимая стойка STAND; если выбранный тип нельзя
запустить, проверяется Smoke, иначе запуск отклоняется. Существующие чужие
loiter/gesture не отменяются ради нового idle.

## Приветствие

Новый вход союзного живого игрока в радиус 5 м выбирает ближайшего допустимого
бойца; одинаковые расстояния разрешаются по immutable entity ID. Гистерезис
сбрасывает вход только дальше 7 м. Расстояние для повторного взведения учитывает
союзных живых управляемых бойцов, включая временно занятых. Непрерывное нахождение
рядом и колебание возле 5 м не создают периодического приветствия.

Один server ledger на identity игрока ограничивает все пары с ботами.
Reservation предшествует попытке SALUTE и рассылке; один actor не принимает
второй token. По уточнению пользователя #38, SALUTE опционален и не вытесняет
Smoke, Pushups или Sit. Во время собственного idle боец произносит только
фразу; отдельный deadline звука не останавливает и не перезапускает emote.
Завершение реплики и disconnect игрока освобождают только greeting-состояние.
Приказ, угроза, смерть и Stop по-прежнему отменяют оба состояния.

SALUTE через native `TryStartCharacterGesture` допускается только вне idle,
loiter, другого gesture, смены предмета/стойки и при STAND. После собственного loiter дополнительно выдерживается 4000 ms для native out-перехода;
в этот период возможна только фраза. Ради приветствия
не меняются поза или предмет в руках. Если native API отказывает, фраза всё
равно отправляется; `GREETING_MODE salute=0` фиксирует отсутствие салюта.
Общий cooldown 30000 ms начинается перед отправкой звука независимо от SALUTE;
частичная ошибка звука не снимает защиту. Время приветствия ограничено 3500 ms.
Новый подход после окончания cooldown разрешён; первый подход не задерживается.

Backend identity переживает смерть, смену персонажа и переподключение внутри
матча. До backend audit используется connection sequence + controller identity;
disconnect убирает временную связь player ID. При появлении backend identity
накопленный cooldown переносится. Без доступной стабильной identity reconnect
не может быть надёжно связан с прежним человеком.

## Звук и сеть

Обе стороны используют один английский event
`SOUND_TUTORIAL_AM_GREETING_C_SOLDIER_E_01` из
`Sounds/VO/Tutorial/Tutorial_VO_Greetings_Soldier_E.acp`. Его единственный sample:
`{F63EDBBB092E14B4}Sounds/VO/Tutorial/Samples/Greetings/Soldier_E/Tutorial_AM_Greeting_C_Soldier_E_01.wav`.
Запись «Good to see you, boss!» не выбирается через язык/faction voice profile.
WAV и configs не копируются в Git.

`AICF_GreetingAudio` на stock game mode доставляет reliable broadcast с token
и RplId. Клиент вызывает `AudioSystem.PlayEvent` с explicit project и transform
головы бойца, обновляет transform и прекращает звук при отмене/смерти/Stop.
Проект не требуется добавлять к sound component конкретного персонажа.
Монотонный token подавляет дубли; одноразовое событие не хранится в RplProp и
не воспроизводится из JIP snapshot. Для отсутствующей streamed entity нет retry.
AudioHandle/состояние API не являются подтверждением субъективного звучания.
Первый sample может загружаться асинхронно: `IsSoundPlayed == false` сразу после
создания handle не означает завершение. Handle сохраняется до ограниченного
deadline либо явной отмены, проект предварительно загружается.

События: `IDLE_REQUESTED`, `IDLE_STARTED`, `SOCIAL_START_FAILED`,
`GREETING_MODE`, `GREETING_STARTED`, `SOCIAL_STOPPED`; поля slot,
generation, member, variant, token, phase, now_ms. `GREETING_AUDIO` отдельно
фиксирует клиентский handle и результат создания звука.

Проверки и реальные ограничения — [TESTING.md](TESTING.md); fixture находится
только в изолированном stage.
