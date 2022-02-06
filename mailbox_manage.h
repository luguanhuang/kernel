
#ifndef _MAILBOX_MANAGE_H
#define _MAILBOX_MANAGE_H

//mbx data info
struct data_info
{
	char *data;//dynamic data
	int len;//data len
	struct list_head	n_list1;		/* d-linked list head */
};

//mbx data structure
struct mbx_info 
{
	unsigned long id;//mbx id
	struct list_head	n_list;		/* mbox d-linked list head */
	struct list_head	n_datalist;		/*data d-linked list head */
	int data_cnt;//data count
};

#endif
