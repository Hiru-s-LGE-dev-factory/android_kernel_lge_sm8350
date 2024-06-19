#ifndef _APPLICATION_CLUSTER_PATTERN_H
#define _APPLICATION_CLUSTER_PATTERN_H

#include "application.h"

extern struct application_manager cluster_pattern_app_manager;
extern unsigned int small_num;

int initialize_cluster_usage_pattern(struct application_manager* app_manager);

int extract_cluster_usage_pattern(struct application_manager* app_manager);
int match_cluster_usage_pattern(struct application_manager* app_manager);

int manage_cluster_applications(struct application_manager* app_manager);

#ifdef LINUX_OPERATING_SYSTEM
int kill_unused_app(struct application_manager* app_manager, void* p);
void preload_cluster_apps(struct application_manager* app_manager);
int is_unused_app(struct application_manager* app_manager, char* cmdline);
void reclaim_cluster_apps(struct application_manager* app_manager,
    struct task_struct *p);
#endif

#endif
