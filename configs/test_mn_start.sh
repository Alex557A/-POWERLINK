#!/bin/bash
# ============================================================
# test_mn_start.sh - оффлайн-запуск Этапа 1 с эмулятором демона
# проект Powerlink (НИОКР Vanad)
# ------------------------------------------------------------
# 1) запускает tools/plk_sim (эмулятор openPOWERLINK) на 2 мс;
# 2) запускает halrun -I -f configs/test_mn.hal (живой тест MN);
# 3) показывает пины powerlink.0 через halcmd;
#
# Требования:
#   - powerlink.so установлен (sudo halcompile --install comp/powerlink.comp)
#   - либо HAL_RTMOD_DIR указывает на каталог с powerlink.so
#
# Переменные окружения:
#   PLK_TEST_NODES  список узлов CN через запятую (по умолч. 1,2,3,4,5)
#   HAL_RTMOD_DIR   каталог с powerlink.so
# ============================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FIFO=/tmp/plk_halrun_in

NODES="${PLK_TEST_NODES:-1,2,3,4,5}"

if [ ! -x "$ROOT/tools/plk_sim" ]; then
    echo "ОШИБКА: соберите сначала tools/plk_sim (make -C tools)" >&2
    exit 1
fi

if [ ! -f "$ROOT/comp/powerlink.so" ]; then
    echo "ОШИБКА: соберите сначала comp/powerlink.so (make -C comp)" >&2
    exit 1
fi

# каталог модулей: по умолчанию системный; можно переопределить HAL_RTMOD_DIR
if [ -z "${HAL_RTMOD_DIR:-}" ]; then
    if [ -w /usr/lib/linuxcnc/modules ] || [ "$(id -u)" = "0" ]; then
        HAL_RTMOD_DIR=/usr/lib/linuxcnc/modules
    else
        HAL_RTMOD_DIR="$ROOT/comp"
        echo "ВНИМАНИЕ: модуль не установлен в систему, используется HAL_RTMOD_DIR=$HAL_RTMOD_DIR" >&2
        echo "  (на стенде: sudo halcompile --install comp/powerlink.comp)" >&2
    fi
fi
export HAL_RTMOD_DIR

# FIFO для управления halrun (интерактивный ввод)
rm -f "$FIFO"
mkfifo "$FIFO"
exec 3>"$FIFO"

# 1) эмулятор демона с указанными узлами
"$ROOT/tools/plk_sim" --period-us 2000 --nodes "$NODES" &
SIM_PID=$!

# 2) halrun в фоне
( cd "$ROOT/configs" && halrun -I -f test_mn.hal < "$FIFO" ) &
HAL_PID=$!

sleep 2

# 3) контроль
halcmd show pin powerlink.0 | head -30

echo ""
echo "Проверка (ТЗ 1.2): mn-up должен быть 1, pl-errors 0:"
halcmd show pin powerlink.0.mn-up
halcmd show pin powerlink.0.pl-errors
halcmd show pin powerlink.0.daemon-ok
halcmd show pin powerlink.0.daemon-heartbeat

echo ""
echo "Просмотр: halcmd show pin powerlink.0"
echo "Останов:  q"
read -r -n 1 key

# 4) останов
echo quit > "$FIFO"
sleep 1
kill "$SIM_PID" 2>/dev/null
wait 2>/dev/null
rm -f "$FIFO"
echo "Готово."
