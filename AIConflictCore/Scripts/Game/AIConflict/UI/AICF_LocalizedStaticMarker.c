// Штатный static marker хранит исходный server/JIP text. Локализуется только
// AICF-подпись серверного маркера непосредственно перед отображением.
modded class SCR_MapMarkerWidgetComponent
{
	override void SetText(string text, bool skipProfanityFilter = false)
	{
		if (m_MarkerObject && m_MarkerObject.GetMarkerOwnerID() <= -1 && text.StartsWith("{AICF:"))
			text = AICF_Localization.Resolve(text);
		super.SetText(text, skipProfanityFilter);
	}
}
