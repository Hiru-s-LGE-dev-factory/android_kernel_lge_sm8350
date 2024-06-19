#include "operating_system.h"
#include "application.h"
#include "pattern_manager.h"

#define PATTERN_DATA_SIZE	(128 * PAGE_SIZE)
static char pattern_data[PATTERN_DATA_SIZE];

static ssize_t pattern_data_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	int ret;

	if (*ppos != 0)
		return 0;

	mutex_lock(&pattern_lock);

	memset(pattern_data, 0x0, PATTERN_DATA_SIZE);

	ret = print_cluster_pattern_to_buf(pattern_data, PATTERN_DATA_SIZE);

	if (copy_to_user(buff, pattern_data, ret)) {
		mutex_unlock(&pattern_lock);
		return -EINVAL;
	}

	mutex_unlock(&pattern_lock);

	*ppos += ret;

	return ret;
}

static ssize_t pattern_data_write(struct file *file, const char __user *buff,
		size_t count, loff_t *ppos)
{
	int ret;

	if (*ppos != 0)
		return 0;

	if (PATTERN_DATA_SIZE < count)
		return -EINVAL;

	mutex_lock(&pattern_lock);

	if (application_manager_thread_start != 0) {
		mutex_unlock(&pattern_lock);
		return -EINVAL;
	}

	memset(pattern_data, 0x0, PATTERN_DATA_SIZE);

	if (copy_from_user(pattern_data, buff, count)) {
		mutex_unlock(&pattern_lock);
		return -EINVAL;
	}

	ret = restore_cluster_pattern_from_buf(pattern_data, count);
	if (ret == -1) {
		pr_info("Restore Fail!!! Reset Application Manager!!!\n");

		clear_application_manager();
		initialize_application_manager();

		application_manager_thread_start = 1;
	}

	mutex_unlock(&pattern_lock);

	*ppos += count;

	return count;
}

static const struct file_operations pattern_data_fops = {
	.read = pattern_data_read,
	.write = pattern_data_write,
};

int initialize_pattern_manager(void)
{
	if (!proc_appboost)
		return -1;

	if (!proc_create("pattern_data", 0600, proc_appboost, &pattern_data_fops))
		return -1;

	mutex_init(&pattern_lock);

	return 0;
}