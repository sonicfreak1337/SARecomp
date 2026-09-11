param(
    [Parameter(Mandatory = $true)]
    [string] $GeneratedCodeDirectory,

    [Parameter(Mandatory = $true)]
    [string] $OutputInclude,

    [Parameter(Mandatory = $true)]
    [string] $OutputJson,

    [string] $SupplementalEntries,

    [ValidateRange(1, 4096)]
    [int] $ExpectedShardCount = 75
)

$ErrorActionPreference = 'Stop'

$source = [IO.Path]::GetFullPath($GeneratedCodeDirectory)
$include = [IO.Path]::GetFullPath($OutputInclude)
$json = [IO.Path]::GetFullPath($OutputJson)
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "Generated code directory does not exist: $source"
}

$shards = @(Get-ChildItem -LiteralPath $source -Filter 'runtime-dispatch-shard-*.cpp' -File |
    Sort-Object Name)
if ($shards.Count -ne $ExpectedShardCount) {
    throw "Expected $ExpectedShardCount runtime dispatch shards, found $($shards.Count)"
}

$pattern = [regex](
    'append_static_block\(blocks,\s*0x[0-9A-F]+u,\s*[0-9]+u,.*' +
    '&fn_([0-9A-F]+)_runtime_entry')
$allEntries = [Collections.Generic.HashSet[uint32]]::new()
$blockRows = 0
foreach ($shard in $shards) {
    foreach ($line in [IO.File]::ReadLines($shard.FullName)) {
        $match = $pattern.Match($line)
        if (-not $match.Success) {
            continue
        }
        $entry = [Convert]::ToUInt32($match.Groups[1].Value, 16)
        [void] $allEntries.Add($entry)
        ++$blockRows
    }
}

$baseEntries = @($allEntries |
    Where-Object { ('{0:X8}' -f $_).StartsWith('8C') } |
    Sort-Object)
if ($blockRows -lt 37608 -or $allEntries.Count -lt 2958 -or
    $baseEntries.Count -lt 2880) {
    throw (
        "Unexpected AOT map shape: blocks=$blockRows all=$($allEntries.Count) " +
        "boot=$($baseEntries.Count)")
}
$sourceFunctionOwnerCount = $allEntries.Count

$supplementalCount = 0
if (-not [string]::IsNullOrWhiteSpace($SupplementalEntries)) {
    $supplementalPath = [IO.Path]::GetFullPath($SupplementalEntries)
    if (-not (Test-Path -LiteralPath $supplementalPath -PathType Leaf)) {
        throw "Supplemental entry file does not exist: $supplementalPath"
    }
    foreach ($rawLine in [IO.File]::ReadLines($supplementalPath)) {
        $line = ($rawLine -replace '#.*$', '').Trim()
        if ($line.Length -eq 0) {
            continue
        }
        $hex = $line -replace '^0[xX]', ''
        if ($hex -notmatch '^[0-9A-Fa-f]{8}$') {
            throw "Invalid supplemental SH-4 entry: $rawLine"
        }
        $entry = [Convert]::ToUInt32($hex, 16)
        if (($entry -band [uint32]1) -ne [uint32]0 -or
            -not ('{0:X8}' -f $entry).StartsWith('8C')) {
            throw "Supplemental entry is outside the aligned 0x8C domain: $rawLine"
        }
        if ($allEntries.Add($entry)) {
            ++$supplementalCount
        }
    }
}

$entries = @($allEntries |
    Where-Object { ('{0:X8}' -f $_).StartsWith('8C') } |
    Sort-Object)
if (@($entries | Where-Object {
        ($_ -band [uint32]1) -ne [uint32]0
    }).Count -ne 0) {
    throw 'Function map contains an unaligned SH-4 entry'
}
foreach ($requiredText in @('8C010000', '8C10F9A0', '8C6602F0')) {
    $required = [Convert]::ToUInt32($requiredText, 16)
    if ($required -notin $entries) {
        throw ('Required function entry is missing: 0x{0:X8}' -f $required)
    }
}

$includeLines = [Collections.Generic.List[string]]::new()
$includeLines.Add('// Generated from the current identity-matched RuntimeOnly dispatch table.')
$includeLines.Add('// Private Sonic Adventure PAL v1.003 bring-up data; do not publish.')
foreach ($entry in $entries) {
    $includeLines.Add(('0x{0:X8}u,' -f $entry))
}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($include)) | Out-Null
[IO.File]::WriteAllLines(
    $include,
    $includeLines,
    [Text.UTF8Encoding]::new($false))

$entryStrings = @($entries | ForEach-Object { '0x{0:X8}' -f $_ })
$material = [Text.Encoding]::UTF8.GetBytes(($entryStrings -join "`n") + "`n")
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $digestBytes = $sha256.ComputeHash($material)
} finally {
    $sha256.Dispose()
}
$digest = (($digestBytes | ForEach-Object { $_.ToString('x2') }) -join '')
$document = [ordered]@{
    schema = 'sonic-adventure-pal-v1003-function-map-v1'
    privacy = 'private-title-data'
    source = 'runtimeonly-current-rte-resume/runtime-dispatch-shards+runtimeonly-misses'
    source_shards = $shards.Count
    source_static_blocks = $blockRows
    source_function_owners = $sourceFunctionOwnerCount
    supplemental_function_entries = $supplementalCount
    selected_boot_function_entries = $entries.Count
    selected_address_domain = '0x8Cxxxxxxxx'
    entry_list_sha256 = "sha256:$digest"
    entries = $entryStrings
}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($json)) | Out-Null
[IO.File]::WriteAllText(
    $json,
    ($document | ConvertTo-Json -Depth 4) + "`n",
    [Text.UTF8Encoding]::new($false))

[pscustomobject]@{
    SourceShards = $shards.Count
    StaticBlocks = $blockRows
    FunctionOwners = $allEntries.Count
    SupplementalFunctionEntries = $supplementalCount
    BootFunctionEntries = $entries.Count
    EntryListSha256 = "sha256:$digest"
    Include = $include
    Json = $json
}
