/*
 * @copyright (c) 2023, MR Development Team
 *
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @date 2023-10-20    MacRsh       First version
 */

#include "include/device/serial.h"

#ifdef MR_USING_SERIAL

static int mr_serial_open(struct mr_dev *dev)
{
    struct mr_serial *serial = (struct mr_serial *)dev;
    struct mr_serial_ops *ops = (struct mr_serial_ops *)dev->drv->ops;

    /* Allocate FIFO buffers */
    int ret = mr_ringbuf_allocate(&serial->rd_fifo, serial->rd_bufsz);
    if (ret != MR_EOK)
    {
        return ret;
    }
    ret = mr_ringbuf_allocate(&serial->wr_fifo, serial->wr_bufsz);
    if (ret != MR_EOK)
    {
        return ret;
    }

    return ops->configure(serial, &serial->config);
}

static int mr_serial_close(struct mr_dev *dev)
{
    struct mr_serial *serial = (struct mr_serial *)dev;
    struct mr_serial_ops *ops = (struct mr_serial_ops *)dev->drv->ops;
    struct mr_serial_config close_config = {0};

    /* Free FIFO buffers */
    mr_ringbuf_free(&serial->rd_fifo);
    mr_ringbuf_free(&serial->wr_fifo);

    return ops->configure(serial, &close_config);
}

static ssize_t mr_serial_read(struct mr_dev *dev, int off, void *buf, size_t size, int sync_or_async)
{
    struct mr_serial *serial = (struct mr_serial *)dev;
    struct mr_serial_ops *ops = (struct mr_serial_ops *)dev->drv->ops;

    if (mr_ringbuf_get_bufsz(&serial->rd_fifo) == 0)
    {
        return ops->read(serial, buf, size);
    } else
    {
        return (ssize_t)mr_ringbuf_read(&serial->rd_fifo, buf, size);
    }
}

static ssize_t mr_serial_write(struct mr_dev *dev, int off, const void *buf, size_t size, int sync_or_async)
{
    struct mr_serial *serial = (struct mr_serial *)dev;
    struct mr_serial_ops *ops = (struct mr_serial_ops *)dev->drv->ops;

    if (sync_or_async == MR_SYNC)
    {
        return ops->write(serial, buf, size);
    } else
    {
        if (mr_ringbuf_get_bufsz(&serial->wr_fifo) == 0)
        {
            return ops->write(serial, buf, size);
        } else
        {
            ssize_t ret = (ssize_t)mr_ringbuf_write(&serial->wr_fifo, buf, size);

            /* Start interrupt sending */
            ops->start_tx(serial);
            return ret;
        }
    }
}

static int mr_serial_ioctl(struct mr_dev *dev, int off, int cmd, void *args)
{
    struct mr_serial *serial = (struct mr_serial *)dev;
    struct mr_serial_ops *ops = (struct mr_serial_ops *)dev->drv->ops;

    switch (cmd)
    {
        case MR_CTRL_SET_CONFIG:
        {
            if (args != MR_NULL)
            {
                struct mr_serial_config config = *(struct mr_serial_config *)args;

                int ret = ops->configure(serial, &config);
                if (ret == MR_EOK)
                {
                    serial->config = config;
                }
                return ret;
            }
            return MR_EINVAL;
        }
        case MR_CTRL_SET_RD_BUFSZ:
        {
            if (args != MR_NULL)
            {
                size_t bufsz = *(size_t *)args;

                int ret = mr_ringbuf_allocate(&serial->rd_fifo, bufsz);
                serial->rd_bufsz = 0;
                if (ret == MR_EOK)
                {
                    serial->rd_bufsz = bufsz;
                }
                return ret;
            }
            return MR_EINVAL;
        }
        case MR_CTRL_SET_WR_BUFSZ:
        {
            if (args != MR_NULL)
            {
                size_t bufsz = *(size_t *)args;

                int ret = mr_ringbuf_allocate(&serial->wr_fifo, bufsz);
                serial->wr_bufsz = 0;
                if (ret == MR_EOK)
                {
                    serial->wr_bufsz = bufsz;
                }
                return ret;
            }
            return MR_EINVAL;
        }

        case MR_CTRL_GET_CONFIG:
        {
            if (args != MR_NULL)
            {
                struct mr_serial_config *config = (struct mr_serial_config *)args;

                *config = serial->config;
                return MR_EOK;
            }
            return MR_EINVAL;
        }
        case MR_CTRL_GET_RD_BUFSZ:
        {
            if (args != MR_NULL)
            {
                *(size_t *)args = serial->rd_bufsz;
                return MR_EOK;
            }
            return MR_EINVAL;
        }
        case MR_CTRL_GET_WR_BUFSZ:
        {
            if (args != MR_NULL)
            {
                *(size_t *)args = serial->wr_bufsz;
                return MR_EOK;
            }
            return MR_EINVAL;
        }

        default:
        {
            return MR_ENOTSUP;
        }
    }
}

static ssize_t mr_serial_isr(struct mr_dev *dev, int event, void *args)
{
    struct mr_serial *serial = (struct mr_serial *)dev;
    struct mr_serial_ops *ops = (struct mr_serial_ops *)dev->drv->ops;

    switch (event)
    {
        case MR_ISR_EVENT_RD_INTER:
        {
            uint8_t data = 0;

            /* Read data to FIFO */
            ops->read(serial, &data, sizeof(data));
            mr_ringbuf_push_force(&serial->rd_fifo, data);

            return (ssize_t)mr_ringbuf_get_data_size(&serial->rd_fifo);
        }
        case MR_ISR_EVENT_WR_INTER:
        {
            uint8_t data = 0;

            /* Write data from FIFO */
            if (mr_ringbuf_pop(&serial->wr_fifo, &data) == sizeof(data))
            {
                ops->write(serial, &data, sizeof(data));
            } else
            {
                ops->stop_tx(serial);
            }

            return (ssize_t)mr_ringbuf_get_data_size(&serial->wr_fifo);
        }

        default:
        {
            return MR_ENOTSUP;
        }
    }
}

/**
 * @brief This function register a serial.
 *
 * @param serial The serial.
 * @param name The name of the serial.
 * @param drv The driver of the serial.
 *
 * @return MR_EOK on success, otherwise an error code.
 */
int mr_serial_register(struct mr_serial *serial, const char *name, struct mr_drv *drv)
{
    static struct mr_dev_ops ops =
        {
            mr_serial_open,
            mr_serial_close,
            mr_serial_read,
            mr_serial_write,
            mr_serial_ioctl,
            mr_serial_isr
        };
    struct mr_serial_config default_config = MR_SERIAL_CONFIG_DEFAULT;

    mr_assert(serial != MR_NULL);
    mr_assert(name != MR_NULL);
    mr_assert(drv != MR_NULL);
    mr_assert(drv->ops != MR_NULL);

    /* Initialize the fields */
    serial->config = default_config;
    mr_ringbuf_init(&serial->rd_fifo, MR_NULL, 0);
    mr_ringbuf_init(&serial->wr_fifo, MR_NULL, 0);
#ifndef MR_CFG_SERIAL_RD_BUFSZ
#define MR_CFG_SERIAL_RD_BUFSZ          (0)
#endif /* MR_CFG_SERIAL_RD_BUFSZ */
#ifndef MR_CFG_SERIAL_WR_BUFSZ
#define MR_CFG_SERIAL_WR_BUFSZ          (0)
#endif /* MR_CFG_SERIAL_WR_BUFSZ */
    serial->rd_bufsz = MR_CFG_SERIAL_RD_BUFSZ;
    serial->wr_bufsz = MR_CFG_SERIAL_WR_BUFSZ;

    /* Register the serial */
    return mr_dev_register(&serial->dev, name, Mr_Dev_Type_Serial, MR_SFLAG_RDWR | MR_SFLAG_NONBLOCK, &ops, drv);
}

#endif /* MR_USING_SERIAL */