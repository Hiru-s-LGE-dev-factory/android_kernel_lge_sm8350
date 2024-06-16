#include "operating_system.h"
#include "application_cluster_pattern.h"
#include "ordered_application_cluster_pattern.h"
#include "application.h"
#include "application_prelaunch.h"
#include "pattern_manager.h"

#define NOISE_APP_NUM	20
static char* noise_app_cmdline_list[NOISE_APP_NUM] = {
	"com.lge.launcher3",
	"com.google.android.googlequicksearchbox:search",
	"com.google.android.gms.persistent",
	"com.google.process.gservices",
	"com.google.process.gapps",
	"com.google.android.gms",
	"android.process.acore",
	"com.android.chrome",
	"android.process.media",
	"com.google.android.webview",
	"com.android.process.gapps",
	"com.facebook.katana",
	"com.lge.lgpay",
	"com.lge.gametuner",
	"com.lge.ims.rcsprovider",
	"com.lge.smartshare.provider",
	"com.lge.covertools",
	"com.lge.ime:setting",
	"com.google.android.youtube.player",
	"com.lge.ellievision"
};

static struct application_histogram app_histogram;

struct mutex pattern_lock;

/**
 * get_cmdline_trylock() - copy the cmdline value to a buffer.
 * @task:     the task whose cmdline value to copy.
 * @buffer:   the buffer to copy to.
 * @buflen:   the length of the buffer. Larger cmdline values are truncated
 *            to this length.
 * Returns the size of the cmdline field copied. Note that the copy does
 * not guarantee an ending NULL byte.
 */
int get_cmdline_trylock(struct task_struct *task, char *buffer, int buflen)
{
	int res = 0;
	unsigned int len;
	struct mm_struct *mm = get_task_mm(task);
	unsigned long arg_start, arg_end, env_start, env_end;
	if (!mm)
		goto out;
	if (!mm->arg_end)
		goto out_mm;	/* Shh! No looking before we're done */

	if (!down_read_trylock(&mm->mmap_sem))
		goto out_mm;

	arg_start = mm->arg_start;
	arg_end = mm->arg_end;
	env_start = mm->env_start;
	env_end = mm->env_end;
	up_read(&mm->mmap_sem);

	len = arg_end - arg_start;

	if (len > buflen)
		len = buflen;

	res = access_process_vm(task, arg_start, buffer, len, FOLL_FORCE);

	/*
	 * If the nul at the end of args has been overwritten, then
	 * assume application is using setproctitle(3).
	 */
	if (res > 0 && buffer[res-1] != '\0' && len < buflen) {
		len = strnlen(buffer, res);
		if (len < res) {
			res = len;
		} else {
			len = env_end - env_start;
			if (len > buflen - res)
				len = buflen - res;
			res += access_process_vm(task, env_start,
						 buffer+res, len,
						 FOLL_FORCE);
			res = strnlen(buffer, res);
		}
	}
out_mm:
	mmput(mm);
out:
	return res;
}

static inline int is_noise_app(const char* cmdline)
{
	int i;

	for (i = 0; i < NOISE_APP_NUM; ++i) {
		if (!strncmp(cmdline, noise_app_cmdline_list[i],
			strlen(noise_app_cmdline_list[i])))
			return 1;
	}

	return 0;
}

static unsigned int app_launch_order;

static int add_app_histogram(struct application* app)
{
	int i;
	struct application* cur_app;

	for (i = 0; i < app_histogram.application_number; ++i) {
		cur_app = &app_histogram.applications[i];
		if (!strcmp(app->cmdline, cur_app->cmdline)) {
			cur_app->repeat_index += 2;
			cur_app->order = app_launch_order;
			++app_launch_order;
			return 0;
		}
	}

	if (MAX_APP_LIST_NUM <= app_histogram.application_number) {
		memset(&app_histogram, 0x0, sizeof(struct application_histogram));
		app_histogram.application_number = 0;
	}

	cur_app = &app_histogram.applications[app_histogram.application_number];
	++app_histogram.application_number;

	memcpy(cur_app, app, sizeof(struct application));
	cur_app->repeat_index = 10000000;
	cur_app->order = app_launch_order;
	++app_launch_order;

	return 0;
}

static int set_running_task()
{
	int running_task_num;
	struct task_struct *tsk;
	struct task_struct *p;
	int i;
	struct application* cur_app;

	running_task_num = 0;

	rcu_read_lock();
	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		if (tsk->exit_state || !tsk->mm)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		for (i = 0; i < app_histogram.application_number; ++i) {
			cur_app = &app_histogram.applications[i];

			if (!strcmp(cur_app->comm, p->comm)) {
				cur_app->kill_count = 0;
				++running_task_num;
				break;
			}
		}

		task_unlock(p);
	}
	rcu_read_unlock();

	return running_task_num;
}

#define MAX_PRELAUNCHED_APPS_NUM	2
static struct application* prelaunched_apps[MAX_PRELAUNCHED_APPS_NUM];
static recent_prelaunched_app_index;

static inline int is_prelaunched_app(struct application* app)
{
	int i;

	for (i = 0; i < MAX_PRELAUNCHED_APPS_NUM; ++i) {
		if (prelaunched_apps[i] == app)
			return 1;
	}

	return 0;
}

static inline int is_prelaunched_comm(char* comm)
{
	int i;
	struct application* prelaunched_app;

	for (i = 0; i < MAX_PRELAUNCHED_APPS_NUM; ++i) {
		prelaunched_app = prelaunched_apps[i];

		if (prelaunched_app &&
			!strcmp(prelaunched_app->comm, comm))
			return 1;
	}

	return 0;
}

static inline void add_prelaunched_app(struct application* app)
{
	prelaunched_apps[recent_prelaunched_app_index] = app;

	++recent_prelaunched_app_index;
	if (MAX_PRELAUNCHED_APPS_NUM <= recent_prelaunched_app_index)
		recent_prelaunched_app_index = 0;
}

static inline int clear_prelaunched_app_by_user_launch(char* cmdline)
{
	int i;
	struct application* prelaunched_app;

	for (i = 0; i < MAX_PRELAUNCHED_APPS_NUM; ++i) {
		prelaunched_app = prelaunched_apps[i];

		if (prelaunched_app &&
			!strcmp(prelaunched_app->cmdline, cmdline)) {
			prelaunched_apps[i]->repeat_index -= 1;
			prelaunched_apps[i] = 0;
			return 1;
		}
	}

	return 0;
}

static inline void clear_prelaunched_apps()
{
	int i;

	for (i = 0; i < MAX_PRELAUNCHED_APPS_NUM; ++i)
		prelaunched_apps[i] = 0;
}

static inline void clear_prelaunched_apps_by_kill()
{
	int i;
	struct application* prelaunched_app;

	for (i = 0; i < MAX_PRELAUNCHED_APPS_NUM; ++i) {
		prelaunched_app = prelaunched_apps[i];

		if (prelaunched_app &&
			prelaunched_app->kill_count == 1) {
			prelaunched_apps[i] = 0;
		}
	}
}

static int prelaunch_app()
{
	int i;
	int j;
	struct application* cur_app;
	int max_repeat_index;
	struct application* selected_app;
	char prelaunch_only_cmdline[MAX_CMDLINE_LEN + 2];

	max_repeat_index = 0;
	selected_app = 0;

	if (app_histogram.application_number == 0)
		return 0;

	i = jiffies % app_histogram.application_number;
	for (j = 0; j < app_histogram.application_number; ++j) {
		cur_app = &app_histogram.applications[i];

		++i;
		if (app_histogram.application_number <= i)
			i = 0;

		if (is_prelaunched_app(cur_app))
			continue;

		if (cur_app->kill_count == 1 &&
			max_repeat_index < cur_app->repeat_index) {
			max_repeat_index = cur_app->repeat_index;
			selected_app = cur_app;
		}
	}

	if (selected_app) {
		pr_info("[app_man] prelaunch task = %s\n", selected_app->cmdline);
		strcpy(prelaunch_only_cmdline, selected_app->cmdline);
		prelaunch_task(prelaunch_only_cmdline);

		selected_app->kill_count = 0;
		selected_app->repeat_index -= 1;
		add_prelaunched_app(selected_app);

		return 1;
	} else {
		pr_info("[app_man] cannot select prelaunch task\n");
	}

	return 0;
}

static int prelaunch_apps()
{
	int i;
	struct application* cur_app;

	for (i = 0; i < app_histogram.application_number; ++i) {
		cur_app = &app_histogram.applications[i];
		cur_app->kill_count = 1;
	}

	if (set_running_task() < small_num) {
		pr_info("[app_man] prelaunch all tasks");
		clear_prelaunched_apps();

		for (i = 0; i < small_num; ++i) {
			if (!prelaunch_app())
				break;

			msleep(500);
		}
	} else {
		pr_info("[app_man] prelaunch one task");
		clear_prelaunched_apps_by_kill();

		prelaunch_app();
	}

	return 0;
}

#define PREV_APP_NUM_TO_CHECK	1

static int add_executed_app_list(struct task_struct *p, int added_app_num, int tasksize)
{
	int len;
	char buf[MAX_CMDLINE_LEN];
	int i;
	unsigned int cur_index;
	struct application* app;

	len = get_cmdline_trylock(p, buf, MAX_CMDLINE_LEN);
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

	/* cancel noise application */
	if (is_noise_app(buf))
		return 0;

	cur_index = executed_app_list.start_index;

	for (i = 0; i < PREV_APP_NUM_TO_CHECK + added_app_num; ++i) {
		app = &executed_app_list.applications[cur_index];

		if (!strcmp(app->cmdline, buf)) {
			app->end_time = jiffies / 100;
			return 0;
		}

		if (cur_index != 0)
			--cur_index;
		else
			cur_index = MAX_APP_LIST_NUM - 1;
	}

	++executed_app_list.start_index;
	if (MAX_APP_LIST_NUM <= executed_app_list.start_index)
		executed_app_list.start_index = 0;

	if (executed_app_list.start_index == executed_app_list.end_index) {
		executed_app_list.end_index = executed_app_list.start_index + 1;
		if (MAX_APP_LIST_NUM <= executed_app_list.end_index)
			executed_app_list.end_index = 0;
	}

	cur_index = executed_app_list.start_index;
	app = &executed_app_list.applications[cur_index];

	app->tgid = p->tgid;
	strncpy(app->cmdline, buf, MAX_CMDLINE_LEN);
	strncpy(app->comm, p->comm, TASK_COMM_LEN);
	app->tasksize = tasksize;
	app->end_time = app->start_time = jiffies / 100;

	add_app_histogram(app);

	if (clear_prelaunched_app_by_user_launch(app->cmdline))
		pr_info("[app_man] clear task = %s because of user app launch\n",
			app->cmdline);

	return 1;
}

static int update_executed_app_list_tasksize(struct task_struct *p, int tasksize)
{
	int len;
	char buf[MAX_CMDLINE_LEN];
	int i;
	unsigned int cur_index;
	struct application* app;

	len = get_cmdline_trylock(p, buf, MAX_CMDLINE_LEN);
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

	/* cancel noise application */
	if (is_noise_app(buf))
		return 0;

	cur_index = executed_app_list.start_index;

	for (i = 0; i < MAX_PATTERN_APP_NUM; ++i) {
		if (executed_app_list.end_index == cur_index)
			break;

		app = &executed_app_list.applications[cur_index];

		if (!strcmp(app->cmdline, buf)) {
			if (app->tasksize == 0)
				app->tasksize = tasksize;
			else
				app->tasksize = (app->tasksize + tasksize) / 2;
		}

		if (cur_index != 0)
			--cur_index;
		else
			cur_index = MAX_APP_LIST_NUM - 1;
	}

	return 0;
}

static int test_task_flag(struct task_struct *p, int flag)
{
	struct task_struct *t;

	for_each_thread(p, t) {
		task_lock(t);
		if (test_tsk_thread_flag(t, flag)) {
			task_unlock(t);
			return 1;
		}
		task_unlock(t);
	}

	return 0;
}

#define DISCARD_PERIOD	(180 * 1000)
#define APP_SCAN_PERIOD	2510
#define FOREGROUND_APP_KEEP_TIME	3830

#define VALID_APP_SIZE	(8 * 1024)
#define MIN_MANAGE_APP_NUM	11
#define APP_NUM_RANGE	6

#define MAX_KILL_COUNT_IN_TURN	1

#define MAX_FOREGROUND_TASK_SIZE	16
static struct task_struct *foreground_tasks[MAX_FOREGROUND_TASK_SIZE];
static int foreground_tasks_tasksize[MAX_FOREGROUND_TASK_SIZE];
#define MAX_KILLABLE_TASK_SIZE	256
static struct task_struct *killable_tasks[MAX_KILLABLE_TASK_SIZE];
static int killable_tasks_tasksize[MAX_KILLABLE_TASK_SIZE];

#define CLEAR_APPS_FREERAM	(100 * 1024)

int application_manager_thread_start;

static int application_manager_thread(void *unused)
{
	struct task_struct *tsk;
	struct task_struct *p;
	struct task_struct *home_tsk;
	int added_app_num;
	int adjusted_app_num;

	int i;
	int foreground_task_num;
	int cur_added_app_num;
	int prev_added_app_num;

	int j;
	int killable_task_num;
	int kill_app_num;

	unsigned long freeram;
	unsigned long prev_freeram;

	msleep(DISCARD_PERIOD);

	initialize_cluster_usage_pattern(&cluster_pattern_app_manager);

	mutex_lock(&pattern_lock);
	application_manager_thread_start = 1;
	mutex_unlock(&pattern_lock);

	home_tsk = 0;

	added_app_num = 0;
	adjusted_app_num = jiffies % (APP_NUM_RANGE + 1);

	prev_added_app_num = -1;

	freeram = 0;
	prev_freeram = 0;

	while (1) {
		msleep(APP_SCAN_PERIOD);

		i = 0;
		j = 0;

		rcu_read_lock();
		for_each_process(tsk) {
			if (tsk->flags & PF_KTHREAD)
				continue;

			if (tsk->exit_state || !tsk->mm)
				continue;

			/* if task no longer has any memory ignore it */
			if (test_task_flag(tsk, TIF_MEMDIE))
				continue;

			p = find_lock_task_mm(tsk);
			if (!p)
				continue;

			if (p->signal->oom_score_adj == 100) {
				home_tsk = p;
				task_unlock(p);
				continue;
			}

			if (p->signal->oom_score_adj == 0 && p == home_tsk) {
				prev_added_app_num = -1;
				task_unlock(p);
				continue;
			}

			if (p->signal->oom_score_adj == 0 &&
				VALID_APP_SIZE < get_mm_rss(p->mm) +
				get_mm_counter(p->mm, MM_SWAPENTS)) {
				if (i < MAX_FOREGROUND_TASK_SIZE) {
					foreground_tasks[i] = p;
					get_task_struct(foreground_tasks[i]);
					foreground_tasks_tasksize[i] = get_mm_rss(p->mm) +
						get_mm_counter(p->mm, MM_SWAPENTS);
					++i;
				}

				task_unlock(p);
				continue;
			}

			if (800 <= p->signal->oom_score_adj) {
				if (j < MAX_KILLABLE_TASK_SIZE) {
					if (!is_prelaunched_comm(p->comm)) {
						killable_tasks[j] = p;
						get_task_struct(killable_tasks[j]);
						killable_tasks_tasksize[j] = get_mm_rss(p->mm) +
							get_mm_counter(p->mm, MM_SWAPENTS);

						++j;
					}
				}

				task_unlock(p);
				continue;
			}

			task_unlock(p);
		}
		rcu_read_unlock();

		msleep(FOREGROUND_APP_KEEP_TIME);

		cur_added_app_num = 0;
		foreground_task_num = i;

		for (i = 0; i < foreground_task_num; ++i) {
			p = foreground_tasks[i];
			if (p->exit_state || !p->mm) {
				put_task_struct(p);
				continue;
			}

			if (p->signal->oom_score_adj != 0) {
				put_task_struct(p);
				continue;
			}

			if (add_executed_app_list(p, cur_added_app_num + prev_added_app_num,
				foreground_tasks_tasksize[i])) {
				++cur_added_app_num;
				++added_app_num;

#ifdef RECLAIM_CLUSTER_APPS
				reclaim_cluster_apps(&cluster_pattern_app_manager, p);
#endif

				if (!preload_next_app(&ordered_cluster_pattern_app_manager, p))
					prelaunch_apps();
			}

			put_task_struct(p);
		}

		prev_added_app_num = cur_added_app_num;

		kill_app_num = 0;
		killable_task_num = j;

		for (j = 0; j < killable_task_num; ++j) {
			p = killable_tasks[j];
			if (p->exit_state || !p->mm) {
				put_task_struct(p);
				continue;
			}

			if (p->signal->oom_score_adj < 900)
				update_executed_app_list_tasksize(p, killable_tasks_tasksize[j]);

			if (kill_app_num < MAX_KILL_COUNT_IN_TURN &&
				900 <= p->signal->oom_score_adj) {
				if (kill_unused_app(&cluster_pattern_app_manager, p)) {
					task_lock(p);
					send_sig(SIGKILL, p, 0);
					task_unlock(p);

					++kill_app_num;
				}
			}

			put_task_struct(p);
		}

		if (MIN_MANAGE_APP_NUM - adjusted_app_num < added_app_num) {
			//print_executed_app_list();

			mutex_lock(&pattern_lock);
			manage_applications();
			mutex_unlock(&pattern_lock);

			added_app_num = 0;
			adjusted_app_num = jiffies % (APP_NUM_RANGE + 1);
		}

		prev_freeram = freeram;
		freeram = global_zone_page_state(NR_FREE_PAGES);

		if (prev_freeram < (CLEAR_APPS_FREERAM * 3 / 4) &&
			CLEAR_APPS_FREERAM < freeram) {
			pr_info("prev_freeram = %lu, freeram = %lu\n", prev_freeram, freeram);
#ifdef PRELOAD_CLUSTER_APPS
			preload_cluster_apps(&cluster_pattern_app_manager);
#endif
		}
	}

	clear_application_manager();

	return 0;
}

static int __init application_manager_init(void)
{
	struct task_struct *application_manager_task;

	initialize_application_manager();
	prelaunch_init();
	initialize_pattern_manager();

	application_manager_task =
		kthread_run(application_manager_thread, NULL,
		"application_manager_task");

	set_user_nice(application_manager_task, MAX_NICE);

	return 0;
}
device_initcall(application_manager_init);
