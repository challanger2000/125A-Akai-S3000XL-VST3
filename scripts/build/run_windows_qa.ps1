# SPDX-License-Identifier: GPL-3.0-only
# Windows x64 production 125A Plugin Tester and Steinberg VST3 SDK gate.
# PUBLIC CI is ROM-FREE. Original firmware/audio tests remain separate.
$ErrorActionPreference = 'Stop'
$exe = Get-ChildItem 'tester' -Recurse -File -Filter '125A_Plugin_Tester_v0.3.2.exe' | Select-Object -First 1
if ($null -eq $exe) { throw 'Pinned v0.3.2 tester executable is missing' }
$bundles = @(Get-ChildItem 'build/windows' -Recurse -Directory -Filter '*.vst3')
if ($bundles.Count -ne 1) { throw "Expected one built VST3 bundle; got $($bundles.Count)" }
$bundle = $bundles[0]
$modules = @(Get-ChildItem $bundle.FullName -Recurse -File -Filter '*.vst3')
if ($modules.Count -ne 1) { throw "Expected one PE module; got $($modules.Count)" }
$reports = Join-Path (Get-Location) 'build/windows-qa-reports'
New-Item -ItemType Directory -Force -Path $reports | Out-Null
$env:125A_PLUGIN_TESTER_REPORT_DIR = $reports
Write-Output "125A_INTEGRATED_QA_BUNDLE=$($bundle.FullName)"
# Published portable v0.3.2 otherwise opens GUI and hangs CI.
# --portable-test invokes the official internal CLI dispatcher.
$p = Start-Process -FilePath $exe.FullName -ArgumentList @('--portable-test', """$($bundle.FullName)""") -PassThru
try {
    Wait-Process -Id $p.Id -Timeout 360 -ErrorAction Stop
} catch {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    '125A_INTEGRATED_QA=TIMEOUT' | Set-Content (Join-Path $reports 'status.txt')
    throw '125A Plugin Tester hung beyond 360 seconds'
}
$p.Refresh()
"125A_PLUGIN_TESTER_EXIT=$($p.ExitCode)" | Tee-Object (Join-Path $reports 'status.txt')
$summary = Get-ChildItem $reports -File -Filter '*_125A_QA_Report.txt' | Select-Object -First 1
if ($null -eq $summary) {throw 'Missing actual 125A QA report'}
$report = Get-Content $summary.FullName -Raw
Get-Content $summary.FullName | Select-Object -Last 17 | ForEach-Object {Write-Output $_}
if ($p.ExitCode -ne 0 -or -not ($report -match 'RESULT:\s*[1-9]\d* PASS / 0 WARNING / 0 FAIL')) {
    throw "125A Plugin Tester did not PASS (exit $($p.ExitCode))"
}
if ($report -notmatch '\[PASS\]\s+Steinberg VST3 validator') {
    throw 'Official Steinberg validator is not PASS'
}
Write-Output 'ROM_FREE_WINDOWS_VST3_HOST_QA=PASS'
