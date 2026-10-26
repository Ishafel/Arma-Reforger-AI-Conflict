# Только source extensions; native logs и generated resource databases не входят.
function Get-AICFVehicleSoakSources([string]$StageRoot){
    @(Get-ChildItem -LiteralPath $StageRoot -Recurse -File | Where-Object {$_.Extension -in @('.c','.conf','.st','.gproj')} | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
}

function Assert-AICFVehicleSoakSources([string]$StageRoot,[object[]]$Expected){
    $sourcePrefix=[IO.Path]::GetFullPath($StageRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    $expectedPaths=@{}
    foreach($source in $Expected){
        $sourcePath=[IO.Path]::GetFullPath([string]$source.path)
        if(!$sourcePath.StartsWith($sourcePrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Source manifest path escapes stage'}
        if($expectedPaths.ContainsKey($sourcePath)){throw 'Duplicate source manifest path'}
        $expectedPaths[$sourcePath]=[string]$source.sha256
    }
    $actual=@(Get-AICFVehicleSoakSources $StageRoot)
    if(!$expectedPaths.Count -or $actual.Count -ne $expectedPaths.Count){throw 'Stage source set changed'}
    foreach($source in $actual){
        if(!$expectedPaths.ContainsKey($source.path) -or $expectedPaths[$source.path] -ne $source.sha256){throw 'Stage source path or hash changed'}
    }
}
