#include <Arduino.h>
#include "Vector/Vector.h"
#include "EEPROM.h"

#define DEFAULT_START_RADIUS 10
#define DEFAULT_STOP_RADIUS 160
#define DEFAULT_STEP 10

#define DEFAULT_FEEDRATE 100
#define DEFAULT_ACCEL 1800
#define DEFAULT_BEGIN_VELOCITY 100

#define DEFAULT_Z_SAFE -250
#define DEFAULT_Z_EXE -320

#define START_RADIUS_ADDRESS 0
#define STOP_RADIUS_ADDRESS 4
#define STEP_ADDRESS 8
#define FEEDRATE_ADDRESS 12
#define ACCEL_ADDRESS 16
#define BEGIN_VEL_ADDRESS 20
#define Z_SAFE_ADDRESS 24
#define Z_EXE_ADDRESS 28

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
int blink_period = 1000;
bool led = false;

float feedrate = DEFAULT_FEEDRATE;
float accel = DEFAULT_ACCEL;
float begin_vel = DEFAULT_BEGIN_VELOCITY;

float z_safe = DEFAULT_Z_SAFE;
float z_exe = DEFAULT_Z_EXE;

float start_radius = DEFAULT_START_RADIUS;
float stop_radius = DEFAULT_STOP_RADIUS;
float step = DEFAULT_STEP;

float M, A,B,C;

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
        create_gcode_queue(start_radius, stop_radius, step);
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
  // saveData();
  // loadData();

  lastMillis = millis();
  Serial.begin(115200);
  Serial.println("IsDelta");
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
  } else {
    receivedString += " ";
    while (receivedString.indexOf(' ') != receivedString.lastIndexOf(' ')) {
      String keyval = receivedString.substring(0, receivedString.indexOf(' '));
    }
  }

  receivedString = "";
  isStringCompleted = false;
}

void create_init_gcode_queue()
{
  InitGcodeQueue.push_back("G28");
  InitGcodeQueue.push_back("M203 S" + String((int)feedrate));
  InitGcodeQueue.push_back("M204 A" + String((int)accel));
  InitGcodeQueue.push_back("M205 S" + String((int)begin_vel));
  InitGcodeQueue.push_back("M360 E0");
  InitGcodeQueue.push_back("G01 X0 Y0 Z" + String(z_safe));
}

void create_gcode_queue(int start_radius, int stop_radius, int step)
{
  GcodeQueue.push_back("M03");
  GcodeQueue.push_back("G01 X" + String(start_radius) + " Y0 Z" + String(z_exe));
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
  GcodeQueue.push_back("G01 X0 Y0 Z" + String(z_safe));
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

void saveData() {
  put(START_RADIUS_ADDRESS, start_radius);
  put(STOP_RADIUS_ADDRESS, stop_radius);
  put(STEP_ADDRESS, step);
  put(FEEDRATE_ADDRESS, feedrate);
  put(ACCEL_ADDRESS, accel);
  put(BEGIN_VEL_ADDRESS, begin_vel);
  put(Z_EXE_ADDRESS, z_exe);
  put(Z_SAFE_ADDRESS, z_safe);
}

void loadData() {
  get(START_RADIUS_ADDRESS, start_radius);
  get(STOP_RADIUS_ADDRESS, stop_radius);
  get(STEP_ADDRESS, step);
  get(FEEDRATE_ADDRESS, feedrate);
  get(ACCEL_ADDRESS, accel);
  get(BEGIN_VEL_ADDRESS, begin_vel);
  get(Z_EXE_ADDRESS, z_exe);
  get(Z_SAFE_ADDRESS, z_safe);
}

void put(int address, float floatout)
{
	unsigned int floatH;
	unsigned int floatL;

	floatH = floatout/1;
	floatL = (floatout - (float)floatH) * 1000;
	
	EEPROM.update(address, floatH);
	EEPROM.update(address + 1, floatL);
}

void get(int address, float &floatin)
{
	unsigned int floatH = EEPROM.read(address);
	unsigned int floatL = EEPROM.read(address + 1);

	floatin = (float)floatH + (float)floatL/1000;
}