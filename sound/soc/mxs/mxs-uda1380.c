/*
 * Copyright 2011 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/soc-dapm.h>

#include "mxs-saif.h"

#define UDA1380_SYSCLK  0x0

static int mxs_uda1380_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int rate = params_rate(params);
	u32 mclk;
	int ret;

	/* uda1380 does not support 512*rate when in 96000 fs */
	switch (rate) {
	case 96000:
		mclk = 256 * rate;
		break;
	default:
		mclk = 512 * rate;
		break;
	}

	/* Set UDA1380's SYSCLK (provided by SAIF MCLK) */
	ret = snd_soc_dai_set_sysclk(codec_dai, UDA1380_SYSCLK, mclk, 0);
	if (ret) {
		dev_err(codec_dai->dev, "Failed to set sysclk to %u.%03uMHz\n",
			mclk / 1000000, mclk / 1000 % 1000);
		return ret;
	}

	/* The SAIF MCLK should be the same as UDA1380_SYSCLK */
	ret = snd_soc_dai_set_sysclk(cpu_dai, MXS_SAIF_MCLK, mclk, 0);
	if (ret) {
		dev_err(cpu_dai->dev, "Failed to set sysclk to %u.%03uMHz\n",
			mclk / 1000000, mclk / 1000 % 1000);
		return ret;
	}

	return 0;
}

static const struct snd_soc_ops mxs_uda1380_hifi_ops = {
	.hw_params = mxs_uda1380_hw_params,
};

#define MXS_UDA1380_DAI_FMT (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | \
	SND_SOC_DAIFMT_CBS_CFS)

static struct snd_soc_dai_link mxs_uda1380_dai[] = {
	{
		.name		= "HiFi Tx",
		.stream_name	= "HiFi Playback",
		.codec_dai_name	= "uda1380",
		.dai_fmt	= MXS_UDA1380_DAI_FMT,
		.ops		= &mxs_uda1380_hifi_ops,
		.playback_only	= true,
	}, {
		.name		= "HiFi Rx",
		.stream_name	= "HiFi Capture",
		.codec_dai_name	= "uda1380",
		.dai_fmt	= MXS_UDA1380_DAI_FMT,
		.ops		= &mxs_uda1380_hifi_ops,
		.capture_only	= true,
	},
};

static struct snd_soc_card mxs_uda1380 = {
	.name		= "mxs_uda1380",
	.owner		= THIS_MODULE,
	.dai_link	= mxs_uda1380_dai,
	.num_links	= ARRAY_SIZE(mxs_uda1380_dai),
};

static int mxs_uda1380_probe(struct platform_device *pdev)
{
	int ret, i;
	struct snd_soc_card *card = &mxs_uda1380;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *saif_np[2], *codec_np;

	saif_np[0] = of_parse_phandle(np, "saif-controllers", 0);
	saif_np[1] = of_parse_phandle(np, "saif-controllers", 1);
	codec_np = of_parse_phandle(np, "audio-codec", 0);
	if (!saif_np[0] || !saif_np[1] || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		return -EINVAL;
	}

	for (i = 0; i < 2; i++) {
		mxs_uda1380_dai[i].codec_name = NULL;
		mxs_uda1380_dai[i].codec_of_node = codec_np;
		mxs_uda1380_dai[i].cpu_dai_name = NULL;
		mxs_uda1380_dai[i].cpu_of_node = saif_np[i];
		mxs_uda1380_dai[i].platform_name = NULL;
		mxs_uda1380_dai[i].platform_of_node = saif_np[i];
	}

	of_node_put(codec_np);
	of_node_put(saif_np[0]);
	of_node_put(saif_np[1]);

	ret = mxs_saif_get_mclk(0, 44100 * 256, 44100);
	if (ret) {
		dev_err(&pdev->dev, "failed to get mclk\n");
		return ret;
	}

	card->dev = &pdev->dev;

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		return ret;
	}

	return 0;
}

static int mxs_uda1380_remove(struct platform_device *pdev)
{
	mxs_saif_put_mclk(0);

	return 0;
}

static const struct of_device_id mxs_uda1380_dt_ids[] = {
	{ .compatible = "fsl,mxs-audio-uda1380", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mxs_uda1380_dt_ids);

static struct platform_driver mxs_uda1380_audio_driver = {
	.driver = {
		.name = "mxs-uda1380",
		.of_match_table = mxs_uda1380_dt_ids,
	},
	.probe = mxs_uda1380_probe,
	.remove = mxs_uda1380_remove,
};

module_platform_driver(mxs_uda1380_audio_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXS ALSA SoC Machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mxs-uda1380");
