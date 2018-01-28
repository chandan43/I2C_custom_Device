#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

struct i2c_Arduino_prv {
	struct i2c_client client_prv;
	struct kobject *at24_kobj;
        char *ptr;
};

static int ledstatus;
struct i2c_Arduino_prv *prv=NULL;

static unsigned write_timeout = 25; /*default timeout for normal I2c  devices */

/* Time_before(a,b) returns true if the time a is before time b. */
static ssize_t led_on(struct i2c_client *client, const char *buf,size_t count)
{
	struct i2c_msg msg;
	unsigned long timeout, write_time;
	ssize_t status;
	char msgbuf[40];
	
	memset(msgbuf, '\0', sizeof(msgbuf));
//	msg.addr = client->addr;
	msg.addr = 0x48;
	
	/**/
	msg.flags = 0; /*0= write , 1 = read */
	msg.buf = msgbuf;

	memcpy(&msg.buf[0], buf, count); /*Data payload */
	
	msg.len = count;  /*first two byte also need to considre as actual len*/
	
	timeout = jiffies + msecs_to_jiffies(write_timeout);  /*changing into ms */
	
	do{
		
		write_time = jiffies;
		status = i2c_transfer(client->adapter, &msg, 1);
		if (status == 1)
			status = count;
		if (status == count)
			return count;
		msleep(1);

	}while(time_before(write_time, timeout));
	
	return -ETIMEDOUT;
}

static ssize_t led_status_write(struct kobject *kobj, struct kobj_attribute *attr,
		 const char *buf, size_t count)
{
	size_t ret;
	int d;
	ret = led_on(&prv->client_prv, buf, count);
	if (ret < 0) {
		pr_info("write failed\n");
		return ret;
	}
	ret=sscanf(buf,"%d",&d);
	if(ret < 0 || ret > 1)
		return -EINVAL;
	if(d == 0) {
		ledstatus = 0;
		pr_info("%s: LED OFF \n",__func__);  
	}else if(d == 1){
		 ledstatus = 1;
		 pr_info("%s: LED ON \n",__func__);  
	       }	
	return count;

}
static ssize_t led_status_show(struct kobject *kobj, struct kobj_attribute *attr, 
		char *buf)
{
	char rbuf;
	if(ledstatus) {
		rbuf = 1;
		pr_info("%s: Led is on...\n",__func__);
		return sprintf(buf,"%d\n",rbuf);
	}else {
		rbuf = 0;
		pr_info("%s: Led is off...\n",__func__);
		return sprintf(buf,"%d\n",rbuf);
	 }	
	return 0;
}

static struct kobj_attribute led  = __ATTR(ledstatus, 0660, led_status_show,led_status_write);

static struct attribute *attrs[] = {
        &led.attr,
        NULL,
};

static struct attribute_group attr_group = {
        .attrs = attrs,
};

static int i2c_Arduino_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	pr_info("%s: Custom Arduino Slave probed......\n",__func__);	
	prv=(struct i2c_Arduino_prv *)kzalloc(sizeof(struct i2c_Arduino_prv), GFP_KERNEL);		
	if(!prv){
		pr_info("Requested memory not allocated\n");
		return -ENOMEM;
	}
	prv->client_prv = *client;
	prv->at24_kobj=kobject_create_and_add("I2C-Arduino", NULL);
	if(!prv->at24_kobj)
		return -ENOMEM;
	ret= sysfs_create_group(prv->at24_kobj, &attr_group);
	if(ret)
		kobject_put(prv->at24_kobj);
	return 0;	
}

static int i2c_Arduino_remove(struct i2c_client *client)
{
	pr_info("Custom Arduino Slave removed\n");
	kfree(prv); 
	kobject_put(prv->at24_kobj);
	return 0;
}

static const struct i2c_device_id i2c_Arduino_ids[]={
	{ "i2c-arduino", 0x48 },
	{ }
};

static  struct i2c_driver i2c_Arduino_driver = {
	.driver = {
		.name = "Custom-Arduino-Slave",
		.owner = THIS_MODULE,
	},
	.probe    = i2c_Arduino_probe,
	.remove   = i2c_Arduino_remove,
	.id_table = i2c_Arduino_ids,
};

/**
 * module_i2c_driver() - Helper macro for registering a modular I2C driver
 * @__i2c_driver: i2c_driver struct
 *
 * Helper macro for I2C drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
module_i2c_driver(i2c_Arduino_driver);


MODULE_DESCRIPTION("I2c Driver for custom  Arduino SA Uno R3 : Slave Address : 0x48");
MODULE_AUTHOR("Chandan jha <beingchandanjha@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(".1");
