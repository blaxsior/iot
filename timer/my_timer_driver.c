#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("blaxsior GNU/Linux");
MODULE_DESCRIPTION("kernel_timer");

#define SELF_DEV_NAME "my_timer_driver"
#define SELF_CLASS_NAME "dummy"

// gpio pin
#define GPIO_LED 591 // 20
#define GPIO_LED_NAME "gpio-25-led"

// variable for timer
static struct timer_list my_timer;

// device
static struct cdev self_device;
// class
static struct class *self_class;
// device number
static dev_t self_device_num;

// buffer
static char buffer[255];
static ssize_t buffer_ptr;

void timer_callback(struct timer_list * data) {
  gpio_set_value(GPIO_LED, 0); // turn off
  printk("led out\n");
}


ssize_t read_operation(struct file *f, char __user *user_buffer, size_t count, loff_t *offset)
{
  int to_copy = min_t(ssize_t, count, buffer_ptr);
  int not_copied = copy_to_user(user_buffer, buffer, to_copy);
  // buffer_ptr = 0;
  int delta = to_copy - not_copied;

  return delta;
}

ssize_t write_operation(struct file *f, const char __user *user_buffer, size_t count, loff_t *offset)
{
  int to_copy = min_t(ssize_t, count, sizeof(buffer));
  int not_copied = copy_from_user(buffer, user_buffer, to_copy);
  buffer_ptr = to_copy;

  int delta = to_copy - not_copied;
  return delta;
}

static int driver_open(struct inode *device_file, struct file *instance)
{
  printk("dev_nr open was called\n");
  return 0;
}

/**
 * @brief called when device file is closed
 */
static int driver_close(struct inode *device_file, struct file *instance)
{
  printk("dev_nr close was called\n");
  return 0;
}

static struct file_operations operations = {
    .owner = THIS_MODULE,
    .read = read_operation,
    .write = write_operation,
    .open = driver_open,
    .release = driver_close
};


static int __init ModuleInit(void)
{
  // number -> device -> class
  // 0 => normal
  // minus => error code
  if(alloc_chrdev_region(&self_device_num, 0, 1, SELF_DEV_NAME) < 0) {
    printk("device number not allocated\n");
    return -1;
  }

  self_class = class_create(SELF_CLASS_NAME);
  if(self_class == NULL) {
    printk("device class cannot be created");
    goto ClassError;
  }

  // create device
  if(device_create(self_class, NULL, self_device_num, NULL, SELF_DEV_NAME) == NULL) {
    printk("cannot create device\n");
    goto Device_Error;
  }

  cdev_init(&self_device, &operations);

  // cannot create cdev
  if(cdev_add(&self_device, self_device_num, 1) < 0) {
    printk("cannot add char device\n");
    goto AddError;
  }

  if(gpio_request(GPIO_LED, GPIO_LED_NAME)) {
    printk("cannot request gpio %s\n", GPIO_LED_NAME);
    goto AddError;
  }

  if(gpio_direction_output(GPIO_LED, 0)) {
    printk("cannot set gpio %s to output\n", GPIO_LED_NAME);
    goto GPIO_Error;
  }

  gpio_set_value(GPIO_LED, 1);
  timer_setup(&my_timer, timer_callback, 0);
  mod_timer(&my_timer, jiffies + msecs_to_jiffies(1000));

  printk("kernel timer init\n");
  return 0;

GPIO_Error:
  gpio_free(GPIO_LED);
AddError:
  device_destroy(self_class, self_device_num);
Device_Error:
  class_destroy(self_class);
ClassError:
  unregister_chrdev(self_device_num, SELF_DEV_NAME);
  return -1;
}

static void __exit ModuleExit(void)
{
  printk("kernel timer exit\n");
  gpio_free(GPIO_LED);
  cdev_del(&self_device);
  device_destroy(self_class, self_device_num);
  class_destroy(self_class);
  unregister_chrdev_region(self_device_num, 1);
}

module_init(ModuleInit);
module_exit(ModuleExit);
