/*
 * SoC audio driver for RoverPC E5/Amoi E860
 *
 * Copyright 2007, 2009 CompuLab, Ltd.
 *
 * Author: Sergey Larin <cerg2010cerg2010@gmail.com>
 *
 * Based on gsm6323_wm9713.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <mach/audio.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	include <sound/soc-dapm.h>
#	include "pxa2xx-pcm.h"
#	include "pxa2xx-ac97.h"
#endif

#include "../codecs/wm9713.h"

#ifndef ARRAY_AND_SIZE
#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)
#endif

/* gsm6323 machine dapm widgets */
static const struct snd_soc_dapm_widget gsm6323_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Front Speaker", NULL),
	SND_SOC_DAPM_LINE("GSM Line In", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* GSM Module */
	{"GSM Line In", NULL, "MONO"},

	/* front speaker connected to SPKR, OUT3 */
	{"Front Speaker", NULL, "SPKR"},
	{"Front Speaker", NULL, "OUT3"},
};

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
static int gsm6323_wm9713_init(struct snd_soc_codec *codec)
{
	/* Add gsm6323 specific widgets */
	snd_soc_dapm_new_controls(codec, ARRAY_AND_SIZE(gsm6323_dapm_widgets));

	/* Set up gsm6323 specific audio path audio_mapnects */
	snd_soc_dapm_add_routes(codec, ARRAY_AND_SIZE(audio_map));

	snd_soc_dapm_enable_pin(codec, "Front Speaker");
	snd_soc_dapm_enable_pin(codec, "GSM Line In");
	snd_soc_dapm_sync(codec);
 	return 0;

}
#else
static int gsm6323_wm9713_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}
#endif

static struct snd_soc_ops gsm6323_ops;

static struct snd_soc_dai_link gsm6323_dai[] = {
	{
		.name = "AC97",
		.stream_name = "AC97 HiFi",
		.init = gsm6323_wm9713_init,
		.ops = &gsm6323_ops,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
		.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_HIFI],
		.codec_dai = &wm9713_dai[WM9713_DAI_AC97_HIFI],
#else
		.cpu_dai_name = "pxa2xx-ac97",
		.codec_dai_name = "wm9713-hifi",
		.codec_name = "wm9713-codec",
		.platform_name = "pxa-pcm-audio",
#endif
	},
	{
		.name = "AC97 Aux",
		.stream_name = "AC97 Aux",
		.ops = &gsm6323_ops,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
		.cpu_dai = &pxa_ac97_dai[PXA2XX_DAI_AC97_AUX],
		.codec_dai = &wm9713_dai[WM9713_DAI_AC97_AUX],
#else
		.cpu_dai_name = "pxa2xx-ac97-aux",
		.codec_dai_name = "wm9713-aux",
		.codec_name = "wm9713-codec",
		.platform_name = "pxa-pcm-audio",
#endif
	},
};

static struct snd_soc_card gsm6323_audio = {
	.name = "GSM6323-audio",
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
	.platform = &pxa2xx_soc_platform,
#else
	.owner = THIS_MODULE,

	.dapm_widgets = gsm6323_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(gsm6323_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
#endif
	.dai_link = gsm6323_dai,
	.num_links = ARRAY_SIZE(gsm6323_dai),
};

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
static struct snd_soc_device gsm6323_snd_devdata = {
	.card = &gsm6323_audio,
	.codec_dev = &soc_codec_dev_wm9713,
};

static struct platform_device *gsm6323_snd_device;
#endif

static int gsm6323_wm9713_probe(struct platform_device *pdev)
{
	int rc;

	if (!machine_is_gsm6323())
		return -ENODEV;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
	gsm6323_snd_device = platform_device_alloc("soc-audio", -1);
	if (!gsm6323_snd_device)
		return -ENOMEM;

	platform_set_drvdata(gsm6323_snd_device, &gsm6323_snd_devdata);
	gsm6323_snd_devdata.dev = &gsm6323_snd_device->dev;

	rc = platform_device_add(gsm6323_snd_device);
	if (!rc)
		return 0;

	platform_device_put(gsm6323_snd_device);
#else
	gsm6323_audio.dev = &pdev->dev;
	rc = devm_snd_soc_register_card(&pdev->dev, &gsm6323_audio);
	if (!rc)
		dev_warn(&pdev->dev, "Be warned that incorrect mixers/muxes "
				"setup will lead to overheating and possible "
				"destruction of your device.");
#endif

	return rc;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
static int __devexit gsm6323_wm9713_remove(struct platform_device *pdev)
{
	platform_device_unregister(gsm6323_snd_device);
	return 0;
}
#endif

static struct platform_driver gsm6323_wm9713_driver = {
	.probe		= gsm6323_wm9713_probe,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
	.remove		= __devexit_p(gsm6323_wm9713_remove),
#endif
	.driver		= {
		.name		= "gsm6323-wm9713",
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
		.owner		= THIS_MODULE,
#else
		.pm     = &snd_soc_pm_ops,
#endif
	},
};

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
static int __init gsm6323_asoc_init(void)
{
	return platform_driver_register(&gsm6323_wm9713_driver);
}

static void __exit gsm6323_asoc_exit(void)
{
	platform_driver_unregister(&gsm6323_wm9713_driver);
}

module_init(gsm6323_asoc_init);
module_exit(gsm6323_asoc_exit);
#else
module_platform_driver(gsm6323_wm9713_driver);
#endif

/* Module information */
MODULE_AUTHOR("Sergey Larin (cerg2010cerg2010@gmail.com)");
MODULE_DESCRIPTION("ALSA SoC WM9713 GSM6323");
MODULE_LICENSE("GPL");
