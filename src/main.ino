#include <Arduino.h>
#include "Vector/Vector.h"

#define START_RADIUS 10
#define STOP_RADIUS 160
#define STEP 10

#define Z_SAFE -250
#define Z_EXE -320

typedef enum
{
  NOT_CONNECTED,
  INITIALIZING,
  FREE,
  EXECUTING
} STATUS;

#define COMMAND_PORT Serial
String receivedString = "";
bool isStringCompleted = false;

String GcodeArray[20];
String InitGcodeArray[20];

Vector<String> InitGcodeQueue;
Vector<String> GcodeQueue;
STATUS stt = NOT_CONNECTED;

unsigned long lastMillis = 0;
unsigned long ledMillis = 0;
int blink_period = 2000;
bool led = false;

void setup()
{
  initData();
}

void loop()
{
  if (millis() - lastMillis >= 200)
  {
    lastMillis = millis();

    if (!digitalRead(2))
    {
      if (stt == FREE)
      {
        stt = EXECUTING;
        create_gcode_queue(START_RADIUS, STOP_RADIUS, STEP);
        send_exe_gcodes();
      }
    }
  }

  if (millis() - ledMillis > blink_period) {
    ledMillis = millis();
    digitalWrite(LED_BUILTIN, led);
    led = !led;
    switch (stt)
    {
    case INITIALIZING:
      blink_period = 1000;
      break;
    case FREE:
      blink_period = 100;
      break;
    case EXECUTING:
      blink_period = 500;
      break;
    }
  }

  serial_execute();
}

void initData()
{
  Serial.end();
  pinMode(2, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  InitGcodeQueue.setStorage(InitGcodeArray);
  GcodeQueue.setStorage(GcodeArray);

  create_init_gcode_queue();

  Serial.begin(115200);
  Serial.println("IsDelta");

  lastMillis = millis();
}

void serial_execute()
{
  while (COMMAND_PORT.available())
  {
    char inChar = (char)COMMAND_PORT.read();

    if (inChar == '\n')
    {
      isStringCompleted = true;
      break;
    }
    else if (inChar != '\r')
      receivedString += inChar;
  }

  if (!isStringCompleted)
    return;

  if (receivedString == "Ok")
  {
    if (stt == INITIALIZING)
    {
      if (InitGcodeQueue.size() == 0)
        stt = FREE;
      else
      {
        COMMAND_PORT.println(InitGcodeQueue.operator[](0));
        InitGcodeQueue.remove(0);
      }
    }
    else if (stt == EXECUTING)
    {
      if (GcodeQueue.size() == 0)
        stt = FREE;
      else
      {
        COMMAND_PORT.println(GcodeQueue.operator[](0));
        GcodeQueue.remove(0);
      }
    }
  }
  else if (receivedString == "YesDelta")
  {
    stt = INITIALIZING;
    send_init_gcodes();
  }

  receivedString = "";
  isStringCompleted = false;
}

void create_init_gcode_queue()
{
  InitGcodeQueue.push_back("G28");
  InitGcodeQueue.push_back("M203 S300");
  InitGcodeQueue.push_back("M204 A1800");
  InitGcodeQueue.push_back("M205 S100");
  InitGcodeQueue.push_back("M360 E0");
  InitGcodeQueue.push_back("G01 X0 Y0 Z" + String(Z_SAFE));
}

void create_gcode_queue(int start_radius, int stop_radius, int step)
{
  GcodeQueue.push_back("M03");
  GcodeQueue.push_back("G01 X" + String(start_radius) + " Y0 Z" + String(Z_EXE));
  GcodeQueue.push_back("G01 X" + String(start_radius) + " Y0 J0 I" + String(start_radius));

  int current_x = start_radius;
  int current_i = start_radius;
  String gc = "";

  while (abs(current_x) <= stop_radius - step)
  {
    if (current_x > 0)
    {
      current_i = -(abs(current_x) + abs(-current_x - step)) / 2;
      current_x = -current_x - step;
    }
    else
    {
      current_i = (abs(current_x) + abs(-current_x + step)) / 2;
      current_x = -current_x + step;
    }

    gc = "G02 X" + String(current_x) + " Y0 I" + String(current_i) + " J0";
    GcodeQueue.push_back(gc);
  }

  GcodeQueue.push_back("M05");
  GcodeQueue.push_back("G01 X0 Y0 Z" + String(Z_SAFE));
}

void send_init_gcodes()
{
  if (InitGcodeQueue.size() == 0)
    stt = FREE;
  else
  {
    COMMAND_PORT.println(InitGcodeQueue.operator[](0));
    InitGcodeQueue.remove(0);
  }
}

void send_exe_gcodes()
{
  if (GcodeQueue.size() == 0)
    stt = FREE;
  else
  {
    COMMAND_PORT.println(GcodeQueue.operator[](0));
    GcodeQueue.remove(0);
  }
}
