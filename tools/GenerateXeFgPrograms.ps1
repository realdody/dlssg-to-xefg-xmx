<#
.SYNOPSIS
    Generates the ported XeSS MFG program set the dll loads from xefg_mfg_programs next to itself.

.DESCRIPTION
    MFG-Unlocker's pinned release dll embeds the XeSS Frame Generation programs the ported MFG
    model needs, plus the validated tensor-core fatbin. This project does not commit or redistribute
    them; the build extracts them here.

    programs.tsv is verified against MFG-Unlocker's pinned manifest SHA-256, every program against
    its manifest row, and the fatbin against its own pin. The output is byte-identical to the set
    the port was developed against.

.PARAMETER Output
    Directory to write programs.tsv, the 54 .dxil programs and xefg_cuda_layers.fatbin to.

.PARAMETER Cache
    Directory for the downloaded release archive and extracted dll. Defaults to %TEMP%.

.PARAMETER Dll
    Use an existing MFGUnlocker.dll instead of downloading the pinned release.
#>
param(
    [Parameter(Mandatory = $true)][string]$Output,
    [string]$Cache,
    [string]$Dll
)

$ErrorActionPreference = 'Stop'

$ReleaseUrl = 'https://github.com/dashdogy/MFG-Unlocker/releases/download/v1.7.0/MFGUnlocker-v1.7.0.zip'
$ReleaseSha256 = '5bd19d03288e1ece1d0bb2030ac072b36758045028da192044dcaad73fd901dd'
$DllSha256 = 'e6b6e6b70a6ed3af839c9647a32fc22660ae6b2d655062c5c072246c91fc10c7'
$ManifestSha256 = 'd74bd783d29280ed372653701ce9b0395362e381faace1213206e0bdd9bfc9e5'
$FatbinResourceId = 7300
$FatbinSha256 = '2ffae52e7878ae7e374ff953b864204f079c3199dc904b2ce7ecf8b37ee130f7'
$FatbinName = 'xefg_cuda_layers.fatbin'
$ProgramCount = 54

# MFG-Unlocker's MIT license, written next to the generated programs. MFG-Unlocker itself is not vendored.
$LicenseText = @'
MIT License

Copyright (c) 2026 Michael Robles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
'@

function Get-Sha256Hex([byte[]]$Bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha.ComputeHash($Bytes)
    } finally {
        $sha.Dispose()
    }
    return [System.BitConverter]::ToString($digest).Replace('-', '').ToLowerInvariant()
}

function Get-FileSha256Hex([string]$Path) {
    # Hash through .NET instead of Get-FileHash so the script also runs in hosts where that cmdlet is unavailable.
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $stream = [System.IO.File]::OpenRead($Path)
        try {
            $digest = $sha.ComputeHash($stream)
        } finally {
            $stream.Dispose()
        }
    } finally {
        $sha.Dispose()
    }
    return [System.BitConverter]::ToString($digest).Replace('-', '').ToLowerInvariant()
}

function Get-LicenseBytes {
    # The here-string drops the newline before its closing delimiter; the upstream file ends with one CRLF.
    return [System.Text.Encoding]::ASCII.GetBytes((($LicenseText.TrimEnd() + "`r`n") -replace '\r?\n', "`r`n"))
}

function Test-LicenseCurrent([string]$Directory) {
    $path = Join-Path $Directory 'LICENSE'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $false
    }
    return (Get-Sha256Hex ([System.IO.File]::ReadAllBytes($path))) -eq (Get-Sha256Hex (Get-LicenseBytes))
}

function Write-License([string]$Directory) {
    [void][System.IO.Directory]::CreateDirectory($Directory)
    [System.IO.File]::WriteAllBytes((Join-Path $Directory 'LICENSE'), (Get-LicenseBytes))
}

function Test-FatbinCurrent([string]$Directory) {
    $path = Join-Path $Directory $FatbinName
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $false
    }
    return (Get-FileSha256Hex $path) -eq $FatbinSha256
}

function Write-Fatbin([string]$Directory, [byte[]]$Bytes) {
    if ((Get-Sha256Hex $Bytes) -ne $FatbinSha256) {
        throw 'The extracted tensor-core fatbin does not match its pin'
    }
    [void][System.IO.Directory]::CreateDirectory($Directory)
    [System.IO.File]::WriteAllBytes((Join-Path $Directory $FatbinName), $Bytes)
    if (-not (Test-FatbinCurrent $Directory)) {
        throw "Verification failed after writing the tensor-core fatbin in $Directory"
    }
}

function Read-ManifestRows([byte[]]$Bytes) {
    $text = [System.Text.Encoding]::ASCII.GetString($Bytes)
    $rows = @()
    foreach ($line in ($text -split "`r?`n")) {
        if ($line.Length -eq 0) {
            continue
        }
        $parts = $line.Split("`t")
        if ($parts.Count -ne 2 -or $parts[0] -notmatch '^[0-9]{4,24}$' -or $parts[1] -notmatch '^[0-9a-f]{64}$') {
            throw "Malformed programs.tsv row: $line"
        }
        $rows += [pscustomobject]@{ Key = $parts[0]; Hash = $parts[1] }
    }
    return $rows
}

function Get-OutputRows([string]$Directory) {
    $manifestPath = Join-Path $Directory 'programs.tsv'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        return $null
    }
    $bytes = [System.IO.File]::ReadAllBytes($manifestPath)
    if ((Get-Sha256Hex $bytes) -ne $ManifestSha256) {
        return $null
    }
    $rows = Read-ManifestRows $bytes
    if ($rows.Count -ne $ProgramCount) {
        return $null
    }
    return $rows
}

function Test-OutputCurrent([string]$Directory) {
    $rows = Get-OutputRows $Directory
    if ($null -eq $rows) {
        return $false
    }
    foreach ($row in $rows) {
        $path = Join-Path $Directory ($row.Key + '.dxil')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            return $false
        }
        if ((Get-FileSha256Hex $path) -ne $row.Hash) {
            return $false
        }
    }
    return $true
}

function Resolve-ProgramSource([string]$ExplicitDll, [string]$CacheDirectory) {
    if ($ExplicitDll) {
        $path = [System.IO.Path]::GetFullPath($ExplicitDll)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "MFGUnlocker.dll not found: $path"
        }
        return $path
    }

    if (-not $CacheDirectory) {
        $CacheDirectory = Join-Path $env:TEMP 'dlssg-to-xefg'
    }
    $CacheDirectory = [System.IO.Path]::GetFullPath($CacheDirectory)
    [void][System.IO.Directory]::CreateDirectory($CacheDirectory)

    $zipPath = Join-Path $CacheDirectory 'MFGUnlocker-v1.7.0.zip'
    $dllPath = Join-Path $CacheDirectory 'MFGUnlocker.dll'

    $download = $true
    if (Test-Path -LiteralPath $zipPath -PathType Leaf) {
        if ((Get-FileSha256Hex $zipPath) -eq $ReleaseSha256) {
            $download = $false
        }
    }
    if ($download) {
        Write-Host "Downloading the pinned MFG-Unlocker v1.7.0 release..."
        $ProgressPreference = 'SilentlyContinue'
        try {
            [Net.ServicePointManager]::SecurityProtocol =
                [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        } catch {
            # Older .NET may already negotiate TLS 1.2; Invoke-WebRequest handles it.
        }
        if (Test-Path -LiteralPath $zipPath) {
            Remove-Item -LiteralPath $zipPath -Force
        }
        Invoke-WebRequest -Uri $ReleaseUrl -OutFile $zipPath -UseBasicParsing
        $actual = Get-FileSha256Hex $zipPath
        if ($actual -ne $ReleaseSha256) {
            Remove-Item -LiteralPath $zipPath -Force
            throw "MFG-Unlocker release archive hash mismatch: $actual"
        }
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
    try {
        $entry = $archive.GetEntry('MFGUnlocker.dll')
        if ($null -eq $entry) {
            throw "MFGUnlocker.dll is not in $zipPath"
        }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dllPath, $true)
    } finally {
        $archive.Dispose()
    }
    return $dllPath
}

function Get-RcdataResources([byte[]]$Image) {
    if ($Image.Length -lt 0x40) {
        throw 'Not a PE image'
    }
    $lfanew = [BitConverter]::ToInt32($Image, 0x3C)
    if ($lfanew -le 0 -or $lfanew + 24 -gt $Image.Length) {
        throw 'Not a PE image'
    }
    if ([BitConverter]::ToUInt32($Image, $lfanew) -ne 0x00004550) {
        throw 'Not a PE image'
    }

    $numberOfSections = [BitConverter]::ToUInt16($Image, $lfanew + 6)
    $sizeOfOptionalHeader = [BitConverter]::ToUInt16($Image, $lfanew + 20)
    $optional = $lfanew + 24
    if ($optional + 112 + 3 * 8 -gt $Image.Length) {
        throw 'Not a PE32+ image'
    }
    if ([BitConverter]::ToUInt16($Image, $optional) -ne 0x20B) {
        throw 'Not a PE32+ image'
    }

    $resourceRva = [BitConverter]::ToUInt32($Image, $optional + 112 + 2 * 8)
    if ($resourceRva -eq 0) {
        throw 'The image has no resource directory'
    }

    $sections = @()
    $sectionTable = $optional + $sizeOfOptionalHeader
    for ($i = 0; $i -lt $numberOfSections; $i++) {
        $at = $sectionTable + $i * 40
        if ($at + 40 -gt $Image.Length) {
            throw 'Truncated section table'
        }
        $sections += [pscustomobject]@{
            VirtualAddress   = [BitConverter]::ToUInt32($Image, $at + 12)
            VirtualSize      = [BitConverter]::ToUInt32($Image, $at + 8)
            PointerToRawData = [BitConverter]::ToUInt32($Image, $at + 20)
            SizeOfRawData    = [BitConverter]::ToUInt32($Image, $at + 16)
        }
    }

    function Convert-RvaToOffset([uint32]$Rva) {
        foreach ($section in $sections) {
            $span = [Math]::Max($section.VirtualSize, $section.SizeOfRawData)
            if ($Rva -ge $section.VirtualAddress -and $Rva -lt ($section.VirtualAddress + $span)) {
                $offset = [int]($Rva - $section.VirtualAddress + $section.PointerToRawData)
                if ($offset -lt 0 -or $offset -gt $Image.Length) {
                    break
                }
                return $offset
            }
        }
        throw ("RVA 0x{0:x} is not mapped by a section" -f $Rva)
    }

    $resourceBase = Convert-RvaToOffset $resourceRva
    $result = New-Object System.Collections.Generic.List[object]

    function Walk-Directory([int]$Offset, [int]$Depth, [string]$Type, [uint32]$Id) {
        if ($Offset + 16 -gt $Image.Length) {
            throw 'Truncated resource directory'
        }
        $named = [BitConverter]::ToUInt16($Image, $Offset + 12)
        $ids = [BitConverter]::ToUInt16($Image, $Offset + 14)
        for ($i = 0; $i -lt ($named + $ids); $i++) {
            $entry = $Offset + 16 + $i * 8
            if ($entry + 8 -gt $Image.Length) {
                throw 'Truncated resource directory entry'
            }
            $name = [BitConverter]::ToUInt32($Image, $entry)
            $target = [BitConverter]::ToUInt32($Image, $entry + 4)
            $subdirectory = ($target -band 0x80000000) -ne 0
            $child = $resourceBase + [int]($target -band 0x7FFFFFFF)
            if ($subdirectory) {
                $nextType = $Type
                $nextId = $Id
                if ($Depth -eq 0) {
                    if (($name -band 0x80000000) -ne 0) {
                        continue
                    }
                    $nextType = [string]$name
                } elseif ($Depth -eq 1) {
                    if (($name -band 0x80000000) -ne 0) {
                        $nextId = 0
                    } else {
                        $nextId = $name
                    }
                }
                Walk-Directory $child ($Depth + 1) $nextType $nextId
            } elseif ($Depth -eq 2 -and $Type -eq '10') {
                $dataEntry = $child
                if ($dataEntry + 16 -gt $Image.Length) {
                    throw 'Truncated resource data entry'
                }
                $dataRva = [BitConverter]::ToUInt32($Image, $dataEntry)
                $size = [BitConverter]::ToUInt32($Image, $dataEntry + 4)
                $fileOffset = Convert-RvaToOffset $dataRva
                if ($size -gt $Image.Length - $fileOffset) {
                    throw 'Resource data runs past the image'
                }
                $bytes = New-Object byte[] $size
                [Array]::Copy($Image, $fileOffset, $bytes, 0, [int]$size)
                $result.Add([pscustomobject]@{ Id = $Id; Bytes = $bytes })
            }
        }
    }

    Walk-Directory $resourceBase 0 '' 0
    return $result
}

function Get-ProgramSet([string]$DllPath) {
    $actual = Get-FileSha256Hex $DllPath
    if ($actual -ne $DllSha256) {
        throw "MFGUnlocker.dll hash mismatch: expected $DllSha256, got $actual"
    }

    $image = [System.IO.File]::ReadAllBytes($DllPath)
    $resources = Get-RcdataResources $image
    Write-Host ("Read {0} RCDATA resources from {1}" -f $resources.Count, (Split-Path -Leaf $DllPath))

    $manifest = $null
    $manifestRows = $null
    $fatbin = $null
    $byHash = @{}

    foreach ($resource in $resources) {
        $hash = Get-Sha256Hex $resource.Bytes
        if ($resource.Id -eq $FatbinResourceId) {
            if ($hash -ne $FatbinSha256) {
                throw "Tensor-core fatbin resource $FatbinResourceId does not match its pin"
            }
            $fatbin = $resource.Bytes
            continue
        }
        if ($null -eq $manifest -and $hash -eq $ManifestSha256) {
            $rows = Read-ManifestRows $resource.Bytes
            if ($rows.Count -eq $ProgramCount) {
                $manifest = $resource.Bytes
                $manifestRows = $rows
                continue
            }
        }
        if (-not $byHash.ContainsKey($hash)) {
            $byHash[$hash] = $resource.Bytes
        }
    }

    if ($null -eq $manifest) {
        throw "The pinned XeSS program manifest ($ManifestSha256) is not in the dll"
    }
    if ($null -eq $fatbin) {
        throw "The pinned tensor-core fatbin ($FatbinSha256) is not in the dll"
    }

    $missing = @()
    foreach ($row in $manifestRows) {
        if (-not $byHash.ContainsKey($row.Hash)) {
            $missing += $row.Key
        }
    }
    if ($missing.Count -ne 0) {
        throw ("The dll is missing {0} program(s): {1}" -f $missing.Count, ($missing -join ', '))
    }

    return [pscustomobject]@{ Manifest = $manifest; Rows = $manifestRows; Programs = $byHash; Fatbin = $fatbin }
}

function Write-ProgramSet([string]$Directory, $Set) {
    [void][System.IO.Directory]::CreateDirectory($Directory)

    Get-ChildItem -LiteralPath $Directory -Filter '*.dxil' -File -ErrorAction SilentlyContinue |
        Remove-Item -Force
    $manifestPath = Join-Path $Directory 'programs.tsv'
    if (Test-Path -LiteralPath $manifestPath) {
        Remove-Item -LiteralPath $manifestPath -Force
    }

    [System.IO.File]::WriteAllBytes($manifestPath, $Set.Manifest)
    Write-Fatbin $Directory $Set.Fatbin

    $written = 0
    foreach ($row in $Set.Rows) {
        $path = Join-Path $Directory ($row.Key + '.dxil')
        [System.IO.File]::WriteAllBytes($path, $Set.Programs[$row.Hash])
        if ((Get-FileSha256Hex $path) -ne $row.Hash) {
            throw "Verification failed after writing $path"
        }
        $written++
    }
    Write-License $Directory
    if (-not (Test-LicenseCurrent $Directory)) {
        throw "Verification failed after writing the LICENSE in $Directory"
    }
    return $written
}

$outputPath = [System.IO.Path]::GetFullPath($Output)
$programsCurrent = Test-OutputCurrent $outputPath
$licenseCurrent = Test-LicenseCurrent $outputPath
$fatbinCurrent = Test-FatbinCurrent $outputPath
if ($programsCurrent -and $licenseCurrent -and $fatbinCurrent) {
    Write-Host "XeFG MFG programs and tensor-core fatbin are up to date in $outputPath"
    exit 0
}
if ($programsCurrent) {
    if (-not $licenseCurrent) {
        Write-License $outputPath
        if (-not (Test-LicenseCurrent $outputPath)) {
            throw "Could not write the XeFG MFG program LICENSE in $outputPath"
        }
    }
    if (-not $fatbinCurrent) {
        $dllPath = Resolve-ProgramSource $Dll $Cache
        Write-Host "Using MFGUnlocker.dll: $dllPath"
        $set = Get-ProgramSet $dllPath
        Write-Fatbin $outputPath $set.Fatbin
    }
    if (-not (Test-LicenseCurrent $outputPath) -or -not (Test-FatbinCurrent $outputPath)) {
        throw "Could not complete the XeFG MFG output in $outputPath"
    }
    Write-Host "Restored the missing XeFG MFG output in $outputPath"
    exit 0
}

$dllPath = Resolve-ProgramSource $Dll $Cache
Write-Host "Using MFGUnlocker.dll: $dllPath"
$set = Get-ProgramSet $dllPath
$written = Write-ProgramSet $outputPath $set

if (-not (Test-OutputCurrent $outputPath) -or -not (Test-LicenseCurrent $outputPath) -or -not (Test-FatbinCurrent $outputPath)) {
    throw "Generated program set did not verify in $outputPath"
}

$unique = @{}
foreach ($row in $set.Rows) {
    $unique[$row.Hash] = $true
}
Write-Host ("Generated {0} XeFG MFG programs ({1} unique blobs) and the tensor-core fatbin in {2}" -f $written, $unique.Count, $outputPath)
