param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$taskSource=Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Respawn/AICF_PlayerPersistenceIdentity.c') -Raw
$taskBody=Get-AICFMethodBody ([pscustomobject]@{Source=$taskSource;Code=(ConvertTo-AICFCodeText $taskSource)}) 'OnPlayerAuditSuccess_S'
if(!$taskBody){throw 'Missing production audit callback'}
if($taskSource -notmatch '\[BaseContainerProps\(category: "Respawn"\)\]'){throw 'Native serialized spawn logic requires metadata'}
$taskExecutable=$taskBody.Replace('super.','base.')
$taskHarness=@'
using System;
public static class AICFPersistenceString { public static bool IsEmpty(this string value) { return value.Length == 0; } }
public static class Replication { public static bool Server; public static bool IsServer(){return Server;} }
public class SCR_PlayerController {
 public bool Tracking=true; public static SCR_PlayerController Cast(SCR_PlayerController value){return value;}
 public static implicit operator bool(SCR_PlayerController value){return value!=null;}
}
public class AICFIdentityPlayers { public SCR_PlayerController Controller=new SCR_PlayerController(); public SCR_PlayerController GetPlayerController(int id){return Controller;} }
public class AICFIdentityPersistence {
 public int Stops; public bool Removed;
 public static implicit operator bool(AICFIdentityPersistence value){return value!=null;}
 public bool StopTracking(SCR_PlayerController controller,bool removeData){Stops++;Removed=removeData;controller.Tracking=false;return true;}
}
public class AICFIdentityBackend { public string Identity=""; public string GetPlayerIdentityId(int playerId){return Identity;} }
public class AICFIdentityGame {
 public AICFIdentityBackend Backend=new AICFIdentityBackend(); public AICFIdentityBackend GetBackendApi(){return Backend;}
 public AICFIdentityPlayers Players=new AICFIdentityPlayers(); public AICFIdentityPlayers GetPlayerManager(){return Players;}
}
public class AICFIdentityNative {
 public int Calls; public AICFIdentityGame Game=new AICFIdentityGame();
 public AICFIdentityPersistence m_Persistence; public int Diagnostics;
 public AICFIdentityGame GetGame(){return Game;}
 public void Print(string value){Diagnostics++;}
 public virtual void OnPlayerAuditSuccess_S(int playerId){Calls++; if(Game.Backend.Identity.Length==0) Game.Players.Controller.Tracking=false;}
}
public class AICFIdentityProduction:AICFIdentityNative {
 public override void OnPlayerAuditSuccess_S(int playerId)
__BODY__
}
public static class AICFIdentityCases {
 public static int Run(){
 int checks=0;
 foreach(bool server in new[]{false,true}) foreach(bool persistence in new[]{false,true})
 foreach(int playerId in new[]{-1,0,1,2}) foreach(string identity in new[]{"","12345678-1234-1234-1234-123456789abc"}){
  Replication.Server=server;
  var target=new AICFIdentityProduction();target.m_Persistence=persistence?new AICFIdentityPersistence():null;target.Game.Backend.Identity=identity;
  target.OnPlayerAuditSuccess_S(playerId);
  bool guard=server&&persistence&&playerId>0&&identity.Length==0;
  if(target.Calls!=(guard?0:1)||target.Diagnostics!=(guard?1:0)) throw new Exception("Native dispatch mismatch");
  if(guard&&(target.Game.Players.Controller.Tracking||target.m_Persistence.Stops!=1||target.m_Persistence.Removed))throw new Exception("Anonymous tracking/data semantics changed");
  if(!guard&&persistence&&target.m_Persistence.Stops!=0)throw new Exception("Unexpected tracking mutation");
  if(identity.Length>0&&!target.Game.Players.Controller.Tracking)throw new Exception("Authenticated identity bypassed");
  checks++;
 }
 return checks;
 }
}
'@
Add-Type -TypeDefinition ($taskHarness.Replace('__BODY__',$taskExecutable))
$taskChecks=[AICFIdentityCases]::Run()
if($taskChecks -ne 32){throw 'Incomplete branch matrix'}
if($taskSource -match '\b(StartTracking|SetId|Save|ClearStorage|SetConfig)\s*\(' -or $taskSource -notmatch 'StopTracking\(controller, false\)'){throw 'Identity guard must use only no-data-removal tracking release'}
Write-Output "PASS PlayerPersistenceIdentity extracted production callback cases=$taskChecks; native reconnect remains a separate gate"
