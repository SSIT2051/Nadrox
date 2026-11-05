/*
 * Syscall Translation Table
 * ARM -> x86_64 syscall number and argument mapping
 */

#include "syscall_table.h"
#include "atl_arm_translate.h"
#include <string.h>
#include <fcntl.h>

static syscall_mapping_t g_syscall_map[SYSCALL_TABLE_SIZE];
static bool g_syscall_table_initialized = false;

static void add_mapping(unsigned arm_n, unsigned x86_n, unsigned nargs, 
                        arg_mapping_style_t in_style, arg_mapping_style_t out_style, 
                        unsigned int flags)
{
    if (arm_n < SYSCALL_TABLE_SIZE) {
        g_syscall_map[arm_n].arm_num = arm_n;
        g_syscall_map[arm_n].x86_num = x86_n;
        g_syscall_map[arm_n].num_args = nargs;
        g_syscall_map[arm_n].arg_in_style = in_style;
        g_syscall_map[arm_n].arg_out_style = out_style;
        g_syscall_map[arm_n].flags = flags;
        g_syscall_map[arm_n].valid = true;
    }
}

/* Initialize the syscall table */
void syscall_table_init(void)
{
    if (g_syscall_table_initialized) return;
    
    memset(g_syscall_map, 0, sizeof(g_syscall_map));
    
    /* Core syscalls */
    add_mapping(0, 0, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(1, 0, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(2, 57, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(3, 0, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(4, 1, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(5, 257, 3, ARM_ARG_REGS_STYLE, X86_ARG_OPEN_STYLE, 1);
    add_mapping(6, 2, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(7, 59, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(8, 16, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(9, 22, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(10, 87, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(11, 59, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(12, 90, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(13, 37, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(14, 165, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(15, 92, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(16, 90, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(19, 8, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(20, 89, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(21, 165, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(22, 76, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(23, 106, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(24, 102, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(25, 201, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(26, 101, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(27, 97, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(29, 94, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(30, 26, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(33, 91, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(34, 40, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(36, 162, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(37, 62, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(38, 82, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(39, 83, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(40, 84, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(41, 32, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(42, 22, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(43, 61, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(45, 12, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(46, 105, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(47, 104, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(49, 103, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(50, 104, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(54, 16, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(55, 85, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(57, 32, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(60, 34, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(61, 161, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(62, 22, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(63, 32, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(64, 102, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(65, 3, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(66, 66, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(67, 16, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(70, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(71, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(72, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(73, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(74, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(75, 9, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(77, 79, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(78, 96, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(79, 55, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(80, 109, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(81, 116, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(83, 74, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(85, 78, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(91, 32, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(92, 151, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(93, 152, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(94, 7, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(95, 11, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(96, 44, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(97, 45, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(98, 14, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(99, 50, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(100, 51, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(101, 148, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(140, 87, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(141, 9, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(142, 4, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(143, 3, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(144, 26, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(145, 9, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(146, 20, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(147, 155, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(148, 16, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(149, 0, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(150, 28, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(151, 150, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(152, 3, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(153, 3, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(168, 64, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(172, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(173, 15, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 1);
    add_mapping(174, 13, 4, ARM_ARG_REGS_STYLE, X86_ARG_SIGACTION_STYLE, 1);
    add_mapping(175, 127, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(176, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(177, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(178, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(179, 15, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 1);
    add_mapping(180, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(181, 1, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(182, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(183, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(184, 72, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(185, 125, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(186, 129, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(187, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(190, 11, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(191, 11, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(192, 9, 6, ARM_ARG_REGS_STYLE, X86_ARG_MMAP_STYLE, 1);
    add_mapping(193, 10, 6, ARM_ARG_REGS_STYLE, X86_ARG_MMAP_STYLE, 1);
    add_mapping(194, 25, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(195, 149, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(196, 27, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(197, 156, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(220, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(221, 44, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(222, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(224, 158, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(226, 178, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(227, 234, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(228, 236, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(229, 238, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(230, 239, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(231, 241, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(232, 243, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(233, 187, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(234, 188, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(235, 189, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(236, 197, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(237, 198, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(238, 199, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(240, 202, 6, ARM_ARG_REGS_STYLE, X86_ARG_FUTEX_STYLE, 1);
    add_mapping(241, 208, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(242, 207, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(243, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(244, 9, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(245, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(246, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(247, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(248, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(249, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(250, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(251, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(252, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(256, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(257, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(258, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(259, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(260, 270, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(262, 23, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(263, 7, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(264, 111, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(265, 109, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(266, 110, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(267, 108, 1, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(268, 35, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(269, 228, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(270, 230, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(271, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(272, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(273, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(274, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(275, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(276, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(277, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(278, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(279, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(280, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(281, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(282, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(283, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(284, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(285, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(286, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(287, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(288, 43, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(289, 44, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(290, 45, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(291, 47, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(292, 46, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(293, 51, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(294, 54, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(295, 45, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(296, 44, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(297, 46, 6, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(298, 47, 6, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(299, 52, 2, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(300, 49, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(301, 55, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(302, 45, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(303, 47, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(304, 46, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(305, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(306, 64, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(307, 68, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(308, 70, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(309, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(310, 71, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(311, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(312, 30, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(313, 29, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(314, 31, 3, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(315, 270, 4, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(316, 250, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(317, 251, 5, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(318, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(319, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(320, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    add_mapping(321, 0, 0, ARM_ARG_REGS_STYLE, X86_ARG_REGS_STYLE, 0);
    
    g_syscall_table_initialized = true;
}

syscall_mapping_t *get_syscall_mapping(uint32_t arm_syscall)
{
    if (!g_syscall_table_initialized) syscall_table_init();
    if (arm_syscall < SYSCALL_TABLE_SIZE) {
        if (g_syscall_map[arm_syscall].valid) {
            return &g_syscall_map[arm_syscall];
        }
    }
    return NULL;
}

uint32_t arm_to_x86_syscall(uint32_t arm_syscall)
{
    if (!g_syscall_table_initialized) syscall_table_init();
    
    if (arm_syscall < SYSCALL_TABLE_SIZE && g_syscall_map[arm_syscall].valid) {
        return g_syscall_map[arm_syscall].x86_num;
    }
    
    return atl_arm_syscall_arm_to_host(arm_syscall);
}

void translate_syscall_args(arm_regs_t *arm_regs, x86_regs_t *x86_regs, syscall_mapping_t *mapping)
{
    if (!arm_regs || !x86_regs || !mapping) return;
    
    memset(x86_regs, 0, sizeof(x86_regs_t));
    
    /* Standard mapping for most syscalls */
    if (mapping->arg_in_style == ARM_ARG_REGS_STYLE || mapping->arg_out_style == X86_ARG_REGS_STYLE) {
        if (mapping->num_args >= 1) x86_regs->rdi = arm_regs->r[0];
        if (mapping->num_args >= 2) x86_regs->rsi = arm_regs->r[1];
        if (mapping->num_args >= 3) x86_regs->rdx = arm_regs->r[2];
        if (mapping->num_args >= 4) x86_regs->r10 = arm_regs->r[3];
        if (mapping->num_args >= 5) x86_regs->r8 = arm_regs->r[4];
        if (mapping->num_args >= 6) x86_regs->r9 = arm_regs->r[5];
    }
    
    /* Special handling for open -> openat */
    if (mapping->arg_out_style == X86_ARG_OPEN_STYLE) {
        x86_regs->rdi = AT_FDCWD;
        x86_regs->rsi = arm_regs->r[0];
        x86_regs->rdx = arm_regs->r[1];
        x86_regs->r10 = arm_regs->r[2];
    }
    
    /* Special handling for mmap */
    if (mapping->arg_out_style == X86_ARG_MMAP_STYLE) {
        if (mapping->num_args >= 1) x86_regs->rdi = arm_regs->r[0];
        if (mapping->num_args >= 2) x86_regs->rsi = arm_regs->r[1];
        if (mapping->num_args >= 3) x86_regs->rdx = arm_regs->r[2];
        if (mapping->num_args >= 4) x86_regs->r10 = arm_regs->r[3];
        if (mapping->num_args >= 5) x86_regs->r8  = arm_regs->r[4];
        if (mapping->num_args >= 6) x86_regs->r9  = arm_regs->r[5];
    }
    
    /* Special handling for futex */
    if (mapping->arg_out_style == X86_ARG_FUTEX_STYLE) {
        if (mapping->num_args >= 1) x86_regs->rdi = arm_regs->r[0];
        if (mapping->num_args >= 2) x86_regs->rsi = arm_regs->r[1];
        if (mapping->num_args >= 3) x86_regs->rdx = arm_regs->r[2];
        if (mapping->num_args >= 4) x86_regs->r10 = arm_regs->r[3];
        if (mapping->num_args >= 5) x86_regs->r8  = arm_regs->r[4];
        if (mapping->num_args >= 6) x86_regs->r9  = arm_regs->r[5];
    }
    
    /* Special handling for rt_sigaction */
    if (mapping->arg_out_style == X86_ARG_SIGACTION_STYLE) {
        if (mapping->num_args >= 1) x86_regs->rdi = arm_regs->r[0];
        if (mapping->num_args >= 2) x86_regs->rsi = arm_regs->r[1];
        if (mapping->num_args >= 3) x86_regs->rdx = arm_regs->r[2];
        if (mapping->num_args >= 4) x86_regs->r10 = arm_regs->r[3];
    }
}
