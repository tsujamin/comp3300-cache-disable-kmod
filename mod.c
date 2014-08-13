/*
 * Benjamin Roberts
 * u5350335@anu.edu.au
 * COMP3300 Assignment One
 */

#include <linux/init.h>
#include <linux/fs.h>

static int __init mod_init(void)
{
	printk("Hello World!\n");
	return 0;
}
static void __exit mod_exit(void)
{
  printk("Goodbye World!\n");
}


module_init(mod_init);
module_exit(mod_exit);
