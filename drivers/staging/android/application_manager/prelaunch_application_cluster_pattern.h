#ifndef _APPLICATION_PRELAUNCH_CLUSTER_PATTERN_H
#define _APPLICATION_PRELAUNCH_CLUSTER_PATTERN_H

#include "application.h"

extern struct application_manager prelaunch_cluster_pattern_app_manager;

int extract_prelaunch_cluster_usage_pattern(struct application_manager* app_manager);
int match_prelaunch_cluster_usage_pattern(struct application_manager* app_manager);

int manage_prelaunch_cluster_applications(struct application_manager* app_manager);

#endif
