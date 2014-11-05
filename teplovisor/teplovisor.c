
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>

#include <media/v4l2-device.h>
#include <media/v4l2-common.h>
#include <media/v4l2-chip-ident.h>
#include <media/media-entity.h>

MODULE_AUTHOR("minerva");
MODULE_DESCRIPTION("teplovisor linux decoder driver");
MODULE_LICENSE("GPL");

/* Number of pixels and number of lines per frame for different standards */
#define NTSC_NUM_ACTIVE_PIXELS	(384)
#define NTSC_NUM_ACTIVE_LINES		(288)

static int streaming = 0;

static struct v4l2_subdev teplovisor_sd;
static struct media_pad teplovisor_pad;

static struct v4l2_mbus_framefmt teplovisor_pad_format = {
    .width  = NTSC_NUM_ACTIVE_PIXELS,
    .height = NTSC_NUM_ACTIVE_LINES,
    .code   = V4L2_MBUS_FMT_YUYV8_2X8,
    .field  = V4L2_FIELD_NONE,
    .colorspace = V4L2_COLORSPACE_SMPTE170M,
};

static struct v4l2_pix_format teplovisor_pix_format = {
    /* NTSC 8-bit YUV 422 */
    .width        = NTSC_NUM_ACTIVE_PIXELS,
    .height       = NTSC_NUM_ACTIVE_LINES,
    .pixelformat  = V4L2_PIX_FMT_UYVY,
    .field        = V4L2_FIELD_NONE,
    .bytesperline = NTSC_NUM_ACTIVE_PIXELS * 2,
    .sizeimage    = NTSC_NUM_ACTIVE_PIXELS * 2 * NTSC_NUM_ACTIVE_LINES,
    .colorspace   = V4L2_COLORSPACE_SMPTE170M,
};

static const struct v4l2_fmtdesc teplovisor_fmtdesc = {
    .index = 0,
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .flags = 0,
    .description = "8-bit UYVY 4:2:2 Format",
    .pixelformat = V4L2_PIX_FMT_UYVY,
};

static struct v4l2_standard teplovisor_standard = {
    .index = 0,
    .id = V4L2_STD_NTSC,
    .name = "NTSC",
    .frameperiod = {1000, 25000},
    .framelines = 312
};

static int teplovisor_querystd(struct v4l2_subdev *sd, v4l2_std_id *std_id)
{
    if (std_id == NULL) return -EINVAL;

    *std_id = V4L2_STD_NTSC;

    return 0;
}

static int
teplovisor_s_std (struct v4l2_subdev *sd, v4l2_std_id std_id)
{
    v4l2_err(&teplovisor_sd, "!!! teplovisor_s_std %d\n", std_id);

    if (std_id != V4L2_STD_NTSC) return -EINVAL;

    return 0;
}

/**
 * tvp514x_s_routing() - V4L2 decoder interface handler for s_routing
 * @sd: pointer to standard V4L2 sub-device structure
 * @input: input selector for routing the signal
 * @output: output selector for routing the signal
 * @config: config value. Not used
 *
 * If index is valid, selects the requested input. Otherwise, returns -EINVAL if
 * the input is not supported or there is no active signal present in the
 * selected input.
 */
static int teplovisor_s_routing(struct v4l2_subdev *sd, u32 input, u32 output, u32 config)
{

    if (input || output) return -EINVAL;

    return 0;
}

static int
teplovisor_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qctrl)
{
    return -EINVAL;
}

static int
teplovisor_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
    return -EINVAL;
}

static int
teplovisor_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
    return -EINVAL;
}

static int
teplovisor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
    struct v4l2_captureparm *cparm;

    if (a == NULL) return -EINVAL;

    if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) /* only capture is supported */
        return -EINVAL;

    memset(a, 0, sizeof(*a));
    a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    cparm = &a->parm.capture;
    cparm->capability = V4L2_CAP_TIMEPERFRAME;
    cparm->timeperframe = teplovisor_standard.frameperiod;

    return 0;
}

static int
teplovisor_s_parm (struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
    struct v4l2_fract *timeperframe;

    if (a == NULL) return -EINVAL;

    if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) /* only capture is supported */
        return -EINVAL;

    timeperframe = &a->parm.capture.timeperframe;

    *timeperframe = teplovisor_standard.frameperiod;

    return 0;
}

static int
teplovisor_s_stream (struct v4l2_subdev *sd, int enable)
{
    int err = 0;

    if (streaming == enable) return 0;

    switch (enable) {
    case 0:
        /* Power Down Sequence */
        streaming = enable;
        break;
    case 1:
        streaming = enable;
        break;
    default:
        err = -ENODEV;
        break;
    }

    return err;
}

static int
teplovisor_mbus_fmt (struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *f)
{
    v4l2_err(&teplovisor_sd, "!!! teplovisor_mbus_fmt %d\n", (unsigned long)f);
    if (f == NULL) return -EINVAL;

    f->code = V4L2_MBUS_FMT_YUYV8_2X8;
    f->width = NTSC_NUM_ACTIVE_PIXELS;
    f->height = NTSC_NUM_ACTIVE_LINES;
    f->field = V4L2_FIELD_NONE;
    f->colorspace = V4L2_COLORSPACE_SMPTE170M;

    return 0;
}

static int
teplovisor_enum_mbus_code (struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh, struct v4l2_subdev_mbus_code_enum *code)
{
    u32 pad = code->pad;
    u32 index = code->index;

    memset(code, 0, sizeof(*code));
    code->index = index;
    code->pad = pad;

    if (index != 0) return -EINVAL;

    code->code = V4L2_MBUS_FMT_YUYV8_2X8;

    return 0;
}

static int
teplovisor_get_pad_format (struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh, struct v4l2_subdev_format *format)
{
    __u32 which	= format->which;

    if (which == V4L2_SUBDEV_FORMAT_ACTIVE)
        format->format = teplovisor_pad_format;
    else {
        format->format.code = V4L2_MBUS_FMT_YUYV8_2X8;
        format->format.width = NTSC_NUM_ACTIVE_PIXELS;
        format->format.height = NTSC_NUM_ACTIVE_LINES;
        format->format.colorspace = V4L2_COLORSPACE_SMPTE170M;
        format->format.field = V4L2_FIELD_NONE;
    }

    return 0;
}

static int
teplovisor_set_pad_format (struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh, struct v4l2_subdev_format *fmt)
{
    v4l2_err(&teplovisor_sd, "!!! teplovisor_set_pad_format\n");
    if ((fmt->format.code != V4L2_MBUS_FMT_YUYV8_2X8) ||
        (fmt->format.colorspace != V4L2_COLORSPACE_SMPTE170M) )
    {
        v4l2_err(&teplovisor_sd, "!!! teplovisor_set_pad_format failed\n");
        return -EINVAL;
    }

    return 0;
}

static const struct v4l2_subdev_core_ops teplovisor_core_ops = {
    .queryctrl = teplovisor_queryctrl,
    .g_ctrl = teplovisor_g_ctrl,
    .s_ctrl = teplovisor_s_ctrl,
    .s_std = teplovisor_s_std,
};

static const struct v4l2_subdev_video_ops teplovisor_video_ops = {
    .s_routing = teplovisor_s_routing,
    .querystd = teplovisor_querystd,
    .g_mbus_fmt = teplovisor_mbus_fmt,
    .s_mbus_fmt = teplovisor_mbus_fmt,
    .try_mbus_fmt = teplovisor_mbus_fmt,
    .g_parm = teplovisor_g_parm,
    .s_parm = teplovisor_s_parm,
    .s_stream = teplovisor_s_stream,
};

static const struct v4l2_subdev_pad_ops teplovisor_pad_ops = {
    .enum_mbus_code = teplovisor_enum_mbus_code,
    .get_fmt = teplovisor_get_pad_format,
    .set_fmt = teplovisor_set_pad_format,
};

static const struct v4l2_subdev_ops teplovisor_ops = {
    .core = &teplovisor_core_ops,
    .video = &teplovisor_video_ops,
    .pad = &teplovisor_pad_ops,
};

static int __devinit teplovisor_probe(struct platform_device *pdev)
{
    int ret;

    v4l2_subdev_init(&teplovisor_sd, &teplovisor_ops);

    teplovisor_sd.owner = THIS_MODULE;

    strlcpy(teplovisor_sd.name, "teplovisor", sizeof(teplovisor_sd.name));

#if defined(CONFIG_MEDIA_CONTROLLER)
    teplovisor_pad.flags       = MEDIA_PAD_FL_OUTPUT;
    teplovisor_sd.flags        |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    teplovisor_sd.entity.flags |= MEDIA_ENT_T_V4L2_SUBDEV_DECODER;

    ret = media_entity_init(&teplovisor_sd.entity, 1, &teplovisor_pad, 0);
    if (ret < 0) {
        v4l2_err(&teplovisor_sd, "%s decoder driver failed to register !!\n", teplovisor_sd.name);
        return ret;
    }
#endif

    pdev->dev.platform_data = &teplovisor_sd;

    v4l2_info(&teplovisor_sd, "%s decoder driver registered !!\n", teplovisor_sd.name);

    return 0;
}

static int __devexit teplovisor_remove(struct platform_device *pdev)
{
    v4l2_device_unregister_subdev(&teplovisor_sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
    media_entity_cleanup(&teplovisor_sd.entity);
#endif
    return 0;
}

static struct platform_driver teplovisor_driver = {
    .driver     = {
        .name   = "teplovisor",
    },
    .probe      = teplovisor_probe,
    .remove     = __devexit_p(teplovisor_remove),
};


static int __init teplovisor_init(void)
{
    return platform_driver_register(&teplovisor_driver);
}

static void __exit teplovisor_exit(void)
{
    platform_driver_unregister(&teplovisor_driver);
}

module_init(teplovisor_init);
module_exit(teplovisor_exit);
