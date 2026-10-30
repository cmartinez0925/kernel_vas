// #define pr_fmt(fmt) "%s::%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/init.h>
#include <linux/kasan.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/vmalloc.h>
#include <asm/fixmap.h>
#include <asm/pgtable.h>

#ifdef CONFIG_X86_64
    #include <asm/page_types.h>
    #include <asm/pgtable_64_types.h>
#endif

#ifdef CONFIG_ARM64
    #include <asm/memory.h>
#endif

MODULE_AUTHOR("Chris Martinez");
MODULE_DESCRIPTION("Display the kernel's virtual address space (x86-64)");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("0.1");

void display_kernel_addr_space(void) {
    char mod_border[] = "=====================================================";
    unsigned long ram_size;
    

    // Print top border
    pr_info("%s\n", mod_border);

    // Calculate the Total RAM on the system
    ram_size = totalram_pages() * PAGE_SIZE;
    pr_info("RAM INFO:\n");
    pr_info("Page Size = %lu, Total RAM = %lu bytes\n" , PAGE_SIZE, ram_size);

    // Calculate Vector Table region
    #ifdef CONFIG_ARM
        pr_info("\n");
        pr_info("VECTOR TABLE REGION:\n");
        pr_info("An array of function pointers that ARM-32 systems will use\n");
        pr_info("to initialize its vectors such that when a processor\n");
        pr_info("exeception or mode change occurs, the processor know what\n");
        pr_info("code to run:\n");
        pr_info("Start: %px    End: %px    Size: %lu\n", 
                (void*)VECTORS_BASE, 
                (void*)(VECTORS_BASE + PAGE_SIZE), 
                PAGE_SIZE);
    #endif

    // Calculate fixmap region
    pr_info("\n");
    pr_info("FIX MAP REGION:\n");
    pr_info("The fixmap region is a range of compile-time special or\n");
    pr_info("or reserved virtual addresses:\n");
    pr_info("Start: %px    End: %px    Size: %d\n", 
            (void*)FIXADDR_START, 
            (void*)FIXADDR_TOP, 
            FIXADDR_SIZE);
  
    // Calculate Kernal Modules region
    pr_info("\n");
    pr_info("KERNEL MODULES REGION:\n");
    pr_info("Allocated memory at load time for their static text and data\n");
    pr_info("Start: %px    End: %px    Size: %lu\n",
            (void*)MODULES_VADDR,
            (void*)MODULES_END,
            MODULES_END - MODULES_VADDR);

    // Calculate KASAN region
    #ifdef CONFIG_KASAN
        pr_info("\n");
        pr_info("KASAN REGION:\n");
        pr_info("The modern kernel employs a powerful mechanism to detect\n");
        pr_info("and report memory issues (bugs). It requires a rather\n");
        pr_info("large shadow memory region which is the KASAN region.\n");
        pr_info("Start: %px    End: %px    Size: %lu\n",
                (void*)KASAN_SHADOW_START,
                (void*)KASAN_SHADOW_END,
                (KASAN_SHADOW_END - KASAN_SHADOW_START));
    #endif

    // Calculate vmemmap region
    pr_info("\n");
    pr_info("Vmemmap REGION:\n");
    pr_info("A region within the kernal VAS that is employed when the\n");
    pr_info("physical memory is sparesemem and when the method employed to\n");
    pr_info("map a Page Frame Number (PFN) to its corresponding virtual\n");
    pr_info("page is the vmemmap one:\n");
    pr_info("Start: %px\n", (void*)VMEMMAP_START);   

    // Calculate vmalloc region
    pr_info("\n");
    pr_info("VMALLOC REGION:\n");
    pr_info("The kernel region from where memory for the vmalloc() APIs\n");
    pr_info("are allocated\n");
    pr_info("Start: %px    End: %px    Size: %lu\n",
            (void*)VMALLOC_START,
            (void*)VMALLOC_END,
            VMALLOC_TOTAL);

    // Calculate Lowmem Region
    pr_info("\n");
    pr_info("LOWMEM REGION:\n");
    pr_info("Direct-mapped RAM. It's the region where the linux kernel maps\n");
    pr_info("and manages (typically) all RAM\n");
    pr_info("Start: %px    End: %px    Size: %lu\n",
            (void*)PAGE_OFFSET,
            (void*)high_memory,
            ((unsigned long)high_memory) - PAGE_OFFSET);    

    // Print bottom border
    pr_info("%s\n", mod_border);
}


static int __init kernel_vas_init(void) {
    pr_info("%s --> INSERTED\n", KBUILD_MODNAME);
    display_kernel_addr_space();
    return 0;
}

static void __exit kernel_vas_exit(void) {
    pr_info("%s --> REMOVED\n", KBUILD_MODNAME);
}

module_init(kernel_vas_init);
module_exit(kernel_vas_exit);
