/*********
ESP32-CAM
*********/

// #define __ESP32__

#include "Arduino.h"
#include "FS.h"     // SD Card ESP32
#include "SD_MMC.h" // SD Card ESP32
#include "SPI.h"
#include "driver/rtc_io.h"
#include "esp_camera.h"
#include "espcam.h"
#include "logging.h"
#include "soc/rtc_cntl_reg.h" // Disable brownour problems
#include "soc/soc.h"          // Disable brownour problems
#include <EEPROM.h>           // read and write from flash memory
#include <ESPCamCodes.h>
#include <HardwareSerial.h>
#include <esp_sntp.h>
#include <time.h>

RTC_DATA_ATTR bool firstBoot{true};

// one wire interface to sensor node: not 1-Wire anymore! NOW: serial via GPIO12 (=uartRxPin)
constexpr gpio_num_t uartRxPin{GPIO_NUM_12};

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

RTC_DATA_ATTR bool pictureSuccess;

void takePicture(void)
{
    // Camera configuration
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
    pinMode(4, INPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_dis(GPIO_NUM_4);

    if (psramFound())
    {
        Log::debug("PSRAM found");
        config.frame_size = FRAMESIZE_SXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
        config.jpeg_quality = 10;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    }
    else
    {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 10;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

    delay(1000);

    // Init Camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Log::error("Camera init failed with error ", static_cast<uint32_t>(err));
        pictureSuccess = false;
        return;
    }
    else
    {
        Log::info("Camera init OK.");
        pictureSuccess = true;
    }
    Log::info("Starting SD Card");
    delay(500);

    // delay(1000);
    if (!SD_MMC.begin("/sdcard", true, false))
    { // Using ("/sdcard", true) sets mode1bit to true: sets SD card to '1_wire' mode: only uses GPIO2 to read and write data to SD
        Log::error("SD Card Mount Failed");
        delay(500);
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE)
    {
        Log::error("No SD Card attached");
        pictureSuccess = false;
        return;
    }

    struct tm timeinfo;

    timeval tv;
    gettimeofday(&tv, NULL);
    timeinfo = *gmtime(&tv.tv_sec);
    String datetimeString;
    constexpr size_t datetimeStringLength{sizeof("0000-00-00 00_00_00")};
    datetimeString.reserve(datetimeStringLength);
    strftime(datetimeString.begin(), datetimeStringLength, "%Y-%m-%d %H_%M_%S", &timeinfo);
    Log::debug("Current datetime:", datetimeString.c_str());
    delay(500);

    camera_fb_t* fb = NULL;

    // set standard settings for gain and white balance (should be kept fixed -> cloudy)
    sensor_t* s = esp_camera_sensor_get();

    // Postprocessing
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
    // White balance
    s->set_whitebal(s, 0); // 0 = disable , 1 = enable, switch off automatic white balancing, to force the fixed WB mode below?
    s->set_awb_gain(s, 1); // 0 = disable , 1 = enable
    s->set_wb_mode(s, 2);  // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    // Exposure
    s->set_exposure_ctrl(s, 1); // 0 = disable , 1 = enable, if too dim the exposure will be longer
    s->set_aec2(s, 0);          // 0 = disable , 1 = enable, ????
    s->set_ae_level(s, 0);      // -2 to 2, too lighten/darken picture on top of automatic exposure
    s->set_aec_value(s, 300);   // 0 to 1200, too set exposure yourself, when AEC is disabled
    // ISO
    s->set_gain_ctrl(s, 0); // 0 = disable , 1 = enable #switch of automatic gain control, to ensure that a fixed gain of zero (next row) is applied
    s->set_agc_gain(s, 0);  // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0); // 0 to 6 #only for automatic gain control, defines the max gain
    // picture correction
    s->set_bpc(s, 0);      // 0 = disable , 1 = enable
    s->set_wpc(s, 1);      // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);  // 0 = disable , 1 = enable
    s->set_lenc(s, 1);     // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);  // 0 = disable , 1 = enable
    s->set_vflip(s, 0);    // 0 = disable , 1 = enable
    s->set_dcw(s, 1);      // 0 = disable , 1 = enable
    s->set_colorbar(s, 0); // 0 = disable , 1 = enable

    // set brightness
    for (int b = -2; b <= 2; b = b + 2)
    {
        s->set_brightness(s, b);
        char b_s = String(b).charAt(0);
        if (b == 2)
        {
            b_s = '+';
        }

        // set gamma correction and exposure control
        s->set_raw_gma(s, 1);
        s->set_exposure_ctrl(s, 1);

        // set automatic exposure level
        for (int ae = -2; ae <= 2; ae = ae + 2)
        {
            s->set_ae_level(s, ae);
            char ae_s = String(ae).charAt(0);
            if (ae == 2)
            {
                ae_s = '+';
            }
            // take picture
            fb = esp_camera_fb_get();
            if (!fb)
            {
                Log::error("Camera capture failed");
                pictureSuccess = false;
                return;
            }

            // save picture
            String path2 = "/" + datetimeString + "_" + b_s + "11_" + ae_s + ".jpg";

            fs::FS& fs = SD_MMC;
            Log::debug("Picture file name: ", path2.c_str());

            File file2 = fs.open(path2.c_str(), FILE_WRITE);
            if (!file2)
            {
                Log::error("Failed to open file in writing mode");
                pictureSuccess = false;
                return;
            }
            else
            {
                file2.write(fb->buf, fb->len); // payload (image), payload length
                Log::info("Saved file to path: ", path2.c_str());
            }

            file2.close();
            esp_camera_fb_return(fb);

            delay(100);
        }
    }

    delay(1000);

    // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_en(GPIO_NUM_4);

    pictureSuccess = true;

    return;
}

void setup()
{
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial2.begin(9600, SERIAL_8N1, uartRxPin, -1, true);

    if (!LittleFS.begin())
    {
        Serial.println("Mounting LittleFS failed! Formatting and restarting ...");
        LittleFS.format();
        ESP.restart();
    }
    Log::log.setSerial(&Serial);
    Log::log.setLogLevel(Log::DEBUG);
    Log::log.setLogfile(true);
    if (firstBoot)
    {
        pictureSuccess = true;
        firstBoot = false;
    }
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    Log::debug("Setup done.");

    Serial.flush();
    ESPCam::Commands(nullptr, true).prompt();
}

void loop()
{
    Log::debug("Cycle started");
    int64_t timeout{esp_timer_get_time() + 15 * 1000 * 1000};
    while (!Serial.available() && timeout > esp_timer_get_time())
        ;
    uint8_t cmd{ESPCamCodes::ENABLE_SLEEP};
    if (Serial.available())
    {
        cmd = Serial.read();
        Serial.print("Received command ");
    }
    time_t owi_time{0};
    timeval owi_time_value{0};
    switch (cmd)
    {
    case ESPCamCodes::SET_TIME:
        Log::info("ESPCam::SET_TIME");
        Serial.readBytes((uint8_t*)&owi_time, sizeof(owi_time));
        Log::info(owi_time);
        owi_time_value = {owi_time, 0};
        sntp_sync_time(&owi_time_value);
        delay(100);
        sntp_sync_time(&owi_time_value);
        break;
    case ESPCamCodes::GET_TIME:
        Log::info("ESPCam::GET_TIME");
        break;
    case ESPCamCodes::GET_STATUS:
        Log::info("ESPCam::GET_STATUS");
        if (pictureSuccess)
        {
            Log::info("SUCCESS PRINT");
            // digitalWrite(stat_pin, HIGH);
        }
        else
        {
            Log::info("FAIL PRINT");
            // digitalWrite(stat_pin, LOW);
        }
        break;
    case ESPCamCodes::TAKE_PICTURE:
        Log::info("ESPCam::TAKE_PICTURE");
        takePicture();
    case ESPCamCodes::ENABLE_SLEEP:
        // switch case fallthrough intended!
        Log::info("ESPCam::ENABLE_SLEEP");
        Serial.flush();
        Serial.end();
        Log::log.close();
        LittleFS.end();
        esp_sleep_enable_ext0_wakeup(uartRxPin, HIGH);
        esp_deep_sleep_start();
        break;
    default:
        Log::info("ESPCam::INVALID");
        break;
    }
}