#ifndef _LINUX_TOPOLOGY_H_
#define _LINUX_TOPOLOGY_H_

#include <linux/cpumask.h>


#ifndef parent_node
#define parent_node(node)	((void)(node),0)
#endif
#ifndef cpumask_of_node
#define cpumask_of_node(node)	((void)node, cpu_online_mask)
#endif
#ifndef pcibus_to_node
#define pcibus_to_node(bus)	((void)(bus), -1)
#endif

#ifndef cpumask_of_pcibus
#define cpumask_of_pcibus(bus)	(pcibus_to_node(bus) == -1 ?		\
				 cpu_all_mask :				\
				 cpumask_of_node(pcibus_to_node(bus)))
#endif

#endif /* _LINUX_TOPOLOGY_H_ */
