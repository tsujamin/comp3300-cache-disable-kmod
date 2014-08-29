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
#include <linux/smp.h>
#include <linux/cpufreq.h>
#include <linux/list.h>

#define PROC_NAME "kerninfo"

struct proc_dir_entry *proc_file;
struct mutex mod_lock;
int cpu_count = 0;

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
 * Doesnt currently switch the MTRR (Intel spec says I should?)
 */
void sysmem_cache_set(void *enable) 
{
	if(enable) {
		asm(	"push %%rax \n\t"
			"movq %%cr0, %%rax \n\t"
			"and $~(1<<30), %%rax \n\t"
			"movq %%rax, %%cr0 \n\t"
			"pop %%rax"
			: : );
	} else {	
		asm(	"push %%rax \n\t"
			"movq %%cr0, %%rax \n\t"
			"or $(1<<30),  %%rax  \n\t"
			"movq %%rax, %%cr0 \n\t"
			"wbinvd \n\t"
			"pop %%rax"
			: : );			
	}
}

/*
 * Calls sysmem_cache_set accross all cores
 */
void smp_sysmem_cache_set(int enable)
{
	printk(KERN_INFO "%s: %s system memory cache", PROC_NAME,
		enable ? "enabling" : "disabling");

	sysmem_cache_set((void *) enable);
	smp_call_function(sysmem_cache_set, (void *) enable, true);
}

/*
 * Print the current sysmem cache status
 */
void sysmem_cache_status_print(struct seq_file *s)
{
	seq_printf(s, "System Memory Caching: %s\n",
		sysmem_cache_enabled() ? "yes" : "no");
}

/*
 * Print governors available on the system (sampled from CPU0)
 */
void cpufreq_print_available_governors(struct seq_file *s)
{
	struct cpufreq_policy freq_policy;
	cpufreq_get_policy(&freq_policy, 0);
	struct cpufreq_governor *head_governor = freq_policy.governor,
				*current_governor;

	seq_printf(s, "Available Governors: ");

	list_for_each_entry(current_governor, &head_governor->governor_list, governor_list)
	{
		if(*(current_governor->name)) 
			seq_printf(s, "%s ", current_governor->name);
		else //Current governor has 0 length string
			seq_printf(s, "%s ", head_governor->name);
	}
	seq_printf(s, "\n");

}

/*
 * Print current governors of each core
 */
void cpufreq_print_current_governors(struct seq_file *s)
{
	struct cpufreq_policy freq_policy;
	int i;	
	seq_printf(s, "CPU\tGovernor\tMin (MHz)\tMax (MHz)\n");
	for(i = 0; i < cpu_count; i++)
	{
		cpufreq_get_policy(&freq_policy, i);
		seq_printf(s, "%d\t%s\t%d\t\t%d\n", 
				i, freq_policy.governor->name,
				freq_policy.min/1000, freq_policy.max/1000);
	}

	seq_printf(s, "\n");
}

/*
 * Prints CPU count
 */
void cpufreq_print_count(struct seq_file *s)
{
	seq_printf(s, "CPUs installed: %d\n", cpu_count);
}

/*
 * Increments the cpu count
 */
void cpufreq_count(void * v)
{
	mutex_lock(&mod_lock);
	cpu_count++;
	mutex_unlock(&mod_lock);
}
/*
 * Show function for the seq_file
 */
int proc_single_show(struct seq_file *s, void *v)
{
	sysmem_cache_status_print(s);	
	cpufreq_print_count(s);
	cpufreq_print_available_governors(s);
	seq_printf(s, "\n");
	cpufreq_print_current_governors(s);
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
	
	smp_sysmem_cache_set(data[0]-'0');

	mutex_unlock(&seq_f->lock);
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
		remove_proc_entry(PROC_NAME, proc_file);
		return -ENOSPC;
	}

	//init the big module lock and count the cpu cores
	mutex_init(&mod_lock);

	cpufreq_count(NULL);
	smp_call_function(cpufreq_count, NULL, true);	

	return 0;
}


/*
 * Cleans up the module, Called when removed.
 */
void __exit mod_exit(void)
{
	smp_sysmem_cache_set(1);

	printk(KERN_INFO "%s: removing procfile\n", PROC_NAME);
	remove_proc_entry(PROC_NAME, proc_file);
}


/*
 * Define the module entry and exit points
 */
module_init(mod_init);
module_exit(mod_exit);

MODULE_AUTHOR("Benjamin Roberts <u5350335@anu.edu.au>");
MODULE_DESCRIPTION("Cool cpu info");
MODULE_LICENSE("GPL");
