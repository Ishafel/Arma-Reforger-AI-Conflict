# Читает source string table; runtime-таблицы не являются источником переводов.
function Read-AICFLocalization {
    param([string]$RepositoryRoot)
    $path = Join-Path $RepositoryRoot 'AIConflictCore/Language/AICF_Localization.st'
    $source = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    $entries = [ordered]@{}
    $pattern = 'Id\s+"(?<id>[^"]+)"\s+Target_en_us\s+"(?<en>(?:\\.|[^"\\])*)"\s+Target_ru_ru\s+"(?<ru>(?:\\.|[^"\\])*)"'
    foreach ($item in [regex]::Matches($source, $pattern)) {
        $id = $item.Groups['id'].Value
        if ($entries.Contains($id)) { throw "Duplicate localization ID: $id" }
        $entries.Add($id, @{ en_us = $item.Groups['en'].Value; ru_ru = $item.Groups['ru'].Value })
    }
    if ($entries.Count -eq 0) { throw 'Empty localization table' }
    return $entries
}

# Только для presentation assertions: разворачивает реальные ссылки на таблицу.
# Отсутствующий ключ остаётся неразвёрнутым, поэтому прежняя проверка не проходит.
function Expand-AICFLocalizedAuditText {
    param([string]$Source, $Entries, [string]$Language = 'en_us')
    return [regex]::Replace($Source, '\{AICF:(?<id>AICF_[A-Za-z0-9_]+)\}', {
        param($match)
        $id = $match.Groups['id'].Value
        if ($Entries.Contains($id)) { return $Entries[$id][$Language] }
        return $match.Value
    })
}
