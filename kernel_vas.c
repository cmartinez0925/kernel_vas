#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/fixmap.h>


MODULE_AUTHOR("Chris Martinez");
MODULE_DESCRIPTION("Showing the Kernel Virtual Address Space. Inspired by Kernel Linux Programming 2d Edition Book");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");


#define DOTS "|                         [ . . . ]                           |\n"

/*Show the differences between the various units*/
#define SHOW_DELTA_BYTES(low, hi) (low), (hi), ((hi) - (low))
#define SHOW_DELTA_KILO(low, hi) (low), (hi), (((hi) - (low)) >> 10)
#define SHOW_DELTA_MEGA(low, hi) (low), (hi), (((hi) - (low)) >> 20)
#define SHOW_DELTA_GIGA(low, hi) (low), (hi), (((hi) - (low)) >> 30)
#define SHOW_DELTA_MEGA_GIGA(low, hi) (low), (hi), (((hi) - (low)) >> 20), (((hi) - (low)) >> 30)

#if (BITS_PER_LONG == 64)
#define SHOW_DELTA_MEGA_GIGA_TERA(low, hi) (low), (hi), (((hi) - (low)) >> 20), (((hi) - (low)) >> 30), (((hi) - (low)) >> 40)
#else // 32-bit
#define SHOW_DELTA_MEGA_GIGA_TERA(low, hi) (low), (hi), (((hi) - (low)) >> 20), (((hi) - (low)) >> 30)
#endif

/*Module parameter to show user VAS*/
static int show_uservas;
module_param(show_uservas, int , 0660);
MODULE_PARM_DESC(show_uservas, "Show some user space VAS details; 0 = no (default), 1 = show");

/*
 * Author: Chris Martinez
 * Date: January 21, 2025
 * Version: 1.0
 * Description: Prints out basic system information
 *
*/
void minsysinfo(void) {
#define MSGLEN   128
    char msg[MSGLEN];
    memset(msg, 0, MSGLEN);
    snprintf(msg, 48, "%s(): minimal platform info:\nCPU: ", __func__);

#ifdef CONFIG_X86
#if(BITS_PER_LONG == 32)
    strlcat(msg, "x86_32, ", MSGLEN);
#else
    strlcat(msg, "x86_64, ", MSGLEN);
#endif
#endif
#ifdef CONFIG_ARM
    strlcat(msg, "ARM-32, ", MSGLEN);
#endif
#ifdef CONFIG_ARM64
    strlcat(msg, "Aarch64, ", MSGLEN);
#endif
#ifdef CONFIG_MIPS
    strlcat(msg, "MIPS, ", MSGLEN);
#endif
#ifdef CONFIG_PPC
    strlcat(msg, "PowerPC, ", MSGLEN);
#endif
#ifdef CONFIG_S390
    strlcat(msg, "IBM S390, ", MSGLEN);
#endif

#ifdef __BIG_ENDIAN
    strlcat(msg, "big-endian; ", MSGLEN);
#else
    strlcat(msg, "little-endian; ", MSGLEN);
#endif

#if(BITS_PER_LONG == 32)
    strlcat(msg, "32-bit OS.\n", MSGLEN);
#elif(BITS_PER_LONG == 64)
    strlcat(msg, "64-bit OS.\n", MSGLEN);
#endif
    pr_info("%s", msg);
}

/*
 * Author: Chris Martinez
 * Date: January 21, 2025
 * Version: 1.0
 * Description: Prints out userspace information
 *
*/
static void show_userspace_info(void) {
	pr_info("+------- Above this line: kernel VAS; below: user VAS --------+\n"
        DOTS
        "|Process environment "
#if (BITS_PER_LONG == 64)
        " %px - %px     | [ %4zu bytes]\n"
        "|          arguments "
        " %px - %px     | [ %4zu bytes]\n"
        "|        stack start  %px                        |\n"
        "|       heap segment "
        " %px - %px     | [ %9zu KB]\n"
        "|static data segment "
        " %px - %px     | [ %4zu bytes]\n"
        "|       text segment "
        " %px - %px     | [ %9zu KB]\n"
#else // 32-bit
        " %px - %px                     | [ %4zu bytes]\n"
        "|          arguments "
        " %px - %px                     | [ %4zu bytes]\n"
        "|        stack start  %px                                |\n"
        "|       heap segment "
        " %px - %px                     | [ %9zu KB]\n"
        "|static data segment "
        " %px - %px                     | [ %4zu bytes]\n"
        "|       text segment "
        " %px - %px                     | [ %9zu KB]\n"
#endif
        DOTS
        "+-------------------------------------------------------------+\n",
        SHOW_DELTA_BYTES((void *)current->mm->env_start, (void *)current->mm->env_end),
        SHOW_DELTA_BYTES((void *)current->mm->arg_start, (void *)current->mm->arg_end),
        (void *)current->mm->start_stack,
        SHOW_DELTA_KILO((void *)current->mm->start_brk, (void *)current->mm->brk),
        SHOW_DELTA_BYTES((void *)current->mm->start_data, (void *)current->mm->end_data),
        SHOW_DELTA_KILO((void *)current->mm->start_code, (void *)current->mm->end_code)
    );   

    pr_info(
#if (BITS_PER_LONG == 64)
	    "Kernel, User VAS (TASK_SIZE) size each = %15zu bytes  [  %zu GB]\n"
#else	// 32-bit
	    "Size of User VAS size (TASK_SIZE) = %10lu bytes            [  %lu GB]\n"
#endif
	    " # userspace memory regions (VMAs) = %d\n",
#if (BITS_PER_LONG == 64)
	    TASK_SIZE, (TASK_SIZE >> 30),
#else	// 32-bit
	    TASK_SIZE, (TASK_SIZE >> 20),
#endif
	    current->mm->map_count
    );

#ifdef DEBUG
	pr_info("[DEBUG] Above statistics are wrt 'current' thread (see below):\n");
	PRINT_CTX();		/* show which process is the one in context */
#endif
}

/*
 * Author: Chris Martinez
 * Date: January 21, 2025
 * Version: 1.0
 * Description: Prints out kernelspace information
 *
*/
static void show_kernelvas_info(void) {
    unsigned long ram_size;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    ram_size = totalram_pages() * PAGE_SIZE;
#else 
    ram_size = totalram_pages * PAGE_SIZE;
#endif
	pr_info("PAGE_SIZE = %lu, total RAM ~= %lu MB (%lu bytes)\n",
        PAGE_SIZE, ram_size/(1014*1024), ram_size); 

#if defined(CONFIG_ARM64)
    pr_info("VA_BITS (CONFIG_ARM64_VA_BITS) = %d\n", VA_BITS);
    if (VA_BITS > 48 && PAGE_SIZE == (64 * 1024))
		pr_info("*** >= ARMv8.2 with LPA? (YMMV, not supported here) ***\n");
#endif
	pr_info("Some Kernel Details [by decreasing address; values are approximate]\n"
		"+-------------------------------------------------------------+\n");

    /* ARM-32 vector table */
#if defined(CONFIG_ARM)
    /* On ARM, the definition of VECTORS_BASE turns up only in kernels >= 4.11 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0)
    pr_info(DOTS
		"|vector table:       "
		" %px - %px                     | [%5zu KB]\n",
		SHOW_DELTA_KILO((void *)VECTORS_BASE, (void *)(VECTORS_BASE + PAGE_SIZE)));
#endif
#endif

/* kernel fixmap region */
	pr_info(DOTS
		"|fixmap region:      "
#if defined(CONFIG_ARM)
		" %px - %px                     | [%5zu MB]\n",
		SHOW_DELTA_MEGA((void *)FIXADDR_START, (void *)FIXADDR_END)
#else
#if defined(CONFIG_ARM64) || defined(CONFIG_X86)
		" %px - %px     | [%9zu MB]\n",
		SHOW_DELTA_MEGA((void *)FIXADDR_START, (void *)(FIXADDR_START + FIXADDR_SIZE))
#endif
#endif
	);

	/* kernel module region
	 * For the modules region, it's high in the kernel segment on typical 64-bit
	 * systems, but the other way around on many 32-bit systems (particularly
	 * ARM-32); so we rearrange the order in which it's shown depending on the
	 * arch, thus trying to maintain a 'by descending address' ordering.
	 */
#if (BITS_PER_LONG == 64)
	pr_info("|module region:      "
		" %px - %px     | [%9zu MB]\n",
		SHOW_DELTA_MEGA((void *)MODULES_VADDR, (void *)MODULES_END)
    );
#endif

#ifdef CONFIG_KASAN		/* KASAN region: Kernel Address SANitizer */
	pr_info("|KASAN shadow:       "
#if (BITS_PER_LONG == 64)
		" %px - %px     | [%9zu MB = %6zu GB ~= %3zu TB]\n",
		SHOW_DELTA_MEGA_GIGA_TERA((void *)KASAN_SHADOW_START, (void *)KASAN_SHADOW_END)
#else  // 32-bit with KASAN enabled
		" %px - %px                     | [%9zu MB = %6zu GB]\n",
		SHOW_DELTA_MEGA_GIGA((void *)KASAN_SHADOW_START, (void *)KASAN_SHADOW_END)
#endif
	);
#endif

	/* sparsemem vmemmap */
#if defined(CONFIG_SPARSEMEM_VMEMMAP) && defined(CONFIG_ARM64) // || defined(CONFIG_X86))
	pr_info(DOTS
		"|vmemmap region:     "
		" %px - %px     | [%9zu MB = %6zu GB ~= %3zu TB]\n",
		SHOW_DELTA_MEGA_GIGA_TERA((void *)VMEMMAP_START, (void *)VMEMMAP_START + VMEMMAP_SIZE)
    );
#endif
#if defined(CONFIG_X86) && (BITS_PER_LONG == 64)
	// TODO: no size macro for X86_64?
	pr_info(DOTS
		"|vmemmap region start %px                        |\n",
		(void *)VMEMMAP_START);
#endif

	/* vmalloc region */
	pr_info("|vmalloc region:     "
#if (BITS_PER_LONG == 64)
		" %px - %px     | [%9zu MB = %6zu GB ~= %3zu TB]\n",
		SHOW_DELTA_MEGA_GIGA_TERA((void *)VMALLOC_START, (void *)VMALLOC_END)
#else  // 32-bit
		" %px - %px                     | [%5zu MB]\n",
		SHOW_DELTA_MEGA((void *)VMALLOC_START, (void *)VMALLOC_END)
#endif
	);

	/* lowmem region (RAM direct-mapping) */
/*
	pr_debug(" PO=%lx=%lu; PAGE_OFFSET + ram_size = %lx = %lu\n"
		"0xc0000000 + ram_size(%lu=0x%lx) = 0x%lx\n",
		(unsigned long)(PAGE_OFFSET), (unsigned long)(PAGE_OFFSET),
		(unsigned long)(PAGE_OFFSET) + ram_size,
		(unsigned long)(PAGE_OFFSET) + ram_size,
		ram_size, ram_size, 0xc0000000 + ram_size);
*/
	pr_info("|lowmem region:      "
#if (BITS_PER_LONG == 32)
		" %px - %px                     | [%5zu MB]\n"
		"|                     ^^^^^^^^                                |\n"
		"|                    PAGE_OFFSET                              |\n",
#else
		" %px - %px     | [%9zu MB]\n"
		"|                     ^^^^^^^^^^^^^^^^                        |\n"
		"|                        PAGE_OFFSET                          |\n",
#endif
		SHOW_DELTA_MEGA((void *)PAGE_OFFSET, (void *)(PAGE_OFFSET) + ram_size));

	/* (possible) highmem region;  may be present on some 32-bit systems */
#if defined(CONFIG_HIGHMEM) && (BITS_PER_LONG == 32)
	pr_info("|HIGHMEM region:     "
		" %px - %px                     | [%5zu MB]\n",
		SHOW_DELTA_MEGA((void *)PKMAP_BASE, (void *)((PKMAP_BASE) + (LAST_PKMAP * PAGE_SIZE)))
    );
#endif

	/*
	 * Symbols for the kernel image itself:
	 *   text begin/end (_text/_etext)
	 *   init begin/end (__init_begin, __init_end)
	 *   data begin/end (_sdata, _edata)
	 *   bss begin/end (__bss_start, __bss_stop)
	 * are only defined *within* the kernel (in-tree) and aren't available for
	 * modules; thus we don't attempt to print them.
	 */

#if (BITS_PER_LONG == 32)	/* modules region: see the comment above reg this */
	pr_info("|module region:      "
		" %px - %px                     | [%5zu MB]\n",
		SHOW_DELTA_MEGA((void *)MODULES_VADDR, (void *)MODULES_END));
#endif
	pr_info(DOTS);
}


static int __init kernel_vas_init(void) {
    pr_info("%s: kernel_vas is INSERTED\n", KBUILD_MODNAME);
    minsysinfo();
    show_userspace_info();

    if (show_uservas) {
        show_userspace_info();
    } else {
     	pr_info("+-------------------------------------------------------------+\n");
		pr_info("%s: skipping show userspace...\n", KBUILD_MODNAME);
    }
    return 0;
}

static void __exit kernel_vas_exit(void) {
    pr_info("%s: kernel_vas is REMOVEDd\n", KBUILD_MODNAME);
}

module_init(kernel_vas_init);
module_exit(kernel_vas_exit);