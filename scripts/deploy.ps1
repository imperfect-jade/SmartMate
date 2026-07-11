[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [string] $QtRoot = $env:QT_ROOT,

    [string] $QtMinGwRoot = $env:QT_MINGW_ROOT
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-ExistingDirectory {
    param(
        [Parameter(Mandatory)]
        [string] $Path,

        [Parameter(Mandatory)]
        [string] $Description
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "$Description is not set. Pass it as a script parameter or set its environment variable."
    }

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Description does not exist: $Path"
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory)]
        [string] $Executable,

        [Parameter(Mandatory)]
        [string[]] $Arguments,

        [Parameter(Mandatory)]
        [string] $FailureMessage
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FailureMessage (exit code: $LASTEXITCODE)"
    }
}

$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$QtRoot = Resolve-ExistingDirectory -Path $QtRoot -Description 'Qt root (QT_ROOT)'
$QtMinGwRoot = Resolve-ExistingDirectory -Path $QtMinGwRoot -Description 'Qt MinGW root (QT_MINGW_ROOT)'

$qtBinDirectory = Join-Path $QtRoot 'bin'
$mingwBinDirectory = Join-Path $QtMinGwRoot 'bin'
$windeployqt = Join-Path $qtBinDirectory 'windeployqt.exe'
$compiler = Join-Path $mingwBinDirectory 'g++.exe'

if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    throw "windeployqt.exe was not found: $windeployqt"
}

if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    throw "The matching Qt MinGW compiler was not found: $compiler"
}

$cmakeCommand = Get-Command cmake.exe -ErrorAction Stop
$preset = $Configuration.ToLowerInvariant()
$buildDirectory = Join-Path $repositoryRoot "build/$preset"
$sourceExecutable = Join-Path $buildDirectory 'SmartMate.exe'
$qmlSourceDirectory = Join-Path $repositoryRoot 'src/view/qml'
$qmlImportDirectory = Join-Path $buildDirectory 'qml'
# Debug构建不等于一定安装了Debug版Qt；先探测DLL，再选择windeployqt运行库模式。
$debugRuntimeCandidates = @(
    (Join-Path $qtBinDirectory 'Qt6Cored.dll'),
    (Join-Path $QtRoot 'plugins/platforms/qwindowsd.dll'),
    (Join-Path $QtRoot 'plugins/sqldrivers/qsqlited.dll')
)
$missingDebugRuntimeFiles = @(
    $debugRuntimeCandidates | Where-Object {
        -not (Test-Path -LiteralPath $_ -PathType Leaf)
    }
)
$hasDebugQtRuntime = $missingDebugRuntimeFiles.Count -eq 0
$runtimeMode = if ($Configuration -eq 'Debug' -and $hasDebugQtRuntime) {
    'debug'
} else {
    'release'
}

# 递归清理前先规范化绝对路径，并确认目标严格位于仓库dist目录内。
$distributionRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'dist'))
$distributionDirectory = [System.IO.Path]::GetFullPath((Join-Path $distributionRoot 'SmartMate'))
$allowedPrefix = $distributionRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar

if (-not $distributionDirectory.StartsWith(
        $allowedPrefix,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean a path outside the dist directory: $distributionDirectory"
}

$env:QT_ROOT = $QtRoot
$env:QT_MINGW_ROOT = $QtMinGwRoot
# 配套Qt与MinGW必须排在PATH最前，防止全局编译器造成ABI混用。
$env:PATH = "$qtBinDirectory;$mingwBinDirectory;$env:PATH"

Push-Location $repositoryRoot
try {
    Write-Host "[1/4] Configure the $Configuration build"
    Invoke-CheckedCommand `
        -Executable $cmakeCommand.Source `
        -Arguments @('--preset', $preset) `
        -FailureMessage 'CMake configuration failed'

    Write-Host '[2/4] Build SmartMate'
    Invoke-CheckedCommand `
        -Executable $cmakeCommand.Source `
        -Arguments @('--build', '--preset', $preset) `
        -FailureMessage 'CMake build failed'

    if (-not (Test-Path -LiteralPath $sourceExecutable -PathType Leaf)) {
        throw "The executable was not found after the build: $sourceExecutable"
    }

    if (Test-Path -LiteralPath $distributionDirectory) {
        # 此递归删除依赖上方的dist子目录校验，不得绕过或拆分两段安全逻辑。
        Remove-Item -LiteralPath $distributionDirectory -Recurse -Force
    }

    New-Item -ItemType Directory -Path $distributionDirectory -Force | Out-Null
    $deployedExecutable = Join-Path $distributionDirectory 'SmartMate.exe'
    Copy-Item -LiteralPath $sourceExecutable -Destination $deployedExecutable

    Write-Host '[3/4] Collect Qt, QML, and MinGW runtime files'
    if ($Configuration -eq 'Debug' -and -not $hasDebugQtRuntime) {
        Write-Host 'Qt debug DLLs are not installed; deploy the Debug build with the matching release Qt runtime.'
    }
    # windeployqt扫描QML导入并收集Qt/MinGW运行库；qoffscreen供无界面启动验证使用。
    $deployArguments = @(
        "--$runtimeMode",
        '--compiler-runtime',
        '--no-system-dxc-compiler',
        '--include-plugins', 'qoffscreen',
        '--skip-plugin-types', 'qmltooling',
        '--translations', 'zh_CN,en',
        '--verbose', '0',
        '--qmldir', $qmlSourceDirectory,
        '--qmlimport', $qmlImportDirectory,
        '--dir', $distributionDirectory,
        $deployedExecutable
    )
    Invoke-CheckedCommand `
        -Executable $windeployqt `
        -Arguments $deployArguments `
        -FailureMessage 'windeployqt failed'

    # 交付前显式检查核心DLL、平台插件和qsqlite，避免只在开发机上能够启动。
    $debugSuffix = if ($runtimeMode -eq 'debug') { 'd' } else { '' }
    $requiredRelativePaths = @(
        'SmartMate.exe',
        "Qt6Core$debugSuffix.dll",
        "Qt6Gui$debugSuffix.dll",
        "Qt6Qml$debugSuffix.dll",
        "Qt6Sql$debugSuffix.dll",
        'libgcc_s_seh-1.dll',
        'libstdc++-6.dll',
        'libwinpthread-1.dll',
        "platforms/qwindows$debugSuffix.dll",
        "platforms/qoffscreen$debugSuffix.dll",
        "sqldrivers/qsqlite$debugSuffix.dll"
    )

    $missingFiles = @(
        foreach ($relativePath in $requiredRelativePaths) {
            $candidate = Join-Path $distributionDirectory $relativePath
            if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
                $relativePath
            }
        }
    )

    if ($missingFiles.Count -gt 0) {
        throw "Required deployment files are missing: $($missingFiles -join ', ')"
    }

    Write-Host '[4/4] Deployment validation passed'
    Write-Host "Output directory: $distributionDirectory"
    Write-Host 'Distribute the entire directory, not SmartMate.exe by itself.'
}
finally {
    Pop-Location
}
