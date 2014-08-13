/*
 * Benjamin Roberts
 * u5350335@anu.edu.au
 * COMP3300 Assignment One
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#define PROC_NAME "kerninfo"


struct proc_dir_entry *proc_file;



/*
 * Show function for the seq_file
 */
int proc_single_show(struct seq_file *s, void *v)
{
	seq_printf(s, "Hello World!\n");
	return 0;
}


/*
 * fops function which translates to a seq_file single open.
 * calls proc_single_show
 */
int proc_single_open(struct inode * i, struct file * f)
{
		return single_open(f, proc_single_show, NULL);
}


/*
 * File operations of the proc file.
 * seq_file operatios are reused excluding open.
 */
static const struct file_operations proc_fops = {
	.open = proc_single_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};


/*
 * Initialises the kernel module. Called when loaded
 */
int __init mod_init(void)
{
	//Create new procfile
	printk(KERN_INFO "%s: inserting procfile\n", PROC_NAME);
	proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_fops);

	//cleanup if procfile creation failed
	if(proc_file == NULL) {
		printk(KERN_ERR "%s: could not create procfile\n", PROC_NAME);
		proc_remove(proc_file);
		return -ENOSPC;
	}


	return 0;
}


/*
 * Cleans up the module, Called when removed.
 */
void __exit mod_exit(void)
{
	printk(KERN_INFO "%s: removing procfile\n", PROC_NAME);
  proc_remove(proc_file);
}


/*
 * Define the module entry and exit points
 */
module_init(mod_init);
module_exit(mod_exit);
