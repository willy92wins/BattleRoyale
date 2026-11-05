<#
.SYNOPSIS
    Restart loop for a DayZ BattleRoyale server that starts every match on a clean world.

.DESCRIPTION
    The BattleRoyale mod shuts the server down when a match ends. This loop then:
      1. deletes the world storage of the mission: the folder storage_<instanceId> inside the
         mission folder (instanceId comes from the server config), so loot and dropped items
         never carry over from one match to the next;
      2. starts DayZServer_x64.exe with the client mods (-mod=) and the server-only mods (-serverMod=);
      3. waits until the server process exits, waits RestartDelaySeconds and starts over.
    The profiles folder (BattleRoyale settings, logs) is never touched.

    Safety checks, the script stops instead of deleting when one fails:
      - the mission folder must be mpmissions\<Mission> under ServerRoot and contain init.c;
      - the template= line of the server config must name that same mission;
      - the server config must define instanceId;
      - no DayZServer_x64.exe from this ServerRoot may be running;
      - the storage folder must not be, or contain, a junction or symbolic link.
    Use -WhatIf to print what one pass would delete and start, without doing it.

    Do not point it at a mission folder that other servers or projects use: their world
    storage is deleted before every start.

.PARAMETER ServerRoot
    DayZ server install folder, the one with DayZServer_x64.exe and mpmissions. Required.

.PARAMETER Mission
    Mission folder name inside mpmissions. Must match template= in the server config.

.PARAMETER Config
    Server config file, absolute or relative to ServerRoot.

.PARAMETER Profiles
    Profiles folder (-profiles=), absolute or relative to ServerRoot. Created if missing.

.PARAMETER Mod
    Client mod list for -mod=, separated by ';' and in load order.

.PARAMETER ServerMod
    Server-only mod list for -serverMod=, separated by ';'. Empty to omit the parameter.

.PARAMETER Port
    Game port (-port=).

.PARAMETER RestartDelaySeconds
    Pause between the server exit and the next start.

.PARAMETER ExtraArgs
    Extra command line parameters for DayZServer_x64.exe.

.EXAMPLE
    .\restart_loop.ps1 -ServerRoot 'D:\DayZServer' -Mod '@CF;@Dabs Framework;@DayZ-Expansion-Core;@DayZ-Expansion;@DayZ-Expansion-Navigation;@Community-Online-Tools;@BattleRoyale'

.EXAMPLE
    .\restart_loop.ps1 -ServerRoot 'D:\DayZServer' -Mod '@CF;@Dabs Framework' -WhatIf
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerRoot,
    [string]$Mission = 'dayzOffline.chernarusplus',
    [string]$Config = 'serverDZ.cfg',
    [string]$Profiles = 'profiles',
    [string]$Mod = '',
    [string]$ServerMod = '',
    [int]$Port = 2302,
    [int]$RestartDelaySeconds = 10,
    [string[]]$ExtraArgs = @('-dologs', '-adminlog', '-freezecheck')
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
$script:Cmdlet = $PSCmdlet

function Write-Log([string]$Message)
{
    Write-Host ('{0:yyyy-MM-dd HH:mm:ss} {1}' -f (Get-Date), $Message)
}

function Resolve-UnderRoot([string]$Path, [string]$Root)
{
    if ([System.IO.Path]::IsPathRooted($Path))
    {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
}

function Format-Argument([string]$Argument)
{
    if ($Argument -match '\s')
    {
        return '"' + $Argument + '"'
    }
    return $Argument
}

# --- Paths and safety checks (run once) ---

if (-not (Test-Path -LiteralPath $ServerRoot -PathType Container))
{
    throw "ServerRoot not found: $ServerRoot"
}
$root = (Resolve-Path -LiteralPath $ServerRoot).ProviderPath
$exe = Join-Path $root 'DayZServer_x64.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf))
{
    throw "DayZServer_x64.exe not found in $root"
}

if ($Mission.Trim() -eq '' -or $Mission -match '[\\/:]' -or $Mission -eq '.' -or $Mission -eq '..')
{
    throw "Mission must be a folder name inside mpmissions, got '$Mission'"
}
$missionDir = Join-Path (Join-Path $root 'mpmissions') $Mission
if (-not (Test-Path -LiteralPath (Join-Path $missionDir 'init.c') -PathType Leaf))
{
    throw "No mission at $missionDir (init.c not found)"
}

$configPath = Resolve-UnderRoot $Config $root
if (-not (Test-Path -LiteralPath $configPath -PathType Leaf))
{
    throw "Server config not found: $configPath"
}
$configText = Get-Content -LiteralPath $configPath -Raw

$templateMatch = [regex]::Match($configText, '(?m)^\s*template\s*=\s*"([^"]+)"')
if (-not $templateMatch.Success)
{
    throw "No template= line in $configPath"
}
if ($templateMatch.Groups[1].Value -ne $Mission)
{
    throw "$configPath loads mission '$($templateMatch.Groups[1].Value)', not '$Mission': fix -Mission or template="
}

$instanceMatch = [regex]::Match($configText, '(?m)^\s*instanceId\s*=\s*(-?\d+)\s*;')
if (-not $instanceMatch.Success)
{
    throw "No instanceId= line in ${configPath}; set it, the world storage folder is named after it"
}
$storageDir = Join-Path $missionDir ('storage_' + $instanceMatch.Groups[1].Value)

$profilesPath = Resolve-UnderRoot $Profiles $root

$arguments = @("-config=$configPath", "-port=$Port", "-profiles=$profilesPath")
if ($Mod -ne '')
{
    $arguments += "-mod=$Mod"
}
if ($ServerMod -ne '')
{
    $arguments += "-serverMod=$ServerMod"
}
$arguments += $ExtraArgs
$argumentLine = ($arguments | ForEach-Object { Format-Argument $_ }) -join ' '

# --- Loop ---

function Assert-ServerStopped
{
    $running = @(Get-Process -Name 'DayZServer_x64' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and ([System.IO.Path]::GetFullPath($_.Path) -eq [System.IO.Path]::GetFullPath($exe)) })
    if ($running.Count -gt 0)
    {
        throw "DayZServer_x64.exe from $root is already running (PID $($running[0].Id)); stop it before using the loop"
    }
}

function Clear-WorldStorage
{
    if (-not (Test-Path -LiteralPath $storageDir))
    {
        Write-Log "No world storage at $storageDir, nothing to delete"
        return
    }

    $item = Get-Item -LiteralPath $storageDir -Force
    if (-not $item.PSIsContainer)
    {
        throw "$storageDir is not a folder"
    }
    if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)
    {
        throw "$storageDir is a junction or symbolic link; remove the link by hand (cmd /c rmdir), the loop does not delete through it"
    }
    $links = @(Get-ChildItem -LiteralPath $storageDir -Recurse -Force -Attributes ReparsePoint -ErrorAction Stop)
    if ($links.Count -gt 0)
    {
        throw "$storageDir contains a junction or symbolic link ($($links[0].FullName)); deleting the tree would follow it"
    }

    if ($script:Cmdlet.ShouldProcess($storageDir, 'Delete world storage'))
    {
        Remove-Item -LiteralPath $storageDir -Recurse -Force
        Write-Log "Deleted world storage $storageDir"
    }
}

Write-Log "Server: $exe"
Write-Log "Mission: $missionDir"
Write-Log "World storage wiped before every start: $storageDir"

$run = 0
while ($true)
{
    $run++
    Assert-ServerStopped
    Clear-WorldStorage

    if (-not (Test-Path -LiteralPath $profilesPath))
    {
        if ($script:Cmdlet.ShouldProcess($profilesPath, 'Create profiles folder'))
        {
            New-Item -ItemType Directory -Path $profilesPath | Out-Null
        }
    }

    Write-Log "Start #${run}: $exe $argumentLine"
    if (-not $script:Cmdlet.ShouldProcess($exe, 'Start DayZ server'))
    {
        break
    }

    $process = Start-Process -FilePath $exe -ArgumentList $argumentLine -WorkingDirectory $root -PassThru
    $null = $process.Handle
    $process.WaitForExit()
    Write-Log "Server exited with code $($process.ExitCode)"

    Write-Log "Next start in $RestartDelaySeconds s, press Ctrl+C to stop the loop"
    Start-Sleep -Seconds $RestartDelaySeconds
}
