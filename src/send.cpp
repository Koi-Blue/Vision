#include "serial.hpp"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

SerialPort::SerialPort(const std::string& port, int baudrate) : fd(-1), portOpen(false) {
    // 打开串口设备
    fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        throw std::runtime_error("Failed to open serial port: " + port);
    }

    // 配置串口参数
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        throw std::runtime_error("Error getting termios attributes");
    }

    // 设置波特率
    speed_t speed;
    switch (baudrate) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 115200: speed = B115200; break;
        default:     speed = B9600;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8数据位
    tty.c_cflag &= ~PARENB;                     // 无校验
    tty.c_cflag &= ~CSTOPB;                     // 1停止位
    tty.c_cflag |= (CLOCAL | CREAD);            // 本地模式

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 原始模式
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);        // 禁用流控
    tty.c_oflag &= ~OPOST;                         // 原始输出

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        throw std::runtime_error("Error setting termios attributes");
    }
    
    portOpen = true;
}

SerialPort::~SerialPort() {
    closePort();
}

bool SerialPort::isOpen() const {
    return portOpen;
}

void SerialPort::sendData(const std::string& data) {
    if (!portOpen) return;
    
    ssize_t written = write(fd, data.c_str(), data.size());
    if (written < 0) {
        throw std::runtime_error("Error writing to serial port");
    }
}

void SerialPort::closePort() {
    if (portOpen) {
        close(fd);
        portOpen = false;
    }
}
