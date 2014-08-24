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
 * Checks CD bit of CR0 for current system emmory caching policy
 */
int sysmem_cache_enabled(void)
{
	long cr0;

	asm("movq %%cr0, %0" : "=r" (cr0) : );

	return !(cr0 & (1 << 30));
}

/*
 * Disables system memory caching
 * Doesnt currently switch the MTRR
 */
void sysmem_cache_set(int enable) 
{
	uint64_t flag = 1 << 30;
	
	printk(KERN_INFO "%s: %s system memory cache", PROC_NAME,
		enable ? "enabling" : "disabling");

	if(enable) {
		asm(	"push %%rax \n\t"
			"movq %%cr0, %%rax \n\t"
			"or %%rax, %0 \n\t"
			"movq %%rax, %%cr0 \n\t"
			"pop %%rax"
			: : "r" (flag) );
	} else {	
		asm(	"push %%rax \n\t"
			"movq %%cr0, %%rax \n\t"
			"and %%rax, %0 \n\t"
			"movq %%rax, %%cr0 \n\t"
			"pop %%rax \n\t"
			"wbinvd"
			: : "r" (~flag) );
	}
}

/*
 * Show function for the seq_file
 */
int proc_single_show(struct seq_file *s, void *v)
{
	seq_printf(s, "Hello World!\n");
	seq_printf(s, "System Memory Caching: %s\n",
		sysmem_cache_enabled() ? "yes" : "no");
	return 0;
}

/*
 * Write function for the seq_file.
 * Will set system caching based on the input (either 1 or 0)
 */
static ssize_t proc_write(struct file *f, const char *data, size_t len, loff_t *offset)
{
	struct seq_file *seq_f = f->private_data;
	mutex_lock(&seq_f->lock);

	if(len != 2 || (data[0] != '1' && data[0] != '0')) {
		printk(KERN_WARNING "%s: invalid write value", PROC_NAME);
		return -EINVAL;
	}

	sysmem_cache_set(data[0] - '0');

	return len;
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
	.release = single_release,
	.write = proc_write
};

/*
 * Initialises the kernel module. Called when loaded
 */
int __init mod_init(void)
{
	//Create new procfile
	printk(KERN_INFO "%s: inserting procfile\n", PROC_NAME);
	proc_file = proc_create(PROC_NAME, 0666, NULL, &proc_fops);

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
	sysmem_cache_set(1);

	printk(KERN_INFO "%s: removing procfile\n", PROC_NAME);
	proc_remove(proc_file);
}


/*
 * Define the module entry and exit points
 */
module_init(mod_init);
module_exit(mod_exit);
