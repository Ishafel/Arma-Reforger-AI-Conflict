function Get-AICFQuotaFloorAuditFailures {
    param([string]$RootCapacity,[string]$Install,[string]$WorldCapture,[string]$StartupCapture,[string]$Start)
    $rules=@(
        @('ROOT_PARTITION_INITIALIZE',$RootCapacity,'quota\.Initialize\s*\(\s*this\s*,\s*m_Config\.GetMaxManagedAgents\s*\(\s*\)\s*,\s*bases\.Count\s*\(\s*\)[\s\S]*GetMaxVehiclesPerFaction\s*\(\s*\)\s*\*\s*2\s*,\s*fiaUnits\s*\)\s*&&\s*quota\.InstallNativeFloor\s*\('),
        @('PRESERVE_HIGHER_NATIVE_FLOOR',$Install,'previous\s*=\s*m_World\.GetLimitOfActiveAIs\s*\([\s\S]*required\s*=\s*m_Limits\.GetTotalLimit\s*\([\s\S]*if\s*\(\s*previous\s*<\s*required\s*\)\s*m_World\.SetLimitOfActiveAIs\s*\(\s*required\s*\)'),
        @('VERIFY_NATIVE_READBACK',$Install,'effective\s*=\s*m_World\.GetLimitOfActiveAIs\s*\([\s\S]*effective\s*<\s*required\s*\)\s*return\s+false'),
        @('OBSERVABLE_PARTITION_FLOOR',$Install,'AI_WORLD_CAPACITY[\s\S]*previous_limit=[\s\S]*required_limit=[\s\S]*effective_limit=[\s\S]*separate_quotas=1[\s\S]*original_native='),
        @('ORIGINAL_WORLD_CAPTURE',$WorldCapture,'original\s*=\s*world\.GetLimitOfActiveAIs\s*\([\s\S]*m_iOriginalNativeLimit\s*=\s*original\s*;'),
        @('STARTUP_READONLY_ORIGINAL',$StartupCapture,'startup\.m_WorldState\s*=\s*world\.AICF_GetQuotaWorldState\s*\('),
        @('CAPTURE_BEFORE_STARTED',$Start,'AICF_AIQuotaStartup\.Capture\s*\([\s\S]*m_QuotaStartup\.Check\s*\([\s\S]*m_bStarted\s*=\s*true')
    )
    foreach($rule in $rules){if($rule[1]-notmatch$rule[2]){$rule[0]}}
}
