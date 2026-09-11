# Private-title-only importer. It never writes a manifest, an adapter, or a
# per-instruction hardware resolution: imported bounds remain subject to the
# Katana full-function proof on the next export.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Requirements,

    [Parameter(Mandatory = $true)]
    [string] $LegacyMap,

    [Parameter(Mandatory = $true)]
    [string] $Boot,

    [Parameter(Mandatory = $true)]
    [string] $Output,

    # Optional private C++ projection of only the legacy-exact boundaries
    # which still need a fresh Katana whole-function proof.  This projection
    # deliberately carries no hooks or per-site resolutions.
    [string] $BoundaryInclude,

    [string] $BootBase = '0x8C010000'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-CanonicalPath([string] $Path) {
    return [IO.Path]::GetFullPath($Path)
}

function Get-Sha256Identity([byte[]] $Bytes) {
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = $hasher.ComputeHash($Bytes)
    } finally {
        $hasher.Dispose()
    }
    return 'sha256:' + (($digest | ForEach-Object { $_.ToString('x2') }) -join '')
}

function ConvertTo-GuestAddress([object] $Value, [string] $Field) {
    if ($Value -is [string]) {
        if ($Value -notmatch '^0x[0-9A-Fa-f]{1,8}$') {
            throw "$Field is not a uint32 hexadecimal address"
        }
        return [uint64][Convert]::ToUInt32($Value.Substring(2), 16)
    }
    try {
        $number = [uint64]$Value
    } catch {
        throw "$Field is not a uint32 address"
    }
    if ($number -gt 4294967295) {
        throw "$Field is outside uint32"
    }
    return $number
}

function Format-GuestAddress([uint64] $Address) {
    if ($Address -gt 4294967295) {
        throw 'guest address is outside uint32'
    }
    return ('0x{0:X8}' -f $Address)
}

function Read-PrivateJson([string] $Path, [string[]] $ExpectedSchemas) {
    $absolute = Get-CanonicalPath $Path
    if (-not (Test-Path -LiteralPath $absolute -PathType Leaf)) {
        throw "Input does not exist: $absolute"
    }
    $bytes = [IO.File]::ReadAllBytes($absolute)
    try {
        $document = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
    } catch {
        throw "Invalid JSON in ${absolute}: $($_.Exception.Message)"
    }
    if ($ExpectedSchemas -notcontains [string]$document.schema) {
        throw "Unexpected schema in ${absolute}: $($document.schema)"
    }
    return [pscustomobject]@{ Path = $absolute; Bytes = $bytes; Document = $document }
}

function Get-NormalizedStringList([object[]] $Values) {
    return @($Values | ForEach-Object { [string]$_ } | Sort-Object -Unique)
}

function Test-HasRegion([string[]] $Regions, [string] $Region) {
    return $Regions -contains $Region
}

function Get-ProviderFamily([string[]] $Regions) {
    if (Test-HasRegion $Regions 'aica') { return 'native_audio' }
    if ((Test-HasRegion $Regions 'unknown') -and
        (Test-HasRegion $Regions 'pvr')) { return 'kamui_ninja_overlay_renderer' }
    if (Test-HasRegion $Regions 'unknown') { return 'title_overlay' }
    if (Test-HasRegion $Regions 'pvr') { return 'kamui_ninja_renderer' }
    foreach ($region in @('sh4_dmac', 'sh4_qacr', 'store_queue', 'g2_dma', 'system_bus')) {
        if (Test-HasRegion $Regions $region) { return 'transfer_ta_system' }
    }
    if (Test-HasRegion $Regions 'sh4_tmu') { return 'host_timing' }
    foreach ($region in @('system_asic', 'sh4_intc', 'sh4_mmu', 'sh4_cache', 'sh4_io')) {
        if (Test-HasRegion $Regions $region) { return 'platform_interrupt_state' }
    }
    return 'unclassified'
}

function Get-PrivateProviderSymbol([string] $ProviderFamily, [uint64] $Entry) {
    return "sonic_native_${ProviderFamily}_owner_$((Format-GuestAddress $Entry).Substring(2).ToLowerInvariant())"
}

function Get-CurrentBoundary($Candidate, [uint64] $Entry) {
    $sizeProperty = $Candidate.PSObject.Properties['exact_size']
    $identityProperty = $Candidate.PSObject.Properties['code_identity']
    if ($null -eq $sizeProperty -or $null -eq $identityProperty -or
        $null -eq $sizeProperty.Value -or $null -eq $identityProperty.Value) {
        return $null
    }
    $size = [uint64]$sizeProperty.Value
    $identity = [string]$identityProperty.Value
    if ($size -eq 0 -or $identity -notmatch '^sha256:[0-9a-f]{64}$' -or
        $Entry + $size -gt 4294967296) {
        return $null
    }
    return [ordered]@{
        state = 'current-export'
        source = 'native-hook-requirements-current-proof'
        start = Format-GuestAddress $Entry
        end = Format-GuestAddress ($Entry + $size)
        byte_size = $size
        code_identity = $identity
    }
}

function Get-ExactLegacyBoundary(
    [object[]] $LegacyFunctions,
    [uint64] $Entry,
    [byte[]] $BootBytes,
    [uint64] $ImageBase) {
    $legacyRows = @()
    foreach ($candidate in $LegacyFunctions) {
        $startProperty = $candidate.PSObject.Properties['pal_start']
        $endProperty = $candidate.PSObject.Properties['pal_end']
        $kindProperty = $candidate.PSObject.Properties['match_kind']
        if ($null -eq $startProperty -or $null -eq $endProperty -or
            $null -eq $kindProperty -or $null -eq $startProperty.Value -or
            $null -eq $endProperty.Value -or $null -eq $kindProperty.Value -or
            ([string]$kindProperty.Value) -notmatch 'exact') {
            continue
        }
        if ((ConvertTo-GuestAddress $startProperty.Value 'legacy pal_start') -ne $Entry) {
            continue
        }
        $start = ConvertTo-GuestAddress $startProperty.Value 'legacy pal_start'
        $end = ConvertTo-GuestAddress $endProperty.Value 'legacy pal_end'
        if ($end -le $start -or ($start -band 1) -ne 0 -or
            ($end -band 1) -ne 0 -or $start -lt $ImageBase) {
            continue
        }
        $beginOffset = $start - $ImageBase
        $endOffset = $end - $ImageBase
        if ($endOffset -gt $BootBytes.LongLength) { continue }
        $legacyRows += [pscustomobject]@{
            Candidate = $candidate
            Start = $start
            End = $end
            BeginOffset = $beginOffset
            EndOffset = $endOffset
        }
    }
    if ($legacyRows.Count -eq 0) { return [pscustomobject]@{ State = 'missing' } }
    $distinct = @($legacyRows | Group-Object { "$($_.Start):$($_.End)" })
    if ($distinct.Count -ne 1) {
        return [pscustomobject]@{ State = 'ambiguous' }
    }
    $match = $distinct[0].Group[0]
    $length = [int]($match.EndOffset - $match.BeginOffset)
    $range = New-Object byte[] $length
    [Array]::Copy($BootBytes, [int]$match.BeginOffset, $range, 0, $length)
    return [pscustomobject]@{
        State = 'imported'
        Boundary = [ordered]@{
            state = 'imported'
            source = 'legacy-idb-pal-map-exact'
            start = Format-GuestAddress $match.Start
            end = Format-GuestAddress $match.End
            byte_size = $match.End - $match.Start
            code_identity = Get-Sha256Identity $range
            legacy_label = if ($null -eq $match.Candidate.idb_name) { $null } else { [string]$match.Candidate.idb_name }
            legacy_match_kind = [string]$match.Candidate.match_kind
            legacy_confidence = if ($null -eq $match.Candidate.confidence) { $null } else { [string]$match.Candidate.confidence }
        }
    }
}

function Write-ReexportBoundaryInclude(
    [string] $Path,
    [object[]] $Plans,
    [string] $RequirementsIdentity) {
    $selected = @($Plans | Where-Object {
        $_.admission -eq 'requires-reexport-full-function-proof'
    } | Sort-Object {
        ConvertTo-GuestAddress $_.owner_entry 'planned owner_entry'
    })
    if ($selected.Count -eq 0) {
        throw 'No legacy-exact boundaries require re-export proof'
    }

    $previousEnd = [uint64]0
    $first = $true
    foreach ($plan in $selected) {
        if ($null -eq $plan.boundary -or
            $plan.boundary.source -ne 'legacy-idb-pal-map-exact' -or
            $plan.boundary.code_identity -notmatch '^sha256:[0-9a-f]{64}$') {
            throw "$($plan.owner_entry): re-export boundary lacks exact private identity"
        }
        $start = ConvertTo-GuestAddress $plan.owner_entry 'planned owner_entry'
        $boundaryStart = ConvertTo-GuestAddress $plan.boundary.start 'planned boundary start'
        $boundaryEnd = ConvertTo-GuestAddress $plan.boundary.end 'planned boundary end'
        $size = [uint64]$plan.boundary.byte_size
        if ($start -ne $boundaryStart -or $boundaryEnd -le $boundaryStart -or
            $boundaryEnd - $boundaryStart -ne $size -or ($size -band 1) -ne 0) {
            throw "$($plan.owner_entry): invalid exact re-export boundary"
        }
        if (-not $first -and $boundaryStart -lt $previousEnd) {
            throw "$($plan.owner_entry): exact re-export boundaries overlap"
        }
        $previousEnd = $boundaryEnd
        $first = $false
    }

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('// Generated by import-native-hook-boundaries.ps1. Do not edit.')
    $lines.Add('// Private Sonic Adventure PAL v1.003 legacy-exact boundaries only.')
    $lines.Add('// Each entry remains contingent on a fresh Katana full-function proof.')
    $lines.Add("// Requirements identity: $RequirementsIdentity")
    $lines.Add('')
    $lines.Add('#pragma once')
    $lines.Add('')
    $lines.Add('#include <array>')
    $lines.Add('#include "katana/runtime/game_project.hpp"')
    $lines.Add('')
    $lines.Add('namespace sonic_adventure::private_data {')
    $lines.Add('')
    $lines.Add('inline constexpr std::array<katana::runtime::GameProjectFunctionBoundary, ' +
        $selected.Count + 'u> legacy_exact_reexport_boundaries{')
    foreach ($plan in $selected) {
        $entry = ConvertTo-GuestAddress $plan.owner_entry 'planned owner_entry'
        $symbol = 'sa_legacy_exact_' + [string]$plan.provider_family + '_owner_' +
            (Format-GuestAddress $entry).Substring(2).ToLowerInvariant()
        $lines.Add('    katana::runtime::GameProjectFunctionBoundary{' +
            (Format-GuestAddress $entry) + 'u, ' + [uint64]$plan.boundary.byte_size +
            'u, "' + $symbol + '"},')
    }
    $lines.Add('};')
    $lines.Add('')
    $lines.Add('} // namespace sonic_adventure::private_data')
    $lines.Add('')

    $outputPath = Get-CanonicalPath $Path
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath)) | Out-Null
    [IO.File]::WriteAllText($outputPath,
        [string]::Join([Environment]::NewLine, $lines),
        [Text.UTF8Encoding]::new($false))
    return [ordered]@{
        path = $outputPath
        sha256 = Get-Sha256Identity ([IO.File]::ReadAllBytes($outputPath))
        boundary_count = $selected.Count
    }
}

$requirementsInput = Read-PrivateJson $Requirements @(
    'katana.native-port-hook-requirements.v1',
    'katana.native-port-hook-requirements.v2',
    'katana.native-port-hook-requirements.v3',
    'katana.native-port-hook-requirements.v4')
$legacyInput = Read-PrivateJson $LegacyMap @(
    'katana-private-legacy-idb-pal-map')
$requirementsDocument = $requirementsInput.Document
$legacy = $legacyInput.Document
$bootPath = Get-CanonicalPath $Boot
if (-not (Test-Path -LiteralPath $bootPath -PathType Leaf)) {
    throw "Boot image does not exist: $bootPath"
}
$bootBytes = [IO.File]::ReadAllBytes($bootPath)
$bootIdentity = Get-Sha256Identity $bootBytes
$bootBaseValue = ConvertTo-GuestAddress $BootBase '--boot-base'

if ($requirementsDocument.PSObject.Properties.Name -notcontains 'function_candidates' -or
    $requirementsDocument.PSObject.Properties.Name -notcontains 'sites') {
    throw 'Native hook requirements has no function_candidates/sites arrays'
}
if ($legacy.PSObject.Properties.Name -notcontains 'functions' -or
    $legacy.PSObject.Properties.Name -notcontains 'sources') {
    throw 'Legacy map has no functions/PAL image identity'
}
if ($legacy.sources.PSObject.Properties.Name -notcontains 'pal_image') {
    throw 'Legacy map has no PAL image identity'
}
if ($bootIdentity -ne $legacy.sources.pal_image.sha256 -or
    $bootBytes.LongLength -ne [int64]$legacy.sources.pal_image.size) {
    throw "Boot image does not match the legacy disassembly identity"
}

$sitesByInstruction = @{}
foreach ($site in @($requirementsDocument.sites)) {
    $instruction = ConvertTo-GuestAddress $site.instruction_address `
        'requirements site instruction_address'
    $key = Format-GuestAddress $instruction
    if ($sitesByInstruction.ContainsKey($key)) {
        throw "Duplicate requirements site: $key"
    }
    $sitesByInstruction[$key] = $site
}

$candidateRows = @()
foreach ($candidate in @($requirementsDocument.function_candidates)) {
    $entry = ConvertTo-GuestAddress $candidate.entry_address 'function candidate entry_address'
    $candidateRows += [pscustomobject]@{ Candidate = $candidate; Entry = $entry }
}
$candidateRows = @($candidateRows | Sort-Object Entry)
for ($index = 1; $index -lt $candidateRows.Count; ++$index) {
    if ($candidateRows[$index - 1].Entry -eq $candidateRows[$index].Entry) {
        throw "Duplicate function candidate: $(Format-GuestAddress $candidateRows[$index].Entry)"
    }
}

$plans = @()
foreach ($row in $candidateRows) {
    $candidate = $row.Candidate
    $entry = $row.Entry
    $entryText = Format-GuestAddress $entry
    $requirementSites = @($candidate.requirement_sites |
        ForEach-Object { Format-GuestAddress (ConvertTo-GuestAddress $_ 'function candidate requirement_site') } |
        Sort-Object -Unique)
    $ambiguousRequirementSiteCount = 0
    $openRequirementSiteCount = 0
    foreach ($siteAddress in $requirementSites) {
        if (-not $sitesByInstruction.ContainsKey($siteAddress)) {
            throw "${entryText}: requirement site is absent from requirements inventory: $siteAddress"
        }
        $site = $sitesByInstruction[$siteAddress]
        $owners = @($site.owner_candidates | ForEach-Object {
            Format-GuestAddress (ConvertTo-GuestAddress $_ 'requirements owner_candidates')
        } | Sort-Object -Unique)
        if ($owners -notcontains $entryText) {
            throw "${entryText}: requirement site does not name candidate as owner: $siteAddress"
        }
        if (@($site.gaps).Count -ne 0) {
            ++$openRequirementSiteCount
        }
        if ($owners.Count -ne 1) {
            ++$ambiguousRequirementSiteCount
        }
    }
    $regions = Get-NormalizedStringList @($candidate.regions)
    $family = Get-ProviderFamily $regions
    $current = Get-CurrentBoundary $candidate $entry
    $imported = Get-ExactLegacyBoundary @($legacy.functions) $entry $bootBytes $bootBaseValue
    $boundary = $current
    $boundaryImport = 'not-needed-current-proof'
    if ($null -eq $current) {
        if ($imported.State -eq 'imported') {
            $boundary = $imported.Boundary
        } else {
            $boundary = $null
        }
        $boundaryImport = $imported.State
    } elseif ($imported.State -eq 'imported' -and
              ($current.byte_size -ne $imported.Boundary.byte_size -or
               $current.code_identity -ne $imported.Boundary.code_identity)) {
        throw "${entryText}: legacy/current exact boundary mismatch"
    }
    $existing = $candidate.current_hook
    $functionCandidate = $candidate.function_hook_candidate -eq $true
    if ($functionCandidate) {
        $admission = if ($null -eq $existing) {
            'ready-current-proof'
        } else {
            'retain-current-proof'
        }
    } elseif ($null -ne $boundary -and
              $boundary.source -eq 'legacy-idb-pal-map-exact') {
        $admission = 'requires-reexport-full-function-proof'
    } else {
        $admission = 'blocked-exact-boundary-unavailable'
    }
    $existingHook = if ($null -eq $existing) {
        $null
    } else {
        [ordered]@{
            symbol = if ($null -eq $existing.symbol) { $null } else { [string]$existing.symbol }
            covered_size = $existing.covered_size
        }
    }
    $symbol = if ($null -ne $existing -and $null -ne $existing.symbol) {
        [string]$existing.symbol
    } else {
        Get-PrivateProviderSymbol $family $entry
    }
    $plans += [pscustomobject]([ordered]@{
        owner_entry = $entryText
        provider_family = $family
        requirement_site_count = $requirementSites.Count
        open_requirement_site_count = $openRequirementSiteCount
        ambiguous_requirement_site_count = $ambiguousRequirementSiteCount
        requirement_sites = $requirementSites
        hardware_site_count = [int]$candidate.hardware_site_count
        dynamic_memory_site_count = [int]$candidate.dynamic_memory_site_count
        regions = $regions
        boundary = $boundary
        boundary_import = $boundaryImport
        current_proof = [ordered]@{
            function_hook_candidate = $functionCandidate
            reason = [string]$candidate.function_hook_candidate_reason
        }
        admission = $admission
        existing_hook = $existingHook
        proposed_function_binding = [ordered]@{
            kind = 'FunctionEntry'
            requirement = 'Required'
            original_policy = 'ReplacesOriginal'
            symbol = $symbol
            hardware_resolution_count = 0
        }
    })
}

$familyRows = @()
foreach ($group in @($plans | Group-Object provider_family | Sort-Object Name)) {
    $groupPlans = @($group.Group)
    $familyRows += [ordered]@{
        provider_family = $group.Name
        owner_count = $groupPlans.Count
        requirement_site_count = @($groupPlans | Measure-Object requirement_site_count -Sum).Sum
        open_requirement_site_count = @($groupPlans | Measure-Object open_requirement_site_count -Sum).Sum
        ambiguous_requirement_site_count = @($groupPlans | Measure-Object ambiguous_requirement_site_count -Sum).Sum
        imported_exact_boundary_count = @($groupPlans | Where-Object {
            $null -ne $_.boundary -and $_.boundary.source -eq 'legacy-idb-pal-map-exact'
        }).Count
        current_proof_ready_count = @($groupPlans | Where-Object {
            $_.admission -eq 'ready-current-proof' -or $_.admission -eq 'retain-current-proof'
        }).Count
        reexport_proof_required_count = @($groupPlans | Where-Object {
            $_.admission -eq 'requires-reexport-full-function-proof'
        }).Count
        blocked_count = @($groupPlans | Where-Object {
            $_.admission -eq 'blocked-exact-boundary-unavailable'
        }).Count
    }
}

$boundaryIncludeReport = $null
if (-not [string]::IsNullOrWhiteSpace($BoundaryInclude)) {
    $boundaryIncludeReport = Write-ReexportBoundaryInclude `
        $BoundaryInclude $plans (Get-Sha256Identity $requirementsInput.Bytes)
}

$outputDocument = [ordered]@{
    schema = 'sonic-adventure-pal-v1003-private-native-hook-plan-v1'
    privacy = 'private-title-data'
    purpose = 'legacy-exact-boundary-import-and-whole-function-hook-plan'
    policy = [ordered]@{
        exact_boundary_source = 'legacy map entries with exact match_kind and a unique PAL start/end'
        byte_identity = 'sha256 over the exact PAL boot byte range'
        full_function_proof = 'Katana export must re-prove every imported boundary before a binding may close sites'
        hardware_resolutions = 'forbidden; whole-function bindings close contained sites only after proof'
    }
    inputs = [ordered]@{
        native_hook_requirements = [ordered]@{
            path = $requirementsInput.Path
            sha256 = Get-Sha256Identity $requirementsInput.Bytes
            schema = [string]$requirementsDocument.schema
            site_count = @($requirementsDocument.sites).Count
            gapped_site_count = @($requirementsDocument.sites | Where-Object { @($_.gaps).Count -ne 0 }).Count
            function_candidate_count = $candidateRows.Count
        }
        legacy_disassembly_map = [ordered]@{
            path = $legacyInput.Path
            sha256 = Get-Sha256Identity $legacyInput.Bytes
            pal_image_sha256 = [string]$legacy.sources.pal_image.sha256
        }
        pal_boot_image = [ordered]@{
            path = $bootPath
            load_address = Format-GuestAddress $bootBaseValue
            byte_size = $bootBytes.LongLength
            sha256 = $bootIdentity
        }
    }
    summary = [ordered]@{
        owner_count = $plans.Count
        requirement_site_count = @($plans | Measure-Object requirement_site_count -Sum).Sum
        open_requirement_site_count = @($plans | Measure-Object open_requirement_site_count -Sum).Sum
        ambiguous_requirement_site_count = @($plans | Measure-Object ambiguous_requirement_site_count -Sum).Sum
        imported_exact_boundary_count = @($plans | Where-Object {
            $null -ne $_.boundary -and $_.boundary.source -eq 'legacy-idb-pal-map-exact'
        }).Count
        current_proof_ready_count = @($plans | Where-Object {
            $_.admission -eq 'ready-current-proof' -or $_.admission -eq 'retain-current-proof'
        }).Count
        reexport_proof_required_count = @($plans | Where-Object {
            $_.admission -eq 'requires-reexport-full-function-proof'
        }).Count
        blocked_exact_boundary_count = @($plans | Where-Object {
            $_.admission -eq 'blocked-exact-boundary-unavailable'
        }).Count
        hardware_resolution_count = 0
    }
    provider_families = $familyRows
    hook_plan = $plans
}
if ($null -ne $boundaryIncludeReport) {
    $outputDocument.boundary_include = $boundaryIncludeReport
}

$outputPath = Get-CanonicalPath $Output
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath)) | Out-Null
$json = $outputDocument | ConvertTo-Json -Depth 16
[IO.File]::WriteAllText($outputPath, $json + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

[pscustomobject]@{
    Owners = $outputDocument.summary.owner_count
    ImportedExactBoundaries = $outputDocument.summary.imported_exact_boundary_count
    CurrentProofReady = $outputDocument.summary.current_proof_ready_count
    RequiresReexportProof = $outputDocument.summary.reexport_proof_required_count
    BlockedExactBoundaries = $outputDocument.summary.blocked_exact_boundary_count
    BoundaryInclude = if ($null -eq $boundaryIncludeReport) { $null } else { $boundaryIncludeReport.path }
    Output = $outputPath
} | ConvertTo-Json -Compress
