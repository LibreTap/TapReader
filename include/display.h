#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_SSD1306.h>

// Extern declarations for shared display objects (if defined in main.ino)
extern LiquidCrystal_I2C lcd;
extern Adafruit_SSD1306 display;

// Display function declarations
void display_lines(String line1, String line2);
void display_clear();
void display_place_card();
void display_processing();
void display_fail();
void display_success();
void display_register_mode();
void display_authenticate_mode();
void display_mode_standby();
void display_connectionloss();
void display_settings_mode();
