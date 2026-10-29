<# build.ps1 - compile the Library Management System on Windows (MSYS2 gcc).
   Run from the repo root:   .\build.ps1            builds library.exe
                             .\build.ps1 -Tests     also builds the test programs
   Needs gcc 16.x from MSYS2 UCRT64 and libmariadb (see README.md). #>
param([switch]$Tests)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$sources = @(
    'src/main.c', 'src/db.c', 'src/book.c', 'src/member.c', 'src/search_sort.c',
    'src/circulation.c', 'src/dbhelpers.c', 'src/queue.c', 'src/reservation.c',
    'src/fine.c', 'src/reports.c'
)

function Build($outFile, $files, $libs) {
    Write-Host "Building $outFile ..."
    & gcc -Wall -Wextra -Iinclude @files -o $outFile @libs
    if ($LASTEXITCODE -ne 0) { throw "Build of $outFile failed" }
}

Build 'library.exe' $sources @('-lmariadb')

if ($Tests) {
    Build 'db_test.exe'        @('src/db.c', 'tests/db_test.c') @('-lmariadb')
    Build 'test_queue.exe'     @('src/queue.c', 'tests/test_queue.c') @()
    Build 'test_fine_calc.exe' @('src/fine.c', 'src/dbhelpers.c', 'src/db.c', 'tests/test_fine_calc.c') @('-lmariadb')
}

Write-Host "Done. Set the password, then run .\library.exe"
Write-Host "  `$env:LIB_DB_PASS = 'your-password'"
