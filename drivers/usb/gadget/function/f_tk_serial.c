#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>

#include "u_serial.h"

struct f_tkser {

	struct gserial		port;
	u8			data_id;
	u8			port_num;
};

static inline struct f_tkser *func_to_tkser(struct usb_function *f)
{
	return container_of(f, struct f_tkser, port.func);
}

/*-------------------------------------------------------------------------*/

/* interface descriptor: */

static struct usb_interface_descriptor tkser_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor tkser_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor tkser_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *tkser_fs_function[] = {
	(struct usb_descriptor_header *) &tkser_interface_desc,
	(struct usb_descriptor_header *) &tkser_fs_in_desc,
	(struct usb_descriptor_header *) &tkser_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor tkser_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor tkser_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *tkser_hs_function[] = {
	(struct usb_descriptor_header *) &tkser_interface_desc,
	(struct usb_descriptor_header *) &tkser_hs_in_desc,
	(struct usb_descriptor_header *) &tkser_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor tkser_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor tkser_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor tkser_ss_bulk_comp_desc = {
	.bLength =              sizeof tkser_ss_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *tkser_ss_function[] = {
	(struct usb_descriptor_header *) &tkser_interface_desc,
	(struct usb_descriptor_header *) &tkser_ss_in_desc,
	(struct usb_descriptor_header *) &tkser_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &tkser_ss_out_desc,
	(struct usb_descriptor_header *) &tkser_ss_bulk_comp_desc,
	NULL,
};

/* string descriptors: */

static struct usb_string tkser_string_defs[] = {
	[0].s = "TK Serial",
	{  } /* end of list */
};

static struct usb_gadget_strings tkser_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		tkser_string_defs,
};

static struct usb_gadget_strings *tkser_strings[] = {
	&tkser_string_table,
	NULL,
};



static int tkser_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_tkser		*tkser = func_to_tkser(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	if (tkser->port.in->enabled) {
		dev_dbg(&cdev->gadget->dev, "reset generic ttyGS%d\n", tkser->port_num);
		gserial_disconnect(&tkser->port);
	}
	if (!tkser->port.in->desc || !tkser->port.out->desc) {
		dev_dbg(&cdev->gadget->dev, "activate generic ttyGS%d\n", tkser->port_num);
		if (config_ep_by_speed(cdev->gadget, f, tkser->port.in) || config_ep_by_speed(cdev->gadget, f, tkser->port.out)) {
			tkser->port.in->desc = NULL;
			tkser->port.out->desc = NULL;
			return -EINVAL;
		}
	}
	gserial_connect(&tkser->port, tkser->port_num);
	return 0;
}

static void tkser_disable(struct usb_function *f)
{
	struct f_tkser	*tkser = func_to_tkser(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	dev_dbg(&cdev->gadget->dev,"generic ttyGS%d deactivated\n", tkser->port_num);
	gserial_disconnect(&tkser->port);
}

static int tkser_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_tkser		*tkser = func_to_tkser(f);
	int			status;
	struct usb_ep		*ep;

	if (tkser_string_defs[0].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0) return status;
		tkser_string_defs[0].id = status;
	}

	status = usb_interface_id(c, f);
	if (status < 0) goto fail;

	tkser->data_id = status;
	tkser_interface_desc.bInterfaceNumber = status;
	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &tkser_fs_in_desc);
	if (!ep) goto fail;
	tkser->port.in = ep;
	ep = usb_ep_autoconfig(cdev->gadget, &tkser_fs_out_desc);
	if (!ep) goto fail;
	tkser->port.out = ep;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	tkser_hs_in_desc.bEndpointAddress = tkser_fs_in_desc.bEndpointAddress;
	tkser_hs_out_desc.bEndpointAddress = tkser_fs_out_desc.bEndpointAddress;

	tkser_ss_in_desc.bEndpointAddress = tkser_fs_in_desc.bEndpointAddress;
	tkser_ss_out_desc.bEndpointAddress = tkser_fs_out_desc.bEndpointAddress;

	status = usb_assign_descriptors(f, tkser_fs_function, tkser_hs_function, tkser_ss_function, NULL);
	if (status) goto fail;

	return 0;
fail:
	ERROR(cdev, "%s: can't bind, err %d\n", f->name, status);
	return status;
}

const unsigned char tk_eeprom[] = {0x00, 0x00, 0xB6, 0x27, 0x12, 0x01};
const unsigned char tk_eeprom_data[] = {	
	0x34,0x00, 0x30,0x00, 0x30,0x00, 0x30,0x00, 0x32,0x00, 0x30,0x00, 0x36,0x00, 0x00,0x00,
    	0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    	0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    	0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00
};

static int tkser_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	int			value = -EOPNOTSUPP;
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;


	switch (ctrl->bRequest) {
	case 0x50:
		value = 64;
		memcpy(req->buf, tk_eeprom_data, value);
		break;
	case 0x51:
		value = 6;
		memcpy(req->buf, tk_eeprom, value);		
		break;	
	}
	if (value >= 0) {

		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) ERROR(cdev, "source/sink response, err %d\n", value);
	}
	return value;
}

static inline struct f_serial_opts *to_f_serial_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_serial_opts, func_inst.group);
}

static void serial_attr_release(struct config_item *item)
{
	struct f_serial_opts *opts = to_f_serial_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations serial_item_ops = {
	.release	= serial_attr_release,
};

static ssize_t f_serial_port_num_show(struct config_item *item, char *page)
{
	return sprintf(page, "%u\n", to_f_serial_opts(item)->port_num);
}

CONFIGFS_ATTR_RO(f_serial_, port_num);

static struct configfs_attribute *acm_attrs[] = {
	&f_serial_attr_port_num,
	NULL,
};

static struct config_item_type serial_func_type = {
	.ct_item_ops	= &serial_item_ops,
	.ct_attrs	= acm_attrs,
	.ct_owner	= THIS_MODULE,
};

static void tkser_free_inst(struct usb_function_instance *f)
{
	struct f_serial_opts *opts;

	opts = container_of(f, struct f_serial_opts, func_inst);
	gserial_free_line(opts->port_num);
	kfree(opts);
}

static struct usb_function_instance *tkser_alloc_inst(void)
{
	struct f_serial_opts *opts;
	int ret;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts) return ERR_PTR(-ENOMEM);
	opts->func_inst.free_func_inst = tkser_free_inst;
	ret = gserial_alloc_line(&opts->port_num);
	if (ret) {
		kfree(opts);
		return ERR_PTR(ret);
	}
	config_group_init_type_name(&opts->func_inst.group, "", &serial_func_type);

	return &opts->func_inst;
}

static void tkser_free(struct usb_function *f)
{
	struct f_tkser *serial;

	serial = func_to_tkser(f);
	kfree(serial);
}

static void tkser_unbind(struct usb_configuration *c, struct usb_function *f)
{
	usb_free_all_descriptors(f);
}

static struct usb_function *tkser_alloc(struct usb_function_instance *fi)
{
	struct f_tkser	*tkser;
	struct f_serial_opts *opts;

	/* allocate and initialize one new instance */
	tkser = kzalloc(sizeof(*tkser), GFP_KERNEL);
	if (!tkser) return ERR_PTR(-ENOMEM);

	opts = container_of(fi, struct f_serial_opts, func_inst);

	tkser->port_num = opts->port_num;
	tkser->port.func.name = "tkser";
	tkser->port.func.strings = tkser_strings;
	tkser->port.func.bind = tkser_bind;
	tkser->port.func.unbind = tkser_unbind;
	tkser->port.func.setup = tkser_setup;
	tkser->port.func.set_alt = tkser_set_alt;
	tkser->port.func.disable = tkser_disable;
	tkser->port.func.free_func = tkser_free;

	return &tkser->port.func;
}

DECLARE_USB_FUNCTION_INIT(tkser, tkser_alloc_inst, tkser_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aleksandr Zaitcev");
