[CmdletBinding()]
param(
    [string]$PackageName = 'LTZK_Developer_Package_20260928'
)

$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$distRoot = Join-Path $workspace 'dist'
$stageRoot = Join-Path $distRoot $PackageName
$sourceRoot = Join-Path $stageRoot '01_Source'
$runtimeRoot = Join-Path $stageRoot '02_Runtime\LTZK'
$docsRoot = Join-Path $stageRoot '03_Docs'

if (Test-Path -LiteralPath $stageRoot) {
    throw "Refusing to overwrite existing package directory: $stageRoot"
}

function Ensure-Directory {
    param([string]$Path)

    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Copy-PackageFile {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Required source file is missing: $Source"
    }

    $parent = Split-Path -Parent $Destination
    Ensure-Directory $parent
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-PackageTree {
    param(
        [string]$SourceRoot,
        [string]$DestinationRoot,
        [string[]]$ExcludedPathNames = @(),
        [string[]]$ExcludedFilePatterns = @()
    )

    if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
        throw "Required source directory is missing: $SourceRoot"
    }

    Ensure-Directory $DestinationRoot
    $sourcePrefixLength = $SourceRoot.Length
    $files = Get-ChildItem -LiteralPath $SourceRoot -File -Recurse -Force | Where-Object {
        $relative = $_.FullName.Substring($sourcePrefixLength).TrimStart('\')
        $relativeParts = $relative -split '\\'
        $pathExcluded = $false
        foreach ($name in $ExcludedPathNames) {
            if ($relativeParts -contains $name) {
                $pathExcluded = $true
                break
            }
        }

        $fileExcluded = $false
        foreach ($pattern in $ExcludedFilePatterns) {
            if ($_.Name -like $pattern) {
                $fileExcluded = $true
                break
            }
        }

        (-not $pathExcluded) -and (-not $fileExcluded)
    }

    foreach ($file in $files) {
        $relative = $file.FullName.Substring($sourcePrefixLength).TrimStart('\')
        $destination = Join-Path $DestinationRoot $relative
        Copy-PackageFile $file.FullName $destination
    }
}

Ensure-Directory $sourceRoot
Ensure-Directory (Join-Path $runtimeRoot 'bin')
Ensure-Directory (Join-Path $runtimeRoot 'plugins')
Ensure-Directory (Join-Path $runtimeRoot 'config')
Ensure-Directory (Join-Path $runtimeRoot 'resources')
Ensure-Directory $docsRoot

$rootSourceFiles = @(
    '.editorconfig',
    'LTZK.sln',
    'LTZK.vcxproj',
    'LTZK.vcxproj.filters',
    'LTZK.cpp',
    'LTZK.h',
    'LTZK.qrc',
    'LTZK.ui',
    'LTZKUiScale.h',
    'CatalogCheckedSelection.h',
    'DataCatalogConnectionDialog.cpp',
    'DataCatalogConnectionDialog.h',
    'DataCatalogProductCatalog.cpp',
    'DataCatalogProductCatalog.h',
    'DataCatalogWidget.cpp',
    'DataCatalogWidget.h',
    'FeatureAttribute.cpp',
    'FeatureAttribute.h',
    'LocalMainAreaClip.ui',
    'LocalProductOperation.cpp',
    'LocalProductOperation.h',
    'LocalRangeExport.ui',
    'LTZKPluginManager.cpp',
    'LTZKPluginManager.h',
    'LTZKQgisInterface.cpp',
    'LTZKQgisInterface.h',
    'PluginBindingConfig.cpp',
    'PluginBindingConfig.h',
    'product_catalog_plugin_api.h',
    'main.cpp'
)

foreach ($fileName in $rootSourceFiles) {
    Copy-PackageFile (Join-Path $workspace $fileName) (Join-Path $sourceRoot $fileName)
}

Copy-PackageTree (Join-Path $workspace 'auth') (Join-Path $sourceRoot 'auth')
Copy-PackageTree (Join-Path $workspace 'database') (Join-Path $sourceRoot 'database')
Copy-PackageTree (Join-Path $workspace 'tests') (Join-Path $sourceRoot 'tests')
Copy-PackageTree (Join-Path $workspace 'tools') (Join-Path $sourceRoot 'tools')
Copy-PackageTree (Join-Path $workspace 'Resources') (Join-Path $sourceRoot 'Resources') @() @('styles.rar', 'styles.zip')
Copy-PackageTree (Join-Path $workspace 'config') (Join-Path $sourceRoot 'config') @() @('database.ini')

Copy-PackageTree (Join-Path $workspace 'map_product_manager_tools') (Join-Path $sourceRoot 'map_product_manager_tools') @(
    '.vs',
    '.codebuddy',
    '.playwright-cli',
    'GeneratedFiles',
    'Debug',
    'Release',
    'obj',
    'bin',
    'node_modules',
    '__pycache__',
    'map_prod.B8F9A1C2'
) @(
    '*.user',
    '*.log',
    '*.bak',
    '*.tmp',
    '*backup*',
    'dump_analyze.txt',
    '输出-生成.txt',
    '_out.txt',
    'tmp_*',
    '_inspect*',
    '_tmp*'
)

Copy-PackageFile (Join-Path $workspace 'deployment\source_start_LTZK.bat') (Join-Path $sourceRoot 'start_LTZK.bat')
Copy-PackageFile (Join-Path $workspace 'deployment\source.env') (Join-Path $sourceRoot '.env')

$sanitizedDatabase = @'
[db]
host=localhost
port=5432
database=ltzk_platform
schema=public
user=YOUR_PASSWORD
password=
'@
Set-Content -LiteralPath (Join-Path $sourceRoot 'config\database.ini') -Value $sanitizedDatabase -Encoding UTF8

$runtimeExe = Join-Path $workspace 'x64\Release\LTZK.exe'
Copy-PackageFile $runtimeExe (Join-Path $runtimeRoot 'bin\LTZK.exe')

$pluginSourceRoot = Join-Path $workspace 'plugins'
$runtimePluginFiles = Get-ChildItem -LiteralPath $pluginSourceRoot -File -Filter '*.dll' -Force
if ($runtimePluginFiles.Count -eq 0) {
    throw "No plugin DLLs were found in: $pluginSourceRoot"
}
foreach ($plugin in $runtimePluginFiles) {
    Copy-PackageFile $plugin.FullName (Join-Path $runtimeRoot "plugins\$($plugin.Name)")
}

Copy-PackageFile (Join-Path $workspace 'config\plugin_bindings.json') (Join-Path $runtimeRoot 'config\plugin_bindings.json')
Copy-PackageFile (Join-Path $workspace 'deployment\package_start_LTZK.bat') (Join-Path $runtimeRoot 'start_LTZK.bat')
Copy-PackageFile (Join-Path $workspace 'deployment\.env') (Join-Path $runtimeRoot '.env')
Copy-PackageFile (Join-Path $workspace 'deployment\工具配置说明.txt') (Join-Path $runtimeRoot '工具配置说明.txt')
Set-Content -LiteralPath (Join-Path $runtimeRoot 'config\database.ini') -Value $sanitizedDatabase -Encoding UTF8
Copy-PackageTree (Join-Path $workspace 'Resources') (Join-Path $runtimeRoot 'resources') @() @('styles.rar', 'styles.zip')

Copy-PackageFile (Join-Path $workspace 'deployment\开发交付包说明.txt') (Join-Path $docsRoot '开发交付包说明.txt')
Copy-PackageFile (Join-Path $workspace 'deployment\源码调试说明.txt') (Join-Path $docsRoot '源码调试说明.txt')
Copy-PackageFile (Join-Path $workspace 'deployment\工具配置说明.txt') (Join-Path $docsRoot '工具配置说明.txt')
Copy-PackageFile (Join-Path $workspace 'docs\postgis_login_permissions_setup.md') (Join-Path $docsRoot 'postgis_login_permissions_setup.md')

$manifest = @'
# LTZK 开发交付包

- 包名：__PACKAGE_NAME__
- 源码快照：2026-09-28
- 工程：Visual Studio 2019 / MSVC v142 / x64，Qt 5，QGIS 3.28.15 / OSGeo4W

## 目录

- `01_Source`：LTZK 主程序 C++/Qt 工程、认证模块、资源、配置、测试脚本、构建工具，以及 `map_product_manager_tools` 插件源码。
- `02_Runtime/LTZK`：当前工作区 `x64/Release/LTZK.exe`、工作区现有插件 DLL、配置、资源和启动脚本，用于接收方做运行验证。
- `03_Docs`：源码调试、运行配置、开发交付和 PostGIS 权限配置说明。

## 接收方首次使用

1. 打开 `01_Source/LTZK.sln`。
2. 根据接收方安装位置修改 `LTZK.vcxproj` 中的 Qt/QGIS/OSGeo4W include、lib、QtMsBuild 和运行时路径；当前工程仍保留原开发机路径作为构建参考。
3. 修改 `01_Source/.env`，或让 QGIS 3.28.15 与 OSGeo4W 放在 `02_Runtime/LTZK` 下并使用默认相对路径。
4. 将 `01_Source/config/database.ini` 中的数据库参数改为接收方环境值；包内密码为空，不含本机数据库口令。
5. 构建 `Debug|x64` 或 `Release|x64` 后，可双击 `01_Source/start_LTZK.bat`；运行包使用 `02_Runtime/LTZK/start_LTZK.bat`。

## 本包明确不包含

- Visual Studio/Qt/QGIS 构建中间目录、`x64` 输出缓存、`package_build`、`GeneratedFiles`、`node_modules`、`.vs`、Python 缓存、日志和历史备份。
- 本机 GIS 测试数据、旧压缩包、Codex 工作记录和无关临时文件。
- `plugin_map_quality_check_tools.dll` 与 `plugin_map_generalization.dll` 对应的插件源码；当前工作区仅提供这两个模块的预编译 DLL。

## 交付注意事项

- `map_product_manager_tools` 未打包 `node_modules`；需要运行其 Node 辅助脚本时，在该目录执行 `npm ci`。
- 插件源码中的默认管理员初始化逻辑仍按现有实现保留，连接真实数据库前请按接收方安全要求检查并修改默认账号/口令与数据库权限。
- 本次只对交付目录和 ZIP 内容做静态完整性校验；未在当前环境重新编译或启动 GUI。
'@
$manifest = $manifest -replace '__PACKAGE_NAME__', $PackageName
Set-Content -LiteralPath (Join-Path $stageRoot 'PACKAGE_MANIFEST.md') -Value $manifest -Encoding UTF8

$requiredPackageFiles = @(
    '01_Source\LTZK.sln',
    '01_Source\LTZK.vcxproj',
    '01_Source\LTZK.cpp',
    '01_Source\LTZK.qrc',
    '01_Source\start_LTZK.bat',
    '01_Source\.env',
    '01_Source\config\plugin_bindings.json',
    '01_Source\config\database.ini',
    '01_Source\map_product_manager_tools\map_product_manager_tools.sln',
    '01_Source\map_product_manager_tools\map_product_manager_tools.vcxproj',
    '02_Runtime\LTZK\bin\LTZK.exe',
    '02_Runtime\LTZK\start_LTZK.bat',
    '02_Runtime\LTZK\config\plugin_bindings.json',
    '02_Runtime\LTZK\config\database.ini',
    '03_Docs\开发交付包说明.txt',
    '03_Docs\源码调试说明.txt',
    'PACKAGE_MANIFEST.md'
)
foreach ($relativePath in $requiredPackageFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $stageRoot $relativePath) -PathType Leaf)) {
        throw "Package validation failed; required file is missing: $relativePath"
    }
}

$forbiddenPathPattern = '\\(node_modules|GeneratedFiles|Debug|Release|obj|\.vs|\.codebuddy|\.playwright-cli|__pycache__|package_build|x64|_codex_|\.superpowers)(\\|$)'
$forbiddenFilePattern = '\.(bak|pdb|ipdb|iobj|obj|tlog|log|zip|rar)$'
$forbiddenFiles = Get-ChildItem -LiteralPath $stageRoot -File -Recurse -Force | Where-Object {
    $_.FullName -match $forbiddenPathPattern -or $_.Name -match $forbiddenFilePattern
}
if ($forbiddenFiles.Count -gt 0) {
    $names = ($forbiddenFiles | Select-Object -First 10 -ExpandProperty FullName) -join "`n"
    throw "Package validation found forbidden files or paths:`n$names"
}

$secretMatches = Get-ChildItem -LiteralPath $stageRoot -File -Recurse -Force | Where-Object {
    $_.Name -ne 'PACKAGE_MANIFEST.md' -and $_.Name -ne 'build_developer_package.ps1'
} | ForEach-Object {
    Select-String -LiteralPath $_.FullName -Pattern 'password=YOUR_PASSWORD' -SimpleMatch -ErrorAction SilentlyContinue
}
if ($secretMatches) {
    throw 'Package validation found the original database password value.'
}

$sourceFiles = Get-ChildItem -LiteralPath $stageRoot -File -Recurse -Force
$manifestSummary = @'

## 机器生成校验摘要

- 文件数量：__FILE_COUNT__
- 运行程序：02_Runtime/LTZK/bin/LTZK.exe
- 插件 DLL 数量：__PLUGIN_COUNT__
- 数据库口令：已清空
- 禁止目录/文件扫描：通过
- 原始 `password=YOUR_PASSWORD` 扫描：通过
'@
$manifestSummary = $manifestSummary -replace '__FILE_COUNT__', $sourceFiles.Count
$manifestSummary = $manifestSummary -replace '__PLUGIN_COUNT__', $runtimePluginFiles.Count
Add-Content -LiteralPath (Join-Path $stageRoot 'PACKAGE_MANIFEST.md') -Value $manifestSummary -Encoding UTF8

Write-Output "Package staging created: $stageRoot"
Write-Output "Files copied: $($sourceFiles.Count)"
Write-Output "Plugin DLLs copied: $($runtimePluginFiles.Count)"
