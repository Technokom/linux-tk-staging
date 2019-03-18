
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
//#include <sound/tlv320aic3x.h>

#include "lm49350.h"

//static const DECLARE_TLV_DB_SCALE(dac_tlv, -12750, 50, 1);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -7650, 150, 1);
static const DECLARE_TLV_DB_SCALE(mic_tlv, 600, 200, 1);
static const DECLARE_TLV_DB_SCALE(adc_tlv, -7650, 150, 1);

#define LM49350_FORMATS (SNDRV_PCM_FMTBIT_S16_LE   \
                        | SNDRV_PCM_FMTBIT_S20_3LE \
                        | SNDRV_PCM_FMTBIT_S24_LE  \
                        | SNDRV_PCM_FMTBIT_S32_LE)

static LIST_HEAD(reset_list);

static const struct reg_default lm49350_reg[] = {
	{ 0x00, 0x00 },
	{ 0x01, 0x00 },
	{ 0x02, 0x00 },
	{ 0x03, 0x00 },
	{ 0x04, 0x00 },
	{ 0x05, 0x00 },
	{ 0x06, 0x00 },
	{ 0x07, 0x00 },
	{ 0x08, 0x00 },
	{ 0x09, 0x00 },
	{ 0x0A, 0x00 },
	{ 0x0B, 0x00 },
	{ 0x0C, 0x00 },
	{ 0x10, 0x00 },
	{ 0x11, 0x00 },
	{ 0x12, 0x00 },
	{ 0x13, 0x00 },
	{ 0x14, 0x00 },
	{ 0x15, 0x00 },
	{ 0x16, 0x00 },
	{ 0x17, 0x00 },
	{ 0x18, 0x00 },
	{ 0x19, 0x00 },
	{ 0x20, 0x00 },
	{ 0x21, 0x00 },
	{ 0x22, 0x00 },
	{ 0x30, 0x00 },
	{ 0x31, 0x00 },
	{ 0x32, 0x00 },
	{ 0x40, 0x00 },
	{ 0x41, 0x00 },
	{ 0x42, 0x00 },
	{ 0x43, 0x00 },
	{ 0x44, 0x00 },
	{ 0x45, 0x00 },
	{ 0x50, 0x00 },
	{ 0x51, 0x00 },
	{ 0x52, 0x00 },
	{ 0x53, 0x00 },
	{ 0x54, 0x00 },
	{ 0x55, 0x00 },
	{ 0x56, 0x00 },
	{ 0x60, 0x00 },
	{ 0x61, 0x00 },
	{ 0x62, 0x00 },
	{ 0x63, 0x00 },
	{ 0x64, 0x00 },
	{ 0x65, 0x00 },
	{ 0x66, 0x00 },
	{ 0x70, 0x00 },
	{ 0x71, 0x00 },
	{ 0x80, 0x00 },
	{ 0x81, 0x00 },
	{ 0x82, 0x00 },
	{ 0x83, 0x00 },
	{ 0x84, 0x00 },
	{ 0x85, 0x00 },
	{ 0x86, 0x00 },
	{ 0x87, 0x00 },
	{ 0x88, 0x00 },
	{ 0x89, 0x00 },
	{ 0x8A, 0x00 },
	{ 0x8B, 0x00 },
	{ 0x8C, 0x00 },
	{ 0x8D, 0x00 },
	{ 0x8E, 0x00 },
	{ 0x8F, 0x00 },
	{ 0x90, 0x00 },
	{ 0x91, 0x00 },
	{ 0x92, 0x00 },
	{ 0x98, 0x00 },
	{ 0x99, 0x00 },
	{ 0x9A, 0x00 },
	{ 0x9B, 0x00 },
	{ 0x9C, 0x00 },
	{ 0xA0, 0x00 },
	{ 0xA1, 0x00 },
	{ 0xA2, 0x00 },
	{ 0xA3, 0x00 },
	{ 0xA4, 0x00 },
	{ 0xA5, 0x00 },
	{ 0xA6, 0x00 },
	{ 0xA7, 0x00 },
	{ 0xA8, 0x00 },
	{ 0xA9, 0x00 },
	{ 0xAA, 0x00 },
	{ 0xAB, 0x00 },
	{ 0xAC, 0x00 },
	{ 0xAD, 0x00 },
	{ 0xAE, 0x00 },
	{ 0xAF, 0x00 },
	{ 0xB0, 0x00 },
	{ 0xB1, 0x00 },
	{ 0xB2, 0x00 },
	{ 0xB8, 0x00 },
	{ 0xB9, 0x00 },
	{ 0xBA, 0x00 },
	{ 0xBB, 0x00 },
	{ 0xBC, 0x00 },
	{ 0xE0, 0x00 },
	{ 0xF1, 0x00 },
	{ 0xF8, 0x00 },
	{ 0xF9, 0x00 },
	{ 0xFA, 0x00 },
	{ 0xFB, 0x00 },
	{ 0xFC, 0x00 },
	{ 0xFD, 0x00 },
	{ 0xFE, 0x00 },
};
//--------------------------------------------------------------------------------------------
static const struct regmap_config lm49350_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0xFE,
	.reg_defaults = lm49350_reg,
	.num_reg_defaults = ARRAY_SIZE(lm49350_reg),
	.cache_type = REGCACHE_RBTREE,
};
//--------------------------------------------------------------------------------------------
#define SOC_LM49350_OUT_SINGLE_R_TLV(xname, reg, shift, max, invert, tlv_array)\
{      .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
       .access = SNDRV_CTL_ELEM_ACCESS_TLV_READ \
                 | SNDRV_CTL_ELEM_ACCESS_READWRITE,\
       .tlv.p = (tlv_array), \
       .info = snd_soc_info_volsw, \
       .get = snd_soc_get_volsw, .put = snd_soc_put_volsw, \
       .private_value = SOC_SINGLE_VALUE(reg, shift, max, invert, 0) }
//--------------------------------------------------------------------------------------------
static const struct snd_kcontrol_new lm49350_snd_controls[] =
{
	SOC_LM49350_OUT_SINGLE_R_TLV("Left DAC Playback Volume", LM49350_DIGITAL_ATTENUATION_DACL1, 0, 0x3f, 0, dac_tlv),
	SOC_LM49350_OUT_SINGLE_R_TLV("Right DAC Playback Volume", LM49350_DIGITAL_ATTENUATION_DACR1, 0, 0x3f, 0, dac_tlv),

  	SOC_LM49350_OUT_SINGLE_R_TLV("Right ADC Volume", ADC_EFFECTS_ADC_R_LEVEL, 0, 0x3f, 0, adc_tlv),
  	SOC_LM49350_OUT_SINGLE_R_TLV("Left ADC Volume", ADC_EFFECTS_ADC_L_LEVEL, 0, 0x3f, 0, adc_tlv),

  	SOC_LM49350_OUT_SINGLE_R_TLV("Left Mic Volume", ANALOG_MIXER_MIC_L_LVL, 0, 0x0f, 0, mic_tlv),
  	SOC_LM49350_OUT_SINGLE_R_TLV("Right Mic Volume", ANALOG_MIXER_MIC_R_LVL, 0, 0x0f, 0, mic_tlv),

	SOC_SINGLE("Left DAC Mute Switch", DAC_MUTE, 2, 1, 0),
	SOC_SINGLE("Right DAC Mute Switch", DAC_MUTE, 3, 1, 0),

	SOC_SINGLE("Left Mic Mute Switch", ANALOG_MIXER_MIC_L_LVL, 5, 1, 0),
    SOC_SINGLE("Right Mic Mute Switch", ANALOG_MIXER_MIC_R_LVL, 5, 1, 0),

    SOC_SINGLE("Speaker Left DAC Switch", ANALOG_MIXER_CLASSD, 1, 1, 0),
    SOC_SINGLE("Speaker Right DAC Switch", ANALOG_MIXER_CLASSD, 0, 1, 0),
    SOC_SINGLE("HPL Left DAC Switch", ANALOG_MIXER_HEADPHONESL, 1, 1, 0),
    SOC_SINGLE("HPL Right DAC Switch", ANALOG_MIXER_HEADPHONESL, 0, 1, 0),
    SOC_SINGLE("HPR Left DAC Switch", ANALOG_MIXER_HEADPHONESR, 1, 1, 0),
    SOC_SINGLE("HPR Right DAC Switch", ANALOG_MIXER_HEADPHONESR, 0, 1, 0),

    SOC_SINGLE("Speaker Left Mic Switch", ANALOG_MIXER_CLASSD, 3, 1, 0),
	SOC_SINGLE("Speaker Right Mic Switch", ANALOG_MIXER_CLASSD, 2, 1, 0),
	SOC_SINGLE("HPL Left Mic Switch", ANALOG_MIXER_HEADPHONESL, 3, 1, 0),
	SOC_SINGLE("HPL Right Mic Switch", ANALOG_MIXER_HEADPHONESL, 2, 1, 0),
    SOC_SINGLE("HPR Left Mic Switch", ANALOG_MIXER_HEADPHONESR, 3, 1, 0),
    SOC_SINGLE("HPR Right Mic Switch", ANALOG_MIXER_HEADPHONESR, 2, 1, 0),
    SOC_SINGLE("ADC Left Mic Switch", ANALOG_MIXER_ADC, 3, 1, 0),
    SOC_SINGLE("ADC Right Mic Switch", ANALOG_MIXER_ADC, 2, 1, 0),

    SOC_SINGLE("Audio Port 2 Swap Switch", AUDIO_PORT_2_INPUT, 4, 1, 0),

};
//--------------------------------------------------------------------------------------------

static const struct snd_kcontrol_new lm49350_left_output_switches[] = {
	SOC_DAPM_SINGLE("From Left DAC", ANALOG_MIXER_HEADPHONESL, 1, 1, 0),
	SOC_DAPM_SINGLE("From Right DAC", ANALOG_MIXER_HEADPHONESL, 0, 1, 0),
};

static const struct snd_kcontrol_new lm49350_right_output_switches[] = {
	SOC_DAPM_SINGLE("From Left DAC", ANALOG_MIXER_HEADPHONESR, 1, 1, 0),
	SOC_DAPM_SINGLE("From Right DAC", ANALOG_MIXER_HEADPHONESR, 0, 1, 0),
};


static const struct snd_kcontrol_new lm49350_internal_output_switches[] = {
	SOC_DAPM_SINGLE("From Left DAC", ANALOG_MIXER_CLASSD, 1, 1, 0),
	SOC_DAPM_SINGLE("From Right DAC", ANALOG_MIXER_CLASSD, 0, 1, 0),
};

static const struct snd_soc_dapm_widget lm49350_dapm_widgets[] = 
{
	SND_SOC_DAPM_AIF_IN("DAC IN", "DAC Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("HPLOUT"),
	SND_SOC_DAPM_OUTPUT("HPROUT"),
	SND_SOC_DAPM_INPUT("LINE1L"),
	SND_SOC_DAPM_INPUT("LINE1R"),
	SND_SOC_DAPM_OUTPUT("SPL"),
	SND_SOC_DAPM_OUTPUT("SPR"),

    //SND_SOC_DAPM_MIXER("Output Left", SND_SOC_NOPM, 0, 0, lm49350_left_output_switches, ARRAY_SIZE(lm49350_left_output_switches)),
    //SND_SOC_DAPM_MIXER("Output Right", SND_SOC_NOPM, 0, 0, lm49350_right_output_switches, ARRAY_SIZE(lm49350_right_output_switches)),
    //SND_SOC_DAPM_MIXER("Output Internal", SND_SOC_NOPM, 0, 0, lm49350_internal_output_switches, ARRAY_SIZE(lm49350_internal_output_switches)),

	SND_SOC_DAPM_OUTPUT("Detection"),
};


//--------------------------------------------------------------------------------------------
static const struct snd_soc_dapm_route intercon[] = 
{

};
//--------------------------------------------------------------------------------------------
static int lm49350_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

    

	switch (params_rate(params)) {
        case 8000:
        	snd_soc_write(codec, DAC_CLOCK,23);
            snd_soc_write(codec, ADC_CLOCK, 23);
            break;
        case 11025:
            snd_soc_write(codec, DAC_CLOCK, 17);//snd_soc_write(codec, DAC_CLOCK, 0x10);
            snd_soc_write(codec, ADC_CLOCK, 17);//snd_soc_write(codec, ADC_CLOCK, 0x10);
            break;
        case 16000:
            snd_soc_write(codec, DAC_CLOCK, 11);//snd_soc_write(codec, DAC_CLOCK, 0x0b);
            snd_soc_write(codec, DAC_CLOCK, 11);//snd_soc_write(codec, ADC_CLOCK, 0x0b);
            break;
        case 22050:
            snd_soc_write(codec, DAC_CLOCK, 7);
            snd_soc_write(codec, ADC_CLOCK, 7);
            break;
        case 32000:
            snd_soc_write(codec, DAC_CLOCK, 5);
            snd_soc_write(codec, ADC_CLOCK, 5);
            break;
        case 44100:
            snd_soc_write(codec, DAC_CLOCK, 4);
            snd_soc_write(codec, ADC_CLOCK, 4);
            break;
        case 48000:
            snd_soc_write(codec, DAC_CLOCK, 0x03);
            snd_soc_write(codec, ADC_CLOCK, 0x03);
            break;
        case 96000:
            snd_soc_write(codec, DAC_CLOCK, 0x01);
            snd_soc_write(codec, ADC_CLOCK, 0x01);
            break;
        default:
            //snd_soc_write(codec, DAC_CLOCK, 0x01);
            //snd_soc_write(codec, ADC_CLOCK, 0x01);
            break;
        }
	return 0;
}
//--------------------------------------------------------------------------------------------
struct lm49350_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	unsigned int sysclk;
	unsigned int dai_fmt;
	unsigned int tdm_delay;
	unsigned int slot_width;
	struct list_head list;
	struct gpio_desc* gpio_power_amp;
	struct gpio_desc* gpio_sd_amp;
};
static const struct snd_soc_dai_ops lm49350_dai_ops = {
	.hw_params	= lm49350_hw_params,
	//.prepare	= lm49350_prepare,
	//.digital_mute	= lm49350_mute,
	//.set_sysclk	= lm49350_set_dai_sysclk,
	//.set_fmt	= lm49350_set_dai_fmt,
	//.set_tdm_slot	= lm49350_set_dai_tdm_slot,
};
//--------------------------------------------------------------------------------------------
static struct snd_soc_dai_driver lm49350_dai = {
	.name = "LM49350",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000 ,//SNDRV_PCM_RATE_8000_192000,
		.formats = LM49350_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000 ,//SNDRV_PCM_RATE_8000_192000,
		.formats = LM49350_FORMATS,},
	.ops = &lm49350_dai_ops,
	.symmetric_rates = 1,
};
//--------------------------------------------------------------------------------------------
static const struct i2c_device_id lm49350_i2c_id[] = {
	{ "lm49350", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm49350_i2c_id);
//--------------------------------------------------------------------------------------------
static int lm49350_set_bias_level(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
	return 0;
}
//--------------------------------------------------------------------------------------------
static int lm49350_init(struct snd_soc_codec *codec)
{
	//struct lm49350_priv *lm49350 = snd_soc_codec_get_drvdata(codec);

	snd_soc_write(codec, BASIC_SETUP_PMC_CLOCK, 0x00);
    snd_soc_write(codec, PMC_SETUP, 0xA0);

	
	snd_soc_write(codec, PLL_CLK_SEL, 0x00);     // input clock to PLL2 - MCLK
  	snd_soc_write(codec, PLL2_M, 9);        // PLL2_M = 5
  	snd_soc_write(codec, PLL2_N, 32); // PLL2_N = 32
  	snd_soc_write(codec, PLL2_N_MOD, 0x00);     // PLL2_N_MOD = 0
  	snd_soc_write(codec, PLL2_P, 49);       // PLL2_P = 25
    // Fout = 6144 kHz

	snd_soc_write(codec, DAC_BASIC, 0x41);//DAC settings OSR 128, clock source PLL2
  	snd_soc_write(codec, DAC_CLOCK, 5);//2048 KHz

    snd_soc_write(codec, 0x20, 0x40);//snd_soc_write(codec, 0x20, 0x43);//ADC settings, stereo, OSR 128, clock source PLL2
  	snd_soc_write(codec, 0x21, 5);   //2048 kHz

	snd_soc_write(codec, 0x42, 0x1);
  	snd_soc_write(codec, 0x43, 0x05);
    snd_soc_write(codec, 0x44, 0x11); //snd_soc_write(codec, 0x44, 0x09);
  	snd_soc_write(codec, 0x45, 0x10);

	snd_soc_write(codec, 0x50, 0x1F);//WriteAIC(0x50, 0x1B);
	snd_soc_write(codec, 0x51, 5);
	//snd_soc_write(codec, 0x51, 25);
	snd_soc_write(codec, 0x52, 0x0A);
	snd_soc_write(codec, 0x53, 0x02);
	snd_soc_write(codec, 0x54, 0x1B);
	snd_soc_write(codec, 0x55, 0x02);
	snd_soc_write(codec, 0x56, 0x02);

	snd_soc_write(codec, 0x60, 0x3e);   // audio port 2 rsv and trmt mono data, RX & TX enable, audio port will transmit the clock, audio port will transmit the sync signal, PCM
    snd_soc_write(codec, 0x61, 47);   //snd_soc_write(codec, 0x61, 63);     // master clocks in the audio port Divides By 8 ,  DAC_SOURCE_CLK ==>   384 kHz
	snd_soc_write(codec, 0x62, 0x00);   // SYNTH_DENOM (1/1), Denominator = 128 ==> devider 128
    snd_soc_write(codec, 0x63, 0x07); //snd_soc_write(codec, 0x63, 0x05);   // Number of Clock Cycles = 24 in mono mode, sync len 1 bit
	snd_soc_write(codec, 0x64, 0x9b);   // TX len = 16 bit, RX len = 16 bit, TX data output padding - High-Z
	snd_soc_write(codec, 0x65, 0x02);   // RX_MODE - 1(I2S/PCM Short), MSB Justified
	snd_soc_write(codec, 0x66, 0x02);   // TX_MODE - 1(I2S/PCM Short), MSB Justified

	snd_soc_write(codec, ANALOG_MIXER_CLASSD, 0x00);

	snd_soc_write(codec, 0x11, 0x00);
	snd_soc_write(codec, 0x12, 0x00);

	snd_soc_write(codec, 0xA8, 0x20);
	snd_soc_write(codec, 0xA9, 0x20);

	snd_soc_write(codec, 0x18, 0x27);
	snd_soc_write(codec, 0x19, 0xA7);

	snd_soc_write(codec, 0x15, 0x00);


    snd_soc_write(codec, ANALOG_MIXER_MIC_L_LVL, 0x00);
    snd_soc_write(codec, ANALOG_MIXER_MIC_R_LVL, 0x00);

	snd_soc_write(codec, BASIC_SETUP_PMC_SETUP, 0x15);//PLL2 Enb

	return 0;
	
}
//--------------------------------------------------------------------------------------------
static int lm49350_probe(struct snd_soc_codec *codec)
{
	struct lm49350_priv *lm49350 = snd_soc_codec_get_drvdata(codec);
    //struct snd_soc_dapm_context *dapm;
	
	INIT_LIST_HEAD(&lm49350->list);
	lm49350->codec = codec;

	regcache_mark_dirty(lm49350->regmap);
	lm49350_init(codec);

 	//dapm = snd_soc_codec_get_dapm(codec);
    //snd_soc_dapm_new_controls(dapm, lm49350_dapm_widgets_output, ARRAY_SIZE(lm49350_dapm_widgets_output));

	return 0;
}
//--------------------------------------------------------------------------------------------
static int lm49350_remove(struct snd_soc_codec *codec)
{
	return 0;
}
//--------------------------------------------------------------------------------------------
static struct snd_soc_codec_driver soc_codec_dev_lm49350 = 
{
	.set_bias_level = lm49350_set_bias_level,	
	.idle_bias_off = true,
	.probe = lm49350_probe,
	.remove = lm49350_remove,
    .component_driver.controls = lm49350_snd_controls,
	.component_driver.num_controls = ARRAY_SIZE(lm49350_snd_controls),
	.component_driver.dapm_widgets = lm49350_dapm_widgets,
	.component_driver.num_dapm_widgets = ARRAY_SIZE(lm49350_dapm_widgets),
	.component_driver.dapm_routes = intercon,
	.component_driver.num_dapm_routes = ARRAY_SIZE(intercon),
};
//--------------------------------------------------------------------------------------------
static int lm49350_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct lm49350_priv *lm49350;
	int ret;

	lm49350 = devm_kzalloc(&i2c->dev, sizeof(struct lm49350_priv), GFP_KERNEL);
	if (!lm49350) return -ENOMEM;
	lm49350->regmap = devm_regmap_init_i2c(i2c, &lm49350_regmap);
	if (IS_ERR(lm49350->regmap)) {
		ret = PTR_ERR(lm49350->regmap);
		return ret;
	}
	
	lm49350->gpio_power_amp = devm_gpiod_get_optional(&i2c->dev, "power-amp", GPIOD_OUT_HIGH);
	lm49350->gpio_sd_amp = devm_gpiod_get_optional(&i2c->dev, "sd-amp", GPIOD_OUT_HIGH);

	if (lm49350->gpio_power_amp){
		gpiod_set_value(lm49350->gpio_power_amp, 1);
		if(lm49350->gpio_sd_amp){
			gpiod_set_value(lm49350->gpio_sd_amp, 1);
		}
	}

//	regcache_cache_only(lm49350->regmap, true);
	regcache_cache_only(lm49350->regmap, false);
	regcache_sync(lm49350->regmap);
	i2c_set_clientdata(i2c, lm49350);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_lm49350, &lm49350_dai, 1);
	if(ret != 0) {
		return ret;
	}
	list_add(&lm49350->list, &reset_list);

	return 0;
}
//--------------------------------------------------------------------------------------------
static int lm49350_i2c_remove(struct i2c_client *client)
{

	snd_soc_unregister_codec(&client->dev);

	return 0;
}
//--------------------------------------------------------------------------------------------
#if defined(CONFIG_OF)
static const struct of_device_id lm49350_of_match[] = {
	{ .compatible = "ti,lm49350", },
	{},
};
MODULE_DEVICE_TABLE(of, lm49350_of_match);
#endif
//--------------------------------------------------------------------------------------------
/* machine i2c codec control layer */
static struct i2c_driver lm49350_i2c_driver = {
	.driver = {
		.name = "lm49350-codec",
		.of_match_table = of_match_ptr(lm49350_of_match),
	},
	.probe	= lm49350_i2c_probe,
	.remove = lm49350_i2c_remove,
	.id_table = lm49350_i2c_id,
};
//--------------------------------------------------------------------------------------------
module_i2c_driver(lm49350_i2c_driver);
//--------------------------------------------------------------------------------------------
MODULE_DESCRIPTION("ASoC LM49350 codec driver");
MODULE_AUTHOR("Alexander Zaytcev");
MODULE_LICENSE("GPL");
