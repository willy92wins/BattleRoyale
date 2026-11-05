<#
.SYNOPSIS
    Restart loop for a DayZ BattleRoyale server that starts every match on a clean world.

.DESCRIPTION
    The BattleRoyale mod shuts the server down when a match ends. This loop then:
      1. moves the world storage of the mission aside: the folder storage_<instanceId> inside the
         mission folder is renamed to storage_<instanceId>.prev-<yyyyMMdd-HHmmss>, so loot and dropped
         items never carry over and the server creates a fresh storage;
      2. deletes the oldest storage_<instanceId>.prev-* folders and keeps the newest KeepPrevious
         (3 by default): a world can be restored for that many restarts;
      3. starts DayZServer_x64.exe with the client mods (-mod=) and the server-only mods (-serverMod=);
      4. waits until the server process exits, waits RestartDelaySeconds and starts over.
    The profiles folder (BattleRoyale settings, logs) is never touched.

    The script stops without renaming, deleting or starting anything when a check fails:
      - the mission folder must be mpmissions\<Mission> under ServerRoot and contain init.c;
      - the server config must have exactly one active template= (comments ignored) and it must
        name that mission;
      - the server config must have exactly one active instanceId= (comments ignored), an integer;
      - no DayZServer_x64.exe may be running from this ServerRoot, and none may be running whose
        executable path cannot be read;
      - mpmissions, the mission folder, the storage folder and every .prev-* folder to delete must
        not be junctions or symbolic links, the storage folders must not contain any, and their
        resolved physical paths must be exactly ServerRoot\mpmissions\<Mission>\<folder>.
    Use -WhatIf to print what one pass would rename, delete and start, without doing it.

    Do not point it at a mission folder that other servers or projects use: their world storage is
    moved away before every start.

    To restore a previous world: stop the loop and the server, rename storage_<instanceId> away and
    rename the wanted storage_<instanceId>.prev-* back to storage_<instanceId>.

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

.PARAMETER KeepPrevious
    Number of previous world storages (storage_<instanceId>.prev-*) kept for restoring.

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
    [ValidateRange(1, 65535)]
    [int]$Port = 2302,
    [ValidateRange(0, 86400)]
    [int]$RestartDelaySeconds = 10,
    [ValidateRange(1, 100)]
    [int]$KeepPrevious = 3,
    [string[]]$ExtraArgs = @('-dologs', '-adminlog', '-freezecheck')
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
$script:Cmdlet = $PSCmdlet

# Physical (final) path of an existing file or folder, with every junction and symbolic link resolved
if (-not ('BRFinalPath' -as [type]))
{
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

public static class BRFinalPath
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFileW(string lpFileName, uint dwDesiredAccess, uint dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern uint GetFinalPathNameByHandleW(SafeFileHandle hFile, StringBuilder lpszFilePath, uint cchFilePath, uint dwFlags);

    public static string Get(string path)
    {
        // No access rights, share read/write/delete, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS (opens folders too)
        using (SafeFileHandle handle = CreateFileW(path, 0, 7, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero))
        {
            if (handle.IsInvalid)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot open " + path);

            StringBuilder buffer = new StringBuilder(32768);
            uint length = GetFinalPathNameByHandleW(handle, buffer, (uint)buffer.Capacity, 0);
            if (length == 0 || length >= buffer.Capacity)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot resolve " + path);

            string result = buffer.ToString();
            if (result.StartsWith(@"\\?\UNC\"))
                return @"\\" + result.Substring(8);
            if (result.StartsWith(@"\\?\"))
                return result.Substring(4);
            return result;
        }
    }
}
'@
}

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

function Test-SamePath([string]$A, [string]$B)
{
    return [string]::Equals($A.TrimEnd('\'), $B.TrimEnd('\'), [System.StringComparison]::OrdinalIgnoreCase)
}

# Server config text without // and /* */ comments; quoted strings are kept as they are
function Remove-CfgComments([string]$Text)
{
    $builder = New-Object System.Text.StringBuilder
    $length = $Text.Length
    $index = 0
    $inString = $false
    while ($index -lt $length)
    {
        $c = $Text[$index]
        $next = [char]0
        if ($index + 1 -lt $length)
        {
            $next = $Text[$index + 1]
        }

        if ($inString)
        {
            [void]$builder.Append($c)
            if ($c -eq [char]'"' -or $c -eq [char]"`n")
            {
                $inString = $false
            }
            $index++
            continue
        }

        if ($c -eq [char]'"')
        {
            $inString = $true
            [void]$builder.Append($c)
            $index++
            continue
        }

        if ($c -eq [char]'/' -and $next -eq [char]'/')
        {
            while ($index -lt $length -and $Text[$index] -ne [char]"`n")
            {
                $index++
            }
            continue
        }

        if ($c -eq [char]'/' -and $next -eq [char]'*')
        {
            $end = $Text.IndexOf('*/', $index + 2, [System.StringComparison]::Ordinal)
            if ($end -lt 0)
            {
                throw 'Unterminated /* comment in the server config'
            }
            # Keep the line breaks of the comment
            foreach ($commented in $Text.Substring($index, $end + 2 - $index).ToCharArray())
            {
                if ($commented -eq [char]"`n")
                {
                    [void]$builder.Append("`n")
                }
            }
            $index = $end + 2
            continue
        }

        [void]$builder.Append($c)
        $index++
    }
    return $builder.ToString()
}

function Assert-NotLink([string]$Path)
{
    $item = Get-Item -LiteralPath $Path -Force
    if (-not $item.PSIsContainer)
    {
        throw "$Path is not a folder"
    }
    if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)
    {
        throw "$Path is a junction or symbolic link; the loop does not rename or delete through links"
    }
}

function Assert-NoLinkInside([string]$Path)
{
    $links = @(Get-ChildItem -LiteralPath $Path -Recurse -Force -Attributes ReparsePoint -ErrorAction Stop)
    if ($links.Count -gt 0)
    {
        throw "$Path contains a junction or symbolic link ($($links[0].FullName)); deleting the tree would follow it"
    }
}

# The folder must resolve physically to exactly <expected parent>\<its name>: no link anywhere on the way
function Assert-PhysicalLocation([string]$Path, [string]$ExpectedPhysical)
{
    $physical = [BRFinalPath]::Get($Path)
    if (-not (Test-SamePath $physical $ExpectedPhysical))
    {
        throw "$Path resolves to $physical instead of $ExpectedPhysical; the loop only works on folders inside the server folder"
    }
}

# --- Paths and configuration checks (run once) ---

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

if ($Mission.Trim() -eq '' -or $Mission -match '[\\/:*?"<>|]' -or $Mission -eq '.' -or $Mission -eq '..')
{
    throw "Mission must be a folder name inside mpmissions, got '$Mission'"
}
$missionsDir = Join-Path $root 'mpmissions'
$missionDir = Join-Path $missionsDir $Mission
if (-not (Test-Path -LiteralPath (Join-Path $missionDir 'init.c') -PathType Leaf))
{
    throw "No mission at $missionDir (init.c not found)"
}

$configPath = Resolve-UnderRoot $Config $root
if (-not (Test-Path -LiteralPath $configPath -PathType Leaf))
{
    throw "Server config not found: $configPath"
}
$activeConfig = Remove-CfgComments (Get-Content -LiteralPath $configPath -Raw)

$templateMatches = [regex]::Matches($activeConfig, '(?<![A-Za-z0-9_])template\s*=\s*"([^"]*)"\s*;', 'IgnoreCase')
if ($templateMatches.Count -ne 1)
{
    throw "$configPath has $($templateMatches.Count) active template= assignments, expected exactly one"
}
if ($templateMatches[0].Groups[1].Value -ne $Mission)
{
    throw "$configPath loads mission '$($templateMatches[0].Groups[1].Value)', not '$Mission': fix -Mission or template="
}

$instanceMatches = [regex]::Matches($activeConfig, '(?<![A-Za-z0-9_])instanceId\s*=\s*([^;]*);', 'IgnoreCase')
if ($instanceMatches.Count -ne 1)
{
    throw "$configPath has $($instanceMatches.Count) active instanceId= assignments, expected exactly one: the world storage folder is named after it"
}
$instanceId = $instanceMatches[0].Groups[1].Value.Trim()
if ($instanceId -notmatch '^-?\d+$')
{
    throw "instanceId in $configPath is '$instanceId', not an integer"
}
$storageName = 'storage_' + $instanceId
$storageDir = Join-Path $missionDir $storageName
$previousPattern = '^' + [regex]::Escape($storageName) + '\.prev-(\d{8}-\d{6})(?:-(\d+))?$'

$profilesPath = Resolve-UnderRoot $Profiles $root
$exePhysical = [BRFinalPath]::Get($exe)

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

# --- Checks repeated before every start ---

function Assert-ServerStopped
{
    foreach ($candidate in @(Get-Process -Name 'DayZServer_x64' -ErrorAction SilentlyContinue))
    {
        $candidatePhysical = $null
        try
        {
            if ($candidate.Path)
            {
                $candidatePhysical = [BRFinalPath]::Get($candidate.Path)
            }
        }
        catch
        {
            $candidatePhysical = $null
        }

        if (-not $candidatePhysical)
        {
            throw "DayZServer_x64.exe (PID $($candidate.Id)) is running and its executable path cannot be read; stop it, or start the loop from the account that runs it"
        }
        if (Test-SamePath $candidatePhysical $exePhysical)
        {
            throw "DayZServer_x64.exe from $root is already running (PID $($candidate.Id)); stop it before using the loop"
        }
    }
}

# Returns the physical path of the mission folder after checking every folder from mpmissions down
function Assert-MissionLocation
{
    Assert-NotLink $missionsDir
    Assert-NotLink $missionDir
    $rootPhysical = [BRFinalPath]::Get($root)
    $missionPhysical = Join-Path (Join-Path $rootPhysical 'mpmissions') $Mission
    Assert-PhysicalLocation $missionsDir (Join-Path $rootPhysical 'mpmissions')
    Assert-PhysicalLocation $missionDir $missionPhysical
    return $missionPhysical
}

function Move-WorldStorage([string]$MissionPhysical)
{
    if (-not (Test-Path -LiteralPath $storageDir))
    {
        Write-Log "No world storage at $storageDir, nothing to move"
        return
    }

    Assert-NotLink $storageDir
    Assert-NoLinkInside $storageDir
    Assert-PhysicalLocation $storageDir (Join-Path $MissionPhysical $storageName)

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $newName = "$storageName.prev-$stamp"
    $suffix = 1
    while (Test-Path -LiteralPath (Join-Path $missionDir $newName))
    {
        $suffix++
        $newName = "$storageName.prev-$stamp-$suffix"
    }

    if ($script:Cmdlet.ShouldProcess($storageDir, "Rename world storage to $newName"))
    {
        Rename-Item -LiteralPath $storageDir -NewName $newName
        Write-Log "Moved world storage to $(Join-Path $missionDir $newName)"
    }
}

function Remove-OldWorldStorage([string]$MissionPhysical)
{
    $previous = @()
    foreach ($folder in @(Get-ChildItem -LiteralPath $missionDir -Directory -Force))
    {
        $match = [regex]::Match($folder.Name, $previousPattern)
        if ($match.Success)
        {
            $order = 0
            if ($match.Groups[2].Success)
            {
                $order = [int]$match.Groups[2].Value
            }
            $previous += New-Object PSObject -Property @{ Name = $folder.Name; Stamp = $match.Groups[1].Value; Order = $order }
        }
    }

    $sorted = @($previous | Sort-Object -Property @{ Expression = 'Stamp'; Descending = $true }, @{ Expression = 'Order'; Descending = $true })
    if ($sorted.Count -le $KeepPrevious)
    {
        return
    }

    foreach ($old in $sorted[$KeepPrevious..($sorted.Count - 1)])
    {
        $oldPath = Join-Path $missionDir $old.Name
        Assert-NotLink $oldPath
        Assert-NoLinkInside $oldPath
        Assert-PhysicalLocation $oldPath (Join-Path $MissionPhysical $old.Name)

        if ($script:Cmdlet.ShouldProcess($oldPath, 'Delete old world storage'))
        {
            Remove-Item -LiteralPath $oldPath -Recurse -Force
            Write-Log "Deleted old world storage $oldPath"
        }
    }
}

# --- Loop ---

Write-Log "Server: $exe"
Write-Log "Mission: $missionDir"
Write-Log "World storage moved aside before every start: $storageDir (keeps the newest $KeepPrevious)"

$run = 0
while ($true)
{
    $run++
    Assert-ServerStopped
    $missionPhysical = Assert-MissionLocation
    Move-WorldStorage $missionPhysical
    Remove-OldWorldStorage $missionPhysical

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
