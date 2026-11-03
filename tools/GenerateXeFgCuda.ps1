<#
.SYNOPSIS
    Builds the XeFG tensor-core kernel fatbin from the ported CUDA sources.

.DESCRIPTION
    Optional. The dll works without this file on its DXIL/DP4a path. The fatbin contains SASS for
    sm_75 (RTX 20), sm_86 (RTX 30), sm_89 (RTX 40) and sm_120 (RTX 50); no CUDA runtime is linked
    or shipped. Requires CUDA Toolkit 13.2 (nvcc V13.2.51) and the MSVC toolset that builds the dll.

.PARAMETER Output
    Directory that receives xefg_cuda_layers.fatbin, normally ...\xefg_mfg_programs.

.PARAMETER CudaRoot
    CUDA Toolkit root, e.g. C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2.

.PARAMETER Source
    Directory containing xefg_cuda_layers.cu and xefg_cuda_params.h.

.PARAMETER VcVars
    vcvars64.bat used to put nvcc in the MSVC toolset's environment.

.PARAMETER VcVarsVersion
    Optional VCToolsVersion passed to vcvars64.bat.

.PARAMETER ExpectedSha256
    Optional expected fatbin hash. Set it after tests/xess_cuda validates a build; a mismatch fails.
#>
param(
    [Parameter(Mandatory = $true)][string]$Output,
    [string]$CudaRoot = $env:CUDA_PATH,
    [Parameter(Mandatory = $true)][string]$Source,
    [string]$VcVars,
    [string]$VcVarsVersion,
    [string]$ExpectedSha256
)

$ErrorActionPreference = 'Stop'

$ExpectedNvcc = 'Cuda compilation tools, release 13.2, V13.2.51'
$Kernels = @(
    'L_aa016078', 'L_198d6bf8', 'L_dcd8becf', 'L_efbe3788', 'L_50f0c205', 'L_a3c3caa1',
    'L_9481d452', 'L_06e97933', 'L_3ed03daf', 'L_748b0baf', 'L_cb391582', 'L_fc1b8a04',
    'L_3459485', 'L_3452733', 'L_flow16_acc17', 'L_3453044'
)
$Gencodes = @(
    'arch=compute_75,code=sm_75',
    'arch=compute_86,code=sm_86',
    'arch=compute_89,code=sm_89',
    'arch=compute_120,code=sm_120'
)

if (-not $CudaRoot) {
    throw 'CudaRoot is not set (pass -CudaRoot or set CUDA_PATH)'
}
$nvcc = Join-Path $CudaRoot 'bin\nvcc.exe'
if (-not (Test-Path -LiteralPath $nvcc -PathType Leaf)) {
    throw "nvcc not found: $nvcc"
}
$sourceFile = Join-Path $Source 'xefg_cuda_layers.cu'
if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
    throw "CUDA kernel source not found: $sourceFile"
}

$version = (& $nvcc --version 2>&1 | Out-String)
if ($version -notmatch [regex]::Escape($ExpectedNvcc)) {
    throw "Unpinned nvcc: expected '$ExpectedNvcc', got: $($version.Trim())"
}

if (-not $VcVars) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installation = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
        if ($installation) {
            $VcVars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
        }
    }
}
if (-not $VcVars -or -not (Test-Path -LiteralPath $VcVars -PathType Leaf)) {
    throw "vcvars64.bat not found (pass -VcVars): $VcVars"
}

[void][System.IO.Directory]::CreateDirectory($Output)
$target = Join-Path $Output 'xefg_cuda_layers.fatbin'

$command = 'call "' + $VcVars + '"'
if ($VcVarsVersion) {
    $command += ' -vcvars_ver=' + $VcVarsVersion
}
$command += ' >nul 2>nul && "' + $nvcc + '" -fatbin -O3'
foreach ($gencode in $Gencodes) {
    $command += ' -gencode ' + $gencode
}
$command += ' -I "' + $Source + '" -o "' + $target + '" "' + $sourceFile + '"'

$compiled = & cmd.exe /d /s /c $command 2>&1
$compiled | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) {
    throw "nvcc failed with exit $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $target -PathType Leaf)) {
    throw "nvcc did not write $target"
}

$bytes = [System.IO.File]::ReadAllBytes($target)
$text = [System.Text.Encoding]::ASCII.GetString($bytes)
foreach ($kernel in $Kernels) {
    if (-not $text.Contains($kernel + "`0")) {
        throw "$kernel is missing from $target"
    }
}

$sha = [System.Security.Cryptography.SHA256]::Create()
try {
    $stream = [System.IO.File]::OpenRead($target)
    try {
        $digest = $sha.ComputeHash($stream)
    } finally {
        $stream.Dispose()
    }
} finally {
    $sha.Dispose()
}
$hash = [System.BitConverter]::ToString($digest).Replace('-', '').ToLowerInvariant()
if ($ExpectedSha256 -and $hash -ne $ExpectedSha256.ToLowerInvariant()) {
    throw "fatbin hash $hash does not match the pinned $ExpectedSha256"
}

Write-Host "Generated $target ($hash, $($bytes.Length) bytes)"
