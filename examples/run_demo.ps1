<#
  Автозапуск демонстрации СУБД (вариант 2, индекс B+-tree).

  Запуск (из любого места):
      powershell -ExecutionPolicy Bypass -File "examples\run_demo.ps1"
  либо правый клик по файлу -> "Выполнить с помощью PowerShell".

  Скрипт сам:
    - переходит в корень проекта (папка над examples\),
    - при необходимости собирает проект,
    - прогоняет демонстрационный сценарий,
    - показывает файл B+-дерева и журнал доступа,
    - запускает клиент-серверный режим.
  Строки "ERROR: ..." в секции валидации — ожидаемые (демонстрация
  обработки ошибок), это НЕ сбой скрипта.
#>

$ErrorActionPreference = 'Stop'

# --- консоль в UTF-8, чтобы кириллица и вывод отображались корректно ---
try { chcp 65001 > $null } catch {}
try { [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false) } catch {}

# --- корень проекта = родитель папки этого скрипта ---
if (-not $PSScriptRoot) { $here = Split-Path -Parent $MyInvocation.MyCommand.Path }
else { $here = $PSScriptRoot }
$root = Split-Path -Parent $here
Set-Location $root

function Section($title) {
    Write-Host ""
    Write-Host ("=" * 72) -ForegroundColor Cyan
    Write-Host "  $title" -ForegroundColor Cyan
    Write-Host ("=" * 72) -ForegroundColor Cyan
}

Write-Host "Папка проекта: $root" -ForegroundColor DarkGray

# --- проверка наличия cmake ---
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "Не найден cmake в PATH. Установите CMake и повторите." -ForegroundColor Red
    exit 1
}

# --- 1. Сборка (только если бинарников ещё нет) ---
Section "1. Сборка проекта"
$existing = Get-ChildItem -Path "$root\build" -Filter cwdb.exe -Recurse -ErrorAction SilentlyContinue
if (-not $existing) {
    Write-Host "Собираю проект..." -ForegroundColor DarkGray
    cmake -G Ninja -S . -B build
    cmake --build build
} else {
    Write-Host "Бинарники уже собраны — пропускаю сборку." -ForegroundColor DarkGray
}

# --- поиск бинарников (Ninja: build\cwdb.exe; MSVC: build\Debug\cwdb.exe) ---
function Find-Exe($name) {
    (Get-ChildItem -Path "$root\build" -Filter $name -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1).FullName
}
$cwdb   = Find-Exe 'cwdb.exe'
$server = Find-Exe 'cwdb-server.exe'
$client = Find-Exe 'cwdb-client.exe'
if (-not $cwdb) {
    Write-Host "cwdb.exe не найден после сборки — прерываю." -ForegroundColor Red
    exit 1
}

# --- 2. Демонстрационный сценарий ---
Section "2. Демонстрационный сценарий (examples\defense_demo.sql)"
Write-Host "Подсказка: строки 'ERROR: ...' в секции валидации — ОЖИДАЕМЫЕ." -ForegroundColor Yellow
Write-Host "Они показывают обработку ошибок; программа при этом не падает." -ForegroundColor Yellow
Write-Host ""
& $cwdb "examples\defense_demo.sql"

# --- 3. Файл индекса B+-tree (вариант 2) ---
Section "3. Файл индекса B+-tree (data\demo\students\index_id.bpt)"
$idx = Join-Path $root "data\demo\students\index_id.bpt"
if (Test-Path $idx) { Get-Content $idx }
else { Write-Host "Файл индекса не найден (запустите сценарий ещё раз)." -ForegroundColor Yellow }

# --- 4. Журнал доступа (Access Logs) ---
Section "4. Журнал доступа — последние записи (data\access.log)"
$log = Join-Path $root "data\access.log"
if (Test-Path $log) { Get-Content $log -Tail 5 }
else { Write-Host "Журнал не найден." -ForegroundColor Yellow }

# --- 5. Клиент-серверный режим (тот же сценарий по сети) ---
Section "5. Клиент-серверный режим (сервер + клиент, тот же движок по TCP)"
if ($server -and $client) {
    $proc = $null
    try {
        Write-Host "Запускаю сервер на 127.0.0.1:5429..." -ForegroundColor DarkGray
        $proc = Start-Process -FilePath $server -ArgumentList '127.0.0.1', '5429' `
                              -PassThru -WindowStyle Minimized
        Start-Sleep -Seconds 1
        Write-Host "Клиент выполняет сценарий через сервер:" -ForegroundColor DarkGray
        Write-Host ""
        & $client --host 127.0.0.1 --port 5429 "examples\defense_demo.sql"
    } catch {
        Write-Host "Клиент-серверная часть пропущена: $($_.Exception.Message)" -ForegroundColor Yellow
    } finally {
        if ($proc -and -not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force
            Write-Host ""
            Write-Host "Сервер остановлен." -ForegroundColor DarkGray
        }
    }
} else {
    Write-Host "cwdb-server/cwdb-client не найдены — раздел пропущен." -ForegroundColor Yellow
}

Section "Демонстрация завершена"
Write-Host "Все разделы отработали. Готово к защите." -ForegroundColor Green

