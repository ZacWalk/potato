$url1 = "https://en.wikipedia.org/w/load.php?lang=en&modules=ext.uls.interlanguage%7Cext.visualEditor.desktopArticleTarget.noscript%7Cext.wikimediamessages.styles%7Cskins.vector.icons,styles%7Cskins.vector.search.codex.styles%7Cwikibase.client.init&only=styles&skin=vector-2022"
$css1 = (Invoke-WebRequest -Uri $url1 -UseBasicParsing).Content
$rootIdx = $css1.IndexOf(':root')
if ($rootIdx -ge 0) {
    $braceStart = $css1.IndexOf('{', $rootIdx)
    $depth = 0
    $braceEnd = -1
    for ($i = $braceStart; $i -lt $css1.Length; $i++) {
        if ($css1[$i] -eq '{') { $depth++ }
        elseif ($css1[$i] -eq '}') { $depth--; if ($depth -eq 0) { $braceEnd = $i; break } }
    }
    $rootBlock = $css1.Substring($rootIdx, $braceEnd - $rootIdx + 1)
    Write-Host "=== :root block (first 2000 chars) ==="
    Write-Host $rootBlock.Substring(0, [Math]::Min($rootBlock.Length, 2000))
    Write-Host ""
    Write-Host "=== :root block length: $($rootBlock.Length) chars ==="
}
Write-Host ""
Write-Host "=== First 5 @supports blocks (first 300 chars each) ==="
$pos = 0
$count = 0
while ($count -lt 5 -and $pos -lt $css1.Length) {
    $idx = $css1.IndexOf('@supports', $pos)
    if ($idx -lt 0) { break }
    $braceStart = $css1.IndexOf('{', $idx)
    $depth = 0
    $braceEnd = -1
    for ($i = $braceStart; $i -lt $css1.Length; $i++) {
        if ($css1[$i] -eq '{') { $depth++ }
        elseif ($css1[$i] -eq '}') { $depth--; if ($depth -eq 0) { $braceEnd = $i; break } }
    }
    $block = $css1.Substring($idx, $braceEnd - $idx + 1)
    Write-Host ""
    Write-Host "--- @supports #$($count+1) ($($block.Length) chars) ---"
    Write-Host $block.Substring(0, [Math]::Min($block.Length, 300))
    $pos = $braceEnd + 1
    $count++
}
Write-Host ""
Write-Host "=== var() with fallback vs without ==="
$withFallback = [regex]::Matches($css1, 'var\(--[^,)]+,\s*[^)]+\)').Count
$withoutFallback = [regex]::Matches($css1, 'var\(--[^,)]+\)').Count
Write-Host "var() with fallback: $withFallback"
Write-Host "var() without fallback (simple): $withoutFallback"
