# USBForge - Write ISO to a removable USB drive (Windows)
# Requires Administrator (UAC). Erases the target disk.
param(
    [Parameter(Mandatory = $true)][string]$IsoPath,
    [Parameter(Mandatory = $true)][string]$DriveLetter
)

$ErrorActionPreference = "Stop"

function Fail([string]$msg) {
    Write-Host "ERROR: $msg" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path -LiteralPath $IsoPath)) {
    Fail "ISO not found: $IsoPath"
}

$letter = $DriveLetter.Trim().TrimEnd(':').ToUpper()
if ($letter.Length -ne 1) {
    Fail "DriveLetter must be a single letter (e.g. E)"
}

Write-Host "USBForge write-iso"
Write-Host "  ISO:    $IsoPath"
Write-Host "  Drive:  ${letter}:"

# Find the partition / disk for this drive letter
$partition = Get-Partition -DriveLetter $letter -ErrorAction SilentlyContinue
if (-not $partition) {
    Fail "No partition found for drive ${letter}:"
}

$diskNumber = $partition.DiskNumber
$disk = Get-Disk -Number $diskNumber
Write-Host "  Disk:   #$diskNumber  ($($disk.FriendlyName), $([math]::Round($disk.Size/1GB,1)) GB, Bus=$($disk.BusType))"

if ($disk.BusType -ne 'USB' -and $disk.BusType -ne 'SD') {
    $confirm = Read-Host "WARNING: BusType is $($disk.BusType), not USB. Type YES to continue"
    if ($confirm -ne 'YES') { Fail "Aborted" }
}

Write-Host "Dismounting volumes on disk #$diskNumber ..."
Get-Disk -Number $diskNumber | Get-Partition | ForEach-Object {
    if ($_.DriveLetter) {
        try { Remove-PartitionAccessPath -DiskNumber $diskNumber -PartitionNumber $_.PartitionNumber -AccessPath "$($_.DriveLetter):\" -ErrorAction SilentlyContinue } catch {}
    }
}

# Raw write using .NET FileStream to \\.\PhysicalDriveN
$isoInfo = Get-Item -LiteralPath $IsoPath
$isoSize = $isoInfo.Length
$destPath = "\\.\PhysicalDrive$diskNumber"
Write-Host "Writing $([math]::Round($isoSize/1MB,1)) MB to $destPath ..."

$bufferSize = 4MB
$buffer = New-Object byte[] $bufferSize
$src = [System.IO.File]::OpenRead($IsoPath)
try {
    $dst = New-Object System.IO.FileStream($destPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
} catch {
    $src.Close()
    Fail "Cannot open $destPath - run as Administrator. $_"
}

try {
    $written = [int64]0
    while (($read = $src.Read($buffer, 0, $buffer.Length)) -gt 0) {
        $dst.Write($buffer, 0, $read)
        $written += $read
        $pct = [int](($written * 100) / $isoSize)
        Write-Progress -Activity "Writing ISO" -Status "$pct% ($([math]::Round($written/1MB,1)) MB)" -PercentComplete $pct
    }
    $dst.Flush()
} finally {
    $dst.Close()
    $src.Close()
    Write-Progress -Activity "Writing ISO" -Completed
}

Write-Host "Syncing..."
try { Get-Disk -Number $diskNumber | Out-Null } catch {}

Write-Host "SUCCESS: ISO written to disk #$diskNumber (${letter}:)." -ForegroundColor Green
Write-Host "You can reboot and boot from this USB drive."
exit 0
