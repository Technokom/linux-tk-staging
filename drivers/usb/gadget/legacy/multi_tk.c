#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>

#include "u_serial.h"


#define DRIVER_DESC		"AutoGRAPH"

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Aleksandr Zaytcev");
MODULE_LICENSE("GPL");

#include "f_mass_storage.h"
#include "u_ecm.h"
#include "u_ether.h"

USB_GADGET_COMPOSITE_OPTIONS();

/***************************** Device Descriptor ****************************/

#define MULTI_VENDOR_NUM	0x27b6	/* TechhoKom Ltd. */
#define MULTI_PRODUCT_NUM	0x0112	/* AutoGRAPH Info new */


static struct usb_device_descriptor device_desc = 
{
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_MISC /* 0xEF */,
	.bDeviceSubClass =	2,
	.bDeviceProtocol =	1,
	.idVendor =		cpu_to_le16(MULTI_VENDOR_NUM),
	.idProduct =		cpu_to_le16(MULTI_PRODUCT_NUM),
};

static const struct usb_descriptor_header *otg_desc[2];

static struct usb_string strings_dev[] = {
	[USB_GADGET_MANUFACTURER_IDX].s = "",
	[USB_GADGET_PRODUCT_IDX].s = DRIVER_DESC,
	[USB_GADGET_SERIAL_IDX].s = "",
	{  } /* end of list */
};

static struct usb_gadget_strings *dev_strings[] = {
	&(struct usb_gadget_strings){
		.language	= 0x0409,	/* en-us */
		.strings	= strings_dev,
	},
	NULL,
};

/****************************** Configurations ******************************/

static struct fsg_module_parameters fsg_mod_data = { .stall = 1 };

#ifdef CONFIG_USB_GADGET_DEBUG_FILES
static unsigned int fsg_num_buffers = CONFIG_USB_GADGET_STORAGE_NUM_BUFFERS;
#else
/*
 * Number of buffers we will use.
 * 2 is usually enough for good buffering pipeline
 */
#define fsg_num_buffers	CONFIG_USB_GADGET_STORAGE_NUM_BUFFERS

#endif /* CONFIG_USB_GADGET_DEBUG_FILES */


FSG_MODULE_PARAMETERS(/* no prefix */, fsg_mod_data);


static struct usb_function_instance *fi_tkser;
static struct usb_function *f_tkser;

static struct usb_function_instance *fi_acm;
static struct usb_function *f_acm;

static struct usb_function_instance *fi_msg;
static struct usb_function *f_msg;



static int acm_ms_do_config(struct usb_configuration *c)
{
	struct fsg_opts *opts;
	int	status;

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	opts = fsg_opts_from_func_inst(fi_msg);

	f_acm = usb_get_function(fi_acm);
	if (IS_ERR(f_acm)) return PTR_ERR(f_acm);

	f_tkser = usb_get_function(fi_tkser);
	if (IS_ERR(f_tkser)) {
		usb_put_function(f_acm);	
		return PTR_ERR(f_tkser);
	}

	f_msg = usb_get_function(fi_msg);
	if (IS_ERR(f_msg)) {
		usb_put_function(f_tkser);
		usb_put_function(f_acm);
		return PTR_ERR(f_msg);
	}

	status = usb_add_function(c, f_acm);
	if (status < 0) {
		usb_put_function(f_msg);
		usb_put_function(f_acm);
		usb_put_function(f_tkser);	
		return status;
	}

	status = usb_add_function(c, f_tkser);
	if (status < 0){
		usb_remove_function(c, f_acm);
		usb_put_function(f_msg);
		usb_put_function(f_acm);
		usb_put_function(f_tkser);	
		return status;
	}

	status = usb_add_function(c, f_msg);
	if (status){
		usb_remove_function(c, f_tkser);
		usb_remove_function(c, f_acm);
		usb_put_function(f_msg);
		usb_put_function(f_acm);
		usb_put_function(f_tkser);	
		return status;
	}
	return 0;
}

/****************************** Gadget Bind ******************************/



//static int ss_config_setup(struct usb_configuration *c, const struct usb_ctrlrequest *ctrl)
//static int tk_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
static int tk_setup(struct usb_configuration *c, const struct usb_ctrlrequest *ctrl)
{

	switch (ctrl->bRequest) {
	case 0x50:
	case 0x51:
		return f_tkser->setup(f_tkser, ctrl);
		break;	
	default:
		return -EOPNOTSUPP;
	}

}

static struct usb_configuration acm_ms_config_driver = 
{
	.label			= DRIVER_DESC,
	.setup                  = tk_setup,
	.bConfigurationValue	= 1,
	/* .iConfiguration = DYNAMIC */
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};

static int __ref multi_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget *gadget = cdev->gadget;
	struct fsg_opts *fsg_opts;
	struct fsg_config config;
	int status;

	if (!can_support_ecm(cdev->gadget)) {
		dev_err(&gadget->dev, "controller '%s' not usable\n", gadget->name);
		return -EINVAL;
	}

	/* set up serial link layer */
	fi_acm = usb_get_function_instance("acm");
	if (IS_ERR(fi_acm)) {
		status = PTR_ERR(fi_acm);
		goto fail0;
	}

	fi_tkser = usb_get_function_instance("tkser");
	if (IS_ERR(fi_tkser)) {
		status = PTR_ERR(fi_tkser);
		goto fail0;
	}

	/* set up mass storage function */
	fi_msg = usb_get_function_instance("mass_storage");
	if (IS_ERR(fi_msg)) {
		status = PTR_ERR(fi_msg);
		goto fail1;
	}

	fsg_config_from_params(&config, &fsg_mod_data, fsg_num_buffers);
	fsg_opts = fsg_opts_from_func_inst(fi_msg);
	fsg_opts->no_configfs = true;
	status = fsg_common_set_num_buffers(fsg_opts->common, fsg_num_buffers);
	if (status) goto fail2;

	status = fsg_common_set_cdev(fsg_opts->common, cdev, config.can_stall);
	if (status) goto fail_set_cdev;

	fsg_common_set_sysfs(fsg_opts->common, true);
	status = fsg_common_create_luns(fsg_opts->common, &config);
	if (status) goto fail_set_cdev;

	fsg_common_set_inquiry_string(fsg_opts->common, config.vendor_name, config.product_name);
	/* allocate string IDs */
	status = usb_string_ids_tab(cdev, strings_dev);
	if (unlikely(status < 0)) goto fail_string_ids;
	device_desc.iManufacturer = strings_dev[USB_GADGET_MANUFACTURER_IDX].id;
	device_desc.iProduct = strings_dev[USB_GADGET_PRODUCT_IDX].id;

	if (gadget_is_otg(gadget) && !otg_desc[0]) {
		struct usb_descriptor_header *usb_desc;
		usb_desc = usb_otg_descriptor_alloc(gadget);
		if (!usb_desc) goto fail_string_ids;
		usb_otg_descriptor_init(gadget, usb_desc);
		otg_desc[0] = usb_desc;
		otg_desc[1] = NULL;
	}


	/* register our configuration */
	status = usb_add_config(cdev, &acm_ms_config_driver, acm_ms_do_config);
	if (status < 0) goto fail_otg_desc;

	usb_composite_overwrite_options(cdev, &coverwrite);

	/* we're done */
	dev_info(&gadget->dev, DRIVER_DESC "\n");
	return 0;


	/* error recovery */
fail_otg_desc:
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;
fail_string_ids:
	fsg_common_remove_luns(fsg_opts->common);
fail_set_cdev:
	fsg_common_free_buffers(fsg_opts->common);
fail2:
	usb_put_function_instance(fi_msg);
fail1:
	usb_put_function_instance(fi_acm);
fail0:
	return status;
}

static int multi_unbind(struct usb_composite_dev *cdev)
{
	usb_put_function(f_msg);
	usb_put_function_instance(fi_msg);
	usb_put_function(f_acm);
	usb_put_function_instance(fi_acm);
	usb_put_function(f_tkser);
	usb_put_function_instance(fi_tkser);
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;
	return 0;
}


/****************************** Some noise ******************************/


static struct usb_composite_driver multi_tk_driver = {
	.name		= "g_multi_tk",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_HIGH,
	.bind		= multi_bind,
	.unbind		= multi_unbind,
	.needs_serial	= 1,
};

module_usb_composite_driver(multi_tk_driver);
