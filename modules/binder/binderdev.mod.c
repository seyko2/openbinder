#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

#undef unix
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = __stringify(KBUILD_MODNAME),
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
};

static const struct modversion_info ____versions[]
__attribute_used__
__attribute__((section("__versions"))) = {
	{ 0x2720b98e, "struct_module" },
	{ 0xe228c822, "kmem_cache_destroy" },
	{ 0x6b13b91c, "cdev_del" },
	{ 0x77414029, "__kmalloc" },
	{ 0xa08e8379, "cdev_init" },
	{ 0xf9a482f9, "msleep" },
	{ 0x7ac3e50f, "del_timer" },
	{ 0xe4b54e9, "page_address" },
	{ 0x4e45624a, "autoremove_wake_function" },
	{ 0x3de88ff0, "malloc_sizes" },
	{ 0x596b1290, "class_simple_create" },
	{ 0x9d1cc6ae, "boot_cpu_data" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x37e74642, "get_jiffies_64" },
	{ 0x7d11c268, "jiffies" },
	{ 0xd7474566, "__copy_to_user_ll" },
	{ 0xf6c3815e, "rb_first" },
	{ 0xe1ce994a, "__alloc_pages" },
	{ 0x1b7d4074, "printk" },
	{ 0x7ee8c5cb, "rb_erase" },
	{ 0x2da418b5, "copy_to_user" },
	{ 0xe2f1e52c, "class_simple_device_remove" },
	{ 0x71e4c6da, "kmem_cache_free" },
	{ 0x625acc81, "__down_failed_interruptible" },
	{ 0x51c47591, "mod_timer" },
	{ 0xba3beea5, "contig_page_data" },
	{ 0x62766b58, "cdev_add" },
	{ 0x2642591c, "kmem_cache_alloc" },
	{ 0xba8b14e3, "rb_prev" },
	{ 0x4292364c, "schedule" },
	{ 0x17d59d01, "schedule_timeout" },
	{ 0x6b2dc060, "dump_stack" },
	{ 0xb1a8e7f2, "class_simple_destroy" },
	{ 0x7ec51f67, "rb_insert_color" },
	{ 0x396594c4, "kmem_cache_create" },
	{ 0xbce89f77, "__wake_up" },
	{ 0x37a0cba, "kfree" },
	{ 0x2e60bace, "memcpy" },
	{ 0x98d23e12, "prepare_to_wait" },
	{ 0x98e4e879, "finish_wait" },
	{ 0x44ba0f42, "rb_next" },
	{ 0x60a4461c, "__up_wakeup" },
	{ 0x96b27088, "__down_failed" },
	{ 0x92f67838, "rb_last" },
	{ 0xf2a644fb, "copy_from_user" },
	{ 0x6b6a9f5f, "class_simple_device_add" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0x760a0f4f, "yield" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "A7F3022F93E254E439D6A40");
