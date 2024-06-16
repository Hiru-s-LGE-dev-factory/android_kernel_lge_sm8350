#include "operating_system.h"
#include "ordered_application_cluster_pattern.h"
#ifdef LINUX_OPERATING_SYSTEM
#include "application_prelaunch.h"
#endif

#define MIN_ORDERED_CLUSTER_PATTERN_APP_NUM	5
#define MAX_ORDERED_CLUSTER_PATTERN_APP_NUM	64

#define PATTERN_MATCH_NUM	2
#define ORDERED_CLUSTER_PATTERN_RATIO	80

static inline void copy_apps(struct application* dest_app,
	struct application* src_app, unsigned int app_num)
{
	int i;

	for (i = 0; i < app_num; ++i)
		memcpy(&dest_app[i], &src_app[i], sizeof(struct application));
}

unsigned int calculate_ordered_cluster_pattern_score(struct application* apps,
	unsigned int start, unsigned int app_num, unsigned int max_app_num)
{
	unsigned int dup_app_num;
	int i;
	int j;
	struct application* cur_app;
	struct application* prev_app;
	unsigned int score;
	unsigned int match_count;
	unsigned int mismatch_count;
	unsigned int pattern_match_count;

	dup_app_num = 0;
	score = 0;

	for (i = 0; i < app_num; ++i) {
		if (apps[i].prev < app_num + start)
			++dup_app_num;
	}

	if (100 * (app_num - dup_app_num) / app_num < ORDERED_CLUSTER_PATTERN_RATIO)
		return score;

	j = app_num;
	pattern_match_count = 0;

	do {
		i = 0;
		cur_app = &apps[i];
		while (j < max_app_num) {
			prev_app = &apps[j];
			if (!strcmp(cur_app->cmdline, prev_app->cmdline))
				break;
			++j;
		}

		if (j == max_app_num)
			return 0;

		++i;
		++j;
		match_count = 1;
		mismatch_count = 0;

		while (i < app_num && j < max_app_num) {
			cur_app = &apps[i];
			prev_app = &apps[j];

			if (!strcmp(cur_app->cmdline, prev_app->cmdline)) {
				++match_count;
				++i;
				++j;
			} else {
				++mismatch_count;
				++j;
			}
		}

		if (match_count < MIN_ORDERED_CLUSTER_PATTERN_APP_NUM)
			return 0;

		if (match_count < mismatch_count)
			return 0;

		score += ((match_count - mismatch_count) * 100 / app_num) + app_num;
	} while (++pattern_match_count < PATTERN_MATCH_NUM);

	return score;
}

int extract_ordered_cluster_usage_pattern(struct application_manager* app_manager)
{
	int i;
	int j;
	struct application_list* app_list;
	struct application* apps;
	unsigned int app_num;

	int best_index;
	int best_num;
	unsigned int pattern_score;
	unsigned int sub_pattern_score;

	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;
	int selected_patterns_index;
	int min_hit_num;

	app_list = app_manager->app_list;
	app_num = app_list->application_number;

	best_index = -1;
	best_num = -1;
	pattern_score = 0;

	for (i = 0; i < MIN_ORDERED_CLUSTER_PATTERN_APP_NUM; ++i) {
		apps = &app_list->applications[i];

		for (j = MIN_ORDERED_CLUSTER_PATTERN_APP_NUM;
			j <= app_num && j <= MAX_ORDERED_CLUSTER_PATTERN_APP_NUM; ++j) {
			if (app_num - i < (j * (PATTERN_MATCH_NUM + 1)))
				break;

			sub_pattern_score =
				calculate_ordered_cluster_pattern_score
				(apps, i, j, app_num - i);

			if (sub_pattern_score <
				PATTERN_MATCH_NUM * ORDERED_CLUSTER_PATTERN_RATIO)
				continue;

			if (pattern_score < sub_pattern_score) {
				pattern_score = sub_pattern_score;
				best_index = i;
				best_num = j;
			}
		}
	}

	if (best_index == -1 || best_num == -1)
		return -1;

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

				if (pattern_app_list->hit_number < 0)
					memset(pattern_app_list, 0x0,
						sizeof(struct pattern_application_list));
			}
		}
	}

	pattern_app_list = &app_usage_pattern->patterns[selected_patterns_index];
	pattern_app_list->pattern_application_number = best_num;
	copy_apps(pattern_app_list->applications,
		&app_list->applications[best_index], best_num);
	pattern_app_list->hit_number = 0;

	return 0;
}

static unsigned int calculate_ordered_cluster_pattern_match_score(
	struct pattern_application_list* pattern_app_list,
	struct application* apps, unsigned int app_num)
{
	int i;
	int j;
	struct application* pattern_apps;
	int start_index;
	unsigned int match_num;
	int score;

	if (pattern_app_list->pattern_application_number <
		MIN_ORDERED_CLUSTER_PATTERN_APP_NUM)
		return 0;

	pattern_apps = pattern_app_list->applications;

	start_index = -1;
	match_num = 0;
	for (i = 0; i < pattern_app_list->pattern_application_number; ++i) {
		if (!strcmp(pattern_apps[i].cmdline, apps[0].cmdline)) {
			start_index = i;
			match_num = 1;
			break;
		}
	}

	if (start_index <= 2)
		return 0;

	++i;
	j = 1;
	while (i < pattern_app_list->pattern_application_number && j < app_num) {
		if (!strcmp(pattern_apps[i].cmdline, apps[j].cmdline)) {
			++i;
			++j;
			++match_num;
		} else {
			++j;
		}
	}

	score = 100 * match_num / app_num + start_index;
	if (ORDERED_CLUSTER_PATTERN_RATIO < score) {
		pattern_app_list->cur_app_index = start_index;
		return score;
	}

	return 0;
}

int match_ordered_cluster_usage_pattern(struct application_manager* app_manager)
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

	if (app_manager->current_pattern_index != -1)
		return ORDERED_CLUSTER_PATTERN_RATIO;

	app_manager->prev_pattern_index = app_manager->current_pattern_index;

	app_list = app_manager->app_list;
	app_num = MAX_ORDERED_CLUSTER_PATTERN_APP_NUM;
	apps = app_list->applications;
	app_usage_pattern = app_manager->app_usage_pattern;

	pattern_score = 0;
	pattern_app_num = 0;
	for (i = 0; i < MAX_PATTERN_NUM; ++i) {
		pattern_app_list = &app_usage_pattern->patterns[i];

		sub_pattern_score =
			calculate_ordered_cluster_pattern_match_score(pattern_app_list, apps,
				pattern_app_list->pattern_application_number / 2);
		if (sub_pattern_score < ORDERED_CLUSTER_PATTERN_RATIO)
			continue;

		sub_pattern_app_num = pattern_app_list->pattern_application_number;
		if (pattern_score < sub_pattern_score) {
			pattern_score = sub_pattern_score;
			pattern_app_num = sub_pattern_app_num;
			app_manager->current_pattern_index = i;
		}
		else if (pattern_score == sub_pattern_score) {
			if (pattern_app_num < sub_pattern_app_num) {
				pattern_app_num = sub_pattern_app_num;
				app_manager->current_pattern_index = i;
			}
		}
	}

	return pattern_score;
}

int manage_ordered_cluster_applications(struct application_manager* app_manager)
{
	return 0;
}

#ifdef LINUX_OPERATING_SYSTEM
static void fail_predict(struct application_manager* app_manager)
{
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;

	app_usage_pattern = app_manager->app_usage_pattern;
	pattern_app_list =
		&app_usage_pattern->patterns[app_manager->current_pattern_index];
	--pattern_app_list->hit_number;

	if (pattern_app_list->hit_number < 0)
		memset(pattern_app_list, 0x0,
			sizeof(struct pattern_application_list));

	app_manager->current_pattern_index = -1;
}

static struct application* success_predict(struct application_manager* app_manager)
{
	struct application_usage_pattern* app_usage_pattern;
	struct pattern_application_list* pattern_app_list;

	app_usage_pattern = app_manager->app_usage_pattern;
	pattern_app_list =
		&app_usage_pattern->patterns[app_manager->current_pattern_index];
	++pattern_app_list->hit_number;

	if (pattern_app_list->cur_app_index == 0) {
		app_manager->current_pattern_index = -1;
		return 0;
	}

	--pattern_app_list->cur_app_index;

	return &pattern_app_list->applications[pattern_app_list->cur_app_index];
}

static int fail_count;
static int success_count;

struct task_struct* get_task_struct_by_comm(char* comm)
{
	struct task_struct *tsk;
	struct task_struct *p;
	struct task_struct *selected;

	selected = 0;

	rcu_read_lock();
	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		if (tsk->exit_state || !tsk->mm)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		if (!strcmp(comm, p->comm)) {
			selected = p;
			get_task_struct(selected);
			task_unlock(p);
			break;
		}

		task_unlock(p);
	}
	rcu_read_unlock();

	return selected;
}

char next_app_cmdline[MAX_CMDLINE_LEN];

int preload_next_app(struct application_manager* app_manager, void* p)
{
	struct task_struct* task;
	int len;
	char buf[MAX_CMDLINE_LEN];
	int i;

	struct application_list* preloaded_apps;
	struct application* app;

	struct task_struct* preload_task;

	memset(next_app_cmdline, 0x0, MAX_CMDLINE_LEN);

	if (app_manager->current_pattern_index == -1)
		return 0;

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

	preloaded_apps = &app_manager->preloaded_app_list;

	if (preloaded_apps->application_number == 0) {
		app = success_predict(app_manager);
		if (strcmp(buf, app->cmdline)) {
			++fail_count;
			pr_info("[app_man][fail to predict %d] prediction app = %s, loaded app = %s\n",
				fail_count, app->cmdline, buf);
			fail_predict(app_manager);
			return 0;
		}
	} else {
		for (i = 0; i < preloaded_apps->application_number; ++i) {
			app = &preloaded_apps->applications[i];
			if (strcmp(buf, app->cmdline)) {
				++fail_count;
				pr_info("[app_man][fail to predict %d] prediction app = %s, loaded app = %s\n",
					fail_count, app->cmdline, buf);
				fail_predict(app_manager);
				preloaded_apps->application_number = 0;
				return 0;
			}
		}
	}

	++success_count;
	pr_info("[app_man][success prediction %d] prediction app = %s, loaded app = %s\n",
		success_count, app->cmdline, buf);

	app = success_predict(app_manager);
	if (!app) {
		preloaded_apps->application_number = 0;
		return 0;
	}

	preload_task = get_task_struct_by_comm(app->comm);
	if (preload_task) {
		pr_info("[app_man][load] preload_task->comm = %s\n", preload_task->comm);
		prefault_task(preload_task);
		put_task_struct(preload_task);
	} else {
		prelaunch_task(app->cmdline);
	}

	memcpy(next_app_cmdline, app->cmdline, MAX_CMDLINE_LEN);
	pr_info("[app_man][load] prediction app = %s (%s)\n", app->cmdline, preload_task ? "warm" : "cold");

	preloaded_apps->application_number = 1;
	copy_apps(&preloaded_apps->applications[0], app, preloaded_apps->application_number);

	return 1;
}
#endif