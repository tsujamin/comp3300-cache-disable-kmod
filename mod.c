/*
 * Benjamin Roberts
 * u5350335@anu.edu.au
 * COMP3300 Assignment One
 *
 * My proc entry provides information on the CPU's cache state and the
 * currently applied governor settings. The cache state relates to the 
 * the on-die caches (ie L1 -> L3).  The module also provides functionality 
 * to toggle use of the system cache on all cores by writing 1 or 0 to its
 * proc file. This may be a desirable  feature in environments where 
 * shared cache timing attacks could possiblyoccur. The list of 
 * available governors is core specific but in practice is usually applied 
 * homogenously.The proc file is created and managed by the seq_file
 * interface.
 *
 * The governor information is mamanged by the cpufreq module and 
 * associated drivers and is stored in policy structures; each with sub 
 * structures. The governor structure is a sub structure of cpu_freq 
 * policy and uses the kernel's linked list structure and associated
 * functions. The cache status is stored in bit 30 of a cpu cores 
 * CR0 register (as documented in the intel process specification).
 *
 * The module heavily uses the smp_call_function routine to perform
 * actions on multiple availabe cpu cores. Toggling of cpu cache
 * usage is performed on the current core before being called on all 
 * other cores. The change requires flipping a bit of the cr0 register
 * and, if disabling, invalidating of current cache lines. This is 
 * performed in gcc inline assembly blocks combined with architcture
 * specific scratch register/move command macros. Printing of the
 * various cpu policy statistics was accomplished by using the 
 * linux linked list iterator macros. The number of cpu cores was required
 * in advace of this operation and was determinedby scheduling an increment
 * operation surrounded by mutex locks on each core.
 *
 * The module works on the Linux 3.8 image used on the CSIT lab images.
 * Due to personal injury sustained before testing on theng the 3.13 image 
 * used for the VM I was unable to port the code.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/cpufreq.h>
#include <linux/list.h>

#define PROC_NAME "cpustate"

/*
 * Define appropriate scratch registers and mov instructions
 * for 32 and 64 bit systems
 */
#ifdef __x86_64__
#define REG_A "rax"
#define MOV "movq"
#else
#define REG_A "eax"
#define MOV "mov"
#endif

/*
 * Module globals
 */
struct proc_dir_entry *proc_file;
struct mutex mod_lock;
int cpu_count = 0;

/*
 * Checks CD bit of CR0 for current system emmory caching policy
 */
int sysmem_cache_enabled(void)
{
	long cr0;

	asm(MOV" %%cr0, %0" : "=r" (cr0) : );
	
	return !(cr0 & (1 << 30));
}

/*
 * Disables system memory caching by setting cr0:30
 * Doesnt currently switch the MTRR
 */
void sysmem_cache_set(void *enable) 
{
	if(enable) {
		asm(	"push %%"REG_A" \n\t"
			MOV" %%cr0, %%"REG_A" \n\t"
			"and $~(1<<30), %%"REG_A" \n\t"
			MOV" %%"REG_A", %%cr0 \n\t"
			"pop %%"REG_A
			: : );
	} else {	
		asm(	"push %%"REG_A" \n\t"
			MOV" %%cr0, %%"REG_A" \n\t"
			"or $(1<<30),  %%"REG_A"  \n\t"
			MOV" %%"REG_A", %%cr0 \n\t"
			"wbinvd \n\t"
			"pop %%"REG_A
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

	//iterate through the governor list printing their names
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

	//loop through available cpu's and print their policy
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
 * Show function for the seq_file.
 * Prints each of the modules info functions.
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

	//Check that data written is 1 or 0
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
 * Re-enables the system cache.
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
MODULE_DESCRIPTION("CPU state and policy information");
MODULE_LICENSE("GPL");
