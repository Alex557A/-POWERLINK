#!/bin/bash
# ============================================================
# install_powerlink.sh — автоматическая сборка и установка
# моста LinuxCNC <-> Ethernet POWERLINK
# проект Powerlink
# ------------------------------------------------------------
# Загружает и собирает все компоненты из официальных
# репозиториев (Debian, openPOWERLINK, EPSG).
# Не требует проприетарного ПО вендоров.
# ============================================================
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== Powerlink — установка зависимостей ==="
sudo apt-get update
sudo apt-get install -y git build-essential cmake libpcap-dev

# LinuxCNC (если не установлен)
if ! command -v halcompile &> /dev/null; then
    echo "=== Установка LinuxCNC ==="
    sudo apt-get install -y linuxcnc-uspace linuxcnc-uspace-dev
fi

echo "=== Сборка компонента powerlink.comp ==="
cd "$ROOT/comp"
halcompile --compile powerlink.comp
sudo halcompile --install powerlink.comp

echo "=== Сборка утилит (эмулятор, тест) ==="
cd "$ROOT/tools"
make clean
make

echo "=== Готово ==="
echo "Собранные файлы:"
ls -la "$ROOT/comp/powerlink.so" "$ROOT/tools/plk_sim" "$ROOT/tools/shm_proto_test"
echo ""
echo "Запуск теста:  ./tools/shm_proto_test"
echo "Запуск эмулятора:  ./tools/plk_sim --period-us 2000 --nodes 1,2,3,4,5"
