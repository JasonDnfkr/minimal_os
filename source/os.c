#include "os.h"

typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;

#define PDE_P       (1 << 0)  // 表项存在
#define PDE_W       (1 << 1)  // 可读写
#define PDE_U       (1 << 2)  // 用户模式访问
#define PDE_PS      (1 << 7)  // 

#define MAP_ADDR    0x80000000

uint8_t map_phy_buffer[4096] __attribute__((aligned(4096))) = { 0x36, 0x99 };

static uint32_t page_table[1024] __attribute__((aligned(4096))) = { PDE_U };

uint32_t pg_dir[1024] __attribute__((aligned(4096))) = {
    [0] = (0) | PDE_P | PDE_W | PDE_U | PDE_PS,
};

struct {
    uint16_t limit_l;
    uint16_t base_l;
    uint16_t basehl_attr;
    uint16_t base_limit;
} gdt_table[256] __attribute__((aligned(8))) = {
    [KERNEL_CODE_SEG / 8] = { 0xffff, 0x0000, 0x9a00, 0x00cf },
    [KERNEL_DATA_SEG / 8] = { 0xffff, 0x0000, 0x9200, 0x00cf },
};
 
#define TEST_ADDR 0x70000000
#define TEST_ADDR_2 0x12345678
void os_init(void) {
    pg_dir[MAP_ADDR >> 22] = (uint32_t)page_table | PDE_P | PDE_W | PDE_U;
    page_table[(MAP_ADDR >> 12) & 0x3ff] = (uint32_t)map_phy_buffer | PDE_P | PDE_W | PDE_U;
    
    pg_dir[TEST_ADDR >> 22] = (uint32_t)page_table | PDE_P | PDE_W | PDE_U;
    page_table[(TEST_ADDR >> 12) & 0x3ff] = (uint32_t)map_phy_buffer | PDE_P | PDE_W | PDE_U;

    pg_dir[TEST_ADDR_2 >> 22] = (uint32_t)page_table | PDE_P | PDE_W | PDE_U;
    page_table[(TEST_ADDR_2 >> 12) & 0x3ff] = (uint32_t)map_phy_buffer | PDE_P | PDE_W | PDE_U;

    uint32_t addr1 = TEST_ADDR;
    *((uint32_t*)TEST_ADDR) = 66;
    map_phy_buffer[1] = 99;
    map_phy_buffer[2] = 88;
}