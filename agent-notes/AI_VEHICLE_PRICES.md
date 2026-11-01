# Цена боевой техники для управляемых отрядов

`AICF_AIVehiclePricePolicy` применяется ровно один раз при заполнении
`AICF_VehiclePresetCatalog.m_aCosts`. Исходный `EntryCost` по-прежнему читает
profile override, native `SCR_EntityCatalogSpawnerData` либо `CAMPAIGN` budget.
Native catalog objects, budgets, `RHSMenuPricePolicy` и покупка игроком не меняются.

Боевой считается catalog entry с native label `TRAIT_ARMED`, `TRAIT_ARMOR`
или `VEHICLE_APC` (API1.8.0.13). Это включает бронированные варианты без оружия:
например, RHS K4386 имеет `TRAIT_ARMOR`/`VEHICLE_APC`, хотя `TRAIT_ARMED` отсутствует.
Невооружённые небронированные машины и грузовики без этих labels сохраняют цену.
Имена моделей, пути, сторона и выбранное TRANSPORT/COMBAT поведение цену не задают.

Положительная цена делится на3 с округлением к ближайшему целому, минимум1.
Используется целочисленное деление и остаток, без float и переполнения при
`INT_MAX`. Нулевая/неизвестная цена0/-1 сохраняется; неизвестная положительной
не становится. Примеры:400→133,1500→500,3500→1167,3750→1250.

Один `m_aCosts` используют UI `List`, `Cost`, server preflight/current checks,
`CanAffordVehiclePreset`, acquisition budget guard, `PurchaseVehiclePreset`
и commander `GetMaximumVehiclePresetCost` для demand склада. Поэтому ручной
пресет и автоматические Co-op preset назначения покупаются по той же цене,
которая показывается в окне и учитывается при строительстве склада.
Rollback возвращает фактически списанную сумму. Identity, authority, supply
obligations, quota, site/ownership и transaction guards не менялись.

Старые automatic trips без vehicle preset не списывали supplies до этой правки.
Они остаются бесплатными: изменение цены не вводит новую платную экономику.
Физический spawn остаётся у acquisition/spawner, цена и debit — у Economy.

## Проверка

`Test-AICombatVehiclePriceContracts.ps1` исполняет извлечённые production bodies:
classification, rounding, `Cost`/UI `List`, `CanPayVehiclePreset`, purchase и
rollback. Проверяет48 случаев и5 отвергаемых unsafe варианта: неверный остаток,
скидка небоевым моделям, потеря minimum1 и списание исходной цены вместо AI quote.
Релевантный baseline4/4PASS; тот же набор с новым контрактом5/5PASS.

`AICF_AICombatPriceProbe.c` — opt-in stage-only readback. Читает native entry
labels/raw player costs и AI costs обеих сторон; `-aicfAIPriceBaseline1`
требует исходную цену, обычный запуск — новую. `-aicfAIPricePurchase1` наблюдает
настоящий `Economy.PurchaseVehiclePreset`, сравнивает quote/debit и неизменность
native human price, затем штатно закрывает сервер. Для платежа используется
canonical tank scaffold с controlled исходным crew и supplies; он не является
доказательством естественной доступности припасов или полного маршрута танка.

Evidence и полные остановленные логи:
`.codex-runtime/ai-combat-price-native-20261101/` в основном workspace;
статические проверки — `.codex-runtime/ai-combat-prices/` ценового worktree.
Результаты functional, Workbench и strict resource gate учитываются отдельно.
Игра человеком/connected client/JIP новой цены — отдельный NOT RUN; работающий
retail клиент пользователя не перезапускается.

Финал ветки: релевантные offline6/6PASS, включая13 parsercontrols нового log auditor. Терминальный WB Stock/RHS/WCS3/3PASS0; raw50/52/78 — strict resourceFAIL. Native WCS10/10 и Stock8/8 price rows подтвердили новый AI quote и неизменный native raw: для3stock вариантов безSpawnerData повторно прочитан немутирующийCAMPAIGN budget. Настоящий T72Purchase: raw/human3500, quote/paid1167, stock10555→9388, тот жеtrip/boundleasegen1, native0/fullstopped/fatal0. Runtime raw28/206 — strictFAIL. Первый fixture считал обязательной отсутствующуюTRAIT_UNARMED label; затем исправлен выбор LIGHT_TRANSPORT. Первая дополнительная scaffold-сборка нарушала canonical modded order; FAIL сохранён, повтор по точным root/ZZ paths дал WB3PASS. Цена production при этих исправлениях fixture не менялась.

Integer guard важен: pinned Math.Max принимает float, поэтому minimum1 проверяется явным int guard. Контракт моделирует Enforce float32 Math.Max и отдельным unsafe вариантом ловит потерю точности INT_MAX→715827904 вместо715827882. Все5 unsafe variants исполняются, не подменяются source regex.