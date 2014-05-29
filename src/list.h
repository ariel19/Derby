#pragma once

#include "types.h"

void list_init(struct list_user *);
void list_add_end(struct list_user *, struct user *);
void list_clean(struct list_user *);
