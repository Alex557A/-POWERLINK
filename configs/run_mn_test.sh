#!/bin/bash
# ============================================================
# run_mn_test.sh - прогон теста MN + захват кадров POWERLINK
# проект Powerlink (НИОКР Vanad)
# ------------------------------------------------------------
# 1) настраивает NIC (PLK_IFACE, по умолч. enp3s0; offload off, promisc)
# 2) запускает tcpdump в фоне
# 3) запускает demo_mn_console (openPOWERLINK MN) на N секунд
# 4) сохраняет pcap и лог в artifacts/
#
# Использование:
#   sudo PLK_CDC_DIR=/path/to/demo_mn_console ./configs/run_mn_test.sh [секунды]
#
# Переменные окружения:
#   PLK_IFACE    сетевой интерфейс (по умолч. enp3s0)
#   PLK_MN_IP    IP MN (по умолч. 192.168.100.240)
#   PLK_CDC_DIR  каталог с demo_mn_console (обязательно указать!)
# ============================================================
set -u

SECONDS=${1:-15}
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IFACE=${PLK_IFACE:-enp3s0}
MN_IP=${PLK_MN_IP:-192.168.100.240}

if [ -z "${PLK_CDC_DIR:-}" ]; then
    echo "ОШИБКА: укажите PLK_CDC_DIR=<каталог с demo_mn_console>" >&2
    echo "Пример: sudo PLK_CDC_DIR=/opt/openPOWERLINK/.../demo_mn_console $0" >&2
    exit 1
fi
CDC_DIR="$PLK_CDC_DIR"

if [ "$(id -u)" != "0" ]; then
    echo "ОШИБКА: нужен root (sudo)" >&2
    exit 1
fi

echo "=== NIC настройка ($IFACE) ==="
ip addr add $MN_IP/24 dev $IFACE 2>/dev/null
ip link set $IFACE up
/usr/sbin/ethtool -K $IFACE rx off tx off gso off tso off gro off 2>/dev/null
ip link set $IFACE promisc on

STAMP=$(date +%Y%m%d-%H%M%S)
PCAP="$ROOT/artifacts/mn_${STAMP}.pcap"
LOG="$ROOT/artifacts/mn_${STAMP}.log"
mkdir -p "$ROOT/artifacts"

echo "=== tcpdump → $PCAP ==="
setsid tcpdump -i $IFACE -nn -e -w "$PCAP" > /tmp/tcpdump_run.log 2>&1 < /dev/null &
TPID=$!
sleep 2

cd "$CDC_DIR"
echo "=== MN (demo_mn_console) на ${SECONDS} с, CDC в $CDC_DIR ==="
(
  sleep $SECONDS
  kill -INT %1 2>/dev/null
) &
./demo_mn_console -d $IFACE > "$LOG" 2>&1

sleep 1
kill -9 $TPID 2>/dev/null
wait $TPID 2>/dev/null

echo "=== Готово ==="
echo "pcap: $PCAP"
echo "лог:  $LOG"
echo "кадров: $(tcpdump -r "$PCAP" -nn 2>/dev/null | wc -l)"
