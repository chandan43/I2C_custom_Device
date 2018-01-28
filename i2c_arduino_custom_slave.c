#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

struct i2c_eeprom_prv {
	struct i2c_client client_prv;
	struct kobject *at24_kobj;
        char *ptr;
        unsigned int size;
        unsigned int pagesize;
        unsigned int address_width;
        unsigned int page_no;
};

struct i2c_eeprom_prv *prv=NULL;

static unsigned read_limit    = 32; /* Page size is 32 so read and write limit should not more then 32 bytes : page 3 of AT24C32 data sheet*/
static unsigned write_max     = 32;
static unsigned write_timeout = 25; /*default timeout for normal I2c  devices */

static size_t at24_eeprom_read(struct i2c_client *client, char *buf,
				unsigned offset, size_t count)
{
	struct i2c_msg msg[2]; /*array of msg buf*/
	u8 msgbuf[2];
	unsigned long timeout, read_time;
	int status;
	memset(msg, 0, sizeof(msg));
	memset(msgbuf, '\0', sizeof(msgbuf));
	if (count > read_limit)
		count = read_limit;

	msgbuf[0] = offset >> 8;
	msgbuf[1] = offset;
	/*writing part i.e perform writing 1 byte and device addr*/
	//msg[0].addr = client->addr;
	msg[0].addr = 0x57;

	msg[0].flags = 0;
	msg[0].buf = msgbuf;

	msg[0].len = 2;
	
	//msg[1].addr = client->addr;
	msg[1].addr = 0x57;
	msg[1].flags = 1; /* Read */
	msg[1].buf = buf; 
	msg[1].len = count;
	
	timeout = jiffies + msecs_to_jiffies(write_timeout);
	do {
		read_time = jiffies;

		status = i2c_transfer(client->adapter, msg, 2);
		if (status == 2)
			status = count;
		if (status == count) {
			return count;
		}
		msleep(1);
	} while (time_before(read_time, timeout));

	return -ETIMEDOUT;

}
/* Time_before(a,b) returns true if the time a is before time b. */
static ssize_t at24_eeprom_write(struct i2c_client *client, const char *buf,
				 unsigned offset, size_t count)
{
	struct i2c_msg msg;
	unsigned long timeout, write_time;
	ssize_t status;
	char msgbuf[40];
	
	memset(msgbuf, '\0', sizeof(msgbuf));
	if (count >= write_max)
		count = write_max; /*Count should not exceed from write_max*/
	/* chip address - NOTE: 7bit addresses are stored in the _LOWER_ 7 bits		*/
//	msg.addr = client->addr;
	msg.addr = 0x57;
	
	/**/
	msg.flags = 0; /*0= write , 1 = read */
	msg.buf = msgbuf;

	msg.buf[0] = (offset >> 8); /*first byte for initiate transfer*/
	msg.buf[1] = offset;  /* Device address */
	
	memcpy(&msg.buf[2], buf, count); /*Data payload */
	
	msg.len = count + 2;  /*first two byte also need to considre as actual len*/
	
	timeout = jiffies + msecs_to_jiffies(write_timeout);  /*changing into ms */
	
	do{
		
		write_time = jiffies;
		/*  execute a single or combined I2C message : P1: Handle to I2C bus ,P2 : One or more messages to execute before STOP,P3: Number of messages to be executed.*/
		status = i2c_transfer(client->adapter, &msg, 1);
		if (status == 1)
			status = count;
		if (status == count)
			return count;
		msleep(1);

	}while(time_before(write_time, timeout));
	
	return -ETIMEDOUT;
}

static ssize_t at24_sys_write(struct kobject *kobj, struct kobj_attribute *attr,
		 const char *buf, size_t count)
{
	size_t ret;

	loff_t off = prv->page_no * 32;

	if (count > write_max) {
		pr_info("write length must be <= 32\n");
		return -EFBIG;
	}
	ret = at24_eeprom_write(&prv->client_prv, buf, (int)off, count);
	if (ret < 0) {
		pr_info("write failed\n");
		return ret;
	}

	return count;

}
static ssize_t at24_sys_read(struct kobject *kobj, struct kobj_attribute *attr, 
		char *buf)
{
	size_t ret;

	loff_t off = prv->page_no * 32;
	ret = at24_eeprom_read(&prv->client_prv, buf, (int)off, prv->pagesize);
	if (ret < 0) {
		pr_info("error in reading\n");
		return ret;
	}
	
	return prv->pagesize;
}
static ssize_t at24_get_offset(struct kobject *kobj, struct kobj_attribute *attr, 
		char *buf)
{
	pr_info("Page: %d\n", prv->page_no);

	return strlen(buf);
}

/**
 * kstrtol - convert a string to a long
 * @s: The start of the string. The string must be null-terminated, and may also
 *  include a single newline before its terminating null. The first character
 *  may also be a plus sign or a minus sign.
 * @base: The number base to use. The maximum supported base is 16. If base is
 *  given as 0, then the base of the string is automatically detected with the
 *  conventional semantics - If it begins with 0x the number will be parsed as a
 *  hexadecimal (case insensitive), if it otherwise begins with 0, it will be
 *  parsed as an octal number. Otherwise it will be parsed as a decimal.
 * @res: Where to write the result of the conversion on success.
 *
 * Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
 * Used as a replacement for the obsolete simple_strtoull. Return code must
 * be checked.
 */
static ssize_t at24_set_offset(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	
	unsigned int long tmp;
	if (!kstrtol(buf, 16, &tmp)) {
		if (tmp < 128) {
			prv->page_no = tmp;
			pr_info("Page: %d\n", prv->page_no);
		} else {
			pr_info("128 pages are available\n");
			pr_info("Choose pages from 0x0 - 0x80\n");
		}
	}
	return count;
}
static ssize_t 
at24_flash_erase(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, 
		size_t count)
{
	size_t ret;
	int page_no;
	char buffer[32];
	for(page_no=0;page_no<=127; page_no++){
		loff_t off = page_no * 32;
		memset(buffer,'\0', 32);
		ret = at24_eeprom_write(&prv->client_prv, buffer, (int)off, 32);
		if (ret < 0) {
			pr_info("erase failed\n");
			return ret;
		}
	}
	return count;
}
static struct kobj_attribute at24_rw     = __ATTR(at24c32, 0660, at24_sys_read,at24_sys_write);
static struct kobj_attribute at24_offset = __ATTR(offset, 0660,at24_get_offset, at24_set_offset);
static struct kobj_attribute at24_erase  = __ATTR(erase, 0220,NULL, at24_flash_erase);

static struct attribute *attrs[] = {
        &at24_rw.attr,
        &at24_offset.attr,
        &at24_erase.attr,
        NULL,
};

static struct attribute_group attr_group = {
        .attrs = attrs,
};

static int i2c_eeprom_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	pr_info("%s: Device at24c32 probed......\n",__func__);	
	prv=(struct i2c_eeprom_prv *)kzalloc(sizeof(struct i2c_eeprom_prv), GFP_KERNEL);		
	if(!prv){
		pr_info("Requested memory not allocated\n");
		return -ENOMEM;
	}
	prv->client_prv = *client;
	ret=device_property_read_u32(&client->dev, "size", &prv->size);
	if(ret){
		dev_err(&client->dev, "Error: missing \"size\" property\n");
		return -ENODEV;
	}
	ret=device_property_read_u32(&client->dev, "pagesize", &prv->pagesize);
	if(ret){
		dev_err(&client->dev, "Error: missing \"pagesize\" property\n");
		return -ENODEV;
	}
	ret=device_property_read_u32(&client->dev, "address-width", &prv->address_width);
	if(ret){
		 dev_err(&client->dev, "Error: missing \"address-width\" property\n");
		return -ENODEV;
	}
	prv->at24_kobj=kobject_create_and_add("at24c32_eeprom", NULL);
	if(!prv->at24_kobj)
		return -ENOMEM;
	ret= sysfs_create_group(prv->at24_kobj, &attr_group);
	if(ret)
		kobject_put(prv->at24_kobj);
	pr_info("       SIZE            :%d\n", prv->size);
	pr_info("       PAGESIZE        :%d\n", prv->pagesize);
	pr_info("       address-width   :%d\n", prv->address_width);
	return 0;	
}

static int i2c_eeprom_remove(struct i2c_client *client)
{
	pr_info("at24_remove\n");
	kfree(prv); 
	kobject_put(prv->at24_kobj);
	return 0;
}

static const struct i2c_device_id i2c_eeprom_ids[]={
	{ "24c32", 0x56 },
	{ }
};

static  struct i2c_driver i2c_eeprom_driver = {
	.driver = {
		.name = "at24",
		.owner = THIS_MODULE,
	},
	.probe    = i2c_eeprom_probe,
	.remove   = i2c_eeprom_remove,
	.id_table = i2c_eeprom_ids,
};

/**
 * module_i2c_driver() - Helper macro for registering a modular I2C driver
 * @__i2c_driver: i2c_driver struct
 *
 * Helper macro for I2C drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
module_i2c_driver(i2c_eeprom_driver);


MODULE_DESCRIPTION("Driver for at24c32 eeprom");
MODULE_AUTHOR("Chandan jha <beingchandanjha@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(".1");
