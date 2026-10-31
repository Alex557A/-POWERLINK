/*
 * SPDX-License-Identifier: GPL-2.0-or-later OR Commercial
 * Copyright (C) 2026 Vanad Project. All rights reserved.
 *
 * Dual-licensed: GPL-2.0-or-later OR commercial (contact author).
 *
 * ============================================================================
 * shm_proto_test.c — SELF-TEST контракта shared memory (powerlink_shm.h)
 * проект Powerlink (НИОКР Vanad)
 * ----------------------------------------------------------------------------
 * Проверяет сквозной обмен «HAL-сторона <-> демон-сторона» на уровне
 * разделяемой памяти: упаковку PReq (54 Б), разбор PRes (84 Б) и статусов.
 *
 * Использует ТЕ ЖЕ функции упаковки/разбора и смещения, что и
 * comp/powerlink.comp (PL_AXIS_UPDATE):
 *   PReq: [0..1] cmd-word, [2..5] setpoint-pos, [6..7] torque-limit
 *   PRes: [0..1] status-word, [2..5] actual-pos, [6..9] actual-vel,
 *         [10..13] actual-torque
 *
 * Запуск: tools/shm_proto_test  (не требует root)
 * Выход: PASS/FAIL по каждому пункту.
 * ============================================================================
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "../comp/powerlink_shm.h"

static int g_fails = 0;
#define CHECK(cond, msg) \
    do { if (cond) printf("  [PASS] %s\n", msg); \
         else { printf("  [FAIL] %s\n", msg); g_fails++; } } while (0)

static void wr_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xff); p[1] = (uint8_t)((v >> 8) & 0xff);
}
static void wr_s16(uint8_t *p, int16_t v) { wr_u16(p, (uint16_t)v); }
static void wr_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);       p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff); p[3] = (uint8_t)((v >> 24) & 0xff);
}
static void wr_s32(uint8_t *p, int32_t v) { wr_u32(p, (uint32_t)v); }
static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t rd_s32(const uint8_t *p) { return (int32_t)rd_u32(p); }

int main(void)
{
    pl_shm_t *s;
    pl_node_shm_t *n;
    int fd;
    uint8_t preq_saved[PL_PREQ_SIZE];
    int i;

    printf("shm_proto_test: контракт powerlink_shm.h v%u\n", PL_SHM_VERSION);
    printf("  sizeof(pl_shm_t)=%zu, sizeof(pl_node_shm_t)=%zu\n",
           sizeof(pl_shm_t), sizeof(pl_node_shm_t));

    /* создание shm (роль демона) */
    shm_unlink(PL_SHM_DEFAULT_NAME);
    fd = shm_open(PL_SHM_DEFAULT_NAME, O_CREAT | O_RDWR, 0666);
    CHECK(fd >= 0, "shm_open(O_CREAT)");
    if (fd < 0) return 1;
    CHECK(ftruncate(fd, sizeof(pl_shm_t)) == 0, "ftruncate");
    s = mmap(0, sizeof(pl_shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK(s != MAP_FAILED, "mmap");
    if (s == MAP_FAILED) return 1;
    memset(s, 0, sizeof(pl_shm_t));

    /* инициализация как в plk_sim */
    s->magic = PL_SHM_MAGIC;
    s->version = PL_SHM_VERSION;
    s->node[0].node_id = 1;
    s->plk_mn_up = 1;
    s->plk_heartbeat = 42;
    s->plk_cycle_count = 100;

    /* --- HAL-сторона: сборка PReq (повторяет PL_AXIS_UPDATE из powerlink.comp) */
    n = &s->node[0];
    for (i = 0; i < PL_PREQ_SIZE; i++) n->preq[i] = 0;  /* raw-окно = 0 */
    wr_u16(n->preq + 0, 0x0006);          /* cmd-word */
    wr_s32(n->preq + 2, 0x12345678);      /* setpoint-pos */
    wr_s16(n->preq + 6, 0x0123);          /* torque-limit */
    memcpy(preq_saved, n->preq, PL_PREQ_SIZE);

    CHECK(n->preq[0] == 0x06 && n->preq[1] == 0x00,
          "PReq[0..1] = cmd-word 0x0006 (LE)");
    CHECK(n->preq[2] == 0x78 && n->preq[3] == 0x56 &&
          n->preq[4] == 0x34 && n->preq[5] == 0x12,
          "PReq[2..5] = setpoint-pos 0x12345678 (LE)");
    CHECK(n->preq[6] == 0x23 && n->preq[7] == 0x01,
          "PReq[6..7] = torque-limit 0x0123 (LE)");
    for (i = 8; i < PL_PREQ_SIZE; i++)
        if (n->preq[i] != 0) { break; }
    CHECK(i == PL_PREQ_SIZE, "PReq[8..53] = зарезервировано (0)");

    /* --- демон-сторона: заполнение PRes (как в plk_sim) */
    wr_u16(n->pres + 0, 0x0237);          /* status-word */
    wr_s32(n->pres + 2, -1234567);        /* actual-pos (отрицательная!) */
    wr_s32(n->pres + 6, 250);             /* actual-vel */
    wr_s32(n->pres + 10, -500);           /* actual-torque */
    n->status = 0x0237;
    n->pres_seq++;
    n->online = 1;
    n->fault = 0;

    /* --- HAL-сторона: разбор PRes (повторяет PL_AXIS_UPDATE) */
    CHECK(rd_u16(n->pres + 0) == 0x0237, "PRes[0..1] -> status-word 0x0237");
    CHECK(rd_s32(n->pres + 2) == -1234567, "PRes[2..5] -> actual-pos (signed)");
    CHECK(rd_s32(n->pres + 6) == 250, "PRes[6..9] -> actual-vel");
    CHECK(rd_s32(n->pres + 10) == -500, "PRes[10..13] -> actual-torque (signed)");
    CHECK(n->online == 1 && n->fault == 0, "node.online=1, node.fault=0");
    CHECK(n->pres_seq > 0, "pres_seq инкрементируется демоном");

    /* контрольные значения на отправку в реальный plk */
    printf("  PReq payload (54 Б):");
    for (i = 0; i < PL_PREQ_SIZE; i++) {
        if (i % 16 == 0) printf("\n   ");
        printf("%02x ", preq_saved[i]);
    }
    printf("\n");

    munmap(s, sizeof(pl_shm_t));
    close(fd);
    shm_unlink(PL_SHM_DEFAULT_NAME);

    printf("\nРЕЗУЛЬТАТ: %s\n", g_fails == 0 ? "PASS" : "FAIL");
    return g_fails;
}
