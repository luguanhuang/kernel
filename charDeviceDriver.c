/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <asm/uaccess.h>    /* for put_user */
//#include <charDeviceDriver.h>
#include "charDeviceDriver.h"
#include <linux/uaccess.h>


MODULE_LICENSE("GPL");

#define FREEIF(p) if(p){kfree(p);p=0;}

static LIST_HEAD(file_sys);

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




DEFINE_MUTEX  (devLock);

static int counter = 0;


/*
 * This function is called when the module is loaded
 * You will not need to modify it for this assignment
 */
int init_module(void)
{
    Major = register_chrdev(0, DEVICE_NAME, &fops);

    if (Major < 0) {
      printk(KERN_ALERT "Registering char device failed with %d\n", Major);
      return Major;
    }

    printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
    printk(KERN_INFO "the driver, create a dev file with\n");
    printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
    printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
    printk(KERN_INFO "the device file.\n");
    printk(KERN_INFO "Remove the device file and module when done.\n");

    return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
    /*  Unregister the device */
	struct msg_info *msgInfo = NULL;

    unregister_chrdev(Major, DEVICE_NAME);
	
	list_for_each_entry(msgInfo, &file_sys, n_list)
	{
		 list_del(&msgInfo->n_list);
		 FREEIF(msgInfo);
		 break;
	}
	
	printk("cleanup_module\n");
}

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
    sprintf(msg, "I already told you %d times Hello world!\n", counter++);
    msg_Ptr = msg;
    try_module_get(THIS_MODULE);
    return SUCCESS;
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

int get_msg_size(void)
{
	int cnt = 0;
	struct msg_info *msgInfo = NULL;
	list_for_each_entry(msgInfo, &file_sys, n_list)
	{
		cnt++;
	}

	return cnt;
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
     #if 0
    int bytes_read = 0;

    /* result of function calls */

    /*
     * If we're at the end of the message, 
     * return 0 signifying end of file 
     */
     
    if (*msg_Ptr == 0)
        return 0;

    /* 
     * Actually put the data into the buffer 
     */
    while (length && *msg_Ptr) {

        /* 
         * The buffer is in the user data segment, not the kernel 
         * segment so "*" assignment won't work.  We have to use 
         * put_user which copies data from the kernel data segment to
         * the user data segment. 
         */
        result = put_user(*(msg_Ptr++), buffer++);
        if (result != 0) {
                 return -EFAULT;
        }
            
        length--;
        bytes_read++;
    }

    /* 
     * Most read functions return the number of bytes put into the buffer
     */
    return bytes_read;
	#else
	struct msg_info *msgInfo = NULL;
	if (list_empty(&file_sys))
	{
		return -EAGAIN;
	}

	if(length > MAX_BUF_LEN)
       length=MAX_BUF_LEN;
	
	
	list_for_each_entry(msgInfo, &file_sys, n_list)
	{
		 copy_to_user(buffer, msgInfo->msgInfo, length);
		 list_del(&msgInfo->n_list);
		 FREEIF(msgInfo);
		 break;
	}
	
	  // 
   
    printk("user read data from driver\n");
    return length;
	#endif
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello  */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
   	//printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
    //return -EINVAL;

	if(len > MAX_BUF_LEN)
	{
		return -EINVAL;
	}

	if (get_msg_size() >= MAX_MSG_SIZE)
	{
		return -EAGAIN;
	}
	
	struct msg_info *msgInfo = kzalloc(sizeof(struct msg_info), GFP_ATOMIC);
	if (msgInfo == NULL) {
		return -ENOMEM;
	}
	
	memset(msgInfo->msgInfo, 0, sizeof(msgInfo->msgInfo));
	copy_from_user(msgInfo->msgInfo , buff, len);
	list_add(&msgInfo->n_list, &file_sys);

	//len = MAX_BUF_LEN;
	//copy_from_user(drv_buf , buff, len);
	//WRI_LENGTH = len;
	printk("user write data to driver\n");
	return len;
}
