#include "display.h"
#include "bitmap.h"

void display_lines(String line1, String line2)
{
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void display_clear()
{
  lcd.clear();
  lcd.noBacklight();
}

void display_place_card()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, place_card, 128, 64, WHITE);
  display.display();
}

void display_processing()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, processing, 128, 64, WHITE);
  display.display();
}

void display_fail()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, failure, 128, 64, WHITE);
  display.display();
}

void display_success()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, succsess, 128, 64, WHITE);
  display.display();
}

void display_register_mode()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, register_mode, 128, 64, WHITE);
  display.display();
}

void display_authenticate_mode()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, authenticate, 128, 64, WHITE);
  display.display();
}

void display_mode_standby()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, standby, 128, 64, WHITE);
  display.display();
}

void display_connectionloss()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, conection_loss, 128, 64, WHITE);
  display.display();
}

void display_settings_mode()
{
  display.clearDisplay();
  display.drawBitmap(0, 0, settings, 128, 64, WHITE);
  display.display();
}