#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <regex>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "serial.hpp"

// 圆环检测全局变量
int min_radius = 10;
int max_radius = 100;

int hue_low = 35;
int hue_high = 85;
int saturation_low = 50;
int saturation_high = 255;
int value_low = 50;
int value_high = 255;

// 共享参数
int minRadius = 10, maxRadius = 195;
int centerRegionSize = 50;
int tolerance = 5;

// 颜色检测参数
int lowRed = 170, highRed = 180;
int lowGreen = 65, highGreen = 80;
int lowBlue = 90, highBlue = 120;

void onTrackbar(int, void*) {}

// Gamma 矫正，对圆环
void adjustGamma(cv::Mat& src, cv::Mat& dst, double gamma) {
    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i)
        p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, gamma) * 255.0);

    cv::LUT(src, lookUpTable, dst);
}

// 颜色增强函数
void enhanceColor(cv::Mat& src, cv::Mat& dst) {
    cv::Mat hsv;
    cv::cvtColor(src, hsv, cv::COLOR_BGR2HSV);

    std::vector<cv::Mat> hsv_channels;
    cv::split(hsv, hsv_channels);

    // 提升饱和度（限制上限防止溢出）55), 2);
    hsv_channels[1] = hsv_channels[1] * 1.5; // 饱和度增强

    // 合并回 HSV 并转换为 BGR
    cv::merge(hsv_channels, hsv);
    cv::cvtColor(hsv, dst, cv::COLOR_HSV2BGR);
}

// 解析二维码数据
std::string parseQRCode(const std::string& data) {
    std::regex qrPattern(R"((\d)(\d)(\d)\+(\d)(\d)(\d))");
    std::smatch match;
    if (std::regex_match(data, match, qrPattern)) {
        return "AA" + match[1].str() + match[2].str() + match[3].str() +
               match[4].str() + match[5].str() + match[6].str() + "E";
    }
    return "";
}

std::string font_text(const std::string& data)
{
    std::regex qrPattern(R"((\d)(\d)(\d)\+(\d)(\d)(\d))");
    std::smatch match;
    if (std::regex_match(data, match, qrPattern)) {
        return match[1].str() + match[2].str() + match[3].str();
    }
    return "";
}

std::string back_text(const std::string& data)
{
    std::regex qrPattern(R"((\d)(\d)(\d)\+(\d)(\d)(\d))");
    std::smatch match;
    if (std::regex_match(data, match, qrPattern)) {
        return match[4].str() + match[5].str() + match[6].str();
    }
    return "";
}



// 物料识别
std::string detectGoods(cv::Mat& frame) {
    cv::Mat hsvImage;
    cv::cvtColor(frame, hsvImage, cv::COLOR_BGR2HSV);
    cv::Rect centerROI(frame.cols / 2 - centerRegionSize / 2, 
                      frame.rows / 2 - centerRegionSize / 2, 
                      centerRegionSize, 
                      centerRegionSize + 25);
					  
	cv::rectangle(
		frame,
		centerROI,
		cv::Scalar(0,0,255),
		2
	);
	
    cv::Mat roi = hsvImage(centerROI);
	
	

    cv::Mat maskRed, maskGreen, maskBlue;
    cv::inRange(roi, cv::Scalar(lowRed, 100, 100), cv::Scalar(highRed, 255, 255), maskRed);
    cv::inRange(roi, cv::Scalar(lowGreen, 100, 100), cv::Scalar(highGreen, 255, 255), maskGreen);
    cv::inRange(roi, cv::Scalar(lowBlue, 100, 100), cv::Scalar(highBlue, 255, 255), maskBlue);
    
    if (cv::countNonZero(maskRed) > 0) return "AB1E";
    if (cv::countNonZero(maskGreen) > 0) return "AB2E";
    if (cv::countNonZero(maskBlue) > 0) return "AB3E";
    return "";
}

// 在全局添加格式化函数
std::string formatOffset(int offset) {
    std::stringstream ss;
    if (offset >= 0) {
        ss << "0" << std::setw(3) << std::setfill('0') << offset; // 正数以0开头
    } else {
        ss << "1" << std::setw(3) << std::setfill('0') << abs(offset); // 负数以1开头
    }
    return ss.str();
}


// 修改后的圆环检测函数
std::string detectAndMarkCircles(const cv::Mat& src, cv::Mat& output) {
    cv::Mat gray, blurred, edges, hsv, mask;
    std::vector<std::vector<cv::Point>> contours;

    // 图像预处理
    cv::cvtColor(src, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv,
                cv::Scalar(hue_low, saturation_low, value_low),
                cv::Scalar(hue_high, saturation_high, value_high),
                mask);
    cv::GaussianBlur(mask, blurred, cv::Size(5, 5), 2);
    cv::Canny(blurred, edges, 50, 150);
    cv::findContours(edges, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    cv::Point imgCenter(src.cols/2, src.rows/2);
    bool foundCenter = false;
    double minDistance = std::numeric_limits<double>::max();
    cv::Point bestCircle(-1, -1);

    // 遍历所有轮廓
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        double perimeter = cv::arcLength(contours[i], true);
        if (perimeter == 0) continue;

        double circularity = 4 * CV_PI * area / (perimeter * perimeter);
        if (circularity > 0.7 && circularity < 1.2) {
            cv::Moments m = cv::moments(contours[i]);
            int cx = static_cast<int>(m.m10/m.m00);
            int cy = static_cast<int>(m.m01/m.m00);

            cv::Point2f center;
            float radius;
            cv::minEnclosingCircle(contours[i], center, radius);

            if (radius >= min_radius && radius <= max_radius) {
                // 计算偏移量
                int dx = cx - imgCenter.x;
                int dy = imgCenter.y - cy;

                // 位置判断逻辑
                if (abs(dx) <= tolerance && abs(dy) <= tolerance) {
                    foundCenter = true;
                    bestCircle = cv::Point(cx, cy);
                    break;
                } else {
                    double distance = sqrt(dx*dx + dy*dy);
                    if (distance < minDistance) {
                        minDistance = distance;
                        bestCircle = cv::Point(cx, cy);
                    }
                }

                // 绘制图形（保留原可视化功能）
                cv::circle(output, cv::Point(cx, cy), 5, cv::Scalar(0, 255, 0), -1);
                cv::drawContours(output, contours, static_cast<int>(i), cv::Scalar(255, 0, 0), 2);
            }
        }
    }

    // 生成结果字符串
    std::stringstream result;
    if (foundCenter) {
        return "AC00000000E"; // 中心位置特殊编码
    } else if (bestCircle.x != -1) {
        int dx = bestCircle.x - imgCenter.x;
        int dy = imgCenter.y - bestCircle.y;
        
        return "AC" + formatOffset(dx) + formatOffset(dy) + "E";
    }
    return "";
}


int main() {
    std::string font_Text;
    std::string back_Text;
    std::string add = "+";
    bool isSet = false;
	SerialPort serial("/dev/stm32", 115200);
    cv::VideoCapture cap1("/dev/JR_1080p_video", cv::CAP_V4L2);
    cv::VideoCapture cap2("/dev/mv_longger", cv::CAP_V4L2);
    // exchange
    // cv::VideoCapture cap1("/dev/mv_longger", cv::CAP_V4L2);
    // cv::VideoCapture cap2("/dev/JR_1080p_video", cv::CAP_V4L2);   
			// cv::putText(frame2, "QR: " + qrResult, cv::Point(10, 30),
			// 			cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
    
    if (!cap1.isOpened() || !cap2.isOpened()) {
        std::cerr << "Error: Could not open cameras." << std::endl;
        return -1;
    }
    cap1.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    cap1.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
    cap2.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    cap2.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
    
    cv::QRCodeDetector qrDetector;
    cv::Mat frame1, frame2, enhanced_frame, output;
    cv::Size size_f(520, 700);
    cv::Mat white_frame = cv::Mat::ones(size_f, CV_8UC3);
    // white_frame *= 255;
    
    
	const int LEFT_WIDTH = 500; //左宽
	const int RIGHT_WIDTH = 620; //右宽
	const int PANEL_HEIGHT = 700; //总高
	
    while (true) {
        cap1 >> frame1;
        cap2 >> frame2;
        if (frame1.empty() || frame2.empty()) break;

        // 图像增强处理
        enhanceColor(frame1, enhanced_frame);
        enhanced_frame.copyTo(output);
        // 检测并获取结果
        std::string qrResult = parseQRCode(qrDetector.detectAndDecode(frame2));
        std::string goodsResult = detectGoods(frame1);
        std::string circleResult = detectAndMarkCircles(enhanced_frame, output);
    
        if (!isSet && !qrResult.empty())
        {
            font_Text = font_text(qrDetector.detectAndDecode(frame2));
            back_Text = back_text(qrDetector.detectAndDecode(frame2));
            
            isSet = true;
	    // cv::putText(white_frame, font_Text, cv::Point(80, 200), cv::FONT_HERSHEY_SIMPLEX, 6.0, cv::Scalar(255, 0, 255), 10);
	    // cv::putText(white_frame, add, cv::Point(180, 350), cv::FONT_HERSHEY_SIMPLEX, 6.0, cv::Scalar(100, 0, 100), 6);
	    // cv::putText(white_frame, back_Text, cv::Point(80, 520), cv::FONT_HERSHEY_SIMPLEX, 6.0, cv::Scalar(255, 0, 255), 10);
	    cv::putText(white_frame, font_Text, cv::Point(80, 200), cv::FONT_HERSHEY_SCRIPT_COMPLEX, 6.0, cv::Scalar(255, 0, 255), 10);
	    cv::putText(white_frame, add, cv::Point(180, 350), cv::FONT_HERSHEY_SCRIPT_COMPLEX, 6.0, cv::Scalar(100, 0, 100), 8);
	    cv::putText(white_frame, back_Text, cv::Point(80, 520), cv::FONT_HERSHEY_SCRIPT_COMPLEX, 6.0, cv::Scalar(255, 0, 255), 10);
        }

        // 输出结果
		if (!qrResult.empty()) {
			serial.sendData(qrResult + "\n");
			std::cout << qrResult << std::endl; // 保留控制台输出
		
		}
		if (!goodsResult.empty()) {
			serial.sendData(goodsResult + "\n");
			std::cout << goodsResult << std::endl;
		}
        
		if (!circleResult.empty()) {
			serial.sendData(circleResult + "\n");
			std::cout << circleResult << std::endl;
		}
        

        // 获取物料检测ROI区域
        cv::Rect centerROI(frame1.cols / 2 - centerRegionSize/2, 
                          frame1.rows / 2 - centerRegionSize/2, 
                          centerRegionSize, 
                          centerRegionSize + 25);
        cv::Mat goodsROI = frame1(centerROI);

        // 调整各个显示区域尺寸
        cv::Mat part1, part2, part3;
        
        // 左侧上半部分：圆环检测（保持宽高比）
        cv::resize(output, part1, cv::Size(LEFT_WIDTH, PANEL_HEIGHT/2), 0, 0, cv::INTER_LINEAR);
        
        // 左侧下半部分：物料ROI显示
        cv::Mat roiDisplay = cv::Mat::zeros(PANEL_HEIGHT/2, LEFT_WIDTH, CV_8UC3);
        cv::Mat resizedROI;
        cv::resize(goodsROI, resizedROI, cv::Size(120, 120)); // 放大ROI
        resizedROI.copyTo(roiDisplay(cv::Rect(100, 30, 120, 120)));
        cv::rectangle(roiDisplay, cv::Rect(100, 30, 120, 120), 
                     cv::Scalar(0, 0, 255), 2);
        cv::putText(roiDisplay, "Material ROI", cv::Point(10, 20), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        roiDisplay.copyTo(part3);

        // 右侧部分：二维码画面（保持宽高比）
        cv::resize(white_frame, part2, cv::Size(RIGHT_WIDTH, PANEL_HEIGHT), 0, 0, cv::INTER_LINEAR);

        // 组合画面
        cv::Mat leftPanel;
        cv::vconcat(part1, part3, leftPanel);   // 垂直拼接左侧面板
        cv::Mat combined;
        cv::hconcat(leftPanel, part2, combined); // 水平拼接左右面板

        // 添加分割线
        cv::line(combined, cv::Point(LEFT_WIDTH, 0), cv::Point(LEFT_WIDTH, PANEL_HEIGHT), 
                cv::Scalar(255, 255, 255), 2);
        cv::line(combined, cv::Point(LEFT_WIDTH, PANEL_HEIGHT/2), 
                cv::Point(0, PANEL_HEIGHT/2), cv::Scalar(255, 255, 255), 1);

        // 显示组合画面
        cv::imshow("Combined View", combined);
        
        if (cv::waitKey(30) == 'q') break;
    }
    cap1.release();
    cap2.release();
    cv::destroyAllWindows();
    return 0;
}
