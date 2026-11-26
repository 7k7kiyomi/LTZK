# 临时脚本：完整解析 zbqd.xlsx 模板（任务结束后删除）
Add-Type -AssemblyName System.IO.Compression

$p = 'D:/GarMap/garmap_release/starmap/bin/scripts/zbqd.xlsx'
$fs = New-Object System.IO.FileStream($p, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
$zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Read)
try {
    $sheetEntry = $zip.Entries | Where-Object { $_.FullName -match '^xl/worksheets/sheet1\.xml$' } | Select-Object -First 1
    $sr = New-Object System.IO.StreamReader($sheetEntry.Open(), [System.Text.Encoding]::UTF8)
    $xml = $sr.ReadToEnd()
    $sr.Close()

    Write-Output '===== MERGE CELLS ====='
    $mm = [regex]::Matches($xml, '<mergeCell[^>]*ref="([^"]+)"')
    foreach ($m in $mm) { Write-Output $m.Groups[1].Value }

    Write-Output '===== RAW ROWS ====='
    $rowMatches = [regex]::Matches($xml, '<row[^>]*>.*?</row>', 'Singleline')
    foreach ($rm in $rowMatches) {
        $rowBody = $rm.Value
        if ($rowBody -match '<row[^>]*r="(\d+)"') { $rn = $Matches[1] } else { $rn = '?' }
        Write-Output ("----- ROW " + $rn + " -----")
        Write-Output $rowBody
    }
} finally {
    $zip.Dispose()
    $fs.Dispose()
}
