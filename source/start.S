/**
 * 功能：16位与32位的启动混合代码
 */
	#include "os.h"

	// 声明本地以下符号是全局的，在其它源文件中可以访问
	.global _start

	// 指定以下的代码生成16位的机器指令，这样才能在启动时的实模式下运行
  	.code16

	// 以下是代码区
 	.text

_start:
# 	jmp $0, $offset // qemu 会自动将cs设为0，也可以不用
offset:
	mov $0,	%ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %gs
	mov %ax, %fs

	# 分配栈空间 由于bios会将硬盘第一个扇区的内容
	# 存放至内存0x7c00中，因此将代码开头的地址
	# 也就是_start的内容放入esp就可以设置栈指针的位置
	mov $_start, %esp

read_self_all:
	mov $_start_32, %bx // 0x7c00 + 512
	mov $0x2, %cx    # int 0x13: 起始扇区2
	mov $0x240, %ax  # 读磁盘 ah:2  al:0x40 (64个扇区)
	mov $0x80, %dx   # 读第一个磁盘

	int $0x13        # 中断：读取磁盘
	jc read_self_all

	cli				 # 关中断

	lgdt gdt_desc    # load GDT
	mov $1, %eax
	lmsw %ax         # CR0 = 1

	jmp $KERNEL_CODE_SEG, $_start_32 # CS 设置为 KERNEL_CODE_SEG, 指向内核代码段

	.org 0x1fe // 510
	.byte 0x55, 0xaa

	.code32			 # 执行32位指令
	.text

_start_32:
	mov $KERNEL_DATA_SEG, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss

	mov $_start, %esp


	call os_init
	mov $pg_dir, %eax
	mov %eax, %cr3   # 分页相关

	mov %cr4, %eax
	orl $(1 << 4), %eax
	mov %eax, %cr4

	mov %cr0, %eax
	orl $(1 << 31), %eax
	mov %eax, %cr0

	jmp .

gdt_desc:
	.word (256 * 8) - 1
	.long gdt_table

