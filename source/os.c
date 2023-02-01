#include "os.h"

typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;

void do_syscall(int func, char* str, char color) {
    static int row = 1;

    if (func == 2) {
        unsigned short* dest = (unsigned short*)0xb8000 + 80 * row;
        while (*str) {
            *dest++ = *str++ | (color << 8);
        }

        row = (row >= 25) ? 0 : row + 1;
    }
}

void sys_show(char* str, char color) {
    uint32_t addr[] = { 0, SYSCALL_SEG };
    __asm__ __volatile__("push %[color]; push %[str]; push %[id]; lcalll *(%[a])"::
        [a]"r"(addr), [color]"m"(color), [str]"m"(str), [id]"r"(2));
}

void task_0(void) {
    char* str = "task a: 1234";
    uint8_t color = 0;

    unsigned short* dest = (unsigned short*) 0xb8000;
    dest[0] = 'a' | 0x3500;
    dest[1] = 'b' | 0x4700;
    while (1) {
        sys_show(str, color++);
    }
}

void task_1(void) {
    char* str = "task b: 5678";

    uint8_t color = 0xff;
    while (1) {
        sys_show(str, color--);
    }
}

#define PDE_P       (1 << 0)  // 表项存在
#define PDE_W       (1 << 1)  // 可读写
#define PDE_U       (1 << 2)  // 用户模式访问
#define PDE_PS      (1 << 7)  // 

#define MAP_ADDR    0x80000000

uint8_t map_phy_buffer[4096] __attribute__((aligned(4096))) = { 0x36, 0x99 };

static uint32_t page_table[1024] __attribute__((aligned(4096))) = { PDE_U };

uint32_t pg_dir[1024] __attribute__((aligned(4096))) = {
    [0] = (0) | PDE_P | PDE_W | PDE_U | PDE_PS, // 打开了一个4M -> 4M 的映射页表
};

uint32_t task0_dpl0_stack[1024];
uint32_t task0_dpl3_stack[1024];
uint32_t task1_dpl0_stack[1024];
uint32_t task1_dpl3_stack[1024];

uint32_t task0_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task0_dpl0_stack + 4 * 1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,  (uint32_t)task_0/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task0_dpl3_stack + 4 * 1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    APP_DATA_SEG, APP_CODE_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, 0x0, 0x0,
};

uint32_t task1_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task1_dpl0_stack + 4 * 1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,  (uint32_t)task_1/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task1_dpl3_stack + 4 * 1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    APP_DATA_SEG, APP_CODE_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, 0x0, 0x0,
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
    // 全是 起始地址0，limit全1，
    [KERNEL_CODE_SEG / 8] = { 0xffff, 0x0000, 0x9a00, 0x00cf },
    [KERNEL_DATA_SEG / 8] = { 0xffff, 0x0000, 0x9200, 0x00cf },

    [APP_CODE_SEG / 8] = { 0xffff, 0x0000, 0xfa00, 0x00cf },
    [APP_DATA_SEG / 8] = { 0xffff, 0x0000, 0xf300, 0x00cf },

    [TASK0_TSS_SEG / 8] = { 0x68, 0, 0xe900, 0x0 },
    [TASK1_TSS_SEG / 8] = { 0x68, 0, 0xe900, 0x0 },

    [SYSCALL_SEG / 8] = { 0, KERNEL_CODE_SEG, 0xec03, 0 },
};

void outb(uint8_t data, uint16_t port) {
    __asm__ __volatile__("outb %[v], %[p]"::[p]"d"(port), [v]"a"(data));
}


void task_sched(void) {
    static int task_tss = TASK0_TSS_SEG;

    task_tss = (task_tss == TASK0_TSS_SEG) ? TASK1_TSS_SEG : TASK0_TSS_SEG;

    // 偏移，选择子?? 
    uint32_t addr[] = { 0, task_tss };
    __asm__ __volatile__("ljmpl *(%[a])"::[a]"r"(addr));
}

void timer_int(void);
void syscall_handler(void);
 
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

    gdt_table[TASK0_TSS_SEG / 8].base_l = (uint16_t)(uint32_t)task0_tss;
    gdt_table[TASK1_TSS_SEG / 8].base_l = (uint16_t)(uint32_t)task1_tss;
    gdt_table[SYSCALL_SEG / 8].limit_l = (uint16_t)(uint32_t)syscall_handler;



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