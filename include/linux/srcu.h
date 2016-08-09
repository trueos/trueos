#ifndef	_LINUX_SRCU_H_
#define	_LINUX_SRCU_H_

#include <sys/param.h>
#include <ck_epoch.h>

struct srcu_struct {
	ck_epoch_record_t *ss_epoch_record;
};
int srcu_read_lock(struct srcu_struct *sp);
void srcu_read_unlock(struct srcu_struct *sp, int idx);
void synchronize_srcu(struct srcu_struct *sp);
int init_srcu_struct(struct srcu_struct *sp);
void cleanup_srcu_struct(struct srcu_struct *sp);

void srcu_barrier(struct srcu_struct *sp);

#endif					/* _LINUX_SRCU_H_ */
