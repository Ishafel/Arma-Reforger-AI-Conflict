// Isolated stage only. Uses native Instigator objects, contexts and a real death event.
modded class AICF_MatchController
{
	protected bool m_bInstigatorProbeRan;
	protected bool m_bInstigatorProbeFinished;
	protected int m_iInstigatorProbeStart;
	protected int m_iInstigatorProbeChecks;
	protected int m_iInstigatorProbeFailures;
	protected IEntity m_InstigatorProbeSource;
	protected IEntity m_InstigatorProbeVictim;
	protected ref Instigator m_InstigatorProbeOther;
	protected bool m_bInstigatorProbeDeath;

	protected void AICF_InstigatorCheck(string name, bool pass)
	{
		m_iInstigatorProbeChecks++;
		if (!pass) m_iInstigatorProbeFailures++;
		Print(string.Format("[AICF][INSTIGATOR_CASE] case=%1 pass=%2", name, pass));
	}

	protected bool AICF_InstigatorOriginal(SCR_InstigatorContextData context, Instigator original, IEntity victim, IEntity killer)
	{
		return context.GetInstigator() == original && context.GetVictimEntity() == victim && context.GetKillerEntity() == killer;
	}

	protected void AICF_InstigatorDeath(SCR_InstigatorContextData context)
	{
		if (!context || context.GetVictimEntity() != m_InstigatorProbeVictim) return;
		m_bInstigatorProbeDeath = true;
		AICF_InstigatorCheck("NATIVE_DEATH_EVENT", AICF_InstigatorOriginal(context, m_InstigatorProbeOther, m_InstigatorProbeVictim, m_InstigatorProbeSource));
		AICF_InstigatorCheck("NATIVE_DEATH_RELATION", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.OTHER_DEATH && context.GetKillerCharacterControlType() == SCR_ECharacterControlType.UNKNOWN && !context.DoesPlayerKillCountAsTeamKill());
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfInstigatorProbe", enabled) || enabled != "1" || !m_bRosterReady || m_bStopped || m_bInstigatorProbeFinished) return;
		if (m_bInstigatorProbeRan)
		{
			if (!m_bInstigatorProbeDeath && System.GetTickCount(m_iInstigatorProbeStart) < 10000) return;
			AICF_InstigatorCheck("DEATH_COMPLETED", m_bInstigatorProbeDeath);
			Print(string.Format("[AICF][INSTIGATOR_DONE] checks=%1 failed=%2", m_iInstigatorProbeChecks, m_iInstigatorProbeFailures));
			m_bInstigatorProbeFinished = true;
			return;
		}
		IEntity playerEntity;
		string requirePlayer;
		if (System.GetCLIParam("aicfInstigatorPlayer", requirePlayer) && requirePlayer == "1")
		{
			array<int> players = {};
			GetGame().GetPlayerManager().GetPlayers(players);
			foreach (int id : players)
			{
				playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
				if (playerEntity) break;
			}
			if (!playerEntity) return;
		}
		AICF_FactionState state = m_USSRState;
		if (state.GetSlotCount() == 0) state = m_USState;
		IEntity victim = AICF_GroupRuntime.ResolveAliveLeader(state.GetSlot(0).GetGroup());
		IEntity friendly = AICF_GroupRuntime.ResolveAliveLeader(state.GetSlot(1).GetGroup());
		if (!victim || !friendly) return;
		m_bInstigatorProbeRan = true;
		m_iInstigatorProbeStart = System.GetTickCount();
		m_InstigatorProbeVictim = victim;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = victim.GetOrigin();
		m_InstigatorProbeSource = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		m_InstigatorProbeOther = Instigator.CreateInstigator(m_InstigatorProbeSource);
		AICF_InstigatorCheck("REAL_OTHER_TYPE", m_InstigatorProbeSource && m_InstigatorProbeOther.GetInstigatorType() == InstigatorType.INSTIGATOR_OTHER && m_InstigatorProbeOther.GetInstigatorPlayerID() == 0);
		SCR_InstigatorContextData context = new SCR_InstigatorContextData(-1, victim, null, m_InstigatorProbeOther);
		AICF_InstigatorCheck("OTHER_RELATION", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.OTHER_DEATH);
		AICF_InstigatorCheck("OTHER_METADATA", AICF_InstigatorOriginal(context, m_InstigatorProbeOther, victim, m_InstigatorProbeSource));
		AICF_InstigatorCheck("OTHER_CONTROL", context.GetVictimCharacterControlType() == SCR_ECharacterControlType.AI && context.GetKillerCharacterControlType() == SCR_ECharacterControlType.UNKNOWN && !context.DoesPlayerKillCountAsTeamKill());
		Instigator friendlyInstigator = Instigator.CreateInstigator(friendly);
		context = new SCR_InstigatorContextData(-1, victim, friendly, friendlyInstigator);
		AICF_InstigatorCheck("NATIVE_FRIENDLY_AI", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_AI && context.GetKillerCharacterControlType() == SCR_ECharacterControlType.AI && AICF_InstigatorOriginal(context, friendlyInstigator, victim, friendly));
		context.CreateInstigatorData(-1, victim, null, m_InstigatorProbeOther);
		AICF_InstigatorCheck("REUSED_CONTEXT", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.OTHER_DEATH && context.GetKillerCharacterControlType() == SCR_ECharacterControlType.UNKNOWN && AICF_InstigatorOriginal(context, m_InstigatorProbeOther, victim, m_InstigatorProbeSource));
		context = new SCR_InstigatorContextData(-1, victim, friendly, m_InstigatorProbeOther);
		AICF_InstigatorCheck("OTHER_CHARACTER_OVERRIDE", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_AI && context.GetKillerCharacterControlType() == SCR_ECharacterControlType.AI && AICF_InstigatorOriginal(context, m_InstigatorProbeOther, victim, friendly));
		context = new SCR_InstigatorContextData(-1, victim, victim, Instigator.CreateInstigator(victim));
		AICF_InstigatorCheck("NATIVE_SUICIDE", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.SUICIDE);
		context = new SCR_InstigatorContextData(-1, victim, null, Instigator.CreateInstigatorGM());
		AICF_InstigatorCheck("NATIVE_GM", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.KILLED_BY_UNLIMITED_EDITOR);
		context = new SCR_InstigatorContextData(-1, victim, null, Instigator.CreateInstigator(null));
		AICF_InstigatorCheck("NATIVE_NONE", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.OTHER_DEATH);
		context = new SCR_InstigatorContextData(-1, victim, null, m_InstigatorProbeOther, true);
		AICF_InstigatorCheck("NATIVE_DELETED", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.DELETED && context.GetInstigator() == m_InstigatorProbeOther);
		context = new SCR_InstigatorContextData(-1, m_InstigatorProbeSource, null, m_InstigatorProbeOther);
		AICF_InstigatorCheck("NATIVE_NON_CHARACTER", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.NOT_A_CHARACTER);
		context = new SCR_InstigatorContextData(-1, null, null, m_InstigatorProbeOther);
		AICF_InstigatorCheck("NATIVE_NULL_VICTIM", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.NOT_A_CHARACTER);
		if (m_USState.GetSlotCount() > 0 && m_USSRState.GetSlotCount() > 0)
		{
			IEntity enemy = AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup());
			context = new SCR_InstigatorContextData(-1, victim, enemy, Instigator.CreateInstigator(enemy));
			AICF_InstigatorCheck("NATIVE_ENEMY_AI", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_AI);
		}
		if (playerEntity)
		{
			Instigator playerInstigator = Instigator.CreateInstigator(playerEntity);
			context = new SCR_InstigatorContextData(-1, victim, playerEntity, playerInstigator);
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
			AICF_InstigatorCheck("NATIVE_PLAYER", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER && context.GetKillerPlayerID() == playerId && AICF_InstigatorOriginal(context, playerInstigator, victim, playerEntity));
			context = new SCR_InstigatorContextData(-1, victim, playerEntity, m_InstigatorProbeOther);
			AICF_InstigatorCheck("OTHER_PLAYER_OVERRIDE", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER && context.GetKillerPlayerID() == playerId && AICF_InstigatorOriginal(context, m_InstigatorProbeOther, victim, playerEntity));
			context = new SCR_InstigatorContextData(-1, playerEntity, null, m_InstigatorProbeOther);
			AICF_InstigatorCheck("OTHER_PLAYER_VICTIM", context.GetVictimKillerRelation() == SCR_ECharacterDeathStatusRelations.OTHER_DEATH && context.GetVictimPlayerID() == playerId && context.GetVictimCharacterControlType() == SCR_CharacterHelper.GetCharacterControlType(playerEntity) && context.GetKillerCharacterControlType() == SCR_ECharacterControlType.UNKNOWN && AICF_InstigatorOriginal(context, m_InstigatorProbeOther, playerEntity, m_InstigatorProbeSource));
		}
		m_Campaign.GetOnControllableDestroyed().Insert(AICF_InstigatorDeath);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(victim.FindComponent(SCR_CharacterDamageManagerComponent));
		damage.EnableDamageHandling(true);
		damage.Kill(m_InstigatorProbeOther);
		AICF_InstigatorCheck("REAL_CHARACTER_DIED", damage.IsDestroyed());
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (m_Campaign) m_Campaign.GetOnControllableDestroyed().Remove(AICF_InstigatorDeath);
		if (m_InstigatorProbeSource) SCR_EntityHelper.DeleteEntityAndChildren(m_InstigatorProbeSource);
		m_InstigatorProbeSource = null;
		m_InstigatorProbeVictim = null;
		m_InstigatorProbeOther = null;
		super.Stop(cleanupEntities);
	}
}
