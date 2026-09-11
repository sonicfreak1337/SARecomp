# Metadata protection for retained Sonic builds. Never reconstruct log records
# or automatically restat: recovery requires the recorded source/tool evidence.
function Get-SonicBoundNinja {
    param([Parameter(Mandatory)][string] $BuildRoot)
    $cache = Join-Path $BuildRoot 'CMakeCache.txt'
    $rows = @(Get-Content -LiteralPath $cache -ErrorAction Stop | Where-Object {
        $_ -match '^CMAKE_MAKE_PROGRAM:(FILEPATH|STRING|UNINITIALIZED)='
    })
    if ($rows.Count -ne 1) { throw 'Retained Sonic build has no unique CMake Ninja binding.' }
    $tool = $rows[0].Substring($rows[0].IndexOf('=') + 1)
    if (-not [IO.Path]::IsPathFullyQualified($tool) -or
        -not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw 'Retained Sonic CMake Ninja binding is not an existing absolute tool.'
    }
    Assert-SonicNinjaLogCompatibility -BuildRoot $BuildRoot -Ninja $tool
    return $tool
}

function Assert-SonicNinjaLogCompatibility {
    param([Parameter(Mandatory)][string] $BuildRoot,
          [Parameter(Mandatory)][string] $Ninja)
    # --version runs without -C and does not open the retained build graph/log.
    $versionText = (& $Ninja --version | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $versionText -notmatch '^\d+\.\d+\.\d+$') {
        throw 'Cannot identify the retained Sonic Ninja tool safely.'
    }
    $log = Join-Path $BuildRoot '.ninja_log'
    if (Test-Path -LiteralPath $log -PathType Leaf) {
        $reader = [IO.File]::OpenText($log)
        try { $header = $reader.ReadLine() } finally { $reader.Dispose() }
        if ($header -notmatch '^# ninja log v([4-7])$') {
            throw 'Unknown Ninja log format; do not allow a tool to discard it.'
        }
        # 1.13.2 is the qualified local v7 reader, not a claim about the first
        # upstream version supporting v7. Older local tools must not probe it.
        if ($Matches[1] -eq '7' -and [version]$versionText -lt [version]'1.13.2') {
            throw "Ninja $versionText is not qualified for the retained v7 log."
        }
    }
}

function Save-SonicNinjaRecoverySnapshot {
    param([Parameter(Mandatory)][string] $BuildRoot,
          [Parameter(Mandatory)][string] $Destination,
          [Parameter(Mandatory)][string] $Ninja)
    if (Test-Path -LiteralPath $Destination) { throw 'Ninja recovery snapshot already exists.' }
    $locks = [Collections.Generic.List[object]]::new()
    try {
        foreach ($name in @('.ninja_log', '.ninja_deps', 'CMakeCache.txt', 'build.ninja')) {
            $path = Join-Path $BuildRoot $name
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
            $item = Get-Item -LiteralPath $path
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
                $item.Length -gt 256MB) { throw 'Unsafe or oversized Ninja recovery metadata.' }
            $stream = [IO.File]::Open($path, 'Open', 'Read', 'Read')
            $locks.Add(@{name=$name; path=$item.FullName; stream=$stream})
        }
        [void][IO.Directory]::CreateDirectory($Destination)
        $records = foreach ($entry in $locks) {
            $target = Join-Path $Destination $entry.name
            $output = [IO.File]::Open($target, 'CreateNew', 'Write', 'None')
            try { $entry.stream.CopyTo($output) } finally { $output.Dispose() }
            $hash = (Get-FileHash -LiteralPath $entry.path -Algorithm SHA256).Hash
            if ($hash -ne (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash) {
                throw 'Ninja recovery copy does not match its locked source.'
            }
            [ordered]@{name=$entry.name; bytes=$entry.stream.Length; sha256=$hash.ToLowerInvariant()}
        }
        [ordered]@{
            schema='sonic-ninja-recovery-snapshot-v1'
            build_root=[IO.Path]::GetFullPath($BuildRoot)
            ninja=[IO.Path]::GetFullPath($Ninja)
            ninja_sha256=(Get-FileHash -LiteralPath $Ninja -Algorithm SHA256).Hash.ToLowerInvariant()
            recovery_authorized=$false
            files=@($records)
        } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Destination 'manifest.json')
    } finally {
        foreach ($entry in $locks) { $entry.stream.Dispose() }
    }
}
