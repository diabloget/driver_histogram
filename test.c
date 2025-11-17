#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define LCD_CLEAR 0

int main() {
    int fd = open("/dev/lcd_i2c", O_WRONLY);
    if (fd < 0) {
        perror("Error abriendo LCD");
        return 1;
    }

    
    write(fd, "Texto inicial", 13);
    printf("PRESS ENTER\n");
    getchar();

    ioctl(fd, LCD_CLEAR, 0);
    usleep(100000);
    
    printf("PRESS ENTER\n");
    getchar();

    write(fd, "Linea 1\nLinea 2", 19);
    
    printf("PRESS ENTER\n");
    getchar();
    
    ioctl(fd, LCD_CLEAR, 0);
    
    close(fd);
    return 0;
}