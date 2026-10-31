# Начальные emotes и приветствие игрока

## Поток обновления дыма сигареты

В 1.8.0.13 native `SCR_CigaretAnimationComponent.EOnPostFixedFrame` вызывает
`ParticleEffectEntity.Play`/StopEmission для отдельных entities. В local Play
North Everon RHS Standard Hard это дало native assertion `Attempt to modify
locked entity from a different thread` на строках 74/75 (enf_entity.cpp
415/575/1894). Это выполняемый игровой путь, не ошибка меню/компиляции.

`Integration/AICF_CigaretteAnimation.c` сохраняет native создание, эмиссию,
накал и удаление эффектов, но после OnAddedToParent переводит компонент
с POSTFIXEDFRAME на FRAME. Оставшийся fixed dispatch ничего не делает;
обычный frame проверяет character parent и оба emitter перед native update.
Отсоединение и удаление снимают обе event masks. Новых таймеров/RPC нет.
Адаптер касается локальной косметики всех stock cigarette components;
authoritative правила матча и выбор idle не меняются.

Отдельная fixture создаёт настоящий stock cigar prop на живых roster бойцах
обеих сторон, многократно вызывает естественный переход охлаждение → Play,
проверяет оба emitter, фактическую event mask, FRAME callbacks без fixed
callbacks, detach и delete. Это не проверка художественного вида дыма или
точного положения сигареты в руке во всех анимациях.

При client streaming возможен скачок world pose локального cigar prop: в full
UI/JIP log наблюдался переход с y≈-828м к реальному бойцу и native
`ParticleEffectComponent: Suspicious parent velocity` >1млнкм/ч. Это не движение
бойца. Простая замена FollowParent на прямой Parent в controlled A/B не помогла.
FRAME adapter теперь сравнивает последовательные prop positions. Скачок больше
`max(5м, timeSlice*100м/с)` сбрасывает только motion history двух собственных
particle entities через их удаление и штатное повторное создание. Прежние
PLAYING состояния восстанавливаются в том же frame; нагрев/охлаждение, native
FollowParent/offset/pivot и VISIBLE policy mouth сохраняются. После native
создания снова снимается POSTFIXEDFRAME и включается FRAME. Pose sample
сбрасывается при detach/delete. Actor, ТС и игровой lifecycle не меняются.
`CIGARETTE_POSE_RESYNC` фиксирует расстояние/время и сохранённые playing states.

`AICF_CigaretteStreamingProbe.c` — отдельная direct-connect fixture вместе с
VehiclePresetProbe/VehiclePresetClientProbe, которые дают настоящий avatar.
Только локальный cosmetic prop получает заведомо большой скачок pose; actor/ТС
не перемещаются. Проверяются три цикла, actual cigar world offset/rotation и
leading parent, mouth parent/position, PLAYING/active particles/simulation clocks,
FRAME-only callbacks и удаление именно CURRENT emitter entities после resync.
Для наблюдения невидимого mouth fixture отдельно включает его FRAME updates;
production VISIBLE policy не меняется. Без такого observer native clock на
невидимом avatar был0 при PLAYING/active1; прежний диагностический FAIL сохранён.

`Test-CigarettePoseContracts.ps1` исполняет извлечённые production threshold и
recreate body: границы distance/dt, все четыре сочетания playing states, отказ
создания одного emitter, FRAME masks и неизменный heat. `Test-CigaretteLog.ps1`
теперь отклоняет Suspicious parent velocity в любом полном sibling log;
положительный summary не скрывает native error. Свежие targeted contracts5/5
PASS и Stock native24/24/WB0/server0/client0/fullstopped относятся к evidence
`.codex-runtime/issue78-finalization-20261029/smoke/production-fixed-v1`.
В formal probe velocity/locked-thread/script-fatal0; strict resources остаётся
FAIL. Controlled emission/pose proof не подтверждает художественный вид дыма.
Исторический experimental `observer-resync-v2` всё ещё имеет full-log FAIL:
12 console velocity errors во время дальней pose до обратного скачка, при
24/24 functional checks. Точная причина этих post-sim перемещений не доказана;
этот запуск не переобъявлялся PASS по одному чистому хвосту или по summary.
Formal production run и свежий UI/JIP оцениваются отдельно по полным логам.

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
