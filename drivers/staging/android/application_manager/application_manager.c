#include "operating_system.h"
#include "application.h"
#include "application_cluster_pattern.h"
#include "ordered_application_cluster_pattern.h"
#include "prelaunch_application_cluster_pattern.h"

struct executed_application_list executed_app_list;
struct application_list app_list;

struct application_manager cluster_pattern_app_manager;
struct application_manager ordered_cluster_pattern_app_manager;
struct application_manager prelaunch_cluster_pattern_app_manager;

void initialize_executed_app_list(void)
{
	executed_app_list.start_index = 0;
	executed_app_list.end_index = 0;
}

void print_executed_app_list(void)
{
	int i;
	struct application* app;

	i = executed_app_list.start_index;

	while (i != executed_app_list.end_index) {
		app = &executed_app_list.applications[i];
#ifdef WINDOWS_OPERATING_SYSTEM
		printf("executed_app_list[%d] cmdline = %s\n", i, app->cmdline);
#endif
#ifdef LINUX_OPERATING_SYSTEM
		pr_info("executed_app_list[%d] cmdline = %s\n", i, app->cmdline);
#endif
		if (i == 0)
			i = MAX_APP_LIST_NUM - 1;
		else
			--i;
	}
}

void print_app_list(void)
{
	int i;
	struct application* app;

	for (i = 0; i < MAX_APP_LIST_NUM; ++i) {
		app = &app_list.applications[i];
#ifdef WINDOWS_OPERATING_SYSTEM
		printf("tgid = %d, cmdline = %s, order = %u, prev = %d, next = %d, repeat_index = %d\n",
			app->tgid, app->cmdline, app->order, app->prev, app->next, app->repeat_index);
#endif
#ifdef LINUX_OPERATING_SYSTEM
		pr_info("tgid = %d, cmdline = %s, order = %u, prev = %d, next = %d, repeat_index = %d\n",
			app->tgid, app->cmdline, app->order, app->prev, app->next, app->repeat_index);
#endif
	}
}

void print_cluster_pattern_app_manager(void)
{
	struct application_list* app_list;
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	struct application* app;
	struct application_list* unused_app_list;
	unsigned int i;
	unsigned int j;

	app_list = cluster_pattern_app_manager.app_list;
	app_usage_pattern = cluster_pattern_app_manager.app_usage_pattern;

#ifdef WINDOWS_OPERATING_SYSTEM
	printf("\n============================== START ==============================\n");
#endif
#ifdef LINUX_OPERATING_SYSTEM
	pr_info("\n============================== START ==============================\n");
#endif

	for (i = 0; i < MAX_PATTERN_NUM; ++i) {
		pattern_app_list = &app_usage_pattern->patterns[i];

		for (j = 0; j < pattern_app_list->pattern_application_number; ++j) {
			app = &pattern_app_list->applications[j];

			if (cluster_pattern_app_manager.current_pattern_index == i) {
#ifdef WINDOWS_OPERATING_SYSTEM
				printf("[selected pattern_app_list][%d] tgid = %d, cmdline = %s, tasksize = %u, repeat_index = %d\n",
					i, app->tgid, app->cmdline, app->tasksize, app->repeat_index);
#endif
#ifdef LINUX_OPERATING_SYSTEM
				pr_info("[selected pattern_app_list][%d] tgid = %d, cmdline = %s, tasksize = %u, repeat_index = %d\n",
					i, app->tgid, app->cmdline, app->tasksize, app->repeat_index);
#endif
			} else {
#ifdef WINDOWS_OPERATING_SYSTEM
				printf("[pattern_app_list][%d] tgid = %d, cmdline = %s, tasksize = %u, repeat_index = %d\n",
					i, app->tgid, app->cmdline, app->tasksize, app->repeat_index);
#endif
#ifdef LINUX_OPERATING_SYSTEM
				pr_info("[pattern_app_list][%d] tgid = %d, cmdline = %s, tasksize = %u, repeat_index = %d\n",
					i, app->tgid, app->cmdline, app->tasksize, app->repeat_index);
#endif
			}
		}
	}

	unused_app_list = &cluster_pattern_app_manager.unused_app_list;
	for (i = 0; i < unused_app_list->application_number; ++i) {
		app = &unused_app_list->applications[i];
#ifdef WINDOWS_OPERATING_SYSTEM
		printf("[unused_app_list][%d] cmdline = %s, tasksize = %u, repeat_index = %d\n",
			i, app->cmdline, app->tasksize, app->repeat_index);
#endif
#ifdef LINUX_OPERATING_SYSTEM
		pr_info("[unused_app_list][%d] cmdline = %s, tasksize = %u, repeat_index = %d\n",
			i, app->cmdline, app->tasksize, app->repeat_index);
#endif
	}

#ifdef WINDOWS_OPERATING_SYSTEM
	printf("\n============================== END ==============================\n");
#endif
#ifdef LINUX_OPERATING_SYSTEM
	pr_info("\n============================== END ==============================\n");
#endif
}

void print_ordered_cluster_pattern_app_manager(void)
{
	struct application_list* app_list;
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	struct application* app;
	unsigned int i;
	unsigned int j;

	app_list = ordered_cluster_pattern_app_manager.app_list;
	app_usage_pattern = ordered_cluster_pattern_app_manager.app_usage_pattern;

#ifdef WINDOWS_OPERATING_SYSTEM
	printf("\n============================== START ==============================\n");
#endif
#ifdef LINUX_OPERATING_SYSTEM
	pr_info("\n============================== START ==============================\n");
#endif

	for (i = 0; i < MAX_PATTERN_NUM; ++i) {
		pattern_app_list = &app_usage_pattern->patterns[i];
#ifdef WINDOWS_OPERATING_SYSTEM
		if (ordered_cluster_pattern_app_manager.current_pattern_index == i) {
			if (pattern_app_list->cur_app_index == 0)
				printf("[selected ordered_pattern_app_list] cur_app = %s is last app\n",
					pattern_app_list->applications[pattern_app_list->cur_app_index].cmdline);
			else
				printf("[selected ordered_pattern_app_list] cur_app = %s, next_app = %s\n",
					pattern_app_list->applications[pattern_app_list->cur_app_index].cmdline,
					pattern_app_list->applications[pattern_app_list->cur_app_index - 1].cmdline);
		}
#endif
#ifdef LINUX_OPERATING_SYSTEM
		if (ordered_cluster_pattern_app_manager.current_pattern_index == i) {
			if (pattern_app_list->cur_app_index == 0)
				pr_info("[selected ordered_pattern_app_list] cur_app = %s is last app\n",
					pattern_app_list->applications[pattern_app_list->cur_app_index].cmdline);
			else
				pr_info("[selected ordered_pattern_app_list] cur_app = %s, next_app = %s\n",
					pattern_app_list->applications[pattern_app_list->cur_app_index].cmdline,
					pattern_app_list->applications[pattern_app_list->cur_app_index - 1].cmdline);
		}
#endif

		for (j = 0; j < pattern_app_list->pattern_application_number; ++j) {
			app = &pattern_app_list->applications[j];
			if (ordered_cluster_pattern_app_manager.current_pattern_index == i) {
#ifdef WINDOWS_OPERATING_SYSTEM
				printf("[selected ordered_pattern_app_list][%d] tgid = %d, cmdline = %s, tasksize = %u, repeat_index = %d\n",
					i, app->tgid, app->cmdline, app->tasksize, app->repeat_index);
#endif
#ifdef LINUX_OPERATING_SYSTEM
				pr_info("[selected ordered_pattern_app_list][%d] tgid = %d, cmdline = %s, tasksize = %u, repeat_index = %d\n",
					i, app->tgid, app->cmdline, app->tasksize, app->repeat_index);
#endif
			}
			else {
#ifdef WINDOWS_OPERATING_SYSTEM
				printf("[ordered_pattern_app_list][%d] tgid = %d, cmdline = %s, tasksize = %u, repeat_index = %d\n",
					i, app->tgid, app->cmdline, app->tasksize, app->repeat_index);
#endif
#ifdef LINUX_OPERATING_SYSTEM
				pr_info("[ordered_pattern_app_list][%d] tgid = %d, cmdline = %s, tasksize = %u, repeat_index = %d\n",
					i, app->tgid, app->cmdline, app->tasksize, app->repeat_index);
#endif
			}
		}
	}

#ifdef WINDOWS_OPERATING_SYSTEM
	printf("\n============================== END ==============================\n");
#endif
#ifdef LINUX_OPERATING_SYSTEM
	pr_info("\n============================== END ==============================\n");
#endif
}

#define MAX_APP_DATA_BUF_SIZE	512

int print_cluster_pattern_to_buf(char* buf, unsigned int max_buf_size)
{
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	struct application* app;
	unsigned int i;
	unsigned int j;
	int len;

	app_usage_pattern = cluster_pattern_app_manager.app_usage_pattern;
	len = 0;

	for (i = 0; i < MAX_PATTERN_NUM; ++i) {
		pattern_app_list = &app_usage_pattern->patterns[i];

		if (pattern_app_list->pattern_application_number == 0)
			continue;

		if (max_buf_size < len + MAX_APP_DATA_BUF_SIZE)
			return -1;

		len += sprintf(buf + len, "C[%u %u %d]\n", i,
			pattern_app_list->pattern_application_number,
			pattern_app_list->hit_number);

		for (j = 0; j < pattern_app_list->pattern_application_number; ++j) {
			app = &pattern_app_list->applications[j];

			if (max_buf_size < len + MAX_APP_DATA_BUF_SIZE)
				return -1;

			len += sprintf(buf + len, "[%s %s %d %d]\n",
				app->cmdline, app->comm, app->tasksize, app->repeat_index);
		}
	}

	app_usage_pattern = ordered_cluster_pattern_app_manager.app_usage_pattern;

	for (i = 0; i < MAX_PATTERN_NUM; ++i) {
		pattern_app_list = &app_usage_pattern->patterns[i];

		if (pattern_app_list->pattern_application_number == 0)
			continue;

		if (max_buf_size < len + MAX_APP_DATA_BUF_SIZE)
			return -1;

		len += sprintf(buf + len, "O[%u %u %d]\n", i,
			pattern_app_list->pattern_application_number,
			pattern_app_list->hit_number);

		for (j = 0; j < pattern_app_list->pattern_application_number; ++j) {
			app = &pattern_app_list->applications[j];

			if (max_buf_size < len + MAX_APP_DATA_BUF_SIZE)
				return -1;

			len += sprintf(buf + len, "[%s %s %d %d]\n",
				app->cmdline, app->comm, app->tasksize, app->repeat_index);
		}
	}

	return len;
}

int restore_cluster_pattern_from_buf(char* pattern_data,
	unsigned int pattern_data_size)
{
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	struct application* app;

	unsigned int pattern_index;
	unsigned int pattern_app_number;
	int hit_number;

	char* cmdline;
	char* comm;

	int i;
	int j;
	int k;

	i = 0;

	while (i < pattern_data_size) {
		while (i < pattern_data_size &&
			pattern_data[i] != 'C' && pattern_data[i] != 'O')
			++i;

		if (!(i + 1 < pattern_data_size))
			return pattern_data_size;

		if (!strncmp(&pattern_data[i], "C[", 2))
			app_usage_pattern = cluster_pattern_app_manager.app_usage_pattern;
		else if (!strncmp(&pattern_data[i], "O[", 2))
			app_usage_pattern = ordered_cluster_pattern_app_manager.app_usage_pattern;

		i += 2;

		pattern_index = 0;
		while (i < pattern_data_size &&
			'0' <= pattern_data[i] && pattern_data[i] <= '9') {
			pattern_index *= 10;
			pattern_index += pattern_data[i] - '0';
			++i;
		}

		if (MAX_PATTERN_NUM < pattern_index)
			return -1;

		while (i < pattern_data_size &&
			!('0' <= pattern_data[i] && pattern_data[i] <= '9'))
			++i;

		pattern_app_number = 0;
		while (i < pattern_data_size &&
			'0' <= pattern_data[i] && pattern_data[i] <= '9') {
			pattern_app_number *= 10;
			pattern_app_number += pattern_data[i] - '0';
			++i;
		}

		if (MAX_PATTERN_APP_NUM < pattern_app_number)
			return -1;

		while (i < pattern_data_size &&
			!('0' <= pattern_data[i] && pattern_data[i] <= '9'))
			++i;

		hit_number = 0;
		while (i < pattern_data_size &&
			'0' <= pattern_data[i] && pattern_data[i] <= '9') {
			hit_number *= 10;
			hit_number += pattern_data[i] - '0';
			++i;
		}

		while (i < pattern_data_size && pattern_data[i] != ']')
			++i;

		if (i + 1 < pattern_data_size)
			++i;
		else
			return -1;

		pattern_app_list = &app_usage_pattern->patterns[pattern_index];
		pattern_app_list->pattern_application_number = pattern_app_number;
		pattern_app_list->hit_number = hit_number;

		for (j = 0; j < pattern_app_list->pattern_application_number; ++j) {
			app = &pattern_app_list->applications[j];
			cmdline = app->cmdline;
			comm = app->comm;

			while (i < pattern_data_size && pattern_data[i] != '[')
				++i;

			if (i + 1 < pattern_data_size)
				++i;
			else
				return -1;

			k = 0;
			while (i < pattern_data_size && k < MAX_CMDLINE_LEN - 1
				&& pattern_data[i] != ' ') {
				cmdline[k] = pattern_data[i];
				++k;
				++i;
			}
			cmdline[k] = 0;

			while (i < pattern_data_size && pattern_data[i] == ' ')
				++i;

			k = 0;
			while (i < pattern_data_size && k < TASK_COMM_LEN - 1 &&
				pattern_data[i] != ' ') {
				comm[k] = pattern_data[i];
				++k;
				++i;
			}
			comm[k] = 0;

			while (i < pattern_data_size && pattern_data[i] == ' ')
				++i;

			app->tasksize = 0;
			while(i < pattern_data_size &&
				'0' <= pattern_data[i] && pattern_data[i] <= '9') {
				app->tasksize *= 10;
				app->tasksize += pattern_data[i] - '0';
				++i;
			}

			while (i < pattern_data_size && pattern_data[i] == ' ')
				++i;

			if (pattern_data[i] == '-') {
				app->repeat_index = -1;
				i += 2;
			} else {
				app->repeat_index = 0;
				while(i < pattern_data_size &&
					'0' <= pattern_data[i] && pattern_data[i] <= '9') {
					app->repeat_index *= 10;
					app->repeat_index += pattern_data[i] - '0';
					++i;
				}
			}

			while (i < pattern_data_size && pattern_data[i] != ']')
				++i;

			if (i + 1 < pattern_data_size)
				++i;
			else
				return -1;
		}
	}

	return -1;
}

static inline void copy_app_list(struct application* dst_app, struct application* src_app)
{
	memcpy(dst_app, src_app, sizeof(struct application));
}

static void copy_executed_app_list_to_app_list(void)
{
	int i;
	int j;
	struct application* executed_app;
	struct application* app;

	i = executed_app_list.start_index;
	j = 0;

	while (i != executed_app_list.end_index) {
		executed_app = &executed_app_list.applications[i];
		app = &app_list.applications[j];

		copy_app_list(app, executed_app);
		app->order = j;
		app->prev = -1;
		app->next = -1;
		app->repeat_index = -1;

		if (i == 0)
			i = MAX_APP_LIST_NUM - 1;
		else
			--i;

		++j;
	}

	app_list.application_number = j;
}

static void build_app_execution_chain(void)
{
	unsigned int i;
	unsigned int j;
	struct application* app;
	struct application* prev_app;

	if (app_list.application_number == 0)
		return;

	for (i = 0; i < app_list.application_number - 1; ++i) {
		app = &app_list.applications[i];

		for (j = i + 1; j < app_list.application_number; ++j) {
			prev_app = &app_list.applications[j];

			if (!strcmp(app->cmdline, prev_app->cmdline)) {
				app->prev = j;
				prev_app->next = i;
				break;
			}
		}
	}
}

static void manage_cluster_pattern_applications(void)
{
	int pattern_score;

	pattern_score = cluster_pattern_app_manager.match_application_usage_pattern(&cluster_pattern_app_manager);

	if (pattern_score == 0)
		cluster_pattern_app_manager.extract_application_usage_pattern(&cluster_pattern_app_manager);

	cluster_pattern_app_manager.manage_applications(&cluster_pattern_app_manager);

	//print_cluster_pattern_app_manager();
}

static void manage_ordered_cluster_pattern_applications(void)
{
	int pattern_score;

	pattern_score = ordered_cluster_pattern_app_manager.match_application_usage_pattern(&ordered_cluster_pattern_app_manager);

	if (pattern_score == 0)
		ordered_cluster_pattern_app_manager.extract_application_usage_pattern(&ordered_cluster_pattern_app_manager);

	ordered_cluster_pattern_app_manager.manage_applications(&ordered_cluster_pattern_app_manager);

	//print_ordered_cluster_pattern_app_manager();
}

static void manage_prelaunch_cluster_pattern_applications(void)
{

}

void manage_applications(void)
{
	copy_executed_app_list_to_app_list();
	build_app_execution_chain();

	manage_cluster_pattern_applications();
	manage_ordered_cluster_pattern_applications();
	manage_prelaunch_cluster_pattern_applications();
}

static void initialize_cluster_pattern_app_manager(void)
{
	cluster_pattern_app_manager.app_list = &app_list;
#ifdef WINDOWS_OPERATING_SYSTEM
	cluster_pattern_app_manager.app_usage_pattern =
		malloc(sizeof(struct application_usage_pattern));
#endif
#ifdef LINUX_OPERATING_SYSTEM
	cluster_pattern_app_manager.app_usage_pattern =
		kmalloc(sizeof(struct application_usage_pattern), GFP_KERNEL);
#endif
	memset(cluster_pattern_app_manager.app_usage_pattern, 0x0,
		sizeof(struct application_usage_pattern));

	cluster_pattern_app_manager.current_pattern_index = -1;
	cluster_pattern_app_manager.prev_pattern_index = -1;

	cluster_pattern_app_manager.extract_application_usage_pattern =
		extract_cluster_usage_pattern;

	cluster_pattern_app_manager.match_application_usage_pattern =
		match_cluster_usage_pattern;

	cluster_pattern_app_manager.manage_applications =
		manage_cluster_applications;
}

static void initialize_ordered_cluster_pattern_app_manager(void)
{
	ordered_cluster_pattern_app_manager.app_list = &app_list;
#ifdef WINDOWS_OPERATING_SYSTEM
	ordered_cluster_pattern_app_manager.app_usage_pattern =
		malloc(sizeof(struct application_usage_pattern));
#endif
#ifdef LINUX_OPERATING_SYSTEM
	ordered_cluster_pattern_app_manager.app_usage_pattern =
		kmalloc(sizeof(struct application_usage_pattern), GFP_KERNEL);
#endif
	memset(ordered_cluster_pattern_app_manager.app_usage_pattern, 0x0,
		sizeof(struct application_usage_pattern));

	ordered_cluster_pattern_app_manager.current_pattern_index = -1;
	ordered_cluster_pattern_app_manager.prev_pattern_index = -1;

	ordered_cluster_pattern_app_manager.extract_application_usage_pattern =
		extract_ordered_cluster_usage_pattern;

	ordered_cluster_pattern_app_manager.match_application_usage_pattern =
		match_ordered_cluster_usage_pattern;

	ordered_cluster_pattern_app_manager.manage_applications =
		manage_ordered_cluster_applications;
}

static void initialize_prelaunch_cluster_pattern_app_manager(void)
{
	prelaunch_cluster_pattern_app_manager.app_list = &app_list;
#ifdef WINDOWS_OPERATING_SYSTEM
	prelaunch_cluster_pattern_app_manager.app_usage_pattern =
		malloc(sizeof(struct application_usage_pattern));
#endif
#ifdef LINUX_OPERATING_SYSTEM
	prelaunch_cluster_pattern_app_manager.app_usage_pattern =
		kmalloc(sizeof(struct application_usage_pattern), GFP_KERNEL);
#endif
	memset(prelaunch_cluster_pattern_app_manager.app_usage_pattern, 0x0,
		sizeof(struct application_usage_pattern));

	prelaunch_cluster_pattern_app_manager.current_pattern_index = -1;
	prelaunch_cluster_pattern_app_manager.prev_pattern_index = -1;

	prelaunch_cluster_pattern_app_manager.extract_application_usage_pattern =
		extract_prelaunch_cluster_usage_pattern;

	prelaunch_cluster_pattern_app_manager.match_application_usage_pattern =
		match_prelaunch_cluster_usage_pattern;

	prelaunch_cluster_pattern_app_manager.manage_applications =
		manage_prelaunch_cluster_applications;
}

void initialize_application_manager(void)
{
	initialize_executed_app_list();

	initialize_cluster_pattern_app_manager();
	initialize_ordered_cluster_pattern_app_manager();
	initialize_prelaunch_cluster_pattern_app_manager();
}

void clear_application_manager(void)
{
#ifdef WINDOWS_OPERATING_SYSTEM
	free(cluster_pattern_app_manager.app_usage_pattern);
	free(ordered_cluster_pattern_app_manager.app_usage_pattern);
	free(prelaunch_cluster_pattern_app_manager.app_usage_pattern);
#endif
#ifdef LINUX_OPERATING_SYSTEM
	kfree(cluster_pattern_app_manager.app_usage_pattern);
	kfree(ordered_cluster_pattern_app_manager.app_usage_pattern);
	kfree(prelaunch_cluster_pattern_app_manager.app_usage_pattern);
#endif
}
