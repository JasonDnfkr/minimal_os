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
    uint16_t offset_l;
    uint16_t selector;
    uint16_t attr;
    uint16_t offset_h;
} idt_table[256] __attribute__((aligned(8))) = {  };

struct {
    uint16_t limit_l;
    uint16_t base_l;
    uint16_t basehl_attr;
    uint16_t base_limit;
} gdt_table[256] __attribute__((aligned(8))) = {
    [KERNEL_CODE_SEG / 8] = { 0xffff, 0x0000, 0x9a00, 0x00cf },
    [KERNEL_DATA_SEG / 8] = { 0xffff, 0x0000, 0x9200, 0x00cf },
};

void outb(uint8_t data, uint16_t port) {
    __asm__ __volatile__("outb %[v], %[p]"::[p]"d"(port), [v]"a"(data));
}


void timer_int(void);
 
#define TEST_ADDR 0x70000000
#define TEST_ADDR_2 0x12345678
void os_init(void) {
    // 对定时器硬件进行配置，使得CPU只接收时钟中断
    // 初始化8259中断控制器，打开定时器中断
    outb(0x11, 0x20);       // 开始初始化主芯片
    outb(0x11, 0xA0);       // 初始化从芯片
    outb(0x20, 0x21);       // 写ICW2，告诉主芯片中断向量从0x20开始
    outb(0x28, 0xa1);       // 写ICW2，告诉从芯片中断向量从0x28开始
    outb((1 << 2), 0x21);   // 写ICW3，告诉主芯片IRQ2上连接有从芯片
    outb(2, 0xa1);          // 写ICW3，告诉从芯片连接g到主芯片的IRQ2上
    outb(0x1, 0x21);        // 写ICW4，告诉主芯片8086、普通EOI、非缓冲模式
    outb(0x1, 0xa1);        // 写ICW4，告诉主芯片8086、普通EOI、非缓冲模式
    outb(0xfe, 0x21);       // 开定时中断，其它屏蔽
    outb(0xff, 0xa1);       // 屏蔽所有中断

    int tmo = 1193180 / 100;
    outb(0x36, 0x43);               // 二进制计数、模式3、通道0
    outb((uint8_t)tmo, 0x40);
    outb(tmo >> 8, 0x40);    
    
    idt_table[0x20].offset_l = (uint32_t)timer_int & 0xffff;
    idt_table[0x20].offset_h = (uint32_t)timer_int >> 16;
    idt_table[0x20].selector = KERNEL_CODE_SEG;
    idt_table[0x20].attr     = 0x8e00;


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