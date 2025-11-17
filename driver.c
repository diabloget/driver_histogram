#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DRIVER_NAME "lcd_i2c"
#define LCD_ROWS 2
#define LCD_COLS 16

// LCD Commands
#define LCD_CLEAR 0x01
#define LCD_HOME 0x02
#define LCD_ENTRY_MODE 0x04
#define LCD_DISPLAY_CTRL 0x08
#define LCD_FUNCTION_SET 0x20
#define LCD_SET_DDRAM 0x80

// Flags 
#define LCD_ENTRY_LEFT 0x02
#define LCD_DISPLAY_ON 0x04
#define LCD_CURSOR_OFF 0x00
#define LCD_BLINK_OFF 0x00
#define LCD_4BIT_MODE 0x00
#define LCD_2LINE 0x08
#define LCD_5x8DOTS 0x00

// chip bits 
#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_EN 0x04
#define LCD_BL 0x08

struct lcd_dev {
    struct i2c_client *client;
    struct cdev cdev;
    dev_t devt;
    struct class *class;
    struct device *device;
    u8 backlight;
    u8 row;
    u8 col;
};

static struct lcd_dev *lcd_device;

// Write byte to I2C expander 
static int lcd_write_byte(struct lcd_dev *lcd, u8 data)
{
    int ret;
    ret = i2c_smbus_write_byte(lcd->client, data | lcd->backlight);
    if (ret < 0)
        pr_err("I2C write failed: %d\n", ret);
    return ret;
}

// Pulse enable bit 
static void lcd_pulse_enable(struct lcd_dev *lcd, u8 data)
{
    lcd_write_byte(lcd, data | LCD_EN);
    udelay(1);
    lcd_write_byte(lcd, data & ~LCD_EN);
    udelay(50);
}

// Write 4 bits to LCD 
static void lcd_write_4bits(struct lcd_dev *lcd, u8 value, u8 mode)
{
    u8 data = (value & 0xF0) | mode;
    lcd_write_byte(lcd, data);
    lcd_pulse_enable(lcd, data);
}

// Send command or data to LCD 
static void lcd_send(struct lcd_dev *lcd, u8 value, u8 mode)
{
    lcd_write_4bits(lcd, value & 0xF0, mode);
    lcd_write_4bits(lcd, value << 4, mode);
}

// Send command 
static void lcd_command(struct lcd_dev *lcd, u8 cmd)
{
    lcd_send(lcd, cmd, 0);
    if (cmd == LCD_CLEAR || cmd == LCD_HOME)
        mdelay(5);  
    else
        udelay(100);
}

// Send data (character) 
static void lcd_data(struct lcd_dev *lcd, u8 data)
{
    lcd_send(lcd, data, LCD_RS);
}

// Initialize LCD 
static int lcd_init(struct lcd_dev *lcd)
{
    lcd->backlight = LCD_BL;
    lcd->row = 0;
    lcd->col = 0;

    // Wait for LCD to power up 
    mdelay(50);

    // Initialize in 4-bit mode 
    lcd_write_4bits(lcd, 0x30, 0);
    mdelay(5);
    lcd_write_4bits(lcd, 0x30, 0);
    udelay(150);
    lcd_write_4bits(lcd, 0x30, 0);
    lcd_write_4bits(lcd, 0x20, 0);

    // Function set: 4-bit, 2 lines, 5x8 font 
    lcd_command(lcd, LCD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2LINE | LCD_5x8DOTS);
    
    // Display control: display on, cursor off, blink off 
    lcd_command(lcd, LCD_DISPLAY_CTRL | LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF);
    
    // Clear display
    lcd_command(lcd, LCD_CLEAR);
    
    // Entry mode: left to right
    lcd_command(lcd, LCD_ENTRY_MODE | LCD_ENTRY_LEFT);

    return 0;
}

// Clear LCD 
static void lcd_clear(struct lcd_dev *lcd)
{
    lcd_command(lcd, LCD_CLEAR);
    mdelay(5);  
    lcd->row = 0;
    lcd->col = 0;
    lcd_command(lcd, LCD_HOME);  
}

// Set cursor position 
static void lcd_set_cursor(struct lcd_dev *lcd, u8 row, u8 col)
{
    static const u8 row_offsets[] = {0x00, 0x40};
    
    if (row >= LCD_ROWS)
        row = LCD_ROWS - 1;
    if (col >= LCD_COLS)
        col = LCD_COLS - 1;
    
    lcd->row = row;
    lcd->col = col;
    lcd_command(lcd, LCD_SET_DDRAM | (col + row_offsets[row]));
}

// Write string to LCD 
static void lcd_print(struct lcd_dev *lcd, const char *str, size_t len)
{
    size_t i;
    
    for (i = 0; i < len; i++) {
        if (str[i] == '\n') {
            // Go to next row 
            lcd->row = (lcd->row + 1) % LCD_ROWS;
            lcd->col = 0;
            lcd_set_cursor(lcd, lcd->row, lcd->col);
        } else if (str[i] == '\r') {
            // Go to start of line 
            lcd->col = 0;
            lcd_set_cursor(lcd, lcd->row, lcd->col);
        } else {
            // Regular character 
            lcd_data(lcd, str[i]);
            lcd->col++;
            if (lcd->col >= LCD_COLS) {
                lcd->col = 0;
                lcd->row = (lcd->row + 1) % LCD_ROWS;
                lcd_set_cursor(lcd, lcd->row, lcd->col);
            }
        }
    }
}

// Character device operations 
static int lcd_open(struct inode *inode, struct file *file)
{
    file->private_data = lcd_device;
    return 0;
}

static ssize_t lcd_write(struct file *file, const char __user *buf, 
                         size_t count, loff_t *ppos)
{
    struct lcd_dev *lcd = file->private_data;
    char *kbuf;
    
    if (count == 0)
        return 0;
    
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    
    // Handle special command: clear screen with "\f" (NO FUNCIONA)
    if (count == 1 && kbuf[0] == '\f') {
        lcd_clear(lcd);
    } else {
        lcd_print(lcd, kbuf, count);
    }
    
    kfree(kbuf);
    return count;
}

static long lcd_ctrl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct lcd_dev *lcd = file->private_data;
    
    switch (cmd) {
    case 0: /* Clear display */
        lcd_clear(lcd);
        break;
    case 1: /* Set cursor position (arg = row << 8 | col) */
        lcd_set_cursor(lcd, (arg >> 8) & 0xFF, arg & 0xFF);
        break;
    case 2: /* Backlight control (arg = 0 or 1) */
        lcd->backlight = arg ? LCD_BL : 0;
        lcd_write_byte(lcd, lcd->backlight);
        break;
    default:
        return -EINVAL;
    }
    
    return 0;
}

static int lcd_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations lcd_fops = {
    .owner = THIS_MODULE,
    .open = lcd_open,
    .write = lcd_write,
    .unlocked_ioctl = lcd_ctrl,
    .release = lcd_release,
};

// I2C probe function 
static int lcd_probe(struct i2c_client *client)
{
    int ret;
    
    lcd_device = kzalloc(sizeof(*lcd_device), GFP_KERNEL);
    if (!lcd_device)
        return -ENOMEM;
    
    lcd_device->client = client;
    i2c_set_clientdata(client, lcd_device);
    
    // Allocate character device 
    ret = alloc_chrdev_region(&lcd_device->devt, 0, 1, DRIVER_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate chrdev region\n");
        goto err_free;
    }
    
    // Initialize cdev 
    cdev_init(&lcd_device->cdev, &lcd_fops);
    lcd_device->cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&lcd_device->cdev, lcd_device->devt, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto err_chrdev;
    }
    
    // Create device class 
    lcd_device->class = class_create(DRIVER_NAME);
    if (IS_ERR(lcd_device->class)) {
        ret = PTR_ERR(lcd_device->class);
        pr_err("Failed to create class\n");
        goto err_cdev;
    }
    
    // Create device node 
    lcd_device->device = device_create(lcd_device->class, NULL, 
                                       lcd_device->devt, NULL, DRIVER_NAME);
    if (IS_ERR(lcd_device->device)) {
        ret = PTR_ERR(lcd_device->device);
        pr_err("Failed to create device\n");
        goto err_class;
    }
    
    // Initialize LCD 
    ret = lcd_init(lcd_device);
    if (ret < 0) {
        pr_err("LCD initialization failed\n");
        goto err_device;
    }
    
    pr_info("LCD I2C driver loaded successfully\n");
    return 0;

err_device:
    device_destroy(lcd_device->class, lcd_device->devt);
err_class:
    class_destroy(lcd_device->class);
err_cdev:
    cdev_del(&lcd_device->cdev);
err_chrdev:
    unregister_chrdev_region(lcd_device->devt, 1);
err_free:
    kfree(lcd_device);
    return ret;
}

// I2C remove function 
static void lcd_remove(struct i2c_client *client)
{
    struct lcd_dev *lcd = i2c_get_clientdata(client);
    
    // Clear and turn off backlight 
    lcd_clear(lcd);
    lcd->backlight = 0;
    lcd_write_byte(lcd, 0);
    
    device_destroy(lcd->class, lcd->devt);
    class_destroy(lcd->class);
    cdev_del(&lcd->cdev);
    unregister_chrdev_region(lcd->devt, 1);
    kfree(lcd);
    
    pr_info("LCD I2C driver unloaded\n");
}

// I2C device ID table 
static const struct i2c_device_id lcd_id[] = {
    { "lcd_i2c", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, lcd_id);

// Device tree compatible table 
static const struct of_device_id lcd_of_match[] = {
    { .compatible = "lcd,i2c-lcd" },
    { }
};
MODULE_DEVICE_TABLE(of, lcd_of_match);

// I2C driver structure 
static struct i2c_driver lcd_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = lcd_of_match,
    },
    .probe = lcd_probe,
    .remove = lcd_remove,
    .id_table = lcd_id,
};

module_i2c_driver(lcd_driver);

MODULE_AUTHOR("Roy");
MODULE_DESCRIPTION("I2C LCD Driver for Raspberry Pi");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");