# AI Conflict Everon RHS — Workshop metadata

Этот файл — источник полей описания для существующей
[страницы Workshop](https://reforger.armaplatform.com/workshop/FA9FDCCA428A43BA).
Поля переносятся владельцем в `Publish Project` по
[инструкции публикации](../../docs/PUBLISHING.md). Номер следующей версии
определяется перед upload; изменение этого файла не обновляет Workshop.

## Поля Publish Project

- Project Name: `AI Conflict Everon RHS`
- Workshop ID: `FA9FDCCA428A43BA`
- Tags: `AI CONFLICT EVERON RHS USMC MSV`

При обновлении описания сохранить текущие visibility, license и preview.
Локального `preview.jpg` для этого addon пока нет; имеющиеся четыре обложки
описаны в [PROMPTS.md](../PROMPTS.md).

## Обязательные зависимости

Список соответствует `AIConflictEveronRHS/addon.gproj`:

- Arma Reforger: `58D0FB3206B6F859`;
- AI Conflict Core: `9178E5822AFE48EA`;
- AI Conflict Arland: `B52C5F6AEDBF423E`;
- AI Conflict Everon: `A4B2E62595F645A4`;
- RHS Content Pack 01: `1337C0DE5DABBEEF`;
- RHS Content Pack 02: `BADC0DEDABBEDA5E`;
- RHS - Status Quo: `595F2BF2F44836FB`;
- AI Conflict Arland RHS: `9F88011DA22B471C`.

## Summary

Autonomous RHS USMC-versus-MSV campaigns on Everon. Adds full-island and
Northern Everon scenarios. Requires AI Conflict dependencies and RHS packages.

## Description

AI Conflict Everon RHS adds autonomous RHS USMC-versus-Russian-MSV campaigns
on Everon, using the official RHS Conflict world. Both sides select objectives,
deploy and reinforce squads, capture bases, use ground transport, tickets and
supplies, and fight to a server-authoritative match result.

Included scenarios:

- `AI Conflict RHS - Everon`: the campaign on the full island.
- `AI Conflict RHS - Northern Everon`: a campaign with two headquarters and
  six capture points in northern Everon. Southern bases are inactive; the
  full island terrain remains accessible.

Required Workshop dependencies:

- AI Conflict Core;
- AI Conflict Arland;
- AI Conflict Everon;
- AI Conflict Arland RHS;
- RHS Content Pack 01;
- RHS Content Pack 02;
- RHS - Status Quo.

The Arland dependencies provide shared integration and RHS content support.
The scenarios supplied by this addon take place on Everon.

How to use:

1. Install and enable `AI Conflict Everon RHS` and all required dependencies.
2. Open `Scenarios` and select `AI Conflict RHS - Everon` or
   `AI Conflict RHS - Northern Everon`. Scenario names follow the game language.
3. You can also join a server running the desired scenario and its complete
   dependency set. Other scenario entries are visible through dependencies;
   choose the RHS Everon entry for this addon.
4. Both factions use AI commanders by default. Session saves are disabled;
   every local play or hosted server starts a new campaign.

No RHS world or content assets are copied or redistributed by this addon.
RHS packages retain their own licenses.

Русский:

AI Conflict Everon RHS добавляет автономные кампании RHS USMC против российских
МСВ на штатной RHS-карте Эверон. Стороны самостоятельно выбирают цели,
создают и пополняют отряды, захватывают базы, используют наземный транспорт,
билеты и снабжение.

В addon входят два сценария:

- `AI Conflict RHS — Эверон` — кампания на полном острове;
- `AI Conflict RHS — Север Эверона` — два штаба и шесть точек захвата на севере.
  Южные базы не участвуют в кампании, ландшафт всего острова остаётся доступным.

Установите и включите `AI Conflict Everon RHS` и все перечисленные зависимости,
затем откройте `Сценарии` и выберите нужную RHS-плитку Эверона. При английском
языке игры названия — `AI Conflict RHS - Everon` и
`AI Conflict RHS - Northern Everon`. Для сетевой игры можно подключиться
к серверу с выбранным сценарием и полным набором зависимостей.

`AI Conflict Arland` и `AI Conflict Arland RHS` нужны как общие зависимости;
карта этих двух сценариев — Эверон. По умолчанию обеими сторонами командует ИИ.
Сохранение прогрессии отключено: каждый локальный запуск или hosting начинает
новую кампанию. Ресурсы RHS не включены в addon и загружаются отдельно.

## Change Notes для обновления описания

Исправлены Summary, Description и Tags: указаны Эверон, оба RHS-сценария,
зависимости и порядок запуска из меню. Этот пункт описывает только обновление
metadata; при публикации вместе с кодом добавить изменения соответствующего
релиза из `releases/`.
