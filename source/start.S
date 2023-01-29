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
	jmp $0, $offset // qemu 会自动将cs设为0，也可以不用

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

	jmp .

	.org 	0x1fe // 510
	.byte 	0x55, 0xaa