#ifndef _APPLICATION_H
#define _APPLICATION_H

#define MAX_CMDLINE_LEN		64

struct application {
	int tgid;
	char cmdline[MAX_CMDLINE_LEN];
	char comm[TASK_COMM_LEN];
	int tasksize;
	unsigned long start_time;
	unsigned long end_time;

	unsigned int order;
	int prev;
	int next;
	int repeat_index;

	unsigned int kill_count;
};

#define MAX_APP_LIST_NUM		256

struct executed_application_list {
	unsigned int start_index;
	unsigned int end_index;
	struct application applications[MAX_APP_LIST_NUM];
};

struct application_list {
	unsigned int application_number;
	struct application applications[MAX_APP_LIST_NUM];
};

struct application_histogram {
	unsigned int application_number;
	struct application applications[MAX_APP_LIST_NUM];
};

#define MAX_PATTERN_APP_NUM	64

struct pattern_application_list {
	unsigned int pattern_application_number;
	struct application applications[MAX_PATTERN_APP_NUM];
	int hit_number;
	unsigned int cur_app_index;
};

#define MAX_PATTERN_NUM	16

struct application_usage_pattern {
	struct pattern_application_list patterns[MAX_PATTERN_NUM];
};

#define EXTRACT_PATTERN_FAIL	(-1)

struct application_manager {
	struct application_list* app_list;
	struct application_usage_pattern* app_usage_pattern;

	int current_pattern_index;
	int prev_pattern_index;

	struct application_list unused_app_list;

	struct application_list preloaded_app_list;

	int (*extract_application_usage_pattern)(struct application_manager* app_manager);
	int (*match_application_usage_pattern)(struct application_manager* app_manager);

	int (*manage_applications)(struct application_manager* app_manager);
};

extern struct executed_application_list executed_app_list;
extern struct application_list app_list;

void print_executed_app_list(void);
void print_app_list(void);

void manage_applications(void);

void initialize_application_manager(void);
void clear_application_manager(void);

#ifdef LINUX_OPERATING_SYSTEM
extern int application_manager_thread_start;
extern struct mutex pattern_lock;
extern struct proc_dir_entry *proc_appboost;

int get_cmdline_trylock(struct task_struct *task, char *buffer, int buflen);
struct task_struct* get_task_struct_by_comm(char* comm);

int print_cluster_pattern_to_buf(char* pattern_data,
	unsigned int pattern_data_size);
int restore_cluster_pattern_from_buf(char* pattern_data,
	unsigned int pattern_data_size);
#endif

#endif