#ifndef UDM_BOOLEAN_H
#define UDM_BOOLEAN_H

extern int  UdmCalcBoolItems(UDM_STACK_ITEM *items,
                             size_t nitems,
                             char *count);

extern void UdmStackItemListFree(UDM_STACKITEMLIST *L);

extern int  UdmStackItemListCopy(UDM_STACKITEMLIST *Dst,
                                 UDM_STACKITEMLIST *Src,
                                 int search_mode);

extern int  UdmStackItemListAdd(UDM_STACKITEMLIST *List,
                                UDM_STACK_ITEM *item);

#endif
