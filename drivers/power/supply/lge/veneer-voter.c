#define pr_fmt(fmt) "CHG: [VOTER] %s: " fmt, __func__
#define pr_voter(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#define pr_dbg_voter(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#include "veneer-primitives.h"

/* DO NOT export voter_list */
struct voter_list {
	struct list_head head;
	struct mutex	 lock;
	enum voter_type	 type;
	struct votable *votable;
};

// at this time, we have 5 voter classes for IUSB, IBAT, IDC, VFLOAT and HVDCP.
// If you need to add other voter class, add here and update the total list
// for convenient iteration.
static struct voter_list voter_list_iusb = {
	.head = LIST_HEAD_INIT(voter_list_iusb.head),
	.lock = __MUTEX_INITIALIZER(voter_list_iusb.lock),
	.type = VOTER_TYPE_IUSB,
};
static struct voter_list voter_list_ibat = {
	.head = LIST_HEAD_INIT(voter_list_ibat.head),
	.lock = __MUTEX_INITIALIZER(voter_list_ibat.lock),
	.type = VOTER_TYPE_IBAT,
};
static struct voter_list voter_list_idc = {
	.head = LIST_HEAD_INIT(voter_list_idc.head),
	.lock = __MUTEX_INITIALIZER(voter_list_idc.lock),
	.type = VOTER_TYPE_IDC,
};
static struct voter_list voter_list_vfloat = {
	.head = LIST_HEAD_INIT(voter_list_vfloat.head),
	.lock = __MUTEX_INITIALIZER(voter_list_vfloat.lock),
	.type = VOTER_TYPE_VFLOAT,
};

static struct voter_list* voter_lists [] = {
	&voter_list_iusb, &voter_list_ibat, &voter_list_idc, &voter_list_vfloat, NULL,
};

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////
static struct voter_list* 	voter_list_get_by_type(enum voter_type type);
static struct voter_list* 	voter_list_get_by_entry(struct voter_entry* entry);
static int 			atomic_voter_id(void);
///////////////////////////////////////////////////////////////////////////////

static struct voter_list* voter_list_get_by_type(enum voter_type type)
{
	int index;

	switch (type) {
	case VOTER_TYPE_IUSB:
		index = 0;
		break;
	case VOTER_TYPE_IBAT:
		index = 1;
		break;
	case VOTER_TYPE_IDC:
		index = 2;
		break;
	case VOTER_TYPE_VFLOAT:
		index = 3;
		break;
	default: pr_voter("Invalid limit voter type %d\n", type);
		index = ARRAY_SIZE(voter_lists) - 1;
		break;
	}

	return voter_lists[index];
}

static struct voter_list* voter_list_get_by_entry(struct voter_entry* entry)
{
	return entry ? voter_list_get_by_type(entry->type) : NULL;
}

static int atomic_voter_id(void)
{
	static int voter_id = 0;
	return voter_id++;
}

struct voter_entry* veneer_voter_search(enum voter_type type, const char* name)
{
	struct voter_list* list_of = voter_list_get_by_type(type);
	struct voter_entry* iter;

	if (list_of) {
		list_for_each_entry(iter, &list_of->head, node) {
			if (!strcmp(iter->name, name)) {
				return iter;
			}
		}
	}

	return NULL;
}

bool veneer_voter_enabled_fake_ui(enum voter_type type)
{
	struct voter_list*	list = voter_list_get_by_type(type);
	bool ret = false;
	struct voter_entry* iter;

	if (list) {
		list_for_each_entry(iter, &list->head, node) {
			if (iter->limit != VOTE_TOTALLY_RELEASED) {
				if (iter->fakeui) {
					ret = true;
					break;
				}
			}
		}
	}

	return ret;
}

enum charging_suspended veneer_voter_suspended(enum voter_type type)
{
	struct voter_list*	list = voter_list_get_by_type(type);
	enum charging_suspended	ret = CHARGING_NOT_SUSPENDED;
	struct voter_entry* iter;

	if (list) {
		list_for_each_entry(iter, &list->head, node) {
			if (iter->limit == VOTE_TOTALLY_BLOCKED) {
				if (iter->fakeui) {
					ret = CHARGING_SUSPENDED_WITH_FAKE_OFFLINE;
					break;
				}
				else
					ret = CHARGING_SUSPENDED_WITH_ONLINE;
			}
		}
	}

	return ret;
}

#define VOTER		"PASSOVER"
void veneer_voter_passover(enum voter_type type, int limit, bool enable)
{
	struct voter_list* list_of = voter_list_get_by_type(type);
	int rc =  0;

	if (list_of) {
		rc = vote_priority(list_of->votable, VOTER, enable && limit != VOTE_TOTALLY_RELEASED, limit, PRIORITY_HIGH);
		if (rc)
			pr_voter("Error of vote(%s, %s) rc = %d\n", vote_title(type), VOTER, rc);
	}
	else
		pr_voter("Couldn't find the list of %d\n", type);

}

bool veneer_voter_register(struct voter_entry* entry, const char* name, enum voter_type type, bool fakeui)
{
	struct voter_list* list_to = voter_list_get_by_type(type);

	if (!veneer_voter_search(type, name)) {
		// Building constants
		strlcpy(entry->name, name, VOTER_NAME_LENGTH);
		entry->type = type;
		entry->fakeui = fakeui;

		// Building runtime default variables
		entry->limit = VOTE_TOTALLY_RELEASED;

		// Building private members
		mutex_lock(&list_to->lock);
		entry->id = atomic_voter_id();
		list_add(&entry->node, &list_to->head);
		mutex_unlock(&list_to->lock);

		return true;
	}
	else {
		pr_voter("duplicated registering %s\n", name);
		return false;
	}
}

void veneer_voter_unregister(struct voter_entry* entry)
{
	struct voter_list* list = voter_list_get_by_entry(entry);

	if (entry && list) {
		mutex_lock(&list->lock);
		list_del(&entry->node);
		mutex_unlock(&list->lock);
	}
	else
		pr_voter("invalid voter to unregister\n");
}

void veneer_voter_set(struct voter_entry* voter, int limit)
{
	struct voter_list* list_of;
	int rc =  0;

	if (!voter) {
		pr_voter("voter is NULL\n");
		return;
	}

	if (voter->limit == limit) {
		if (limit != VOTE_TOTALLY_RELEASED)
			pr_dbg_voter("voting values(%d) are same for %s, %s\n",
				limit, voter->name, vote_title(voter->type));
		return;
	}

	list_of = voter_list_get_by_entry(voter);
	if (list_of) {
		rc = vote(list_of->votable, voter->name, limit != VOTE_TOTALLY_RELEASED, limit);
		if (rc)
			pr_voter("Error of vote(%s, %s) rc = %d\n", vote_title(voter->type), voter->name, rc);

		voter->limit = limit;
	}
	else
		pr_voter("Couldn't find the list of %s\n", voter->name);
}

void veneer_voter_rerun(struct voter_entry* voter)
{
	struct voter_list* list_of;

	if (!voter) {
		pr_voter("voter is NULL\n");
		return;
	}

	list_of = voter_list_get_by_entry(voter);
	if (list_of) {
		rerun_election(list_of->votable);
	}
	else
		pr_voter("Couldn't find the list of %s\n", voter->name);
}

void veneer_voter_release(struct voter_entry* voter)
{
	veneer_voter_set(voter, VOTE_TOTALLY_RELEASED);
}

bool veneer_voter_enabled(struct voter_entry* voter)
{
	return voter->limit != VOTE_TOTALLY_RELEASED;
}

void veneer_voter_find_votable(enum voter_type type, const char *name)
{
	struct voter_list* list_of = voter_list_get_by_type(type);

	list_of->votable = find_votable(name);
	if (IS_ERR(list_of->votable)) {
		pr_voter("Error to get %s votable\n", name);
		list_of->votable = NULL;
	}
}

bool veneer_voter_create(void)
{
	veneer_voter_find_votable(VOTER_TYPE_IUSB, "USB_ICL");
	veneer_voter_find_votable(VOTER_TYPE_IBAT, "FCC");
	veneer_voter_find_votable(VOTER_TYPE_IDC, "DC_ICL");
	veneer_voter_find_votable(VOTER_TYPE_VFLOAT, "FV");

	return true;
}

void veneer_voter_destroy(void) {}
