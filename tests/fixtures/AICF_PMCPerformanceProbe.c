// Stage-only RHS/WCS callback timing; no additional gameplay operations.
modded class SCR_ChimeraCharacter
{
	override protected void AICF_ApplyPMCEquipment()
	{
		int started = System.GetTickCount();
		super.AICF_ApplyPMCEquipment();
		int elapsed = System.GetTickCount(started);
		Print(string.Format("[AICF][PMC_PERFORMANCE] entity=%1 elapsed_ms=%2 applied=%3 frame=%4 added_gameplay_calls=0", GetID(), elapsed, AICF_PMCEquipmentApplied(), GetWorld().GetFrameNumber()));
	}
}
