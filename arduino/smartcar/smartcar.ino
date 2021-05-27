#include <vector>

#include <MQTT.h>
#include <WiFi.h>
#ifdef __SMCE__
#include <OV767X.h>
#endif

#include <Smartcar.h>

#ifndef __SMCE__
WiFiClient net;
#endif
MQTTClient mqtt(256);

const int SIDE_FRONT_PIN = 0;
ArduinoRuntime arduinoRuntime;
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 5000; //5 seconds
const auto oneSecond = 1000UL;
BrushedMotor leftMotor(arduinoRuntime, smartcarlib::pins::v2::leftMotorPins);
BrushedMotor rightMotor(arduinoRuntime, smartcarlib::pins::v2::rightMotorPins);
DifferentialControl control(leftMotor, rightMotor);
const int TRIGGER_PIN = 6; // D6
const int ECHO_PIN = 7;    // D7
const unsigned int MAX_DISTANCE = 300;
SR04 front(arduinoRuntime, TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
GP2D120 sideFrontIR(arduinoRuntime,
                    SIDE_FRONT_PIN); // measure distances between 5 and 25 centimeters
SimpleCar car(control);
int latestSpeed = 0;
int latestAngle = 0;
int magnitude = 0;
int score = 0;

char hostname[50];
char portNumber[50];

boolean stopping = false;

std::vector<char> frameBuffer;

void setup()
{

  Serial.begin(9600);
  Serial.setTimeout(200);
#ifdef __SMCE__
  Camera.begin(QVGA, RGB888, 15);
  frameBuffer.resize(Camera.width() * Camera.height() * Camera.bytesPerPixel());

  Serial.println("Localhost Initialized"); // Debugging
  mqtt.begin(WiFi);

#else
  mqtt.begin(net);
#endif
  mqttHandler();
  startMillis = millis();
}

void loop()
{
  if (mqtt.connected())
  {
    mqtt.loop();
    const auto currentTime = millis();

#ifdef __SMCE__
    static auto previousFrame = 0UL;
    if (currentTime - previousFrame >= 65)
    {
      previousFrame = currentTime;
      Camera.readFrame(frameBuffer.data());
      mqtt.publish("/Group10/camera", frameBuffer.data(), frameBuffer.size(),
                   false, 0);
    }
#endif
    static auto previousTransmission = 0UL;
    if (currentTime - previousTransmission >= oneSecond)
    {
      previousTransmission = currentTime;

      const auto distance = String(front.getDistance());
      mqtt.publish("/Group10/sensor/ultrasound/front", distance);
    }
  }

  currentMillis = millis(); //get the current "time" (actually the number of milliseconds since the program started)
  updateScore();
  handleInput();
  delay(35);
}

void handleInput()
{
  float distance = front.getDistance();
  // serialMsg(distance);
  // distanceHandler(0, 200, distance);
  if (stopping == true)
  {
    stopTank();
  }
}

void distanceHandler(float lowerBound, float upperBound, float distance)
{
  if (distance > lowerBound && distance < upperBound)
  {
    handleObstacle();
  }
}

void handleObstacle()
{
  magnitude = latestSpeed /* * 0.4*/;
  car.setSpeed(magnitude);
}

void stopTank()
{
  if (latestSpeed == 0)
  {
    stopping = false;
  }

  latestSpeed = latestSpeed * 0.75;
  latestAngle = 0;
  car.setSpeed(latestSpeed);
  car.setAngle(latestAngle);
}

// Connect to a new broker. Works when switching to a custom broker but not when switching back to localhost
void connect(char host[], char port[])
{
#ifdef __SMCE__
  Camera.begin(QVGA, RGB888, 15);
  frameBuffer.resize(Camera.width() * Camera.height() * Camera.bytesPerPixel());

  String hostTemp = String(host);
  String portTemp = String(port);

  if (host[0] == 0 || port[0] == 0)          // default, connect to localhost
  {                                          // Todo Make this work when switching back from custom broker
    Serial.println("Localhost Initialized"); // Debugging
    mqtt.disconnect();                       // disconnect from previous broker
    mqtt.begin(WiFi);
    Serial.print(mqtt.connected()); // Check new connection. returns 1 if true
  }
  else if (hostTemp.equals("10.0.2.2"))  // if user input port is "10.0.2.2", connect to localhost
  {                                      // Todo Make this work when switching back from custom broker
    Serial.println("Localhost enabled"); // Debugging
    mqtt.disconnect();                   // disconnect from previous broker
    mqtt.begin(WiFi);
    Serial.print(mqtt.connected()); // Check new connection. returns 1 if true
  }
  else // else, connect to user input.
  {
    //Serial.println(hostTemp);
    //Serial.println(portTemp);
    Serial.println("Custom host enabled"); // Debugging
    String stringTemp = String(portNumber);
    int intTemp = stringTemp.toInt();

    mqtt.disconnect(); // disconnect from previous broker
    mqtt.begin(hostname, intTemp, WiFi);
    Serial.print(mqtt.connected()); // Check new connection. returns 1 if true
  }
#else
  Serial.println("net"); // Debugging
  mqtt.begin(net);
#endif
  Serial.println("mqttHandler"); // Debugging
  mqttHandler();
  startMillis = millis();
}

void mqttHandler()
{
  if (mqtt.connect("arduino", "public", "public"))
  {
    mqtt.subscribe("/Group10/manual/#", 1);
    mqtt.onMessage([](String topic, String message) {
      if (topic == "/Group10/manual/forward")
      {
        Serial.println(message);
        latestSpeed = message.toInt();
        car.setAngle(latestAngle);
        car.setSpeed(latestSpeed);
        stopping = false;
      }
      else if (topic == "/Group10/manual/backward")
      {
        latestSpeed = (-1) * message.toInt();
        car.setAngle(latestAngle);
        car.setSpeed(latestSpeed);
        stopping = false;
      }
      else if (topic == "/Group10/manual/turnleft")
      {
        latestAngle = (-1) * message.toInt();
        car.setAngle(latestAngle);
        stopping = false;
      }
      else if (topic == "/Group10/manual/turnright")
      {
        latestAngle = message.toInt();
        car.setAngle(latestAngle);
        stopping = false;
      }
      else if (topic == "/Group10/manual/break")
      {
        latestSpeed = 0;
        car.setSpeed(latestSpeed);
        stopping = false;
      }
      else if (topic == "/Group10/manual/stopping" || topic == "/Group10/manual/nocontrol")
      {
        stopping = true;
      }
      else if (topic == "/Group10/manual/server/ip")
      {
        memset(hostname, '\0', sizeof(hostname));
        message.toCharArray(hostname, 50);
      }
      else if (topic == "/Group10/manual/server/p")
      {
        memset(portNumber, '\0', sizeof(portNumber));
        message.toCharArray(portNumber, 50);

        connect(hostname, portNumber); // TODO; Make this work 100% of times. It is not at the moment.
      }
    });
  }
}

void testtest()
{
  Serial.print("wtf");
}

void serialMsg(float distance)
{
  if (distance > 0 && (currentMillis - startMillis) >= period)
  { //The user is updated on the distance to an obstacle every 7 seconds
    String msg1 = "There is an obstacle in ";
    String msg2 = " cm.";
    Serial.print(msg1);
    Serial.print(distance);
    Serial.println(msg2);
    startMillis = currentMillis;
  }
  else if ((currentMillis - startMillis) >= period)
  {
    String msg = "No obstacle detected.";
    Serial.println(msg);
    startMillis = currentMillis;
  }
  Serial.print("Current Speed: ");
  Serial.println(latestSpeed);
  Serial.print("Current Angle: ");
  Serial.println(latestAngle);
}
void updateScore()
{

  float distanceToScore = sideFrontIR.getDistance();
  if (distanceToScore > 0 && distanceToScore < 15 && (currentMillis - startMillis) >= period)
  {
    score += 1;
    Serial.println(score);
    startMillis = currentMillis;
    mqtt.publish("/Group10/manual/score", String(score));
  }
}
//void angleMsg()
//{
//
//if(latestAngle > 0){
//        Serial.print("Turning ");
//        Serial.print(latestAngle);
//        Serial.println(" degrees right.");
//    }
//    else if(latestAngle == 0){
//        Serial.println("Going straight ahead.");
//    }
//    else{
//        Serial.print("Turning ");
//        Serial.print(latestAngle);
//        Serial.println(" degrees left.");
//    }
//}