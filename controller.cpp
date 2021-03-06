#include "mbed.h"
#include "physcom.h"
#include "constants.hpp"

physcom::M3pi robot;

void toRange(float &var) {
  if (var > MOTOR_MAX)
    var = MOTOR_MAX;
  else if (var < MOTOR_MIN)
    var = MOTOR_MIN;
}

float getLinePosition(const int *sensorData, const int *sensorDataOld,
                      const float &linePositionOld) {
  bool lineDetected = false;
  uint32_t average = 0;
  uint16_t sum = 0;
  uint8_t relIndex = 0;

  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    relIndex = i + SENSOR_FIRST_INDEX;
    // Ignore values below noise threshold
    if (sensorData[relIndex] > NOISE_THRESHOLD) {
      // Was line detected at all?
      if (sensorData[relIndex] > LINE_DETECTION_THRESHOLD)
        lineDetected = true;
      average += sensorData[relIndex] * 1000 * i;
      sum += sensorData[relIndex];
    }
  }

  // If no line is detected we set an extremum line position based on old data
  if (!lineDetected) {
    if (linePositionOld < (RANGE_CENTER / (RANGE_MIN + RANGE_MAX)))
      return -1;
    else
      return 1;
  }
  // Normalize (-1 to 1) and return
  return ((((average / static_cast<float>(sum)) / RANGE_MAX) * 2) - 1);
}

bool detectTJunction(const int *optoData) {
  bool detected = false;
  uint32_t sum = 0;
  for (size_t i = 0; i < 5; i++) {
    sum += optoData[i];
  }
  detected = sum > (5 * ABOVE_LINE_THRESHOLD);
  return detected;
}

int main() {
  wait_ms(1000);
  robot.sensor_auto_calibrate();

  float motorR;
  float motorL;

  float linePosition = 0.0;
  float linePositionOld = 0.0;
  float controlVariable;

  int sensorData[5];
  int sensorDataOld[5] = {0, 0, 0, 0, 0};

  float derivative, proportional;
  // float integral = 0;
  wait_ms(100);

  while (true) {
    robot.calibrated_sensors(sensorData);

    // Right turn bias if extremas are detected
    if (sensorData[4] > sensorData[0]) sensorData[0] = 0;

    // Stop at T junction
    if (detectTJunction(sensorData)) {
      robot.activate_motor(0, 0);
      robot.activate_motor(1, 0);
      return 0;
    }

    // Get the position of the line.
    linePosition = getLinePosition(sensorData, sensorDataOld, linePositionOld);

    // PD computation
    // -------------------------------------------------------------------------
    proportional = linePosition;
    // integral += proportional;
    derivative = linePosition - linePositionOld;

    controlVariable = proportional * KP;
    // controlVariable += integral * KI;
    controlVariable += derivative * KD;
    // -------------------------------------------------------------------------

    // Remember the last position.
    linePositionOld = linePosition;

    // Compute new speeds
    motorR = MOTOR_MAX - controlVariable;
    motorL = MOTOR_MAX + controlVariable;
    toRange(motorR);
    toRange(motorL);

    // Control motors
    robot.activate_motor(0, motorL);
    robot.activate_motor(1, motorR);
  }
}
