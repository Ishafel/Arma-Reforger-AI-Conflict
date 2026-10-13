// Native FinishLoiter освобождает movement, но не выводит graph из Gestures.
modded class SCR_CharacterCommandLoiter
{
	void AICF_FinishSocialLoiter()
	{
		// DONE не позволяет OnDeactivate перезаписать hard exit мягким -1.
		m_eState = ELoiterCommandState.DONE;
		m_pCharAnimComponent.CallCommand(m_pStaticTable.m_CommandGesture, -2, 0.0);
		FinishLoiter();
	}
}

class AICF_SocialLoiterExit
{
	static bool Matches(SCR_LoiterCustomAnimData current, SCR_LoiterCustomAnimData expected)
	{
		return current && expected && current.m_iCommandBindID == expected.m_iCommandBindID &&
			current.m_iCommandValue == expected.m_iCommandValue && current.m_sGraphName == expected.m_sGraphName &&
			current.m_sGraphInstanceName == expected.m_sGraphInstanceName && current.m_sGraphBindingName == expected.m_sGraphBindingName;
	}
}

modded class SCR_CharacterControllerComponent
{
	void AICF_StopSocialLoiter(SCR_LoiterCustomAnimData expected)
	{
		RplComponent replication = GetCharacter().GetRplComponent();
		if (!Replication.IsServer() || !replication || !replication.IsOwner() || !expected ||
			GetScrInputContext().m_CustomAnimData != expected) return;
		// Тот же component и reliable channel, что у штатного Rpc_StartLoitering_BCNO.
		// Отмена не обгоняет старт и не откладывается до следующей анимации.
		int type = GetScrInputContext().m_iLoiteringType;
		Rpc(AICF_StopSocialLoiter_Broadcast, type, expected);
		AICF_StopSocialLoiter_Broadcast(type, expected);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void AICF_StopSocialLoiter_Broadcast(int type, SCR_LoiterCustomAnimData expected)
	{
		SCR_ScriptedCharacterInputContext context = GetScrInputContext();
		if (!context || context.m_iLoiteringType != type || !AICF_SocialLoiterExit.Matches(context.m_CustomAnimData, expected)) return;
		SCR_CharacterCommandLoiter loiter = SCR_CharacterCommandLoiter.Cast(GetAnimationComponent().GetCommandScripted());
		if (loiter && loiter.GetScriptedInputContext() == context) loiter.AICF_FinishSocialLoiter();
		// Отменяется также ещё не начатая holster/align заявка. Повторный RPC безопасен.
		context.m_iLoiteringType = -1;
		context.m_CustomAnimData = null;
	}
}
