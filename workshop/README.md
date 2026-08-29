# Workshop release assets

Этот каталог хранит проверяемый source of truth для полей ручной публикации.
Workbench не читает Markdown автоматически: значения копируются владельцем в
`Publish Project` по `docs/PUBLISHING.md`.

Два addon публикуются раздельно и строго в порядке Core -> Arland. Preview
assets не входят в игровые addon и передаются Resource Publisher как внешние
Workshop-изображения.

Финальные ImageGen briefs и ограничения зафиксированы в `PROMPTS.md`.

Ограничения текущего набора:

- версия первого Test release: `0.1.0`;
- категория: `Systems & Features`;
- license: `Custom`, Apache License 2.0;
- preview: квадратный JPEG без текста/логотипов, не более 2 MiB;
- visibility до проверки packaged build: `Test`.

После ручного изменения metadata в Workbench сначала обнови соответствующий
`metadata.md`, чтобы репозиторий оставался canonical источником.
