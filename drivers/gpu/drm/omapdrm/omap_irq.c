/*
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Rob Clark <rob.clark@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "omap_drv.h"

struct omap_irq_wait {
	struct list_head node;
	wait_queue_head_t wq;
	uint32_t irqmask;
	int count;
};

/* call with wait_lock and dispc runtime held */
static void omap_irq_update(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_irq_wait *wait;
	uint32_t irqmask = priv->irq_mask;

	assert_spin_locked(&priv->wait_lock);

	list_for_each_entry(wait, &priv->wait_list, node)
		irqmask |= wait->irqmask;

	DBG("irqmask=%08x", irqmask);

	priv->dispc_ops->write_irqenable(irqmask);
}

static void omap_irq_wait_handler(struct omap_irq_wait *wait)
{
	wait->count--;
	wake_up(&wait->wq);
}

struct omap_irq_wait * omap_irq_wait_init(struct drm_device *dev,
		uint32_t irqmask, int count)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_irq_wait *wait = kzalloc(sizeof(*wait), GFP_KERNEL);
	unsigned long flags;

	init_waitqueue_head(&wait->wq);
	wait->irqmask = irqmask;
	wait->count = count;

	spin_lock_irqsave(&priv->wait_lock, flags);
	list_add(&wait->node, &priv->wait_list);
	omap_irq_update(dev);
	spin_unlock_irqrestore(&priv->wait_lock, flags);

	return wait;
}

int omap_irq_wait(struct drm_device *dev, struct omap_irq_wait *wait,
		unsigned long timeout)
{
	struct omap_drm_private *priv = dev->dev_private;
	unsigned long flags;
	int ret;

	ret = wait_event_timeout(wait->wq, (wait->count <= 0), timeout);

	spin_lock_irqsave(&priv->wait_lock, flags);
	list_del(&wait->node);
	omap_irq_update(dev);
	spin_unlock_irqrestore(&priv->wait_lock, flags);

	kfree(wait);

	return ret == 0 ? -1 : 0;
}

/**
 * enable_vblank - enable vblank interrupt events
 * @dev: DRM device
 * @pipe: which irq to enable
 *
 * Enable vblank interrupts for @crtc.  If the device doesn't have
 * a hardware vblank counter, this routine should be a no-op, since
 * interrupts will have to stay on to keep the count accurate.
 *
 * RETURNS
 * Zero on success, appropriate errno if the given @crtc's vblank
 * interrupt cannot be enabled.
 */
int omap_irq_enable_vblank(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct omap_drm_private *priv = dev->dev_private;
	unsigned long flags;
	enum omap_channel channel = omap_crtc_channel(crtc);

	DBG("dev=%p, crtc=%u", dev, channel);

	spin_lock_irqsave(&priv->wait_lock, flags);
	priv->irq_mask |= priv->dispc_ops->mgr_get_vsync_irq(channel);
	omap_irq_update(dev);
	spin_unlock_irqrestore(&priv->wait_lock, flags);

	return 0;
}

/**
 * disable_vblank - disable vblank interrupt events
 * @dev: DRM device
 * @pipe: which irq to enable
 *
 * Disable vblank interrupts for @crtc.  If the device doesn't have
 * a hardware vblank counter, this routine should be a no-op, since
 * interrupts will have to stay on to keep the count accurate.
 */
void omap_irq_disable_vblank(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct omap_drm_private *priv = dev->dev_private;
	unsigned long flags;
	enum omap_channel channel = omap_crtc_channel(crtc);

	DBG("dev=%p, crtc=%u", dev, channel);

	spin_lock_irqsave(&priv->wait_lock, flags);
	priv->irq_mask &= ~priv->dispc_ops->mgr_get_vsync_irq(channel);
	omap_irq_update(dev);
	spin_unlock_irqrestore(&priv->wait_lock, flags);
}

static void omap_irq_fifo_underflow(struct omap_drm_private *priv,
				    u32 irqstatus)
{
	static DEFINE_RATELIMIT_STATE(_rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);
	static const struct {
		const char *name;
		u32 mask;
	} sources[] = {
		{ "gfx", DISPC_IRQ_GFX_FIFO_UNDERFLOW },
		{ "vid1", DISPC_IRQ_VID1_FIFO_UNDERFLOW },
		{ "vid2", DISPC_IRQ_VID2_FIFO_UNDERFLOW },
		{ "vid3", DISPC_IRQ_VID3_FIFO_UNDERFLOW },
	};

	const u32 mask = DISPC_IRQ_GFX_FIFO_UNDERFLOW
		       | DISPC_IRQ_VID1_FIFO_UNDERFLOW
		       | DISPC_IRQ_VID2_FIFO_UNDERFLOW
		       | DISPC_IRQ_VID3_FIFO_UNDERFLOW;
	unsigned int i;

	spin_lock(&priv->wait_lock);
	irqstatus &= priv->irq_mask & mask;
	spin_unlock(&priv->wait_lock);

	if (!irqstatus)
		return;

	if (!__ratelimit(&_rs))
		return;

	DRM_ERROR("FIFO underflow on ");

	for (i = 0; i < ARRAY_SIZE(sources); ++i) {
		if (sources[i].mask & irqstatus)
			pr_cont("%s ", sources[i].name);
	}

	pr_cont("(0x%08x)\n", irqstatus);
}

static void omap_irq_ocp_error_handler(struct drm_device *dev,
	u32 irqstatus)
{
	if (!(irqstatus & DISPC_IRQ_OCP_ERR))
		return;

	dev_err_ratelimited(dev->dev, "OCP error\n");
}

static irqreturn_t omap_irq_handler(int irq, void *arg)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_irq_wait *wait, *n;
	unsigned long flags;
	unsigned int id;
	u32 irqstatus;

	irqstatus = priv->dispc_ops->read_irqstatus();
	priv->dispc_ops->clear_irqstatus(irqstatus);
	priv->dispc_ops->read_irqstatus();        /* flush posted write */

	VERB("irqs: %08x", irqstatus);

	for (id = 0; id < priv->num_crtcs; id++) {
		struct drm_crtc *crtc = priv->crtcs[id];
		enum omap_channel channel = omap_crtc_channel(crtc);

		if (irqstatus & priv->dispc_ops->mgr_get_vsync_irq(channel)) {
			drm_handle_vblank(dev, id);
			omap_crtc_vblank_irq(crtc);
		}

		if (irqstatus & priv->dispc_ops->mgr_get_sync_lost_irq(channel))
			omap_crtc_error_irq(crtc, irqstatus);
	}

	omap_irq_ocp_error_handler(dev, irqstatus);
	omap_irq_fifo_underflow(priv, irqstatus);
	omap_wb_irq(priv->wb_private, irqstatus);

	spin_lock_irqsave(&priv->wait_lock, flags);
	list_for_each_entry_safe(wait, n, &priv->wait_list, node) {
		if (wait->irqmask & irqstatus)
			omap_irq_wait_handler(wait);
	}
	spin_unlock_irqrestore(&priv->wait_lock, flags);

	return IRQ_HANDLED;
}

static const u32 omap_underflow_irqs[] = {
	[OMAP_DSS_GFX] = DISPC_IRQ_GFX_FIFO_UNDERFLOW,
	[OMAP_DSS_VIDEO1] = DISPC_IRQ_VID1_FIFO_UNDERFLOW,
	[OMAP_DSS_VIDEO2] = DISPC_IRQ_VID2_FIFO_UNDERFLOW,
	[OMAP_DSS_VIDEO3] = DISPC_IRQ_VID3_FIFO_UNDERFLOW,
};

/*
 * We need a special version, instead of just using drm_irq_install(),
 * because we need to register the irq via omapdss.  Once omapdss and
 * omapdrm are merged together we can assign the dispc hwmod data to
 * ourselves and drop these and just use drm_irq_{install,uninstall}()
 */

int omap_drm_irq_install(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;
	unsigned int num_mgrs = priv->dispc_ops->get_num_mgrs();
	unsigned int max_planes;
	unsigned int i;
	int ret;

	spin_lock_init(&priv->wait_lock);
	INIT_LIST_HEAD(&priv->wait_list);

	priv->irq_mask = DISPC_IRQ_OCP_ERR;

	max_planes = min(ARRAY_SIZE(priv->planes),
			 ARRAY_SIZE(omap_underflow_irqs));
	for (i = 0; i < max_planes; ++i) {
		if (priv->planes[i])
			priv->irq_mask |= omap_underflow_irqs[i];
	}

	for (i = 0; i < num_mgrs; ++i)
		priv->irq_mask |= priv->dispc_ops->mgr_get_sync_lost_irq(i);

	if (priv->dispc_ops->has_writeback())
		priv->irq_mask |= OMAP_WB_IRQ_MASK;

	priv->dispc_ops->runtime_get();
	priv->dispc_ops->clear_irqstatus(0xffffffff);
	priv->dispc_ops->runtime_put();

	ret = priv->dispc_ops->request_irq(omap_irq_handler, dev);
	if (ret < 0)
		return ret;

	dev->irq_enabled = true;

	return 0;
}

void omap_drm_irq_uninstall(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;

	if (!dev->irq_enabled)
		return;

	dev->irq_enabled = false;

	priv->dispc_ops->free_irq(dev);
}
