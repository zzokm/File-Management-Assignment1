param(
    [string]$LeaderID = "20242447",
    [string]$TAname = "Nesma"
)

# Sanitize inputs
$CleanTA = ($TAname -replace '\s+', '_').Trim()
$ZipName = "${LeaderID}_${CleanTA}_Assignment1.zip"
$StagingDir = Join-Path $PSScriptRoot ".staging_package"

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "     HEALTHCARE SYSTEM ASSIGNMENT PACKAGING TOOL       " -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "Team Leader ID : $LeaderID" -ForegroundColor Yellow
Write-Host "TA Name        : $CleanTA" -ForegroundColor Yellow
Write-Host "Package File   : $ZipName" -ForegroundColor Yellow
Write-Host ""

# 1. Clean previous staging directory and zip if exists
if (Test-Path $StagingDir) {
    Remove-Item $StagingDir -Recurse -Force
}
if (Test-Path (Join-Path $PSScriptRoot $ZipName)) {
    Remove-Item (Join-Path $PSScriptRoot $ZipName) -Force
}

# 2. Create clean staging directory
New-Item -ItemType Directory -Path $StagingDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $StagingDir "include") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $StagingDir "src") -Force | Out-Null

# 3. Copy source and header files only (no docs, no build files)
Write-Host "Copying core source and header files..." -ForegroundColor Green
Copy-Item (Join-Path $PSScriptRoot "include\*.h") -Destination (Join-Path $StagingDir "include") -Force
Copy-Item (Join-Path $PSScriptRoot "src\*.cpp") -Destination (Join-Path $StagingDir "src") -Force
Copy-Item (Join-Path $PSScriptRoot "CMakeLists.txt") -Destination (Join-Path $StagingDir "CMakeLists.txt") -Force

# 4. Place simplified cover page as Cover Page.txt at the root inside the package
Write-Host "Adding simplified submission cover page as 'Cover Page.txt'..." -ForegroundColor Green
if (Test-Path (Join-Path $PSScriptRoot "Cover Page.txt")) {
    Copy-Item (Join-Path $PSScriptRoot "Cover Page.txt") -Destination (Join-Path $StagingDir "Cover Page.txt") -Force
} else {
    $CoverPageContent = @"
Course: File Management and Processing (IS321)
Assignment: Assignment 1 - Healthcare Management System
Teaching Assistant (TA): Nesma
Lab / Section: S1, S2

Team Members:
1. Yehia Hassan Abdelmoaty - 20242447
2. Mahmoud Sherif Farouk - 20240552
3. Abdullah Mohamed Raafat - 20240328
4. Youssef Ramy Hassan - 20240689
"@
    Set-Content -Path (Join-Path $StagingDir "Cover Page.txt") -Value $CoverPageContent -Encoding UTF8
}

# 5. Compress into zip archive
Write-Host "Creating zip package: $ZipName..." -ForegroundColor Green
Compress-Archive -Path (Join-Path $StagingDir "*") -DestinationPath (Join-Path $PSScriptRoot $ZipName) -Force

# 6. Clean up temporary staging directory
Remove-Item $StagingDir -Recurse -Force

Write-Host ""
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host " [SUCCESS] Submission package created successfully!    " -ForegroundColor Green
Write-Host " File: $ZipName" -ForegroundColor Green
Write-Host " Size: $((Get-Item (Join-Path $PSScriptRoot $ZipName)).Length) bytes" -ForegroundColor Green
Write-Host "=======================================================" -ForegroundColor Cyan
