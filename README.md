# Powerlink — Мост LinuxCNC ↔ Ethernet POWERLINK
# (совместим с сервоприводами POWERLINK, реализующими профиль CiA402)

НИОКР по отработке протокола POWERLINK между управляющим узлом (MN)
на базе LinuxCNC с ядром реального времени и ведомыми узлами (CN) —
сервоприводами с профилем CiA402.

## Структура проекта

```
├── README.md                  этот файл
├── ARCHITECTURE.md            архитектура Clean Room (разделение открытого/закрытого)
├── NOTICE                     атрибуция сторонних компонентов (BSD/GPL)
├── LICENSE                    GPLv2
├──
├── comp/                      компонент-мост powerlink.comp
│   ├── powerlink_shm.h        контракт shared memory
│   ├── powerlink.comp         исходник компонента LinuxCNC
│   ├── powerlink.so           скомпилированный модуль (после make)
│   └── powerlink.c            сгенерированный C (после make)
├──
├── tools/                     утилиты
│   ├── plk_sim.c              эмулятор демона openPOWERLINK (offline)
│   ├── plk_sim                собранный бинарник
│   ├── shm_proto_test.c       self-test контракта shared memory
│   └── Makefile               сборка утилит
├──
├── configs/                   конфигурации для тестов
│   ├── test_mn.ini            минимальная конфигурация LinuxCNC (IV.2)
│   ├── test_mn.hal            HAL-скрипт MN, personality=0 (Этап 1)
│   ├── test_mn_axes.hal       HAL-скрипт MN, personality=0x3f (Э��ап 2)
│   └── test_mn_start.sh       запуск эмулятора + HAL (offline)
├──
├── proposals/                 примеры конфигураций (IV.1)
│   └── powerlink.proposed.hal пример конфигурации после НИОКР
└──
```

## Сборка

```bash
# 1. Компонент powerlink.comp (HAL realtime)
cd comp
halcompile --compile powerlink.comp          # → powerlink.so
# sudo halcompile --install powerlink.comp   # установка в систему

# 2. Утилиты (юзерленд, offline)
cd tools
make              # → plk_sim, shm_proto_test
```

## Быстрый старт (offline, с эмулятором)

```bash
# 1. Собрать всё
make -C comp   # (halcompile --compile comp/powerlink.comp)
make -C tools

# 2. Проверить контракт shared memory
./tools/shm_proto_test

# 3. Запустить эмулятор + HAL (если модуль установлен)
./configs/test_mn_start.sh
```

## Этапы по ТЗ

Этапы проекта описаны в [Техзадача.txt](Техзадача.txt) (внутренний документ, в репозиторий не включён).


## 💼 Коммерческие услуги / Commercial Services

Проект Powerlink распространяется под **GPLv2** (см. [LICENSE](LICENSE)).
GPLv2 гарантирует свободу использования для любых целей, включая
коммерческие (производство, встраивание в станки, продажа изделий
с этим компонентом) — **без покупки лицензии**. Отдельная лицензия
«на использование» не требуется и не продаётся.

Для компаний доступны **платные услуги** исполнителя (работы и сервис,
а не передача прав — поэтому они полностью совместимы с GPL):

| Услуга | Стоимость |
|:-------|:----------|
| 🛠️ Консультация по внедрению POWERLINK | договорная |
| 🔧 Адаптация/интеграция под ваше оборудование (HAL-конфигурации, настройка шины) | договорная |
| 🚀 Техническая поддержка и сопровождение (SLA) | договорная |
| 📦 Пуско-наладка стенда, обучение | договорная |

*Все работы выполняются только на открытых средствах (openPOWERLINK,
EPSG OpenConfigurator, LinuxCNC). Использование коммерческого ПО
изготовителей приводов (в т.ч. для генерации файлов описания устройств)
не требуется и не входит в услуги; такие файлы предоставляет Заказчик
в составе документации на своё оборудование.*

**Важно про лицензирование:**

- `comp/powerlink.comp` лицензируется **только GPLv2** в силу связывания
  с GPL-библиотеками LinuxCNC (HAL, RTAPI). Предоставление этого компонента
  на проприетарных условиях невозможно и **не предлагается**.
- Утилиты `tools/` (`plk_sim`, `shm_proto_test`) — авторский
  код исполнителя. По запросу доступна **двойная (коммерческая) лицензия**
  для встраивания этих утилит в закрытые продукты — по отдельному договору.

**Оплата (РФ):**

> 👤 Исполнитель — самозанятый (НПД, налог 4% физлица / 6% юрлица)
> 💳 Т-Банк (Тинькофф)
> 🔗 Платёжная ссылка: `[ВСТАВИТЬ ССЫЛКУ ПОСЛЕ ПОДКЛЮЧЕНИЯ Т-БАНК КАССЫ]`
> 📱 Принимаются карты Visa/MC/Мир и СБП

*Услуги (консультации, адаптация, наладка) оплачиваются в рамках НПД.
Двойное коммерческое лицензирование утилит оформляется отдельным
договором и подразумевает статус ИП/ООО — условия согласовываются
индивидуально.*

По вопросам — открывайте [Issue](https://github.com/Alex557A/-POWERLINK/issues) или пишите на почту.

---

## License / Лицензия

This project is distributed under the **GNU General Public License v2 (GPLv2)**.
See the [LICENSE](LICENSE) file for details.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the clean-room architecture description
and [NOTICE](NOTICE) for third-party component attribution (openPOWERLINK BSD, LinuxCNC GPL).

- 

---

## IMPORTANT / ВАЖНО

**Заказчик оффлайн / вне зоны действия сети (Customer is offline / out of network coverage).**

**Исполнителю: перед публикацией или распространением любых материалов данного репозитория
рекомендуется проконсультироваться с юристом относительно правовых норм использования третьих
сторонних документов, файлов конфигурации, дампов трафика и иных материалов, которые могут
содержать интеллектуальную собственность изготовителей оборудования или заказчика.

(Prior to publishing or distributing any materials from this repository, the executor is advised
to consult with a lawyer regarding the legal use of third-party documents, configuration files,
traffic captures, and other materials that may contain intellectual property of equipment
manufacturers or the customer.)**

---

