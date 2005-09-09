#ifndef _KLONE_QUEUE_H_
#define _KLONE_QUEUE_H_
#include <klone/sysqueue.h>

#ifndef LIST_ENTRY_NULL
#define LIST_ENTRY_NULL { NULL, NULL }
#endif

#ifndef LIST_FOREACH
#define LIST_FOREACH(var, head, entries)	        				    \
    for (var = (head)->lh_first; var != NULL; var = var->entries.le_next)
#endif

#ifndef LIST_FIRST
#define	LIST_FIRST(head)		((head)->lh_first)
#endif 

#ifndef TAILQ_ENTRY_NULL
#define TAILQ_ENTRY_NULL { NULL, NULL }
#endif

#ifndef TAILQ_FIRST
#define	TAILQ_FIRST(head)		((head)->tqh_first)
#endif 

#ifndef TAILQ_END
#define	TAILQ_END(head)			NULL
#endif

#ifndef TAILQ_NEXT
#define	TAILQ_NEXT(elm, field)		((elm)->field.tqe_next)
#endif

#ifndef TAILQ_LAST
#define TAILQ_LAST(head, headname)					\
	(*(((struct headname *)((head)->tqh_last))->tqh_last))
#endif

#ifndef TAILQ_PREV
#define TAILQ_PREV(elm, headname, field)				\
	(*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#endif

#ifndef TAILQ_EMPTY
#define	TAILQ_EMPTY(head)						\
	(TAILQ_FIRST(head) == TAILQ_END(head))
#endif

#ifndef TAILQ_FOREACH
#define TAILQ_FOREACH(var, head, field)					\
	for((var) = TAILQ_FIRST(head);					\
	    (var) != TAILQ_END(head);					\
	    (var) = TAILQ_NEXT(var, field))
#endif

#ifndef TAILQ_FOREACH_REVERSE
#define TAILQ_FOREACH_REVERSE(var, head, field, headname)		\
	for((var) = TAILQ_LAST(head, headname);				\
	    (var) != TAILQ_END(head);					\
	    (var) = TAILQ_PREV(var, headname, field))
#endif

#endif
