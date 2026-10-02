# Compiles the RelDyn body bridge (RelDynBodyOBlush, then RelDynBody) with the Creation Kit compiler, the way
# D:\DwemerDistro\build\compile.ps1 does for the AIAgent scripts, with the extra sources OBlushHandle.psc is type-checked against:
# OBlush, OStim Standalone (OBlushHandle names OsexIntegrationMain), JContainers (OStim's OUtils) and stubs\NiOverride.psc (the stock
# headers_unused stub is trimmed; this adds the real SKEE signatures OBlush and OUtils call). Only the two scripts' own bytecode is written;
# the dependencies are type-checked, not built. Output: D:\DwemerDistro\build\out-reldyn-blush
$sk   = 'C:\Games\Steam\steamapps\common\Skyrim Special Edition'
$pc   = Join-Path $sk 'Papyrus Compiler\PapyrusCompiler.exe'
$van  = Join-Path $sk 'Data\Source\Scripts'
$B    = 'D:\DwemerDistro\build'
$Root = Split-Path -Parent $PSScriptRoot                 # AIAgent\Optional\RelDynBody
$Aia  = Split-Path -Parent (Split-Path -Parent $Root)    # AIAgent
$out  = Join-Path $B 'out-reldyn-blush'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter *.pex | Remove-Item -Force
$imports = @(
    (Join-Path $Root 'Source\Scripts'),
    (Join-Path $Aia 'Source\Scripts'),                   # AIAgentFunctions.psc (the native declarations RelDynBody calls)
    "$B\headers_skse",
    "$B\src\skyui\dist\Data\Scripts\Source",
    "$B\src\papyrusutil\Scripts\Source",
    "$B\headers",
    $van,
    "$B\src\minll\headers",
    'D:\Modding\CHIM-Vanilla\mods\OBlush\Source\Scripts',
    'D:\Modding\CHIM-Vanilla\mods\OStim Standalone\Scripts\Source',
    'D:\Modding\CHIM-Vanilla\mods\JContainers SE\scripts\source',
    (Join-Path $PSScriptRoot 'stubs'),
    "$B\headers_unused"
) -join ';'
foreach ($n in 'RelDynBodyOBlush.psc', 'RelDynBody.psc') {
    & $pc $n "-f=$van\TESV_Papyrus_Flags.flg" "-i=$imports" "-o=$out" 2>&1 | ForEach-Object { "$_" }
}
Get-ChildItem $out -Filter *.pex | ForEach-Object { "built " + $_.Name + " " + $_.Length }
