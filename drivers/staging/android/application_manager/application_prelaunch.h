#ifndef _APPLICATION_LAUNCH_H
#define _APPLICATION_LAUNCH_H

//#define BUF_SIZE	5120
#define BUF_SIZE	128
#define BUF_POINT(idx)	(namebuf + idx)

struct prelaunch_trigger {
	wait_queue_head_t event_wait;
	struct mutex lock;
	int event;
};

int prefault_task(struct task_struct *tsk);
int prelaunch_init(void);
int prelaunch_task(char *comm);

#endif

