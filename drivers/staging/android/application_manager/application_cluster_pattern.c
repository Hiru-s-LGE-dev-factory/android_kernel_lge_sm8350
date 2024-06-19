#include "operating_system.h"
#include "application_cluster_pattern.h"
#ifdef LINUX_OPERATING_SYSTEM
#include "application_prelaunch.h"
#endif

#define MAX_CLUSTER_PATTERN_APP_NUM	64

#define CLUSTER_PATTERN_RATIO	78
#define PATTERN_MATCH_NUM	2

static unsigned long mem_available_pages;
static unsigned int min_cluster_pattern_app_num;
unsigned int small_num;

int initialize_cluster_usage_pattern(struct application_manager* app_manager)
{
	struct sysinfo i;
	unsigned long mem_size;
	unsigned long swap_space_size;
	unsigned long reserved_mem_size;

	memset(&i, 0x0, sizeof(struct sysinfo));
	si_meminfo(&i);
	si_swapinfo(&i);

	mem_size = i.totalram;
	swap_space_size = i.totalswap;

	if (mem_size < (3 * (1024 * 1024) >> (PAGE_SHIFT - 10))) {
		reserved_mem_size = mem_size * 70 / 100;
		min_cluster_pattern_app_num = 6;
		small_num = 8;
	} else if (mem_size < (4 * (1024 * 1024) >> (PAGE_SHIFT - 10))) {
		reserved_mem_size = mem_size * 60 / 100;
		min_cluster_pattern_app_num = 7;
		small_num = 11;
	} else if (mem_size < (6 * (1024 * 1024) >> (PAGE_SHIFT - 10))) {
		reserved_mem_size = mem_size * 50 / 100;
		min_cluster_pattern_app_num = 9;
		small_num = 15;
	} else {
		reserved_mem_size = (3 * (1024 * 1024) >> (PAGE_SHIFT - 10));
		min_cluster_pattern_app_num = 9;
		small_num = 18;
	}

	mem_available_pages = ((mem_size - reserved_mem_size) * 40 / 100) +
		(swap_space_size * 25 / 100);

	pr_info("[app_man] mem_size = %lu pages\n", mem_size);
	pr_info("[app_man] swap_space_size = %lu pages\n", swap_space_size);
	pr_info("[app_man] reserved_mem_size = %lu pages\n", reserved_mem_size);
	pr_info("[app_man] mem_available_pages = %lu pages\n",
		mem_available_pages);

	pr_info("[app_man] min_cluster_pattern_app_num = %u\n",
		min_cluster_pattern_app_num);

	pr_info("[app_man] small_num = %u\n", small_num);

	return 0;
}

static void ascending_sort(int* sort, int size)
{
	int i;
	int j;
	int temp;

	for (i = 0; i < size - 1; ++i) {
		temp = sort[i];
		for (j = 0; j < size - (i + 1); ++j) {
			if (sort[j] > sort[j + 1]) {
				temp = sort[j + 1];
				sort[j + 1] = sort[j];
				sort[j] = temp;
			}
		}
	}
}

static unsigned int calculate_cluster_pattern_score(struct application* apps,
	unsigned int app_num)
{
	unsigned int i;
	unsigned int dup_app_num;

	unsigned int prev_min_index;
	unsigned int prev_max_index;

	int prevs[MAX_CLUSTER_PATTERN_APP_NUM];
	unsigned int min_index;
	unsigned int max_index;

	unsigned int non_repeat_app_num;

	unsigned int pattern_match_count;
	unsigned int cur_ratio;
	unsigned int sub_score;
	unsigned int score;

	dup_app_num = 0;

	prev_min_index = 0;
	prev_max_index = app_num - 1;

	non_repeat_app_num = 0;

	pattern_match_count = 0;
	cur_ratio = 0;
	sub_score = 0;
	score = 0;

	for (i = 0; i < app_num; ++i) {
		if (apps[i].prev < app_num)
			++dup_app_num;
	}

	if (100 * (app_num - dup_app_num) / app_num < CLUSTER_PATTERN_RATIO)
		return score;

	while (1) {
		for (i = 0; i < app_num; ++i) {
			prevs[i] = i;
			while (prevs[i] != -1 && prevs[i] <= prev_max_index)
				prevs[i] = apps[prevs[i]].prev;
		}

		ascending_sort(prevs, app_num);

		min_index = 0;
		non_repeat_app_num = 0;

		for (i = 0; i < app_num; ++i) {
			if (prevs[i] == -1) {
				++non_repeat_app_num;
				min_index = i + 1;

				if ((100 - CLUSTER_PATTERN_RATIO) <
					(100 * non_repeat_app_num / app_num))
					return score;
			}
		}

		max_index = app_num - 1;

		while (1) {
			cur_ratio = 100 * (max_index - min_index + 1 - dup_app_num) / app_num;
			if (cur_ratio < CLUSTER_PATTERN_RATIO)
				return score;

			sub_score = (cur_ratio * (max_index - min_index + 1)) /
				(prevs[max_index] - prevs[min_index] + 1) + (14 * app_num / 10);

			if (CLUSTER_PATTERN_RATIO < sub_score) {
				score += sub_score;
				break;
			}

			if (prevs[min_index + 1] - prevs[min_index] <
				prevs[max_index] - prevs[max_index - 1])
				--max_index;
			else
				++min_index;

			if (max_index <= min_index)
				return score;
		}

		prev_min_index = prevs[min_index];
		prev_max_index = prevs[max_index];

		++pattern_match_count;
		if (PATTERN_MATCH_NUM <= pattern_match_count)
			return score;
	}

	return 0;
}

static inline void copy_apps(struct application* dest_app,
	struct application* src_app, unsigned int app_num)
{
	int i;

	for (i = 0; i < app_num; ++i)
		memcpy(&dest_app[i], &src_app[i], sizeof(struct application));
}

static void build_apps(unsigned int app_num, struct application* apps)
{
	int i;
	int j;
	struct application* app;
	struct application* prev_app;

	if (app_num == 0)
		return;

	for (i = 0; i < app_num; ++i) {
		app = &apps[i];
		app->repeat_index = -1;
	}

	for (i = 0; i < app_num; ++i) {
		app = &apps[i];
		if (app->repeat_index != -1)
			continue;

		for (j = i + 1; j < app_num; ++j) {
			prev_app = &apps[j];
			if (prev_app->repeat_index != -1)
				continue;

			if (!strcmp(app->cmdline, prev_app->cmdline)) {
				app->repeat_index = i;
				prev_app->repeat_index = i;
			}
		}
	}
}

static inline void clear_unused_apps(struct application_manager* app_manager)
{
	app_manager->unused_app_list.application_number = 0;
}

static void set_unused_apps_from_cluster_pattern_apps(struct application_manager* app_manager)
{
	int current_pattern_index;
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	unsigned int total_apps_size;

	int i;
	struct application* pattern_app;
	int repeat_index;

	int max_tasksize_index;
	unsigned int max_tasksize;
	unsigned int tasksize[MAX_CLUSTER_PATTERN_APP_NUM];
	struct application_list* unused_apps;

	clear_unused_apps(app_manager);

	current_pattern_index = app_manager->current_pattern_index;

	if (current_pattern_index < 0) {
		app_manager->unused_app_list.application_number = 0;
		return;
	}

	app_usage_pattern = app_manager->app_usage_pattern;
	pattern_app_list = &app_usage_pattern->patterns[current_pattern_index];

	repeat_index = -1;
	total_apps_size = 0;

	for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
		pattern_app = &pattern_app_list->applications[i];
		tasksize[i] = 0;

		if (pattern_app->repeat_index == -1) {
			total_apps_size += pattern_app->tasksize;
			tasksize[i] = pattern_app->tasksize;
		} else if (pattern_app->repeat_index < repeat_index) {
			total_apps_size += pattern_app->tasksize;
			repeat_index = pattern_app->repeat_index;
		}
	}

	pr_info("[app_man] mem_available_pages = %u\n", mem_available_pages);
	pr_info("[app_man] total_apps_size = %u\n", total_apps_size);

	unused_apps = &app_manager->unused_app_list;
	unused_apps->application_number = 0;
	repeat_index = -1;

	for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
		pattern_app = &pattern_app_list->applications[i];

		if (mem_available_pages < total_apps_size &&
			4 <= pattern_app->kill_count) {
			if (pattern_app->repeat_index == -1) {
				total_apps_size -= pattern_app->tasksize;
			} else if (repeat_index < pattern_app->repeat_index) {
				repeat_index = pattern_app->repeat_index;
				total_apps_size -= pattern_app->tasksize;
			} else {
				pattern_app->kill_count = 0;
				tasksize[i] = 0;
				continue;
			}

			pattern_app->kill_count = 0;
			tasksize[i] = 0;
			memcpy(&unused_apps->applications[unused_apps->application_number],
				pattern_app, sizeof(struct application));
			++unused_apps->application_number;
		}
	}

	while (mem_available_pages < total_apps_size) {
		max_tasksize_index = -1;
		max_tasksize = 0;

		for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
			if (max_tasksize < tasksize[i]) {
				max_tasksize = tasksize[i];
				max_tasksize_index = i;
			}
		}

		if (max_tasksize_index == -1)
			return;

		tasksize[max_tasksize_index] = 0;

		pattern_app_list->applications[max_tasksize_index].kill_count = 0;

		memcpy(&unused_apps->applications[unused_apps->application_number],
			&pattern_app_list->applications[max_tasksize_index],
			sizeof(struct application));
		++unused_apps->application_number;

		total_apps_size -= max_tasksize;
	}
}

int extract_cluster_usage_pattern(struct application_manager* app_manager)
{
	int i;
	int j;
	struct application_list* app_list;
	unsigned int app_num;
	struct application* apps;
	unsigned int sub_pattern_score;
	unsigned int pattern_score;

	int best_index;
	int best_num;

	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	int selected_patterns_index;
	int min_hit_num;

	app_list = app_manager->app_list;
	app_num = app_list->application_number;

	if (app_num < min_cluster_pattern_app_num)
		return EXTRACT_PATTERN_FAIL;

	best_index = -1;
	best_num = -1;
	pattern_score = 0;

	for (i = 0; i < 1; ++i) {
		apps = &app_list->applications[i];

		for (j = min_cluster_pattern_app_num;
			j <= app_num && j <= MAX_CLUSTER_PATTERN_APP_NUM; ++j) {
			sub_pattern_score = calculate_cluster_pattern_score(apps, j);

			if (sub_pattern_score < PATTERN_MATCH_NUM * CLUSTER_PATTERN_RATIO)
				continue;

			if (pattern_score <= sub_pattern_score) {
				pattern_score = sub_pattern_score;
				best_index = i;
				best_num = j;
			}
		}
	}

	if (best_index == -1 || best_num == -1) {
		clear_unused_apps(app_manager);
		return -1;
	}

	selected_patterns_index = -1;
	app_usage_pattern = app_manager->app_usage_pattern;

	for (i = 0; i < MAX_PATTERN_NUM; ++i) {
		pattern_app_list = &app_usage_pattern->patterns[i];
		if (pattern_app_list->pattern_application_number == 0) {
			selected_patterns_index = i;
			break;
		}
	}

	min_hit_num = -1;

	if (selected_patterns_index == -1) {
		pattern_app_list = &app_usage_pattern->patterns[0];
		min_hit_num = pattern_app_list->hit_number;
		selected_patterns_index = 0;

		for (i = 1; i < MAX_PATTERN_NUM; ++i) {
			pattern_app_list = &app_usage_pattern->patterns[i];
			if (pattern_app_list->hit_number < min_hit_num) {
				min_hit_num = pattern_app_list->hit_number;
				selected_patterns_index = i;
			}
		}

		for (i = 0; i < MAX_PATTERN_NUM; ++i) {
			if (i != selected_patterns_index) {
				pattern_app_list = &app_usage_pattern->patterns[i];
				--pattern_app_list->hit_number;
			}
		}
	}

	pattern_app_list = &app_usage_pattern->patterns[selected_patterns_index];
	pattern_app_list->pattern_application_number = best_num;
	copy_apps(pattern_app_list->applications,
		&app_list->applications[best_index], best_num);
	pattern_app_list->hit_number = 0;

	build_apps(pattern_app_list->pattern_application_number, pattern_app_list->applications);

	return 0;
}

#define EXIST_NUM_PENALTY_VALUE	1

static unsigned int calculate_cluster_pattern_match_score(
	struct pattern_application_list* pattern_app_list,
	struct application* apps, unsigned int app_num)
{
	int i;
	int j;
	struct application* pattern_apps;
	int repeat_index;
	int dup_app_num;
	int exist_num;
	int score;

	if (pattern_app_list->pattern_application_number <
		min_cluster_pattern_app_num)
		return 0;

	pattern_apps = pattern_app_list->applications;

	repeat_index = -1;
	exist_num = 0;
	for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
		if (pattern_apps[i].repeat_index != -1 &&
			pattern_apps[i].repeat_index <= repeat_index)
			continue;
		else if (repeat_index < pattern_apps[i].repeat_index)
			repeat_index = pattern_apps[i].repeat_index;

		for (j = 0; j < app_num; ++j) {
			dup_app_num = -1;
			if (apps[j].prev < pattern_app_list->pattern_application_number ||
				apps[j].next != -1)
				dup_app_num = 1;

			if (!strcmp(pattern_apps[i].cmdline, apps[j].cmdline)) {
				if (pattern_apps[i].repeat_index == -1) {
					if (dup_app_num == -1)
						++exist_num;
					else
						exist_num -= EXIST_NUM_PENALTY_VALUE;
				} else if (pattern_apps[i].repeat_index != -1) {
					if (dup_app_num == 1)
						++exist_num;
					else
						exist_num -= EXIST_NUM_PENALTY_VALUE;
				}
			}
		}
	}

	score = (100 * exist_num / app_num) +
		((pattern_app_list->pattern_application_number -
		min_cluster_pattern_app_num) *
		(100 - CLUSTER_PATTERN_RATIO) * 4 / 100);
	if (CLUSTER_PATTERN_RATIO < score)
		return score;

	return 0;
}

static void update_cluster_pattern_apps_tasksize(
	struct pattern_application_list* pattern_app_list,
	struct application* apps, unsigned int app_num)
{
	struct application* pattern_apps;
	int i;
	int j;

	pattern_apps = pattern_app_list->applications;

	for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
		for (j = 0; j < app_num; ++j) {
			if (!strcmp(pattern_apps[i].cmdline, apps[j].cmdline)) {
				if (apps[j].tasksize != 0) {
					if (pattern_apps[i].tasksize == 0)
						pattern_apps[i].tasksize = apps[j].tasksize;
					else
						pattern_apps[i].tasksize =
							(pattern_apps[i].tasksize + apps[j].tasksize) / 2;
				}

				break;
			}
		}
	}
}

static void update_cluster_pattern_apps_kill_count(
	struct pattern_application_list* pattern_app_list,
	struct application* apps, unsigned int app_num)
{
	struct application* pattern_apps;
	int i;
	int j;
	int k;
	char* processed_apps[MAX_CLUSTER_PATTERN_APP_NUM];
	unsigned int processed_apps_num;
	int is_processed_app;

	pattern_apps = pattern_app_list->applications;
	processed_apps_num = 0;

	for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
		is_processed_app = 0;
		for (k = 0; k < processed_apps_num; ++k) {
			if (!strcmp(pattern_apps[i].cmdline, processed_apps[k])) {
				is_processed_app = 1;
				break;
			}
		}
		if (is_processed_app)
			continue;

		processed_apps[processed_apps_num] = pattern_apps[i].cmdline;
		++processed_apps_num;

		for (j = 1; j < app_num; ++j) {
			if (!strcmp(pattern_apps[i].cmdline, apps[j].cmdline)) {
				if (pattern_apps[i].tgid != apps[j].tgid) {
					pattern_apps[i].tgid = apps[j].tgid;
					++pattern_apps[i].kill_count;
				} else {
					pattern_apps[i].kill_count = 0;
				}

				break;
			}
		}
	}
}

static void clear_cluster_pattern_apps_kill_count(
	struct pattern_application_list* pattern_app_list)
{
	struct application* pattern_apps;
	int i;

	pattern_apps = pattern_app_list->applications;

	for (i = 0; i < pattern_app_list->pattern_application_number; ++i)
		pattern_apps[i].kill_count = 0;
}

int match_cluster_usage_pattern(struct application_manager* app_manager)
{
	int i;

	struct application_list* app_list;
	unsigned int app_num;
	struct application* apps;

	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;

	unsigned int sub_pattern_score;
	unsigned int sub_pattern_app_num;
	unsigned int pattern_score;
	unsigned int pattern_app_num;

	app_manager->prev_pattern_index = app_manager->current_pattern_index;
	app_manager->current_pattern_index = -1;

	app_list = app_manager->app_list;
	app_num = MAX_CLUSTER_PATTERN_APP_NUM;
	apps = app_list->applications;
	app_usage_pattern = app_manager->app_usage_pattern;

	pattern_score = 0;
	pattern_app_num = 0;
	for (i = 0; i < MAX_PATTERN_NUM; ++i) {
		pattern_app_list = &app_usage_pattern->patterns[i];

		update_cluster_pattern_apps_tasksize(pattern_app_list, apps, app_num);

		sub_pattern_score =
			calculate_cluster_pattern_match_score(pattern_app_list, apps,
				pattern_app_list->pattern_application_number / 2);
		if (sub_pattern_score < CLUSTER_PATTERN_RATIO)
			continue;

		sub_pattern_app_num = pattern_app_list->pattern_application_number;
		if (pattern_score < sub_pattern_score) {
			pattern_score = sub_pattern_score;
			pattern_app_num = sub_pattern_app_num;
			app_manager->current_pattern_index = i;
		} else if (pattern_score == sub_pattern_score) {
			if (pattern_app_num < sub_pattern_app_num) {
				pattern_app_num = sub_pattern_app_num;
				app_manager->current_pattern_index = i;
			}
		}
	}

	if (app_manager->prev_pattern_index != -1) {
		if (app_manager->prev_pattern_index ==
			app_manager->current_pattern_index) {
			pattern_app_list =
				&app_usage_pattern->patterns[app_manager->current_pattern_index];
			++(pattern_app_list->hit_number);

			update_cluster_pattern_apps_kill_count(pattern_app_list, apps, app_num);
		} else {
			pattern_app_list =
				&app_usage_pattern->patterns[app_manager->prev_pattern_index];
			--(pattern_app_list->hit_number);

			clear_cluster_pattern_apps_kill_count(pattern_app_list);

			if (pattern_app_list->hit_number < 0)
				memset(pattern_app_list, 0x0,
					sizeof(struct pattern_application_list));
		}
	}

	if (app_manager->current_pattern_index == -1) {
		clear_unused_apps(app_manager);
		return pattern_score;
	}

	set_unused_apps_from_cluster_pattern_apps(app_manager);

	return pattern_score;
}

#ifdef LINUX_OPERATING_SYSTEM
static void reference_pages(struct application* app)
{
	struct task_struct* task;

	task = get_task_struct_by_comm(app->comm);
	if (task) {
		prefault_task(task);
		put_task_struct(task);
	} else {
		prelaunch_task(app->cmdline);
	}
}
#endif

static void manage_small_num_apps_cluster(struct application_manager* app_manager)
{
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	struct application* app;
	int i;

	app_usage_pattern = app_manager->app_usage_pattern;

	if (app_manager->current_pattern_index == -1)
		return;

	pattern_app_list =
		&app_usage_pattern->patterns[app_manager->current_pattern_index];

	if (pattern_app_list->pattern_application_number <= small_num) {
		for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
			app = &pattern_app_list->applications[i];
#ifdef LINUX_OPERATING_SYSTEM
			reference_pages(app);
#endif
		}
	}
}

int manage_cluster_applications(struct application_manager* app_manager)
{
	manage_small_num_apps_cluster(app_manager);

	return 0;
}

#ifdef LINUX_OPERATING_SYSTEM
extern char next_app_cmdline[MAX_CMDLINE_LEN];

int kill_unused_app(struct application_manager* app_manager, void* p)
{
	struct task_struct* task;
	int len;
	char buf[MAX_CMDLINE_LEN];
	int i;

	struct application_list* unused_apps;
	struct application* app;

	task = (struct task_struct*)p;

	len = get_cmdline_trylock(task, buf, MAX_CMDLINE_LEN);
	if (len == 0)
		return 0;

	buf[len - 1] = 0;
	i = len - 1;
	while (0 < --i) {
		if (buf[i] == ' ')
			buf[i] = 0;
		else
			break;
	}

	unused_apps = &app_manager->unused_app_list;

	for (i = 0; i < unused_apps->application_number; ++i) {
		app = &unused_apps->applications[i];
		if (!strcmp(app->cmdline, next_app_cmdline))
			continue;

		if (!strcmp(buf, app->cmdline)) {
			pr_info("[app_man] kill unused app %s\n", buf);
			return 1;
		}
	}

	return 0;
}

void preload_cluster_apps(struct application_manager* app_manager)
{
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	struct application_list* unused_apps;

	struct application* app;
	struct application* unused_app;

	int i;
	int j;
	int is_unused_app;

	app_usage_pattern = app_manager->app_usage_pattern;

	if (app_manager->current_pattern_index == -1)
		return;

	pattern_app_list =
		&app_usage_pattern->patterns[app_manager->current_pattern_index];

	unused_apps = &app_manager->unused_app_list;

	for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
		app = &pattern_app_list->applications[i];
		is_unused_app = 0;

		for (j = 0; j < unused_apps->application_number; ++j) {
			unused_app = &unused_apps->applications[j];
			if (!strcmp(app->cmdline, unused_app->cmdline)) {
				is_unused_app = 1;
				break;
			}
		}

		if (is_unused_app == 0)
			reference_pages(app);
	}
}

int is_unused_app(struct application_manager* app_manager, char* cmdline)
{
	struct application_list* unused_apps;
	struct application* unused_app;
	int i;

	unused_apps = &app_manager->unused_app_list;

	for (i = 0; i < unused_apps->application_number; ++i) {
		unused_app = &unused_apps->applications[i];
		if (!strcmp(unused_app->cmdline, cmdline))
			return 1;
	}

	return 0;
}

#ifdef _CONFIG_PROCESS_RECLAIM
static void reclaim_pages(struct application* app)
{
	struct task_struct* task;
	struct reclaim_param rp;

	task = get_task_struct_by_comm(app->comm);
	if (task) {
		rp = reclaim_task_anon(task, app->tasksize);
		put_task_struct(task);

		pr_info("[app_man] reclaim %d pages\n", rp.nr_reclaimed);
	}
}
#endif

#define RECLAIM_TASK_NUM	5
static int prev_reclaim_task_num;

void reclaim_cluster_apps(struct application_manager* app_manager,
	struct task_struct *p)
{
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	struct application_list* unused_apps;

	struct application* app;
	struct application* unused_app;

	int i;
	int j;
	int is_unused_app;
	int reclaim_task_count;

	app_usage_pattern = app_manager->app_usage_pattern;

	if (app_manager->current_pattern_index == -1)
		return;

	pattern_app_list =
		&app_usage_pattern->patterns[app_manager->current_pattern_index];

	if (pattern_app_list->pattern_application_number <= small_num)
		return;

	unused_apps = &app_manager->unused_app_list;

	reclaim_task_count = 0;

	for (i = prev_reclaim_task_num;
		i < pattern_app_list->pattern_application_number; ++i) {
		app = &pattern_app_list->applications[i];
		is_unused_app = 0;

		for (j = 0; j < unused_apps->application_number; ++j) {
			unused_app = &unused_apps->applications[j];
			if (!strcmp(app->cmdline, unused_app->cmdline) ||
				!strcmp(app->comm, p->comm) ||
				!strcmp(app->cmdline, next_app_cmdline)) {
				is_unused_app = 1;
				break;
			}
		}

#ifdef _CONFIG_PROCESS_RECLAIM
		if (is_unused_app == 0)
			reclaim_pages(app);
#endif
		++reclaim_task_count;
		if (RECLAIM_TASK_NUM <= reclaim_task_count) {
			prev_reclaim_task_num = i;
			break;
		}
	}

	if (reclaim_task_count < RECLAIM_TASK_NUM)
		prev_reclaim_task_num = 0;
}
#endif
