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
	protected ref AICF_SocialActor m_AICFWeaponActor;
	protected IEntity m_AICFWeapon;
	protected EntityID m_AICFWeaponId;
	protected int m_iAICFWeaponDeadline;
	protected bool m_bAICFUnequipSeen;

	bool AICF_HasSocialWeaponRecovery() { return m_AICFWeaponActor != null; }

	protected void AICF_ClearSocialWeaponRecovery()
	{
		m_AICFWeaponActor = null;
		m_AICFWeapon = null;
	}

	protected override void OnApplyControls(IEntity owner, float timeSlice)
	{
		super.OnApplyControls(owner, timeSlice);
		if (!m_AICFWeaponActor) return;
		RplComponent replication = GetCharacter().GetRplComponent();
		if (!Replication.IsServer() || !replication || !replication.IsOwner() ||
			!m_AICFWeaponActor.HasIdentity() || m_AICFWeaponActor.Controller() != this ||
			!m_AICFWeapon || m_AICFWeapon.GetID() != m_AICFWeaponId ||
			GetLifeState() != ECharacterLifeState.ALIVE || IsPlayerControlled() ||
			System.GetTickCount() >= m_iAICFWeaponDeadline || GetScrInputContext().m_iLoiteringType >= 0 || IsUsingItem())
		{
			AICF_ClearSocialWeaponRecovery();
			return;
		}
		if (IsChangingItem()) { m_bAICFUnequipSeen = true; return; }
		IEntity held = GetCurrentItemInHands();
		// Select/equip input может начать unequip только на следующем frame.
		if (!m_bAICFUnequipSeen && held == m_AICFWeapon) return;
		if (held) { AICF_ClearSocialWeaponRecovery(); return; }
		BaseWeaponManagerComponent manager = GetWeaponManagerComponent();
		if (!manager) { AICF_ClearSocialWeaponRecovery(); return; }
		array<IEntity> weapons = {};
		manager.GetWeaponsList(weapons);
		if (!weapons.Contains(m_AICFWeapon)) { AICF_ClearSocialWeaponRecovery(); return; }
		if (TryEquipRightHandItem(m_AICFWeapon, EEquipItemType.EEquipTypeWeapon)) AICF_ClearSocialWeaponRecovery();
	}

	void AICF_StopSocialLoiter(SCR_LoiterCustomAnimData expected, AICF_SocialActor actor)
	{
		RplComponent replication = GetCharacter().GetRplComponent();
		if (!Replication.IsServer() || !replication || !replication.IsOwner() || !expected ||
			GetScrInputContext().m_CustomAnimData != expected) return;
		// Тот же component и reliable channel, что у штатного Rpc_StartLoitering_BCNO.
		// Отмена не обгоняет старт и не откладывается до следующей анимации.
		int type = GetScrInputContext().m_iLoiteringType;
		if (!IsLoitering() && actor && actor.m_HolsteredWeapon)
		{
			m_AICFWeaponActor = actor;
			m_AICFWeapon = actor.m_HolsteredWeapon;
			m_AICFWeaponId = actor.m_HolsteredWeaponId;
			m_iAICFWeaponDeadline = System.GetTickCount() + AICF_SocialConfig.PREPARE_MS;
			m_bAICFUnequipSeen = IsChangingItem();
		}
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
