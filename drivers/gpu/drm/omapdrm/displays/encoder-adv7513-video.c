#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <linux/of_irq.h>
#include <linux/hdmi.h>

#include "../dss/omapdss.h"
#include <drm/drm_edid.h>

#include "encoder-adv7513.h"


/* ADI recommended values for proper operation. */
static const struct reg_sequence adv7511_fixed_registers[] = {
        { 0x98, 0x03 },
        { 0x9a, 0xe0 },
        { 0x9c, 0x30 },
        { 0x9d, 0x61 },
        { 0xa2, 0xa4 },
        { 0xa3, 0xa4 },
        { 0xe0, 0xd0 },
        { 0xf9, 0x00 },
        { 0x55, 0x02 },
};


static const uint8_t adv7511_register_defaults[] = {
        0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00 */
        0x00, 0x00, 0x01, 0x0e, 0xbc, 0x18, 0x01, 0x13,
        0x25, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 10 */
        0x46, 0x62, 0x04, 0xa8, 0x00, 0x00, 0x1c, 0x84,
        0x1c, 0xbf, 0x04, 0xa8, 0x1e, 0x70, 0x02, 0x1e, /* 20 */
        0x00, 0x00, 0x04, 0xa8, 0x08, 0x12, 0x1b, 0xac,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 30 */
        0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xb0,
        0x00, 0x50, 0x90, 0x7e, 0x79, 0x70, 0x00, 0x00, /* 40 */
        0x00, 0xa8, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x02, 0x0d, 0x00, 0x00, 0x00, 0x00, /* 50 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 60 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 70 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 80 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, /* 90 */
        0x0b, 0x02, 0x00, 0x18, 0x5a, 0x60, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x80, 0x08, 0x04, 0x00, 0x00, /* a0 */
        0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* b0 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* c0 */
        0x00, 0x03, 0x00, 0x00, 0x02, 0x00, 0x01, 0x04,
        0x30, 0xff, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, /* d0 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x01,
        0x80, 0x75, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, /* e0 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x75, 0x11, 0x00, /* f0 */
        0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};



static bool adv7511_register_volatile(struct device *dev, unsigned int reg)
{
    switch (reg) {
        case ADV7511_REG_CHIP_REVISION:
        case ADV7511_REG_SPDIF_FREQ:
        case ADV7511_REG_CTS_AUTOMATIC1:
        case ADV7511_REG_CTS_AUTOMATIC2:
        case ADV7511_REG_VIC_DETECTED:
        case ADV7511_REG_VIC_SEND:
        case ADV7511_REG_AUX_VIC_DETECTED:
        case ADV7511_REG_STATUS:
        case ADV7511_REG_GC(1):
        case ADV7511_REG_INT(0):
        case ADV7511_REG_INT(1):
        case ADV7511_REG_PLL_STATUS:
        case ADV7511_REG_AN(0):
        case ADV7511_REG_AN(1):
        case ADV7511_REG_AN(2):
        case ADV7511_REG_AN(3):
        case ADV7511_REG_AN(4):
        case ADV7511_REG_AN(5):
        case ADV7511_REG_AN(6):
        case ADV7511_REG_AN(7):
        case ADV7511_REG_HDCP_STATUS:
        case ADV7511_REG_BCAPS:
        case ADV7511_REG_BKSV(0):
        case ADV7511_REG_BKSV(1):
        case ADV7511_REG_BKSV(2):
        case ADV7511_REG_BKSV(3):
        case ADV7511_REG_BKSV(4):
        case ADV7511_REG_DDC_STATUS:
        case ADV7511_REG_EDID_READ_CTRL:
        case ADV7511_REG_BSTATUS(0):
        case ADV7511_REG_BSTATUS(1):
        case ADV7511_REG_CHIP_ID_HIGH:
        case ADV7511_REG_CHIP_ID_LOW:
            return true;
    }

    return false;
}



static const char * const adv7511_supply_names[] = {
        "avdd",
        "dvdd",
        "pvdd",
        "bgvdd",
        "dvdd-3v",
};



static const struct regmap_config adv7511_regmap_config = {
        .reg_bits = 8,
        .val_bits = 8,

        .max_register = 0xff,
        .cache_type = REGCACHE_RBTREE,
        .reg_defaults_raw = adv7511_register_defaults,
        .num_reg_defaults_raw = ARRAY_SIZE(adv7511_register_defaults),

        .volatile_reg = adv7511_register_volatile,
};


static int adv7513_parse_dt(struct device_node *np, struct adv7511_link_config *config)
{
    const char *str;
    int ret;

    of_property_read_u32(np, "adi,input-depth", &config->input_color_depth);
    if (config->input_color_depth != 8 && config->input_color_depth != 10 &&
        config->input_color_depth != 12)
        return -EINVAL;

    ret = of_property_read_string(np, "adi,input-colorspace", &str);
    if (ret < 0)
        return ret;

    if (!strcmp(str, "rgb"))
        config->input_colorspace = HDMI_COLORSPACE_RGB;
    else if (!strcmp(str, "yuv422"))
        config->input_colorspace = HDMI_COLORSPACE_YUV422;
    else if (!strcmp(str, "yuv444"))
        config->input_colorspace = HDMI_COLORSPACE_YUV444;
    else
        return -EINVAL;

    ret = of_property_read_string(np, "adi,input-clock", &str);
    if (ret < 0)
        return ret;

    if (!strcmp(str, "1x"))
        config->input_clock = ADV7511_INPUT_CLOCK_1X;
    else if (!strcmp(str, "2x"))
        config->input_clock = ADV7511_INPUT_CLOCK_2X;
    else if (!strcmp(str, "ddr"))
        config->input_clock = ADV7511_INPUT_CLOCK_DDR;
    else
        return -EINVAL;

    if (config->input_colorspace == HDMI_COLORSPACE_YUV422 ||
        config->input_clock != ADV7511_INPUT_CLOCK_1X) {
        ret = of_property_read_u32(np, "adi,input-style",
                                   &config->input_style);
        if (ret)
            return ret;

        if (config->input_style < 1 || config->input_style > 3)
            return -EINVAL;

        ret = of_property_read_string(np, "adi,input-justification",
                                      &str);
        if (ret < 0)
            return ret;

        if (!strcmp(str, "left"))
            config->input_justification =
                    ADV7511_INPUT_JUSTIFICATION_LEFT;
        else if (!strcmp(str, "evenly"))
            config->input_justification =
                    ADV7511_INPUT_JUSTIFICATION_EVENLY;
        else if (!strcmp(str, "right"))
            config->input_justification =
                    ADV7511_INPUT_JUSTIFICATION_RIGHT;
        else
            return -EINVAL;

    } else {
        config->input_style = 1;
        config->input_justification = ADV7511_INPUT_JUSTIFICATION_LEFT;
    }

    of_property_read_u32(np, "adi,clock-delay", &config->clock_delay);
    if (config->clock_delay < -1200 || config->clock_delay > 1600)
        return -EINVAL;

    config->embedded_sync = of_property_read_bool(np, "adi,embedded-sync");

    /* Hardcode the sync pulse configurations for now. */
	config->sync_pulse = ADV7511_INPUT_SYNC_PULSE_DE;
	config->vsync_polarity = ADV7511_SYNC_POLARITY_LOW;
	config->hsync_polarity = ADV7511_SYNC_POLARITY_LOW;

    return 0;
}

static void __adv7511_power_on(struct adv7511 *adv7511)
{
    adv7511->current_edid_segment = -1;

    regmap_update_bits(adv7511->regmap, ADV7511_REG_POWER,
                       ADV7511_POWER_POWER_DOWN, 0);
    if (adv7511->i2c_main->irq) {

        regmap_write(adv7511->regmap, ADV7511_REG_INT_ENABLE(0), ADV7511_INT0_EDID_READY | ADV7511_INT0_HPD);
        regmap_write(adv7511->regmap, ADV7511_REG_INT_ENABLE(1), ADV7511_INT1_DDC_ERROR);
    }

    regmap_update_bits(adv7511->regmap, ADV7511_REG_POWER2, ADV7511_REG_POWER2_HPD_SRC_MASK, ADV7511_REG_POWER2_HPD_SRC_NONE);
}



static void adv7511_power_on(struct adv7511 *adv7511)
{
    __adv7511_power_on(adv7511);
    regcache_sync(adv7511->regmap);
    adv7511->powered = true;
}

static void __adv7511_power_off(struct adv7511 *adv7511)
{
    /* TODO: setup additional power down modes */
    regmap_update_bits(adv7511->regmap, ADV7511_REG_POWER, ADV7511_POWER_POWER_DOWN, ADV7511_POWER_POWER_DOWN);
    regcache_mark_dirty(adv7511->regmap);
}

static void adv7511_power_off(struct adv7511 *adv7511)
{
    __adv7511_power_off(adv7511);
    adv7511->powered = false;
}

static int adv7511_packet_disable(struct adv7511 *adv7511, unsigned int packet)
{
    if (packet & 0xff)
        regmap_update_bits(adv7511->regmap, ADV7511_REG_PACKET_ENABLE0, packet, 0x00);

    if (packet & 0xff00) {
        packet >>= 8;
        regmap_update_bits(adv7511->regmap, ADV7511_REG_PACKET_ENABLE1, packet, 0x00);
    }

    return 0;
}



static void adv7511_hpd_work(struct work_struct *work)
{
    struct adv7511 *adv7511 = container_of(work, struct adv7511, hpd_work);
    enum drm_connector_status status;
    unsigned int val;
    int ret;



    ret = regmap_read(adv7511->regmap, ADV7511_REG_STATUS, &val);
    if (ret < 0)
        status = connector_status_disconnected;
    else if (val & ADV7511_STATUS_HPD)
        status = connector_status_connected;
    else
        status = connector_status_disconnected;

    /*
     * The bridge resets its registers on unplug. So when we get a plug
     * event and we're already supposed to be powered, cycle the bridge to
     * restore its state.
     */
    if (status == connector_status_connected &&
        adv7511->connector.status == connector_status_disconnected &&
        adv7511->powered) {
        regcache_mark_dirty(adv7511->regmap);
        adv7511_power_on(adv7511);
    }

    if (adv7511->connector.status != status) {
        adv7511->connector.status = status;
        //drm_kms_helper_hotplug_event(adv7511->connector.dev);
    }
}



static int adv7511_irq_process(struct adv7511 *adv7511, bool process_hpd)
{
    unsigned int irq0, irq1;
    int ret;

    ret = regmap_read(adv7511->regmap, ADV7511_REG_INT(0), &irq0);
    if (ret < 0)
        return ret;

    ret = regmap_read(adv7511->regmap, ADV7511_REG_INT(1), &irq1);
    if (ret < 0)
        return ret;

    regmap_write(adv7511->regmap, ADV7511_REG_INT(0), irq0);
    regmap_write(adv7511->regmap, ADV7511_REG_INT(1), irq1);

    if (process_hpd && irq0 & ADV7511_INT0_HPD) {
        schedule_work(&adv7511->hpd_work);
    }

    if (irq0 & ADV7511_INT0_EDID_READY || irq1 & ADV7511_INT1_DDC_ERROR) {
        adv7511->edid_read = true;

        if (adv7511->i2c_main->irq)
            wake_up_all(&adv7511->wq);
    }

    return 0;
}



static int adv7511_wait_for_edid(struct adv7511 *adv7511, int timeout)
{
    int ret;

    if (adv7511->i2c_main->irq) {
        ret = wait_event_interruptible_timeout(adv7511->wq, adv7511->edid_read, msecs_to_jiffies(timeout));
    } else {
        for (; timeout > 0; timeout -= 25) {
            ret = adv7511_irq_process(adv7511, false);
            if (ret < 0)
                break;

            if (adv7511->edid_read)
                break;

            msleep(25);
        }
    }

    return adv7511->edid_read ? 0 : -EIO;
}


static int adv7511_get_edid_block(void *data, u8 *buf, unsigned int block, size_t len)
{
    struct adv7511 *adv7511 = data;
    struct i2c_msg xfer[2];
    uint8_t offset;
    unsigned int i;
    int ret;

    if (len > 128)
        return -EINVAL;

    if (adv7511->current_edid_segment != block / 2) {
        unsigned int status;

        ret = regmap_read(adv7511->regmap, ADV7511_REG_DDC_STATUS,
                          &status);
        if (ret < 0)
            return ret;

        if (status != 2) {
            adv7511->edid_read = false;
            regmap_write(adv7511->regmap, ADV7511_REG_EDID_SEGMENT,
                         block);
            ret = adv7511_wait_for_edid(adv7511, 200);
            if (ret < 0)
                return ret;
        }

        /* Break this apart, hopefully more I2C controllers will
         * support 64 byte transfers than 256 byte transfers
         */

        xfer[0].addr = adv7511->i2c_edid->addr;
        xfer[0].flags = 0;
        xfer[0].len = 1;
        xfer[0].buf = &offset;
        xfer[1].addr = adv7511->i2c_edid->addr;
        xfer[1].flags = I2C_M_RD;
        xfer[1].len = 64;
        xfer[1].buf = adv7511->edid_buf;

        offset = 0;

        for (i = 0; i < 4; ++i) {
            ret = i2c_transfer(adv7511->i2c_edid->adapter, xfer,
                               ARRAY_SIZE(xfer));
            if (ret < 0)
                return ret;
            else if (ret != 2)
                return -EIO;

            xfer[1].buf += 64;
            offset += 64;
        }

        adv7511->current_edid_segment = block / 2;
    }

    if (block % 2 == 0)
        memcpy(buf, adv7511->edid_buf, len);
    else
        memcpy(buf, adv7511->edid_buf + 128, len);

    return 0;
}



static int adv7511_get_modes(struct adv7511 *adv7511, struct drm_connector *connector)
{
    struct edid *edid;
    unsigned int count;

    /* Reading the EDID only works if the device is powered */
    if (!adv7511->powered) {
        unsigned int edid_i2c_addr =
                (adv7511->i2c_main->addr << 1) + 4;

        __adv7511_power_on(adv7511);

        /* Reset the EDID_I2C_ADDR register as it might be cleared */
        regmap_write(adv7511->regmap, ADV7511_REG_EDID_I2C_ADDR,
                     edid_i2c_addr);
    }

    edid = drm_do_get_edid(connector, adv7511_get_edid_block, adv7511);

    if (!adv7511->powered)
        __adv7511_power_off(adv7511);

    //kfree(adv7511->edid);
    //adv7511->edid = edid;
    if (!edid)
        return 0;

    drm_mode_connector_update_edid_property(connector, edid);
    count = drm_add_edid_modes(connector, edid);

    //adv7511_set_config_csc(adv7511, connector, adv7511->rgb);

    return count;
}


static irqreturn_t adv7511_irq_handler(int irq, void *devid)
{
    struct adv7511 *adv7511 = devid;
    int ret;

    ret = adv7511_irq_process(adv7511, true);
    return ret < 0 ? IRQ_NONE : IRQ_HANDLED;



}


static void adv7511_set_link_config(struct adv7511 *adv7511, const struct adv7511_link_config *config)
{
    /*
     * The input style values documented in the datasheet don't match the
     * hardware register field values :-(
     */
    static const unsigned int input_styles[4] = { 0, 2, 1, 3 };

    unsigned int clock_delay;
    unsigned int color_depth;
    unsigned int input_id;

    clock_delay = (config->clock_delay + 1200) / 400;
    color_depth = config->input_color_depth == 8 ? 3
                                                 : (config->input_color_depth == 10 ? 1 : 2);

    /* TODO Support input ID 6 */
    if (config->input_colorspace != HDMI_COLORSPACE_YUV422)
        input_id = config->input_clock == ADV7511_INPUT_CLOCK_DDR ? 5 : 0;
    else if (config->input_clock == ADV7511_INPUT_CLOCK_DDR)
        input_id = config->embedded_sync ? 8 : 7;
    else if (config->input_clock == ADV7511_INPUT_CLOCK_2X)
        input_id = config->embedded_sync ? 4 : 3;
    else
        input_id = config->embedded_sync ? 2 : 1;

    regmap_update_bits(adv7511->regmap, ADV7511_REG_I2C_FREQ_ID_CFG, 0xf, input_id);
    regmap_update_bits(adv7511->regmap, ADV7511_REG_VIDEO_INPUT_CFG1, 0x7e, (color_depth << 4) | (input_styles[config->input_style] << 2));
    regmap_write(adv7511->regmap, ADV7511_REG_VIDEO_INPUT_CFG2, config->input_justification << 3);
    regmap_write(adv7511->regmap, ADV7511_REG_TIMING_GEN_SEQ, config->sync_pulse << 2);

    regmap_write(adv7511->regmap, 0xba, clock_delay << 5);

    adv7511->embedded_sync = config->embedded_sync;
    adv7511->hsync_polarity = config->hsync_polarity;
    adv7511->vsync_polarity = config->vsync_polarity;
    adv7511->rgb = config->input_colorspace == HDMI_COLORSPACE_RGB;
}



static int adv7511_init_regulators(struct adv7511 *adv)
{
    struct device *dev = &adv->i2c_main->dev;
    const char * const *supply_names;
    unsigned int i;
    int ret;


    supply_names = adv7511_supply_names;
    adv->num_supplies = ARRAY_SIZE(adv7511_supply_names);


    adv->supplies = devm_kcalloc(dev, adv->num_supplies,
                                 sizeof(*adv->supplies), GFP_KERNEL);
    if (!adv->supplies)
        return -ENOMEM;

    for (i = 0; i < adv->num_supplies; i++)
        adv->supplies[i].supply = supply_names[i];

    ret = devm_regulator_bulk_get(dev, adv->num_supplies, adv->supplies);
    if (ret)
        return ret;

    return regulator_bulk_enable(adv->num_supplies, adv->supplies);
}

/*
static const struct omapdss_hdmi_ops adv7513_hdmi_ops = {
        .connect		= adv7513_connect,
        .disconnect		= adv7513_disconnect,

        .enable			= adv7513_enable,
        .disable		= adv7513_disable,

        .check_timings		= adv7513_check_timings,
        .set_timings		= adv7513_set_timings,
        .get_timings		= adv7513_get_timings,

        .read_edid		= adv7513_read_edid,
        .detect			= adv7513_detect,
        .register_hpd_cb        = adv7513_register_hpd_cb,
        .unregister_hpd_cb      = adv7513_unregister_hpd_cb,
        .enable_hpd		= adv7513_enable_hpd,
        .disable_hpd		= adv7513_disable_hpd,
        .set_hdmi_mode		= adv7513_set_hdmi_mode,
        .set_infoframe		= adv7513_set_infoframe,
};
*/


static int adv7513_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct adv7511_link_config link_config;
    struct device *dev = &client->dev;
    unsigned int main_i2c_addr = client->addr << 1;
    unsigned int edid_i2c_addr = main_i2c_addr + 4;
    unsigned int val;

    struct adv7511 *ddata;
    struct omap_dss_device *dssdev;
    int ret = 0;

    ddata = devm_kzalloc(&client->dev, sizeof(*ddata), GFP_KERNEL);
    if (ddata == NULL) {
        return -ENOMEM;
    }

    ddata->i2c_main = client;
    ddata->powered = false;
    ddata->status = connector_status_disconnected;

    memset(&link_config, 0, sizeof(link_config));

    ret = adv7513_parse_dt(dev->of_node, &link_config);
    if (ret) {
        return ret;
    }

    ret = adv7511_init_regulators(ddata);
    if (ret) {
        dev_err(dev, "failed to init regulators\n");
        return ret;
    }


    ddata->gpio_pd = devm_gpiod_get_optional(dev, "pd", GPIOD_OUT_HIGH);
    if (IS_ERR(ddata->gpio_pd)) {
        ret = PTR_ERR(ddata->gpio_pd);
        goto err_tpi;
    }

    if (ddata->gpio_pd) {
        mdelay(5);
        gpiod_set_value_cansleep(ddata->gpio_pd, 0);
    }


    if (client->irq) {
        init_waitqueue_head(&ddata->wq);

        ret = devm_request_threaded_irq(dev, client->irq, NULL, adv7511_irq_handler, IRQF_ONESHOT, dev_name(dev), ddata);


        if (ret) {
            goto err_reg;
        }
    }



    ddata->regmap = devm_regmap_init_i2c(client, &adv7511_regmap_config);
    if (IS_ERR(ddata->regmap)) {
        ret = PTR_ERR(ddata->regmap);
        goto err_reg;
    }



    ret = regmap_read(ddata->regmap, ADV7511_REG_CHIP_REVISION, &val);
    if (ret) {
        goto err_reg;
    }
    dev_dbg(dev, "Rev. %d\n", val);


    adv7511_power_on(ddata);

    ret = regmap_register_patch(ddata->regmap, adv7511_fixed_registers, ARRAY_SIZE(adv7511_fixed_registers));

    if (ret) {
        goto err_reg;
    }

    regmap_write(ddata->regmap, ADV7511_REG_HDCP_HDMI_CFG, 0x82);

    regmap_write(ddata->regmap, ADV7511_REG_EDID_I2C_ADDR, edid_i2c_addr);
    regmap_write(ddata->regmap, ADV7511_REG_PACKET_I2C_ADDR, main_i2c_addr - 0xa);
    regmap_write(ddata->regmap, ADV7511_REG_CEC_I2C_ADDR, main_i2c_addr - 2);

    adv7511_set_link_config(ddata, &link_config);

    adv7511_packet_disable(ddata, 0xffff);
    regmap_write(ddata->regmap, ADV7511_REG_CEC_CTRL, ADV7511_CEC_CTRL_POWER_DOWN);


    ddata->i2c_edid = i2c_new_dummy(client->adapter, edid_i2c_addr >> 1);
    if (!ddata->i2c_edid) {
        ret = -ENOMEM;
        goto err_reg;
    }

    INIT_WORK(&ddata->hpd_work, adv7511_hpd_work);





    adv7511_power_off(ddata);





    i2c_set_clientdata(client, ddata);




    adv7511_power_on(ddata);

    ret = adv7511_irq_process(ddata, false);

    dssdev = &ddata->dssdev;
    dssdev->dev = &client->dev;
    //dssdev->ops.hdmi = &sii9022_hdmi_ops;
    dssdev->type = OMAP_DISPLAY_TYPE_DPI;
    dssdev->output_type = OMAP_DISPLAY_TYPE_HDMI;
    dssdev->owner = THIS_MODULE;
    dssdev->port_num = 1;


    ret = omapdss_register_output(dssdev);
    if (ret) {
        dev_err(&client->dev, "Failed to register output\n");
        goto err_reg;
    }


    return 0;
err_reg:
err_tpi:
    omap_dss_put_device(ddata->in);
    return ret;
}


static int adv7513_remove(struct i2c_client *client)
{
    struct adv7511 *ddata = dev_get_drvdata(&client->dev);
    struct omap_dss_device *dssdev = &ddata->dssdev;



    omapdss_unregister_output(dssdev);

    WARN_ON(omapdss_device_is_enabled(dssdev));
    if (omapdss_device_is_enabled(dssdev)) {
        //sii9022_disable(dssdev);
    }


    WARN_ON(omapdss_device_is_connected(dssdev));
    if (omapdss_device_is_connected(dssdev)) {
        //sii9022_disconnect(dssdev, dssdev->dst);
    }


    omap_dss_put_device(ddata->in);

    return 0;
}



static const struct i2c_device_id adv7513_id[] = {
        { "adv7513", 0 },
        { },
};

static const struct of_device_id adv7513_of_match[] = {
        { .compatible = "omapdss,adi,adv7513", },
        {},
};

MODULE_DEVICE_TABLE(i2c, adv7513_id);

static struct i2c_driver adv7513_driver = {
        .driver = {
                .name  = "adv7513",
                .owner = THIS_MODULE,
                .of_match_table = adv7513_of_match,
        },
        .probe		= adv7513_probe,
        .remove		= adv7513_remove,
        .id_table	= adv7513_id,
};

module_i2c_driver(adv7513_driver);

MODULE_AUTHOR("Aleksandr Zaitsev <sasha@tk-chel.ru>");
MODULE_DESCRIPTION("ADV7513 HDMI Encoder Driver");
MODULE_LICENSE("GPL");
