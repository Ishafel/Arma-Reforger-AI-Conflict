// RHS_AFRF currently exposes the western HQ identity-voice signal even though
// it represents the stable USSR side. Keep the compatibility fix in the RHS
// root addon and preserve the faction-defined signal for every other faction.
modded class SCR_Faction
{
	override int GetIndentityVoiceSignal()
	{
		if (GetFactionKey() == "RHS_AFRF")
			return 1;

		return super.GetIndentityVoiceSignal();
	}
}
