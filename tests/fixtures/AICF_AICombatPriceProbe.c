// Только stage: native readback цены/labels. Не меняет native player catalog.
modded class AICF_VehiclePresetCatalog
{
 int AICF_RawCost(SCR_EntityCatalogEntry entry)
 {
  return EntryCost(entry, Resource.Load(entry.GetPrefab()));
 }
}
modded class AICF_MatchController
{
 protected bool m_bAIPriceRead;
 protected bool m_bAIPriceFinished;
 override protected void Update()
 {
  super.Update();
  if (!System.IsCLIParam("aicfAIPriceProbe") || !m_bRosterReady || m_bStopped || m_bAIPriceFinished) return;
  if (m_bAIPriceRead)
  {
   if (System.IsCLIParam("aicfAIPricePurchase") && AICF_AIPriceReceipt.s_bPaid)
   {
    m_bAIPriceFinished = true;
    Print("[AICF][AI_PRICE_PURCHASE_DONE] production_debit_observed=1");
    GetGame().GetCallqueue().CallLater(AICF_PriceClose, 5000, false);
   }
   return;
  }
  m_bAIPriceRead = true;
  int checks; int failures;
  AICF_PriceFaction(m_USFaction, checks, failures);
  AICF_PriceFaction(m_USSRFaction, checks, failures);
  Print(string.Format("[AICF][AI_PRICE_DONE] checks=%1 failures=%2 profile=%3", checks, failures, m_ContentProfile.GetProfileKey()));
  if (!System.IsCLIParam("aicfAIPricePurchase"))
  {
   m_bAIPriceFinished = true;
   GetGame().GetCallqueue().CallLater(AICF_PriceClose, 5000, false);
  }
 }
 protected void AICF_PriceFaction(SCR_CampaignFaction faction, inout int checks, inout int failures)
 {
  AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
  SCR_EntityCatalog nativeCatalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE);
  FactionKey stable = m_ContentProfile.GetStableFactionKey(faction.GetFactionKey());
  int maximumRole = 3;
  if (m_ContentProfile.HasCoopTankSlots()) maximumRole = 4;
  for (int role; role <= maximumRole; role++)
  {
   ResourceName selected;
   if (role == 0)
   {
    array<string> lightSuffixes = {}; m_ContentProfile.BuildVehicleSuffixPreference(stable, AICF_EVehicleKind.LIGHT_TRANSPORT, lightSuffixes);
    array<ResourceName> lightCandidates = {}; catalog.OwnCandidates(lightSuffixes, lightCandidates);
    if (!lightCandidates.IsEmpty()) selected = lightCandidates[0];
   }
   else
   {
    array<string> suffixes = {}; m_ContentProfile.BuildCoopVehicleSuffixPreference(stable, role, suffixes);
    array<ResourceName> candidates = {}; catalog.OwnCandidates(suffixes, candidates);
    if (!candidates.IsEmpty()) selected = candidates[0];
   }
   checks++;
   SCR_EntityCatalogEntry entry = nativeCatalog.GetEntryWithPrefab(selected);
   if (!entry) { failures++; Print(string.Format("[AICF][AI_PRICE_CASE] faction=%1 role=%2 passed=0 reason=ENTRY_MISSING", stable, role)); continue; }
   int raw = catalog.AICF_RawCost(entry);
   int ai = catalog.Cost(selected);
   bool armed = entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED);
   bool armor = entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMOR);
   bool apc = entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC);
   bool unarmed = entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_UNARMED);
   int expected = raw;
   if (!System.IsCLIParam("aicfAIPriceBaseline") && (armed || armor || apc) && raw > 0)
   {
    expected = raw / 3; if (raw % 3 == 2) expected++; expected = Math.Max(1, expected);
   }
   SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
   int human = -1; string humanSource = "SPAWNER_DATA";
   if (data) human = data.GetSupplyCost();
   else { human = catalog.AICF_RawCost(entry); humanSource = "CAMPAIGN_BUDGET"; }
   bool passed = raw > 0 && ai == expected && (human < 0 || human == raw);
   if (!passed) failures++;
   Print(string.Format("[AICF][AI_PRICE_CASE] faction=%1 role=%2 passed=%3 raw=%4 ai=%5 armed=%6 armor=%7 apc=%8 unarmed=%9", stable, role, passed, raw, ai, armed, armor, apc, unarmed) + " human=" + human + " human_source=" + humanSource + " prefab=" + selected);
  }
  catalog.Clear();
 }
 void AICF_PriceClose() { GetGame().RequestClose(); }
 override protected void Stop(bool cleanupEntities)
 {
  GetGame().GetCallqueue().Remove(AICF_PriceClose);
  super.Stop(cleanupEntities);
 }
}modded class SCR_GameModeCampaign
{
 override void OnGameStart()
 {
  super.OnGameStart();
  if (System.IsCLIParam("aicfAIPriceProbe") && Replication.IsServer()) GetGame().GetCallqueue().CallLater(AICF_PriceDeadline, 180000, false);
 }
 void AICF_PriceDeadline() { Print("[AICF][AI_PRICE_TIMEOUT]", LogLevel.ERROR); GetGame().RequestClose(); }
 override void OnGameEnd()
 {
  GetGame().GetCallqueue().Remove(AICF_PriceDeadline);
  super.OnGameEnd();
 }
}
class AICF_AIPriceReceipt
{
 static bool s_bPaid;
}
modded class AICF_EconomySystem
{
 override bool PurchaseVehiclePreset(AICF_TransportTrip trip, SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent base, ResourceName prefab, out int paid)
 {
  int before = -1; if (base) before = base.GetSupplies();
  AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
  SCR_EntityCatalog nativeCatalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE);
  SCR_EntityCatalogEntry entry = nativeCatalog.GetEntryWithPrefab(prefab);
  SCR_EntityCatalogSpawnerData data;
  int raw = -1; int human = -1;
  if (entry)
  {
   raw = catalog.AICF_RawCost(entry);
   data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
   if (data) human = data.GetSupplyCost();
  }
  int quote = catalog.Cost(prefab);
  catalog.Clear();
  bool result = super.PurchaseVehiclePreset(trip, faction, base, prefab, paid);
  if (System.IsCLIParam("aicfAIPricePurchase") && result)
  {
   int after = base.GetSupplies(); int humanAfter = -1; if (data) humanAfter = data.GetSupplyCost();
   bool passed = paid == quote && before - after == quote && raw > quote && human == humanAfter && human == raw;
   Print(string.Format("[AICF][AI_PRICE_DEBIT] operation=%1 raw=%2 quote=%3 paid=%4 before=%5 after=%6 human_before=%7 human_after=%8 passed=%9", trip.GetOperationId(), raw, quote, paid, before, after, human, humanAfter, passed) + " prefab=" + prefab);
   AICF_AIPriceReceipt.s_bPaid = passed;
  }
  return result;
 }
}
