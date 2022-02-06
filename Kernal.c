/*
 *  reversi.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <asm/uaccess.h>    /* for put_user */
#include "Kernal.h"
#include <linux/uaccess.h>
#include <linux/string.h>
#include<linux/sched.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");

#define FREEIF(p) if(p){kfree(p);p=0;}

static int readcnt = 0;
static int task_queue_size = 0;
/* 
 * This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 *
 */

int total_proc = 0;
int firstLoad = 1;
static LIST_HEAD(file_sys);

DEFINE_MUTEX  (devLock);

/*
 * Methods
 */

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
    
    mutex_lock (&devLock);
    if (Device_Open) {
    mutex_unlock (&devLock);
    return -EBUSY;
    }
    Device_Open++;
    mutex_unlock (&devLock);
    //sprintf(msg, "I already told you %d times Hello world!\n", counter++);
    //msg_Ptr = msg;
    try_module_get(THIS_MODULE);
    return 0;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
    mutex_lock (&devLock);
    Device_Open--;      /* We're now ready for our next caller */
    mutex_unlock (&devLock);
    /* 
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module. 
     */
    module_put(THIS_MODULE);

    return 0;
}

void FreeProc(void)
{
	struct task_data *taskData, *next;
	list_for_each_entry_safe(taskData, next, &file_sys, n_list)
	{
		list_del(&taskData->n_list);
		FREEIF(taskData);
	}
}

int LoadProc(void)
{
	struct task_struct *task;
	int cnt = 0;
	int testcnt = 0;
	int total_proc = 0;
	struct task_data *taskData;
	StTaskData stTaskData;
	FreeProc();
	task_queue_size = 0;
	for_each_process(task) //Traversal process
	{
		total_proc++;
	}
	//printk("total_proc=%d", total_proc);
	for_each_process(task) //Traversal process
	{	
		stTaskData.taskInfo[cnt].pid = task->pid;
		stTaskData.taskInfo[cnt].ppid = task->parent->pid;
		stTaskData.taskInfo[cnt].cpu = task_cpu(task);
		stTaskData.taskInfo[cnt].state = task->state;
		testcnt++;
		//printk("Task (pid = %d) state=%ld cpu=%d ppid=%d comm=%s total_proc=%d testcnt=%d\n",
		//stTaskData.taskInfo[cnt].pid, stTaskData.taskInfo[cnt].state,
		//stTaskData.taskInfo[cnt].cpu, 
		//stTaskData.taskInfo[cnt].ppid, task->comm, total_proc,
		//testcnt);
		cnt++;
		if (cnt >= TASK__MAX_SIZE || testcnt == total_proc)//Exceeds the largest array
		//if (cnt >= TASK__MAX_SIZE)//Exceeds the largest array
		{
			taskData = kzalloc(sizeof(struct task_data), GFP_ATOMIC);
			if (taskData == NULL) 
			{
				printk("error");
				return -ENOMEM;
			}
	
			stTaskData.cnt = cnt;
			stTaskData.total = total_proc;
			memcpy(&taskData->stTaskData, &stTaskData, sizeof(stTaskData));
			list_add_tail(&taskData->n_list, &file_sys);
			cnt = 0;
			memset(&(stTaskData), 0, sizeof(stTaskData));
			task_queue_size++;
		}	
	}
	
	//printk("stTaskData.cnt=%d cnt=%d", taskData->stTaskData.cnt, cnt);
	return 0;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 * This is just an example about how to copy a message in the user space
 * You will need to modify this function
 */
static ssize_t device_read(struct file *filp,   /* see include/linux/fs.h   */
               char *buffer,    /* buffer to fill with data */
               size_t length,   /* length of the buffer     */
               loff_t * offset)
{
    /*
     * Number of bytes actually written to the buffer 
     */
    //printk("device_read: func begin readcnt=%d task_queue_size=%d", readcnt, task_queue_size);
	struct task_data *taskData;
	int i = 0;

	if (firstLoad == 1)
	{
		
		FreeProc();
		LoadProc();
		//printk("device_read: func end readcnt=%d task_queue_size=%d firstLoad=%d LoadProc 111", 
			//readcnt, task_queue_size, firstLoad);
		firstLoad = 0;
	}
	
	if (readcnt >= task_queue_size)
	{	
		FreeProc();
		LoadProc();
		//printk("device_read: func end readcnt=%d task_queue_size=%d firstLoad=%d LoadProc 222", 
			//readcnt, task_queue_size, firstLoad);
		readcnt = 0;	
	}

	list_for_each_entry(taskData, &file_sys, n_list)
	{
		if (i == readcnt)
		{
			taskData->stTaskData.total = task_queue_size;
			copy_to_user(buffer, &taskData->stTaskData, length);
			break;
		}
		
		i++;
	}

	readcnt++;
	//printk("device_read: func end readcnt=%d task_queue_size=%d", readcnt, task_queue_size);	
    return length;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello  */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{	
	return len;
}

static int __init misc_init(void)
{
    int error;
    error = misc_register(&misc_device);
    if (error) 
	{
        printk("can't misc_register :(\n");
        return error;
    }

    printk("misc_init:Call misc_register succeed\n");
    return 0;
}

static void __exit misc_exit(void)
{
	FreeProc();
    misc_deregister(&misc_device);
	
    printk("misc_exit:Call misc_exit succeed\n");
}

module_init(misc_init)
module_exit(misc_exit)

MODULE_DESCRIPTION("Simple Misc Driver");
//MODULE_AUTHOR("Nick Glynn <n.s.glynn@gmail.com>");
MODULE_LICENSE("GPL");

