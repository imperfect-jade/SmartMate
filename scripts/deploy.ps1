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
    Write-Host "[1/5] Configure the Widgets-only $Configuration build"
    Invoke-CheckedCommand `
        -Executable $cmakeCommand.Source `
        -Arguments @('--preset', $preset, '-DSMARTMATE_BUILD_QML_BASELINE=OFF') `
        -FailureMessage 'CMake configuration failed'

    Write-Host '[2/5] Build the official SmartMate target'
    Invoke-CheckedCommand `
        -Executable $cmakeCommand.Source `
        -Arguments @('--build', '--preset', $preset, '--target', 'SmartMate') `
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

    Write-Host '[3/5] Collect Qt Widgets and MinGW runtime files'
    if ($Configuration -eq 'Debug' -and -not $hasDebugQtRuntime) {
        Write-Host 'Qt debug DLLs are not installed; deploy the Debug build with the matching release Qt runtime.'
    }
    # 正式程序只允许扫描可执行文件的 Widgets 依赖；qoffscreen供发布目录冒烟测试使用。
    $deployArguments = @(
        "--$runtimeMode",
        '--compiler-runtime',
        '--no-system-dxc-compiler',
        '--include-plugins', 'qoffscreen',
        '--translations', 'zh_CN,en',
        '--verbose', '0',
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
        "Qt6Widgets$debugSuffix.dll",
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

    $forbiddenRuntimeFiles = @(
        foreach ($pattern in @('Qt6Qml*.dll', 'Qt6Quick*.dll', 'Qt6QuickControls2*.dll')) {
            Get-ChildItem -Path (Join-Path $distributionDirectory $pattern) `
                -File -ErrorAction SilentlyContinue
        }
    )
    $deployedQmlDirectory = Join-Path $distributionDirectory 'qml'
    if ($forbiddenRuntimeFiles.Count -gt 0 -or
        (Test-Path -LiteralPath $deployedQmlDirectory)) {
        $forbiddenNames = @($forbiddenRuntimeFiles | ForEach-Object { $_.Name })
        if (Test-Path -LiteralPath $deployedQmlDirectory) {
            $forbiddenNames += 'qml/'
        }
        throw "QML/Qt Quick files were unexpectedly deployed: $($forbiddenNames -join ', ')"
    }

    Write-Host '[4/5] Run the deployed executable with only packaged runtimes'
    $originalPath = $env:PATH
    try {
        $env:PATH = "$distributionDirectory;$env:SystemRoot;$env:SystemRoot\System32"
        Invoke-CheckedCommand `
            -Executable $deployedExecutable `
            -Arguments @('--smoke-test', '-platform', 'offscreen') `
            -FailureMessage 'Deployed SmartMate smoke test failed'
    }
    finally {
        $env:PATH = $originalPath
    }

    Write-Host '[5/5] Deployment validation passed'
    Write-Host "Output directory: $distributionDirectory"
    Write-Host 'Distribute the entire directory, not SmartMate.exe by itself.'
}
finally {
    Pop-Location
}
