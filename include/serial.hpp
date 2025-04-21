#ifndef SERIAL_HPP
#define SERIAL_HPP

#include <string>

class SerialPort {
public:
    SerialPort(const std::string& port = "/dev/ttyUSB0", int baudrate = 115200);
    ~SerialPort();
    
    bool isOpen() const;
    void sendData(const std::string& data);
    void closePort();

private:
    int fd; // 文件描述符
    bool portOpen;
};

#endif // SERIAL_HPP
