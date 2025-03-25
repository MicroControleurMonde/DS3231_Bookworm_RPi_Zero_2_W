#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/rtc.h>

/*
 * Information:
 * OS Release: PRETTY_NAME="Raspbian GNU/Linux 12 (bookworm)"
 * OS Name: NAME="Raspbian GNU/Linux"
 * OS Version ID: VERSION_ID="12"
 */

/* IOCTL command definition for retrieving the last message */
#define DS3231_LAST_MESSAGE _IOR('A', 0xFF, u8)

/* Register addresses for RTC (Real-Time Clock) on the DS3231 */
enum DS3231_REG_RTC {
    DS3231_REG_SEC = 0x00,  /* Seconds Register */
    DS3231_REG_MIN = 0x01,  /* Minutes Register */
    DS3231_REG_HRS = 0x02,  /* Hours Register */
    DS3231_REG_WDAY = 0x03, /* Day of Week Register */
    DS3231_REG_MDAY = 0x04, /* Day of Month Register */
    DS3231_REG_MON = 0x05,  /* Month Register */
    DS3231_REG_YEAR = 0x06  /* Year Register */
};

/* Control registers for configuration of the DS3231 */
enum DS3231_REG_CTL {
    DS3231_REG_CONTROL = 0x0E, /* Control Register */
    DS3231_REG_STATUS = 0x0F,  /* Status Register */
    DS3231_REG_AGEOFF = 0x10,  /* Aging Offset Register */
    DS3231_REG_TEMPMSB = 0x11, /* Temperature MSB Register */
    DS3231_REG_TEMPLSB = 0x12  /* Temperature LSB Register */
};

/* Bit masks for RTC register fields */
enum DS3231_RTC_MSK {
    DS3231_MSK_SEC = 0x0F,   /* Mask for seconds */
    DS3231_MSK_10SEC = 0x70, /* Mask for tens of seconds */
    DS3231_MSK_MIN = 0x0F,   /* Mask for minutes */
    DS3231_MSK_10MIN = 0x70, /* Mask for tens of minutes */
    DS3231_MSK_HR = 0x0F,    /* Mask for hours */
    DS3231_MSK_10HR = 0x10,  /* Mask for tens of hours */
    DS3231_MSK_20HR = 0x20,  /* Mask for 20-hour format */
    DS3231_MSK_DAY = 0x0F,   /* Mask for day of the month */
    DS3231_MSK_10DAY = 0x30, /* Mask for tens of days */
    DS3231_MSK_MON = 0x0F,   /* Mask for month */
    DS3231_MSK_10MON = 0x10, /* Mask for tens of months */
    DS3231_MSK_CENTURY = 0x80, /* Mask for century indicator */
    DS3231_MSK_YEAR = 0x0F,  /* Mask for year */
    DS3231_MSK_10YEAR = 0xF0  /* Mask for tens of years */
};

/* Bit masks for control registers (e.g., interrupt enable) */
enum DS3231_CTL_MSK {
    DS3231_MSK_EOSC = 0x80,   /* Enable oscillator stop flag */
    DS3231_MSK_INTCN = 0x04,  /* Interrupt control enable */
    DS3231_MSK_A2IE = 0x02,   /* Alarm 2 interrupt enable */
    DS3231_MSK_A1IE = 0x01,   /* Alarm 1 interrupt enable */
    DS3231_MSK_OSF = 0x80,    /* Oscillator stop flag */
    DS3231_MSK_HR_SELECT = 0x40 /* 24-hour or 12-hour mode select */
};

/* Variable to store the last message sent */
u8 last_message;

/* Function to calculate the day of the week (0=Sunday, 6=Saturday) */
int calculate_wday(int year, int month, int day) {
    int century, yearOfCentury, h;
    if (month < 3) {
        month += 12;
        year--;
    }
    century = year / 100;
    yearOfCentury = year % 100;
    h = (day + ((13 * (month + 1)) / 5) + yearOfCentury + (yearOfCentury / 4) + (century / 4) - (2 * century)) % 7;
    return (h + 5) % 7;
}

/* Function to calculate the day of the year (1-366) */
int calculate_yday(int day, int mon, int year) {
    static const int days[2][13] = {
        {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334}, /* Non-leap year */
        {0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}  /* Leap year */
    };
    return days[is_leap_year(year)][mon] + day;
}

/* Function to read the current time from the DS3231 RTC */
static int ds3231_read_time(struct device *dev, struct rtc_time *tm) {
    struct i2c_client *client = to_i2c_client(dev);
    s32 ret;
    u8 reg;

    /* Read seconds */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_SEC);
    if (ret < 0) {
        pr_err("Error %d during seconds read\n", ret);
        return ret;
    }
    reg = (u8)(ret);
    tm->tm_sec = ((reg >> 4) & 0x7) * 10 + (reg & 0xF);

    /* Read minutes */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_MIN);
    if (ret < 0) {
        pr_err("Error %d during minutes read\n", ret);
        return ret;
    }
    reg = (u8)(ret);
    tm->tm_min = ((reg >> 4) & 0x7) * 10 + (reg & 0xF);

    /* Read hours */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_HRS);
    if (ret < 0) {
        pr_err("Error %d during hours read\n", ret);
        return ret;
    }
    reg = (u8)(ret);
    tm->tm_hour = ((reg & (DS3231_MSK_20HR | DS3231_MSK_10HR)) >> 4) * 10 + (reg & DS3231_MSK_HR);

    /* Read day of the month */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_MDAY);
    if (ret < 0) {
        pr_err("Error %d during day read\n", ret);
        return ret;
    }
    reg = (u8)(ret);
    tm->tm_mday = ((reg >> 4) & 0x3) * 10 + (reg & 0xF);

    /* Read month */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_MON);
    if (ret < 0) {
        pr_err("Error %d during month read\n", ret);
        return ret;
    }
    reg = (u8)(ret);
    tm->tm_mon = (((reg >> 4) & 0x1) * 10 + (reg & 0xF)) - 1;  /* Month is 0-indexed */
    tm->tm_year = (reg & DS3231_MSK_CENTURY) ? 100 : 0;

    /* Read year */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_YEAR);
    if (ret < 0) {
        pr_err("Error %d during year read\n", ret);
        return ret;
    }
    reg = (u8)(ret);
    tm->tm_year += ((reg >> 4) & 0xF) * 10 + (reg & 0xF);

    /* Calculate day of the week and day of the year */
    tm->tm_wday = calculate_wday(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    tm->tm_yday = calculate_yday(tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);

    /* DST not supported */
    tm->tm_isdst = 0;

    return 0;
}

/* Function to write the time to the DS3231 RTC */
static int ds3231_write_time(struct device *dev, struct rtc_time *tm) {
    struct i2c_client *client = to_i2c_client(dev);
    s32 ret;

    /* Write seconds */
    ret = i2c_smbus_write_byte_data(client, DS3231_REG_SEC, ((tm->tm_sec / 10) << 4) | (tm->tm_sec % 10));
    if (ret < 0) {
        pr_err("Error %d during seconds write\n", ret);
        return ret;
    }

    /* Write minutes */
    ret = i2c_smbus_write_byte_data(client, DS3231_REG_MIN, ((tm->tm_min / 10) << 4) | (tm->tm_min % 10));
    if (ret < 0) {
        pr_err("Error %d during minutes write\n", ret);
        return ret;
    }

    /* Write hours */
    ret = i2c_smbus_write_byte_data(client, DS3231_REG_HRS, ((tm->tm_hour / 20) << 5) | (((tm->tm_hour % 20) / 10) << 4) | ((tm->tm_hour % 20) % 10));
    if (ret < 0) {
        pr_err("Error %d during hour write\n", ret);
        return ret;
    }

    /* Write day of the month */
    ret = i2c_smbus_write_byte_data(client, DS3231_REG_MDAY, ((tm->tm_mday / 10) << 4) | (tm->tm_mday % 10));
    if (ret < 0) {
        pr_err("Error %d during day write\n", ret);
        return ret;
    }

    /* Write month */
    ret = i2c_smbus_write_byte_data(client, DS3231_REG_MON, ((tm->tm_year >= 100) << 7) | (((tm->tm_mon + 1) / 10) << 4) | ((tm->tm_mon + 1) % 10));
    if (ret < 0) {
        pr_err("Error %d during month write\n", ret);
        return ret;
    }

    /* Write year */
    ret = i2c_smbus_write_byte_data(client, DS3231_REG_YEAR, (((tm->tm_year % 100) / 10) << 4) | ((tm->tm_year % 100) % 10));
    if (ret < 0) {
        pr_err("Error %d during year write\n", ret);
        return ret;
    }

    return 0;
}

/* IOCTL function to handle device-specific commands */
static int ds3231_ioctl(struct device *dev, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case DS3231_LAST_MESSAGE:
            /* Return the last message to user-space */
            if (copy_to_user((void __user *)arg, &last_message, sizeof(u8)))
                return -EFAULT;
            return sizeof(u8);
        break;
        default:
            return -EINVAL;
    }
}

/* RTC operations structure, including read/write functions */
static const struct rtc_class_ops ds3231_rtc_ops = {
    .read_time = ds3231_read_time,
    .set_time = ds3231_write_time,
    .ioctl = ds3231_ioctl
};

/* Probe function to initialize the DS3231 RTC driver */
static int ds3231_probe(struct i2c_client *client) {
    s32 ret;
    u8 reg;
    struct rtc_device *rtc;
    last_message = 0;

    /* Allocate an RTC device */
    rtc = devm_rtc_allocate_device(&client->dev);
    if (IS_ERR(rtc)) {
        dev_err(&client->dev, "Failed to allocate RTC device\n");
        return PTR_ERR(rtc);
    }

    rtc->ops = &ds3231_rtc_ops;
    rtc->range_max = U32_MAX;  /* Max possible range for RTC */
    i2c_set_clientdata(client, rtc);

    /* Register the RTC device */
    ret = devm_rtc_register_device(rtc);
    if (ret) {
        dev_err(&client->dev, "Failed to register RTC device\n");
        return ret;
    }

    /* Read and configure the control register */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_CONTROL);
    if (ret < 0) {
        pr_err("Communication failed\n");
        return ret;
    }
    reg = (u8)ret;

    /* Disable interrupts and stop oscillator if needed */
    if (reg & DS3231_MSK_A1IE || reg & DS3231_MSK_A2IE || reg & DS3231_MSK_INTCN || reg & DS3231_MSK_EOSC) {
        reg &= ~DS3231_MSK_A1IE;
        reg &= ~DS3231_MSK_A2IE;
        reg &= ~DS3231_MSK_INTCN;
        reg &= ~DS3231_MSK_EOSC;
        if (i2c_smbus_write_byte_data(client, DS3231_REG_CONTROL, reg) < 0) {
            pr_err("Communication failed\n");
            return ret;
        }
    }

    /* Read and clear oscillator stop flag if set */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_STATUS);
    if (ret < 0) {
        pr_err("Status read failed\n");
        return ret;
    }
    reg = (u8)ret;
    
    if (reg & DS3231_MSK_OSF) {
        reg &= ~DS3231_MSK_OSF;
        if (i2c_smbus_write_byte_data(client, DS3231_REG_STATUS, reg) < 0) {
            pr_err("Communication failed\n");
            return ret;
        }
    }
    
    /* Check and adjust hour mode to 24-hour format if needed */
    ret = i2c_smbus_read_byte_data(client, DS3231_REG_HRS);
    if (ret < 0) {
        pr_err("Hour mode read failed\n");
        return ret;
    }
    reg = (u8)ret;
    if (reg & DS3231_MSK_HR_SELECT) {
        reg &= ~DS3231_MSK_HR_SELECT;
        if (i2c_smbus_write_byte_data(client, DS3231_REG_HRS, reg) < 0) {
            pr_err("Communication failed\n");
            return ret;
        }
    }
    return 0;
}

/* Remove function when the device is removed */
static void ds3231_remove(struct i2c_client *client)
{
    // No specific action needed at the moment
}

/* Device ID table */
static const struct i2c_device_id ds3231_rtc_id[] = {
    {"ds3231", 0},   /* The device name */
    {}
};
MODULE_DEVICE_TABLE(i2c, ds3231_rtc_id);

/* Device tree match table */
static const struct of_device_id ds3231_of_match[] = {
    {.compatible = "maxim,ds3231"}, /* Compatible with DS3231 RTC */
    {}
};
MODULE_DEVICE_TABLE(of, ds3231_of_match);

/* I2C driver structure */
static struct i2c_driver ds3231_driver = {
    .driver = {
        .name = "rtc-ds3231",      /* Driver name */
        .of_match_table = ds3231_of_match,  /* Device tree match table */
    },
    .probe = ds3231_probe,
    .remove = ds3231_remove,
    .id_table = ds3231_rtc_id,
};

/* Register the I2C driver */
module_i2c_driver(ds3231_driver);

/* Module metadata */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MicroControleurMonde");
MODULE_DESCRIPTION("DS3231 driver for RPi Zero 2W - https://github.com/MicroControleurMonde");
