#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/rwsem.h>
#include <linux/hugetlb.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/pagewalk.h>

#include <asm/tlbflush.h>

#include "application_prelaunch.h"

struct proc_dir_entry *proc_appboost;

static int prefault_pte_range(pmd_t *pmd, unsigned long addr,
		unsigned long end, struct mm_walk *walk)
{
	pte_t *orig_pte, *pte, ptent;
	struct page *page;
	struct vm_area_struct *vma = walk->vma;
	unsigned int mm_flags = FAULT_FLAG_REMOTE;

	orig_pte = pte_offset_map(pmd, addr);
	for (pte = orig_pte; addr < end; pte++, addr += PAGE_SIZE) {
		ptent = *pte;

		if (pte_present(ptent)) {
			page = vm_normal_page(vma, addr, ptent);
			if (page)
				mark_page_accessed(page);
			continue;
		}
		if (pte_none(ptent))
			continue;

		handle_mm_fault(vma, addr & PAGE_MASK, mm_flags);
	}

	cond_resched();
	return 0;
}

int prefault_task(struct task_struct *tsk)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct mm_walk reclaim_walk = {};
	struct mm_walk_ops reclaim_walk_ops = {};

	mm = get_task_mm(tsk);
	if (!mm)
		return -1;

	if (!down_read_trylock(&mm->mmap_sem)) {
		mmput(mm);
		return -1;
	}

	reclaim_walk.mm = mm;
	reclaim_walk_ops.pmd_entry = prefault_pte_range;

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (is_vm_hugetlb_page(vma))
			continue;

		if (vma->vm_flags & VM_LOCKED)
			continue;

		reclaim_walk.private = vma;
		walk_page_range(mm, vma->vm_start, vma->vm_end, &reclaim_walk_ops, vma);
	}

	flush_tlb_mm(mm);
	up_read(&mm->mmap_sem);
	mmput(mm);

	return 0;
}

static struct prelaunch_trigger *t;
static void *namebuf;
//static int start_idx, next_idx;

static unsigned int prelaunch_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = DEFAULT_POLLMASK;

	poll_wait(file, &t->event_wait, wait);

	if (cmpxchg(&t->event, 1, 0) == 1)
		ret |= POLLPRI;

	return ret;
}

static ssize_t prelaunch_data_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	int ret = 0;

	if (*ppos != 0)
		return 0;

	mutex_lock(&t->lock);
	// include NULL
	ret = strlen(namebuf) + 1;

	if (copy_to_user(buff, namebuf, ret)) {
		mutex_unlock(&t->lock);
		return -EINVAL;
	}

	mutex_unlock(&t->lock);

	*ppos += ret;

	return ret;
}

static const struct file_operations prelaunch_fops = {
	//.open = prelaunch_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.poll = prelaunch_poll,
	//.release = prelaunch_release,
};

static const struct file_operations prelaunch_data_fops = {
	.read = prelaunch_data_read,
};

static ssize_t prelaunch_test_write(struct file *file, const char __user *buff,
		size_t count, loff_t *ppos)
{
	char buffer[1024];

	memset(buffer, 0, sizeof(buffer));

	if (copy_from_user(buffer, buff, count - 1))
		return -EFAULT;

	printk(KERN_ERR "[PRELAUNCH] buffer: %s / count: %d\n", buffer, count);

	mutex_lock(&t->lock);
	memset(namebuf, 0, BUF_SIZE);
	// add target process name to copy
	// include NULL
	strncpy(namebuf, buffer, count);

	mutex_unlock(&t->lock);

	if (cmpxchg(&t->event, 0, 1) == 0)
		wake_up_interruptible(&t->event_wait);

	return count;
}

static const struct file_operations prelaunch_test_fops = {
	.write = prelaunch_test_write,
};

#ifdef CONFIG_LGE_SREADAHEAD
extern char prelaunch_application_cmdline[256];
#endif

int prelaunch_task(char *comm)
{
	int ret = 0;
	mutex_lock(&t->lock);
	memset(namebuf, 0, BUF_SIZE);
	// include NULL
	ret = strlen(comm) + 1;
	strncpy(namebuf, comm, ret);
#ifdef CONFIG_LGE_SREADAHEAD
	strncpy(prelaunch_application_cmdline, comm, ret);
#endif
	mutex_unlock(&t->lock);

	if (cmpxchg(&t->event, 0, 1) == 0)
		wake_up_interruptible(&t->event_wait);

	return 0;
}

int prelaunch_init(void)
{
	proc_appboost = proc_mkdir("appboost", NULL);
	if (!proc_appboost)
		return -1;

	if (!proc_create("prelaunch_poll", 0444, proc_appboost, &prelaunch_fops))
		goto err;

	if (!proc_create("prelaunch_data", 0444, proc_appboost, &prelaunch_data_fops))
		goto err;

	if (!proc_create("prelaunch_test", 0600, proc_appboost, &prelaunch_test_fops))
		goto err;

	t = kmalloc(sizeof(*t), GFP_KERNEL);
	if (!t)
		return -ENOMEM;

	t->event = 0;
	init_waitqueue_head(&t->event_wait);
	mutex_init(&t->lock);

	//namebuf = vzalloc(BUF_SIZE);
	namebuf = kmalloc(sizeof(BUF_SIZE), GFP_KERNEL);
	memset(namebuf, 0x0, BUF_SIZE);

	return 0;
err:
	remove_proc_subtree("appboost", NULL);
	return -1;
}
