#ifndef _LM49350_H
#define _LM49350_H

#define BASIC_SETUP_PMC_SETUP                0x00
#define BASIC_SETUP_PMC_CLOCK                0x01
#define PMC_SETUP                            0x02
#define PLL_CLK_SEL                          0x03
#define PLL2_M                               0x09
#define PLL2_N                               0x0A
#define PLL2_N_MOD                           0x0B
#define PLL2_P                               0x0C
#define ANALOG_MIXER_HEADPHONESL             0x11
#define ANALOG_MIXER_HEADPHONESR             0x12
#define ANALOG_MIXER_OUTPUT_OPTIONS          0x14
#define ANALOG_MIXER_ADC                     0x15
#define ANALOG_MIXER_MIC_L_LVL               0x16
#define ANALOG_MIXER_MIC_R_LVL               0x17
#define ANALOG_MIXER_HP_SENSE                0x1B
#define ADC_BASIC                            0x20
#define ADC_CLOCK                            0x21
#define DAC_BASIC                            0x30
#define DAC_MUTE                             0x30
#define DAC_CLOCK                            0x31
#define AUDIO_PORT_2_INPUT                   0x43
#define DAC_IP_SELECT                        0x44
#define AUDIO_PORT1_BASIC                    0x50
#define PLL_M                                0x04
#define PLL_N                                0x05
#define PLL_N_MOD                            0x06
#define PLL_P1                               0x07
#define ANALOG_MIXER_CLASSD                  0x10
#define ANALOG_MIXER_AUX_OUT                 0x13
#define ANALOG_MIXER_AUXL_LVL                0x18
#define ADC_MIXER                            0x23
#define AUDIO_PORT1_IP                       0x42
#define ADC_EFFECTS_HPF                      0x80
#define ADC_EFFECTS_ADC_ALC4                 0x84
#define ADC_EFFECTS_ADC_ALC5                 0x85
#define ADC_EFFECTS_ADC_ALC6                 0x86
#define ADC_EFFECTS_ADC_ALC7                 0x87
#define ADC_EFFECTS_ADC_L_LEVEL              0x89
#define ADC_EFFECTS_ADC_R_LEVEL              0x8A
#define DAC_EFFECTS_DAC_ALC1                 0xA0
#define DAC_EFFECTS_DAC_ALC4                 0xA3
#define DAC_EFFECTS_DAC_ALC5                 0xA4
#define DAC_EFFECTS_DAC_ALC6                 0xA5
#define DAC_EFFECTS_DAC_ALC7                 0xA6
#define DAC_EFFECTS_DAC_L_LEVEL              0xA8
#define DAC_EFFECTS_DAC_R_LEVEL              0xA9
#define SPREAD_SPECTRUM_RESET                0xF0


#define LM49350_DAC_CONTROL3                  0x71
#define LM49350_DAC_CONTROL4                  0xff
#define LM49350_DAC_CONTROL5                  0x30
#define LM49350_RESET                         0xf0
#define LM49350_ADC_CONTROL1                  0x20
#define LM49350_MUTE                          0x0C
#define LM49350_DIGITAL_ATTENUATION_DACL1     0xA8
#define LM49350_DIGITAL_ATTENUATION_DACR1     0xA9

#endif
