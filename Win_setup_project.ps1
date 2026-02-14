# Force l'output console en UTF-8
chcp 65001 > $null
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

function OK($msg) {
    Write-Host "[OK] " -ForegroundColor DarkGreen -NoNewline
    Write-Host $msg -ForegroundColor DarkGray
}

function ERR($msg) {
    Write-Host "[ERROR] " -ForegroundColor Red -NoNewline
    Write-Host $msg -ForegroundColor DarkGray
}

function WARN($msg) {
    Write-Host "[WARNING] " -ForegroundColor DarkYellow -NoNewline
    Write-Host $msg -ForegroundColor DarkGray
}

function INFO($msg) {
    Write-Host "[INFO] " -ForegroundColor Cyan -NoNewline
    Write-Host $msg -ForegroundColor Gray
}

#Deplacement dans le dossier du script
Set-Location $PSScriptRoot

# Verif de tout le proj
INFO("BRACE FOR IMPACT")

#Verification de l'existance de l'engine
$enginePath = "engine"

$engineMissing = -not (Test-Path $enginePath)
$engineEmpty   = $false

if (-not $engineMissing) {
    $engineEmpty = -not (Get-ChildItem $enginePath -Force -ErrorAction SilentlyContinue | Select-Object -First 1)
}

if ($engineMissing -or $engineEmpty) {

    do {
        WARN("Le dossier engine est manquant ou vide.")
        $choice = Read-Host "Recuperer les sous-modules requis ? (o/n)"
        $choice = $choice.Trim().ToLower()
    } while ($choice -ne "o" -and $choice -ne "n")

    if ($choice -eq "o") {
        if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
            ERR("Git n'est pas installe ou pas dans le PATH.")
            exit 1
        }

        INFO("Mise a jour/Recuperation des submodules...")
        git submodule update --init --recursive

        if ($LASTEXITCODE -ne 0) {
            ERR("Erreur pendant la mise a jour des submodules (code $LASTEXITCODE).")
            exit $LASTEXITCODE
        }

        OK("Sous-modules mis a jour.")
    } else {
        INFO("Sous-modules, non recupere")
    }

} else {
    OK("Le dossier engine n'est pas vide.")
}
