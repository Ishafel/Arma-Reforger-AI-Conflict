param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$path=Join-Path $RepositoryRoot 'tools/Invoke-AICFQuotaStockPressure.ps1'
$source=[IO.File]::ReadAllText($path)
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Runner parser errors'}
foreach($name in @('Assert-AICFPressureSources','Get-AICFPressureLogFindings')){
 $function=$ast.Find({param($node)$node-is[Management.Automation.Language.FunctionDefinitionAst]-and$node.Name-eq$name},$true)
 if(!$function){throw "Actual runner function missing $name"}
 Invoke-Expression $function.Extent.Text
}
$cases=0
foreach($text in @('SCRIPT (E): before DONE',('[DONE] passed=1'+"`n"+'SCRIPT (F): after DONE'),'Access violation','Virtual Machine Exception','Application crashed','ENGINE (F): fatal','GUI (E): invalid UI','GUI (F): native UI fatal','UNKNOWN (F): fatal')){
 if((Get-AICFPressureLogFindings $text).fatal.Count-ne1){throw "Full fatal scan lost an error: $text"};$cases++
}
if((Get-AICFPressureLogFindings '[AICF][ERROR] old failure').project.Count-ne1){throw 'Project failure hidden'};$cases++
if((Get-AICFPressureLogFindings 'RESOURCES (E): missing mesh').resources.Count-ne1){throw 'Resource error hidden'};$cases++
if((Get-AICFPressureLogFindings 'UNKNOWN (E): unclassified error').resources.Count-ne1){throw 'Unknown error severity hidden'};$cases++
if((Get-AICFPressureLogFindings '[DONE] passed=1').fatal.Count){throw 'False fatal'};$cases++
$taskAddons=@('Core','Map')
$taskRoot=Join-Path $RepositoryRoot ('.codex-runtime/pressure-runner-contract-'+[guid]::NewGuid().ToString('N'))
$taskStage=Join-Path $taskRoot 'stage';$taskFrozenAudit=Join-Path $taskRoot 'audit.ps1'
try{
 foreach($name in $taskAddons){[void][IO.Directory]::CreateDirectory((Join-Path $taskStage $name))}
 Set-Content -LiteralPath $taskFrozenAudit -Value 'frozen auditor'
 $original=Join-Path $taskStage 'Core/production.c';$fixture=Join-Path $taskStage 'Map/fixture.c'
 Set-Content -LiteralPath $original -Value 'compiled source';Set-Content -LiteralPath $fixture -Value 'compiled fixture'
 $manifest=[pscustomobject]@{files=@([pscustomobject]@{path='Core/production.c';sha256=(Get-FileHash $original).Hash});fixtures=@([pscustomobject]@{path='Map/fixture.c';sha256=(Get-FileHash $fixture).Hash});auditor_sha256=(Get-FileHash $taskFrozenAudit).Hash}
 Assert-AICFPressureSources $manifest;$cases++
 foreach($target in @($original,$fixture,$taskFrozenAudit)){
  $saved=[IO.File]::ReadAllBytes($target);[IO.File]::WriteAllText($target,'postcompile replacement');$rejected=$false
  try{Assert-AICFPressureSources $manifest}catch{$rejected=$true}
  [IO.File]::WriteAllBytes($target,$saved)
  if(!$rejected){throw 'Compiled/frozen SHA replacement accepted'};$cases++
 }
 $extra=Join-Path $taskStage 'Core/uncompiled.c';Set-Content -LiteralPath $extra -Value 'uncompiled extra';$rejected=$false
 try{Assert-AICFPressureSources $manifest}catch{$rejected=$true}
 if(!$rejected){throw 'Uncompiled production extra accepted'};$cases++
}finally{
 $resolved=[IO.Path]::GetFullPath($taskRoot);$boundary=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot '.codex-runtime'))+[IO.Path]::DirectorySeparatorChar
 if(!$resolved.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected temporary target'}
 if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
foreach($required in @('source_manifest_sha256-ne(Get-FileHash','[IO.FileMode]::CreateNew','-Role Server -Variant Stock -AICommanderMode BOTH','-aicfQuotaStockPressure','-aicfQuotaBootReadback','$taskNative.Count-ge2-or$taskFree-le8',"Game destroyed.",'$taskAuditExit-ne0-or$taskFatal.Count-or$taskProject.Count-or$taskResources.Count-or!$taskPerformance')){
 if(!$source.Contains($required)){throw "Canonical pressure runner boundary missing $required"}
}
if($source-match'\bStart-Process\b|Stop-Process|SkipCompile'){throw 'Runner bypasses canonical launcher/compile or deletes processes'}
"PASS actual stock pressure runner cases=$cases source_boundaries=8 native=NOT_RUN"
