
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>

#include "mailbox_manage.h"


static LIST_HEAD(mbx_list);//mbox list
static int mbox_size = 0;//mbox size
/* lock for mbox table */
static DEFINE_RWLOCK(__mbox_lock);

//create mbox
SYSCALL_DEFINE1(create_mbox_421, unsigned long, id) 
{
	//ret = capable(CAP_DAC_OVERRIDE);
	struct mbx_info *mbxInfo;
	
	int ret = capable(CAP_SYS_ADMIN);//Only root can operate this function
	if (ret == 0)
	{
		return -EPERM;	
	}
	
	list_for_each_entry(mbxInfo, &mbx_list, n_list) 
	{
		if (id == mbxInfo->id) //id is exist yet
		{
			return -EEXIST;
		}
	}

	mbxInfo = kzalloc(sizeof(struct mbx_info), GFP_ATOMIC);//Dynamically allocate memory
	if (mbxInfo == NULL) 
	{
		return -ENOMEM;
	}

	mbxInfo->id = id;
	//list_add(&mbxInfo->n_list, &mbx_list);//mbox add to the list
	list_add_tail(&mbxInfo->n_list, &mbx_list);//mbox add to the list
	INIT_LIST_HEAD(&mbxInfo->n_datalist);//init datalist
	mbxInfo->data_cnt = 0;//set data count to zero
	mbox_size++;//set mbox to zero
	//printk("Call create_mbx_421 succeed id=%lu mbox_size=%d\n", id, mbox_size);
	
	return 0;
}

//remove bmox and data info
SYSCALL_DEFINE0(reset_mbox_421) 
{
	struct mbx_info *mbxInfo, *next;
	struct data_info *dataInfo, *next1;
	
	int ret = capable(CAP_SYS_ADMIN);//Only root can operate this function
	if (ret == 0)
	{
		return -EPERM;	
	}

	write_lock_bh(&__mbox_lock);//add write lock
	list_for_each_entry_safe(mbxInfo, next, &mbx_list, n_list) 
	{
		list_for_each_entry_safe(dataInfo, next1, &mbxInfo->n_datalist, n_list1) 
		{
			list_del(&dataInfo->n_list1);//delete data list
			kfree(dataInfo->data);//delete data
			kfree(dataInfo);
		}

		list_del(&mbxInfo->n_list);//delete list
		kfree(mbxInfo);// free memory
	}

	write_unlock_bh(&__mbox_lock);//remove write lock
	mbox_size = 0;//set mbox to zero

	return 0;
}

// remove mbox info
SYSCALL_DEFINE1(remove_mbox_421, unsigned long, id) 
{
	struct mbx_info *mbxInfo, *next;
	int ret;
	ret = capable(CAP_SYS_ADMIN);//Only root can operate this function
	if (ret == 0)
	{
		return -EPERM;	
	}
	write_lock_bh(&__mbox_lock);//add write lock
	list_for_each_entry_safe(mbxInfo, next, &mbx_list, n_list) 
	{
		if (id == mbxInfo->id)
		{			
			if (list_empty(&mbxInfo->n_datalist))//if list is empty
			{
				list_del(&mbxInfo->n_list);//delete list
				kfree(mbxInfo);//free memory
				mbox_size--;//decrement mbox size
				write_unlock_bh(&__mbox_lock);//remove write lock
				return 0;
			}
		}
	}
	write_unlock_bh(&__mbox_lock);//remove write lock
	return -ENOENT;
}

SYSCALL_DEFINE3(send_msg_421, unsigned long, id, const unsigned char __user *, msg, long, n) 
{
	struct mbx_info *mbxInfo;
	struct data_info *dataInfo;
	if (!msg || n < 0) //param error
	{
		return -EINVAL;
	}
	write_lock_bh(&__mbox_lock);//add write lock
	list_for_each_entry(mbxInfo, &mbx_list, n_list) 
	{
		//find the same id
		if (id == mbxInfo->id) 
		{
			dataInfo = kzalloc(sizeof(struct data_info), GFP_ATOMIC);//Dynamically allocate memory
			if (dataInfo == NULL) 
			{
				write_unlock_bh(&__mbox_lock);//remove write lock
				return -ENOMEM;
			}

			
			dataInfo->data = kzalloc(n, GFP_ATOMIC);//Dynamically allocate memory
			if (dataInfo->data == NULL) 
			{
				write_unlock_bh(&__mbox_lock);//remove write lock
				return -ENOMEM;
			}
			
			memcpy(dataInfo->data, msg, n);//copy data
			dataInfo->len = n;//add data len
			list_add_tail(&dataInfo->n_list1, &mbxInfo->n_datalist);
			mbxInfo->data_cnt++;//increment data count
			write_unlock_bh(&__mbox_lock);//remove write lock
			return n;
		}
	}	
	write_unlock_bh(&__mbox_lock);//remove write lock
	return -ENOENT;
}

SYSCALL_DEFINE3(recv_msg_421, unsigned long, id, unsigned char __user*, msg, long, n) 
{
	struct mbx_info *mbxInfo, *next;
	struct data_info *dataInfo;
	int size;
	
	if (!msg || n <= 0) //param error
	{
		return -EINVAL;
	}

	write_lock_bh(&__mbox_lock);//add write lock
	list_for_each_entry_safe(mbxInfo, next, &mbx_list, n_list) 
	{
		if (id == mbxInfo->id)//find the same id
		{
			if (!list_empty(&mbxInfo->n_datalist))//if list is not empty
			{
				dataInfo = list_first_entry(&mbxInfo->n_datalist, typeof(*dataInfo), n_list1);//get list first node
				size = n > dataInfo->len ? dataInfo->len : n;//get min size
				memcpy(msg, dataInfo->data, size);//copy data
				list_del(&dataInfo->n_list1);//delete list
				kfree(dataInfo->data);//delete data
				kfree(dataInfo);
				mbxInfo->data_cnt--;//decrement data count
				write_unlock_bh(&__mbox_lock);//remove write lock
				return size;	
			}
			else
			{
				write_unlock_bh(&__mbox_lock);//remove write lock
				return 0;		
			}
		}
	}	
	write_unlock_bh(&__mbox_lock);//remove write lock
	return -ENOENT;
}

SYSCALL_DEFINE1(count_msg_421, unsigned long, id) 
{
	struct mbx_info *mbxInfo;

	//add read lock
	read_lock_bh(&__mbox_lock);
	list_for_each_entry(mbxInfo, &mbx_list, n_list) 
	{
		if (id == mbxInfo->id)//find same id
		{			
			read_unlock_bh(&__mbox_lock);//remove read lock
			return mbxInfo->data_cnt;//return data count
		}
	}
	
	read_unlock_bh(&__mbox_lock);//remove read lock
	return -ENOENT;
	        
}

SYSCALL_DEFINE1(len_msg_421, unsigned long, id) 
{
	struct mbx_info *mbxInfo;
	struct data_info *dataInfo;

	read_lock_bh(&__mbox_lock);//add read lock
	list_for_each_entry(mbxInfo, &mbx_list, n_list) 
	{
		if (id == mbxInfo->id)
		{
			if (!list_empty(&mbxInfo->n_datalist))//if list is not empty
			{
				dataInfo = list_first_entry(&mbxInfo->n_datalist, typeof(*dataInfo), n_list1);//get list first node
				read_unlock_bh(&__mbox_lock);//remove read lock
				return dataInfo->len;	
			}
			else
			{
				read_unlock_bh(&__mbox_lock);//remove read lock
				return 0;	
			}
		}
	}
	
	read_unlock_bh(&__mbox_lock);//remove read lock
	return -ENOENT;
	        
}

SYSCALL_DEFINE1(print_mbox_421, unsigned long, id) 
{
	struct mbx_info *mbxInfo;
	struct data_info *dataInfo;
	int i;
	char data[1024];
	int ret = 0;
	int total = 0;
	int cnt = 0;

	read_lock_bh(&__mbox_lock);//add read lock
	printk("print_mbox_421: func begin\n");
	list_for_each_entry(mbxInfo, &mbx_list, n_list) 
	{
		if (id == mbxInfo->id)
		{
			//Traverse the linked list
			list_for_each_entry(dataInfo, &mbxInfo->n_datalist, n_list1) 
			{
				ret = 0;
				total = 0;
				memset(data, 0, sizeof(data));

				for (i=0; i<dataInfo->len; i++)
				{
					ret = snprintf(data + total, sizeof(data) - total, "c ", dataInfo->data[i]);
					total += ret;
					if (total % 16 == 0)//Line break every 16 bytes
					{
						printk("%s\n", data);//display data
						memset(data, 0, sizeof(data));
						ret = 0;
						total = 0;
					}
				}

				cnt++;
				if (cnt < mbxInfo->data_cnt)
					printk("%s\n---\n", data);
			}
		}
	}

	printk("print_mbox_421: func end\n");
	read_unlock_bh(&__mbox_lock);//remove read lock
	return 0;
	        
}


SYSCALL_DEFINE2(list_mbox_421, unsigned long __user *, mbxes, long, k) 
{
	struct mbx_info *mbxInfo;
	int cnt = 0;

		//param error
	if (!mbxes || k <= 0) 
	{
		return -EINVAL;
	}
	
	read_lock_bh(&__mbox_lock);//add read lock	

	//Traverse the linked list
	list_for_each_entry(mbxInfo, &mbx_list, n_list) 
	{
		if (cnt > k)//Copy up to K data
		{
			break;
		}

		mbxes[cnt] = mbxInfo->id;
		cnt++;
	}
	read_unlock_bh(&__mbox_lock);//remove read lock
	return cnt;
}

SYSCALL_DEFINE0(count_mbox_421) 
{	
	return mbox_size;
}

SYSCALL_DEFINE3(peek_msg_421, unsigned long, id, unsigned char __user*, msg, long, n) 
{
	struct mbx_info *mbxInfo;
	struct data_info *dataInfo;
	int size;
	//param error
	if (!msg || n <= 0) 
	{
		return -EINVAL;
	}
	
	//add read lock
	read_lock_bh(&__mbox_lock);
	list_for_each_entry(mbxInfo, &mbx_list, n_list) 
	{
		if (id == mbxInfo->id)//find the same id
		{
			if (!list_empty(&mbxInfo->n_datalist))//if list is not empty
			{
				dataInfo = list_first_entry(&mbxInfo->n_datalist, typeof(*dataInfo), n_list1);//get list first node
				size = n > dataInfo->len ? dataInfo->len : n;//get min size
				memcpy(msg, dataInfo->data, size);//copy data
				read_unlock_bh(&__mbox_lock);//remove read lock
				return size;	
			}
			else
			{
				read_unlock_bh(&__mbox_lock);//remove read lock
				return 0;	
			}
		}
	}	

	read_unlock_bh(&__mbox_lock);//remove read lock
	return -ENOENT;
}


