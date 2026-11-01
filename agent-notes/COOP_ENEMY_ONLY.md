# Co-op: автономная армия только у противника

Уточнение пользователя01.11.2026: сторона игроков в Co-op не должна иметь
автономных боевых отрядов. В2471562 требование «для обеих сторон» было
неверно истолковано как10+10. Исторические проверки этого commit доказывали
ошибочное поведение и не подтверждают исправленный контракт.

## Поведение

| Режим | Автономный US | Автономный USSR | Commander |
|---|---:|---:|---|
| STANDARD |10|10|BOTH|
| Co-op за US |0|10|USSR|
| Co-op за USSR / РФ |10|0|US|

`AICF_MatchRules` исключает стабильную faction стороны игроков при подсчёте
slots. Runtime faction keys RHS нормализуются прежним content profile.
FactionState и экономика сохраняются; initial spawn, replacement, recruitment
и vehicle lifecycle обходят нулевое число slots без отдельного удаления AI.
Минимальный управляемый roster снова100 в Co-op,200 в STANDARD;
обычный запас конфигурации даёт default120/220.

`AICF_CoopFleetDefaults.Apply` отвергает сторону без managed slots до загрузки
каталога и preview. Противник получает4 грузовика,1 вооружённую лёгкую машину,
2 БТР; WCS ещё2 танка, остальные пешие. При выборе противоположной стороны
та же схема применяется к другой армии. Ручные назначения не перезаписываются.
Launcher выбирает совместимый commander; несовместимый явный CLI по-прежнему
отклоняется существующим CONFIG_INVALID gate.

Командная панель людей использует прежний player-only gate. Их поражение
остаётся территориальным; автоматическая стройка отключена. Штатные player
groups, купленные бойцы, строители и заказанная логистика имеют прежних владельцев.
RU/EN подсказки и брифинг снова описывают отсутствие автономной армии людей.

## Проверки

- До правок test/fixture:5/5 профильных проверок старого контракта PASS.
  Это baseline ошибки, не PASS нового требования.
- Та же новая actual-source проверка против policy из2471562 даёт ожидаемый
  FAIL `ENEMY_COMMANDER_1`; с исправленным commander, но старым10+10 predicate
  даёт FAIL `MANAGED_ROSTER_1_US`. Текущий0/10 контракт проходит без замены assertions.
- Новые профильные5 и соседние4 —9/9 PASS. Сохранены actual policy/mode,
  runtime faction mapping, Standard, player-only UI, ticket/construction,
  identity и отрицательные проверки логов.
- `Build-AICFLocalization.ps1`:720 entries; свежие LocalizationStatic и
  RuntimeLauncherStatic —2/2 PASS. Первый launcher baseline пересёкся по времени
  с записью tools policy и не используется как чистое доказательство «до»;
  исходный результат и времена сохранены, актуальный запуск выполнен отдельно.
- Workbench на изолированном393 production snapshot и двух read-only probes:
  Stock/RHS/WCS3/3 compile PASS, native0/validation/fatal0.
  Resource50/52/78 остаётся строгим FAIL.
- Native Stock и WCS за каждую из двух сторон плюс Stock STANDARD:
  5/5 functional PASS, Fleet70/70, Scenario92/92,175 postwitnesses.
  Во всех четырёх Co-op запусках human groups/bindings/model inspection/spawn0;
  у противника10groups,7bindings в Stock или9 в WCS. STANDARD сохранил10+10.
  Каждый прошёл30s observation, public CoopScenarioLog exit0, native0,
  Game destroyed,393 source drift0/fatal0. Resource28/28/206/206/28 — FAIL.
- Полный offline284 в этой коррекции не повторялся. Новый результат —11/11
  профильных и соседних проверок; прежний284/284 относится к2471562.

Evidence: `.codex-runtime/coop-enemy-only-20261101/` и
`.codex-runtime/coop-human0-correction-20261101/REPORT.md`, native matrix:
`.codex-runtime/coop-enemy-only-native-20261101/REPORT.md`.
Все команды запуска — через `tools/Start-AICFRuntime.ps1`; GUI automation нет.
Работающий retail клиент игрока и его матч не перезапускаются автоматически.
Новая policy требует перезапуска клиента и нового матча, а не удаления групп
из уже начатой игры.

Strict resource/native-only persistence blockers и исходные window/OOM остаются
отдельными; это исправление их не объявляет устранёнными. Human UI/input —
NOT RUN: агент не управляет GUI. Новый direct-connect client/JIP не запускался;
работающий клиент пользователя использует прежний build. Native server policy
не заменяет проверку нового клиентского пути. Issue78 остаётся OPEN, PR79 draft.
