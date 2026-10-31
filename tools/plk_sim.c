/*
 * SPDX-License-Identifier: GPL-2.0-or-later OR Commercial
 * Copyright (C) 2026 Vanad Project. All rights reserved.
 *
 * Dual-licensed: GPL-2.0-or-later OR commercial (contact author).
 *
 * ============================================================================
 * plk_sim.c - ЭМУЛЯТОР ДЕМОНА openPOWERLINK (для оффлайн-отработки моста)
 * проект Powerlink (НИОКР Vanad)
 * ----------------------------------------------------------------------------
 * Имитирует userspace-демон openPOWERLINK (plk) на уровне контракта shared
 * memory (comp/powerlink_shm.h): создаёт объект /powerlink, ведёт циклический
 * обмен 2 мс, заполняет PRes/статусы узлов, читает PReq.
 *
 * Использование (вместе с powerlink.comp в HAL):
 *   tools/plk_sim --period-us 2000 --nodes 1,2,3,4,5
 *
 * Опции:
 *   --period-us N      период цикла, мкс (по умолчанию 2000)
 *   --nodes id,id,...  список узлов CN (по умолчанию 1,2,3,4,5)
 *   --mn-nodeid N      node-id MN (по умолчанию 240)
 *   --drop-node id     имитация потери узла (online=0, fault=1)
 *   --fault-node id    имитация Fault узла (fault=1)
 *   --sync-delay N     число циклов до mn_up=1 (по умолчанию 5)
 *   --no-sync          никогда не переходить в синхронный обмен
 *   --dump             печатать PReq/PRes каждого узла (hex) каждый цикл
 *   --cycles N         выполнить N циклов и завершиться
 *
 * На стенде роль этого демона исполняет реальный plk openPOWERLINK;
 * протокол shm остаётся тем же.
 * ============================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#include "../comp/powerlink_shm.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

static void wr_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xff); p[1] = (uint8_t)((v >> 8) & 0xff);
}
static void wr_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);       p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff); p[3] = (uint8_t)((v >> 24) & 0xff);
}
static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void dump_pdo(const char *tag, int node, const uint8_t *p, int n) {
    int i;
    printf("%s node %2d (%3d Б):", tag, node, n);
    for (i = 0; i < n; i++) {
        if (i % 16 == 0) printf("\n  ");
        printf("%02x ", p[i]);
    }
    printf("\n");
}


int main(int argc, char **argv)
{
    int period_us = 2000;
    int mn_nodeid = 240;
    int sync_delay = 5;
    int dump = 0, no_sync = 0;
    long cycles = -1;
    int nodes[PL_MAX_NODES];
    int drop_nodes[PL_MAX_NODES] = {0};
    int fault_nodes[PL_MAX_NODES] = {0};
    int n_nodes = 0;
    int fd, i;
    pl_shm_t *s;
    struct timespec next, dt;
    unsigned long cnt = 0;
    int32_t pos = 0;

    /* разбор аргументов */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--period-us") && i + 1 < argc) {
            period_us = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--mn-nodeid") && i + 1 < argc) {
            mn_nodeid = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--sync-delay") && i + 1 < argc) {
            sync_delay = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--cycles") && i + 1 < argc) {
            cycles = atol(argv[++i]);
        } else if (!strcmp(argv[i], "--nodes") && i + 1 < argc) {
            char *tok = strtok(argv[++i], ",");
            while (tok && n_nodes < PL_MAX_NODES) {
                nodes[n_nodes++] = atoi(tok);
                tok = strtok(NULL, ",");
            }
        } else if (!strcmp(argv[i], "--drop-node") && i + 1 < argc) {
            int id = atoi(argv[++i]);
            if (id >= 1 && id < PL_MAX_NODES) drop_nodes[id] = 1;
        } else if (!strcmp(argv[i], "--fault-node") && i + 1 < argc) {
            int id = atoi(argv[++i]);
            if (id >= 1 && id < PL_MAX_NODES) fault_nodes[id] = 1;
        } else if (!strcmp(argv[i], "--dump")) {
            dump = 1;
        } else if (!strcmp(argv[i], "--no-sync")) {
            no_sync = 1;
        } else {
            fprintf(stderr, "plk_sim: неизвестная опция: %s\n", argv[i]);
            return 1;
        }
    }
    if (n_nodes == 0) {
        static int def[] = {1, 2, 3, 4, 5};
        memcpy(nodes, def, sizeof(def));
        n_nodes = 5;
    }

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    /* создание/инициализация shm */
    fd = shm_open(PL_SHM_DEFAULT_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        return 1;
    }
    if (ftruncate(fd, sizeof(pl_shm_t)) < 0) {
        perror("ftruncate");
        return 1;
    }
    s = mmap(0, sizeof(pl_shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (s == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    memset(s, 0, sizeof(pl_shm_t));
    s->magic = PL_SHM_MAGIC;
    s->version = PL_SHM_VERSION;
    for (i = 0; i < n_nodes; i++)
        s->node[i].node_id = (uint32_t)nodes[i];

    printf("plk_sim: shm %s создан (magic=0x%08x, version=%u), период %d мкс\n",
           PL_SHM_DEFAULT_NAME, s->magic, s->version, period_us);
    printf("plk_sim: узлы:");
    for (i = 0; i < n_nodes; i++) printf(" %d", nodes[i]);
    printf(", MN node-id=%d\n", mn_nodeid);


    /* цикл эмуляции */
    clock_gettime(CLOCK_MONOTONIC, &next);
    while (!g_stop) {
        int node_idx;
        cnt++;
        s->plk_heartbeat++;
        s->plk_cycle_count = (uint32_t)cnt;

        /* синхронизация MN */
        if (!no_sync && cnt > (unsigned long)sync_delay)
            s->plk_mn_up = 1;
        s->plk_cycle_ns = (uint32_t)period_us * 1000u;

        for (node_idx = 0; node_idx < n_nodes; node_idx++) {
            int id = nodes[node_idx];
            pl_node_shm_t *n = &s->node[node_idx];
            uint16_t sw;
            uint8_t preq[PL_PREQ_SIZE];

            if (drop_nodes[id]) {
                n->online = 0;
                n->fault = 1;
                n->status = 0;
                s->plk_errors++;   /* потеря узла - ошибка шины */
                continue;
            }
            n->online = 1;
            n->fault = fault_nodes[id] ? 1 : 0;

            /* слово состояния по образцу CiA402: 0x0237 = Operation Enabled */
            sw = 0x0237;
            wr_u16(n->pres + 0, sw);
            wr_u32(n->pres + 2, (uint32_t)pos);        /* actual-pos */
            wr_u32(n->pres + 6, 100u);                  /* actual-vel */
            wr_u32(n->pres + 10, 0u);                   /* actual-torque */
            n->status = (uint32_t)sw;
            n->pres_seq++;

            pos += 1000;

            if (dump) {
                memcpy(preq, n->preq, sizeof(preq));
                dump_pdo("PReq", id, preq, PL_PREQ_SIZE);
                dump_pdo("PRes", id, n->pres, PL_PRES_SIZE);
            }
        }

        /* задержка до следующего тика */
        next.tv_nsec += (long)period_us * 1000L;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_sec += 1;
            next.tv_nsec -= 1000000000L;
        }
        while (1) {
            struct timespec now;
            long delta_ns;
            clock_gettime(CLOCK_MONOTONIC, &now);
            delta_ns = (next.tv_sec - now.tv_sec) * 1000000000L +
                       (next.tv_nsec - now.tv_nsec);
            if (delta_ns <= 0) break;
            if (delta_ns > 1000000L) delta_ns = 1000000L;
            dt.tv_sec = 0;
            dt.tv_nsec = delta_ns;
            nanosleep(&dt, NULL);
        }

        if (cycles > 0 && (long)cnt >= cycles)
            break;
    }

    printf("plk_sim: останов по сигналу, циклов: %lu\n", cnt);
    munmap(s, sizeof(pl_shm_t));
    close(fd);
    shm_unlink(PL_SHM_DEFAULT_NAME);
    return 0;
}
