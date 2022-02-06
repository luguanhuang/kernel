/* Global definition for the example character device driver */
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define DEVICE_NAME "process_list"	/* Dev name as it appears in /proc/devices   */

typedef struct
{	
	int pid;
	int ppid;
	long state;
	int cpu;
}StTaskInfo;

#define TASK__MAX_SIZE 40//Maximum size of the array

typedef struct
{
	int cnt;
	int total;
	StTaskInfo taskInfo[TASK__MAX_SIZE];	
}StTaskData;


struct task_data
{
	struct list_head	n_list;		/* d-linked list head */
	StTaskData stTaskData;			/* scheduler name */
};

/* 
 * Global variables are declared as static, so are global within the file. 
 */
static int Device_Open = 0;	/* Is device open?  
				 * Used to prevent multiple access to device */

static const struct file_operations fops = {
	.read 			= device_read,
    .write			= device_write,
    .open			= device_open,
    .release		= device_release,
    .llseek 		= no_llseek,
};

struct miscdevice misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &fops,
};

