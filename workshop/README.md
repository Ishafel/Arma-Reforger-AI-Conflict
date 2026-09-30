# Workshop release assets

Этот каталог хранит проверяемый source of truth для полей ручной публикации.
Workbench не читает Markdown автоматически: значения копируются владельцем в
`Publish Project` по [инструкции публикации](../docs/PUBLISHING.md).

Шесть addon публикуются раздельно. Порядок зависимостей:
Core -> Arland -> Everon и Arland RHS -> Everon RHS.
Everon RHS требует обе ветви и RHS packages. Arland WCS + RHS зависит от Core, Arland и Arland RHS, а также пакетов WCS; публикуется после этой цепочки.

| Addon | Metadata | Сценарии при русском языке игры |
|---|---|---|
| AI Conflict Core | [metadata.md](AIConflictCore/metadata.md) | Общая зависимость, собственной плитки нет |
| AI Conflict Arland | [metadata.md](AIConflictArland/metadata.md) | AI Conflict — Арланд |
| AI Conflict Everon | [metadata.md](AIConflictEveron/metadata.md) | AI Conflict — Эверон; AI Conflict — Север Эверона |
| AI Conflict Arland RHS | [metadata.md](AIConflictArlandRHS/metadata.md) | AI Conflict RHS — Арланд |
| AI Conflict Everon RHS | [metadata.md](AIConflictEveronRHS/metadata.md) | AI Conflict RHS — Эверон; AI Conflict RHS — Север Эверона |
| AI Conflict Arland WCS + RHS | [metadata.md](AIConflictArlandWCSRHS/metadata.md) | AI Conflict WCS + RHS — Арланд |

Preview assets не входят в игровые addon и передаются Resource Publisher как
внешние Workshop-изображения. В репозитории четыре оригинальные обложки; Arland WCS + RHS использует копию обложки Arland RHS. Локального preview
для Everon RHS пока нет. При исправлении его описания сохранить текущую
опубликованную обложку.

ImageGen briefs существующих обложек зафиксированы в [PROMPTS.md](PROMPTS.md).

Ограничения текущего набора:

- текущие подготовленные патчноуты: [0.1.23](../releases/0.1.23.md); metadata нового Arland WCS + RHS подготовлены для этой версии;
- категория: `Systems & Features`;
- license: `Custom`, Apache License 2.0;
- preview: квадратный JPEG без текста/логотипов, не более 2 MiB;
- visibility до проверки packaged build: `Test`.

## Обновление описаний существующих страниц

Сначала обнови соответствующий `metadata.md`, затем перенеси `Summary` и
`Description` в `Publish Project` именно этого addon. Для Everon RHS также
перенеси `Tags` с `EVERON`. Старые Version и Change Notes в исходных metadata
относятся к предыдущим релизам: номер и notes следующей публикации сверяй с
текущей Workshop-страницей и `releases/`. Не переноси старое `Test` на уже
опубликованную страницу при исправлении только текста; visibility, license и
preview сохраняются.

У Arland должно быть явно указано наличие собственной плитки. Everon RHS
должен описывать Эверон и оба его RHS-сценария; имена Arland в списке
зависимостей корректны. После upload повторно проверь текст на странице
Workshop: Markdown из репозитория не импортируется автоматически.
