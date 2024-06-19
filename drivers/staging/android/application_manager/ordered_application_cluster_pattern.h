#ifndef _ORDERED_APPLICATION_CLUSTER_PATTERN_H
#define _ORDERED_APPLICATION_CLUSTER_PATTERN_H

#include "application.h"

extern struct application_manager ordered_cluster_pattern_app_manager;

int extract_ordered_cluster_usage_pattern(struct application_manager* app_manager);
int match_ordered_cluster_usage_pattern(struct application_manager* app_manager);

int manage_ordered_cluster_applications(struct application_manager* app_manager);

#ifdef LINUX_OPERATING_SYSTEM
int preload_next_app(struct application_manager* app_manager, void* p);
#endif

#endif
