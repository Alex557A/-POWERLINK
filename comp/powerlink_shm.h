/*
 * SPDX-License-Identifier: GPL-2.0-or-later OR Commercial
 * Copyright (C) 2026 Vanad Project. All rights reserved.
 *
 * Dual-licensed: GPL-2.0-or-later OR commercial (contact author).
 *
 * ============================================================================
 * powerlink_shm.h — СОГЛАШЕНИЕ ОБ ИНТЕРФЕЙСЕ SHARED MEMORY
 * проект Powerlink (НИОКР Vanad), редакция соглашения: 1
 * ----------------------------------------------------------------------------
 * Общий интерфейс между двумя сторонами:
 *   1. comp/powerlink.comp  — компонент-мост LinuxCNC (HAL), читает/пишет
 *   2. tools/plk_sim.c      — эмулятор демона openPOWERLINK (для оффлайн-тестов)
 *      (на стенде роль «демона» исполняет plk openPOWERLINK userspace)
 *
 * Имя объекта: /powerlink (POSIX shm, /dev/shm/powerlink).
 * Права: создаёт ДЕМОН (O_CREAT), компонент открывает O_RDWR.
 *
 * Размеры PDO соответствуют профилю привода CiA402 (объекты 0x6040/0x6041,
 * 0x6064, 0x606C, 0x6077 — типовые размеры полей u16/s32). Точные размеры
 * PReq/PRes и резервные области — параметры интеграции конкретного привода
 * (задаются в соответствии с его XDD-описанием):
 *   PReq (MN -> CN, выход) — 54 Б
 *   PRes (CN -> MN, вход)  — 84 Б
 *
 * ВАЖНО (синхронизация):
 *   - запись в поля node[N].pres и последующий pres_seq++ выполняет
 *     ТОЛЬКО демон; powerlink.comp только читает;
 *   - запись в node[N].preq выполняет ТОЛЬКО powerlink.comp; демон только
 *     читает (перед отправкой в цикл);
 *   - daemon_heartbeat инкрементирует демон каждый цикл; powerlink.comp
 *     использует его как watchdog (пин daemon-ok);
 *   - односторонность записи исключает гонки в пределах цикла 2 мс.
 * ============================================================================
 */
#ifndef POWERLINK_SHM_H
#define POWERLINK_SHM_H

#include <stdint.h>

#define PL_SHM_MAGIC        0x504C534DUL  /* "PLSM" */
#define PL_SHM_VERSION      1
#define PL_MAX_NODES        16            /* максимум CN в кольце POWERLINK */
#define PL_PREQ_SIZE        54            /* PReq payload, MN -> CN */
#define PL_PRES_SIZE        84            /* PRes payload, CN -> MN */
#define PL_SHM_DEFAULT_NAME "/powerlink"  /* POSIX shm object name */

/* Данные одного узла (CN) в разделяемой памяти. */
typedef struct {
    uint32_t node_id;      /* POWERLINK node-id CN, 0 = слот свободен */
    uint32_t online;       /* 1 = узел в циклическом обмене (синхронен) */
    uint32_t fault;        /* 1 = узел в Fault / потерян */
    uint32_t status;       /* слово состояния привода (XDD, см. журнал 2.2а);
                              бит 0 = online, бит 1 = fault, биты 2..31 — флаги/statusword */
    uint32_t pres_seq;     /* инкремент демона после каждого обновления pres[] */
    uint8_t  preq[PL_PREQ_SIZE]; /* PReq payload: MN -> CN (пишет powerlink.comp) */
    uint8_t  pres[PL_PRES_SIZE]; /* PRes payload: CN -> MN (пишет демон) */
} pl_node_shm_t;

/* Корневая структура разделяемой памяти.
 * Поля с префиксом plk_ — чтобы не пересекаться с C-идентификаторами
 * пинов/параметров powerlink.comp (halcompile генерирует для них макросы). */
typedef struct {
    uint32_t magic;            /* PL_SHM_MAGIC */
    uint32_t version;          /* PL_SHM_VERSION */
    uint32_t plk_heartbeat;    /* инкремент каждый цикл демона */
    uint32_t plk_mn_up;        /* 1 = MN в состоянии Operational, идёт циклический обмен */
    uint32_t plk_errors;       /* счётчик ошибок шины (растёт при сбоях) */
    uint32_t plk_retries;      /* счётчик ретраев PRes */
    uint32_t plk_cycle_count;  /* номер цикла POWERLINK */
    uint32_t plk_cycle_ns;     /* фактический период демона, нс */
    uint32_t plk_state;        /* NMT-состояние стека openPOWERLINK */
    uint32_t plk_cfg_cycle_ns; /* запрос периода от HAL (пишет powerlink.comp), нс */
    uint32_t plk_cfg_mn_nodeid;/* запрос node-id MN от HAL (пишет powerlink.comp) */
    pl_node_shm_t node[PL_MAX_NODES];
    uint8_t  pad[64];          /* запас под расширение */
} pl_shm_t;

#endif /* POWERLINK_SHM_H */