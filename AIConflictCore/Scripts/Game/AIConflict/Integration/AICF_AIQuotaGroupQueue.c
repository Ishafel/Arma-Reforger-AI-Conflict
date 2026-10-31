// Native queue and direct script entrypoints share the same full-basket gate.
// The protected factory repeats it immediately before the actual native birth.
modded class SCR_AIGroup
{
	override void RequestSpawn(int slotsWanted = -1, float observerRange = 0)
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetWorld());
		if (quota && !SCR_Global.IsEditMode(this) && !quota.AdmitGroupRequest(this, slotsWanted)) return;
		SCR_AIWorld world;
		AICF_AIQuotaNativeQueueJournal journal;
		AICF_AIQuotaNativeQueueRecord record;
		if (Replication.IsServer() && GetGame() && GetWorld() == GetGame().GetWorld() && !SCR_Global.IsEditMode(this))
		{
			world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			if (world) journal = world.AICF_GetNativeQuotaJournal();
			if (journal) record = journal.BeforeRequest(this, slotsWanted, observerRange);
		}
		super.RequestSpawn(slotsWanted, observerRange);
		if (journal) journal.AfterRequest(record);
	}

	override void SpawnUnits()
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetWorld());
		if (quota && !SCR_Global.IsEditMode(this) && !quota.AdmitGroupRequest(this, -1)) return;
		super.SpawnUnits();
	}

	override bool ExpandOneMember()
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetWorld());
		if (quota && !SCR_Global.IsEditMode(this) && !quota.AdmitGroupRequest(this, -1)) return false;
		bool expanded = super.ExpandOneMember();
		if (quota && IsExpandComplete()) quota.CancelGroupRequest(this);
		return expanded;
	}

	override void DespawnMembers()
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetWorld());
		if (quota) quota.CancelGroupRequest(this);
		if (Replication.IsServer() && GetGame())
		{
			SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			if (world && world.AICF_GetNativeQuotaJournal()) world.AICF_GetNativeQuotaJournal().Cancel(this);
		}
		super.DespawnMembers();
	}
}
