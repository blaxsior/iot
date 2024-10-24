#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("blaxsior GNU/Linux");
MODULE_DESCRIPTION("gpio driver => LED / button");

/* device & device classes */
// save device number
static dev_t my_device_num;
// save class
static struct class *my_class;
// save device
static struct cdev my_device;

#define SELF_MODULE_NAME "gpio_driver"
#define SELF_MODULE_CLASS "dummy_class"

#define BTN_NO 585 // 14
#define LED_NO 592 // 21

static int is_light_on = 0;
static int irq_no = -1;
/**
 * @brief toggle led light state
 */
static irqreturn_t button_interrupt(int irq, void *data)
{
  is_light_on = !is_light_on;
  gpio_set_value(LED_NO, is_light_on);
  printk("GPIO Interrupt! LED is now %s\n", is_light_on ? "ON" : "OFF");
  return IRQ_HANDLED;
}

/**
 * @brief read data from buffer
 */
static ssize_t driver_read(struct file *File, char *user_buffer, size_t count, loff_t *offs)
{
  // File: data source
  // user_buffer: data destination
  // count: length of user buffer

  int to_copy, not_copied, delta;
  char tmp[3] = " \n";

  printk("value of led: %d\n", gpio_get_value(LED_NO));
  tmp[0] = gpio_get_value(LED_NO) + '0';
  printk("button state: %d\n", gpio_get_value(BTN_NO));

  /* get amount of data to copy */
  to_copy = min_t(int, count, sizeof(tmp));

  not_copied = copy_to_user(user_buffer, &tmp, to_copy);

  // calculate not copied data delta value
  delta = to_copy - not_copied;

  // return remain length
  return delta;
}

/**
 * @brief write data to buffer
 */
static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs)
{
  int to_copy, not_copied, delta;
  char value;
  /* get amount of data to copy */
  to_copy = min_t(int, count, sizeof(value));

  /* copy data to user */
  // cannot use memcopy, instead use copy_to_user
  not_copied = copy_from_user(&value, user_buffer, to_copy);
  /* set led */
  switch (value)
  {
  case '0':
    gpio_set_value(LED_NO, 0);
    is_light_on = 0;
    break;
  case '1':
    gpio_set_value(LED_NO, 1);
    is_light_on = 1;
    break;
  }
  // calculate not copied data delta value
  delta = to_copy - not_copied;

  // return remain length
  return delta;
}

/**
 * @brief called when device file is opened
 */
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

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .read = driver_read,
    .write = driver_write};

/**
 * @brief called when module is loaded into kernel
 */
static int __init ModuleInit(void)
{
  printk("Hello, Kernel World!\n"); // cannot printf => no commandline
  // allocate device number (dyn & MAJOR)
  if (alloc_chrdev_region(&my_device_num, 0, 1, SELF_MODULE_NAME) < 0)
  {
    printk("device nr could not allocated\n");
    return -1;
  }
  printk("read_write - device nr. major: %d, minor: %d \n", MAJOR(my_device_num), MINOR(my_device_num));

  /* create device class */
  if ((my_class = class_create(SELF_MODULE_CLASS)) == NULL)
  {
    printk("device class cannot be created\n");
    goto ClassError;
  }

  /* create device file*/
  if (device_create(my_class, NULL, my_device_num, NULL, SELF_MODULE_NAME) == NULL)
  {
    printk("cannot create device file\n");
    goto FileError;
  }

  /* initialize device file */
  cdev_init(&my_device, &fops);

  /* register device to kernel */
  if (cdev_add(&my_device, my_device_num, 1) == -1)
  {
    printk("registering of device to kernel failed!\n");
    goto AddError;
  }
  /* gpio init 21 ( led ) */
  if (gpio_request(LED_NO, "rpi-gpio-21"))
  {
    printk("cannot allocate gpio 21\n");
    goto AddError;
  }

  /* set gpio 21 as output direction */
  if (gpio_direction_output(LED_NO, 0))
  {
    printk("cannot set gpio 21 to output\n");
    goto GPIO21Error;
  }

  /* gpio init 15 (button) */
  if (gpio_request(BTN_NO, "rpi-gpio-14"))
  {
    printk("cannot allocate gpio 14\n");
    goto AddError;
  }

  /* set gpio 15 as input direction */
  if (gpio_direction_input(BTN_NO))
  {
    printk("cannot set gpio 14 to output\n");
    goto GPIO15Error;
  }

  /* request irq for button */
  if ((irq_no = gpio_to_irq(BTN_NO)) < 0)
  {
    printk("cannot request irq for gpio 14\n");
    goto GPIO15Error;
  }

  int result = request_irq(irq_no, button_interrupt,
                           IRQF_TRIGGER_RISING,
                           "button_handler", NULL);

  if (result) // cannot use irq
  {
    printk("cannot use irq for button\n");
    goto GPIO15IRQError;
  }
  // gpio_set_value(BTN_NO, 1);

  return 0;

GPIO15IRQError:
  // free_irq(BTN_NO, NULL);
GPIO15Error:
  gpio_free(BTN_NO);
GPIO21Error:
  gpio_free(LED_NO);
AddError:
  device_destroy(my_class, my_device_num);
FileError:
  class_destroy(my_class);
ClassError:
  unregister_chrdev(my_device_num, SELF_MODULE_NAME);
  return -1;
}

/**
 * @brief called when module is removed from kernel
 */
static void __exit ModuleExit(void)
{
  free_irq(irq_no, NULL);
  // gpio_set_value(BTN_NO, 0);
  gpio_free(BTN_NO);
  gpio_set_value(LED_NO, 0);
  gpio_free(LED_NO);
  cdev_del(&my_device);
  device_destroy(my_class, my_device_num);
  class_destroy(my_class);
  unregister_chrdev_region(my_device_num, 1);
  printk("Bye, Kernel World...\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
