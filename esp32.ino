#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3AYhEctVb"
#define BLYNK_TEMPLATE_NAME "DC2"
#define BLYNK_AUTH_TOKEN "Iidf5sCRManbvztaKOGmQHxQhJaZcEvB"
#include <BlynkSimpleEsp32.h>

// WiFi credentials
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "1";  // WiFi name
char pass[] = "55555555";   // WiFi password

// First L298N (Steering motors)
const uint8_t M1A_EN = 22;  // First L298N Enable A (Left motor PWM)
const uint8_t M1B_EN = 19;  // First L298N Enable B (Right motor PWM)
const uint8_t M1_IN1 = 2;   // Left motor forward
const uint8_t M1_IN2 = 4;   // Left motor reverse
const uint8_t M1_IN3 = 12;  // Right motor forward
const uint8_t M1_IN4 = 13;  // Right motor reverse

// Second L298N (Additional motors)
const uint8_t M2A_EN = 25;  // Second L298N Enable A
const uint8_t M2B_EN = 26;  // Second L298N Enable B
const uint8_t M2_IN1 = 27;  // Third motor forward
const uint8_t M2_IN2 = 14;  // Third motor reverse
const uint8_t M2_IN3 = 32;  // Fourth motor forward
const uint8_t M2_IN4 = 33;  // Fourth motor reverse

// Motor speeds
uint8_t mainSpeed = 200;     // Speed for main motors (0-255)
uint8_t turnSpeed = 180;     // Speed during turns
uint8_t auxiliarySpeed = 200; // Speed for auxiliary motors (0-255)

// Direction states packed into a single byte for efficiency
uint8_t directionState = 0;
#define STATE_FORWARD      0x01
#define STATE_BACKWARD     0x02
#define STATE_RIGHT        0x04
#define STATE_LEFT         0x08
#define STATE_AUX_FORWARD  0x10
#define STATE_AUX_BACKWARD 0x20

// Function prototypes
void updateMotorOutputs();

// Button handler for main movement
BLYNK_WRITE(V1) { // Forward button
    if (param.asInt()) {
        directionState |= STATE_FORWARD;
        directionState &= ~STATE_BACKWARD;
    } else {
        directionState &= ~STATE_FORWARD;
    }
    updateMotorOutputs();
}

BLYNK_WRITE(V2) { // Backward button
    if (param.asInt()) {
        directionState |= STATE_BACKWARD;
        directionState &= ~STATE_FORWARD;
    } else {
        directionState &= ~STATE_BACKWARD;
    }
    updateMotorOutputs();
}

BLYNK_WRITE(V3) { // Right button
    if (param.asInt()) {
        directionState |= STATE_RIGHT;
        directionState &= ~STATE_LEFT;
    } else {
        directionState &= ~STATE_RIGHT;
    }
    updateMotorOutputs();
}

BLYNK_WRITE(V4) { // Left button
    if (param.asInt()) {
        directionState |= STATE_LEFT;
        directionState &= ~STATE_RIGHT;
    } else {
        directionState &= ~STATE_LEFT;
    }
    updateMotorOutputs();
}

// Controls for auxiliary motors
BLYNK_WRITE(V5) { // Auxiliary Forward button
    if (param.asInt()) {
        directionState |= STATE_AUX_FORWARD;
        directionState &= ~STATE_AUX_BACKWARD;
    } else {
        directionState &= ~STATE_AUX_FORWARD;
    }
    updateMotorOutputs();
}

BLYNK_WRITE(V6) { // Auxiliary Backward button
    if (param.asInt()) {
        directionState |= STATE_AUX_BACKWARD;
        directionState &= ~STATE_AUX_FORWARD;
    } else {
        directionState &= ~STATE_AUX_BACKWARD;
    }
    updateMotorOutputs();
}

// Speed control sliders
BLYNK_WRITE(V7) { // Main motors speed control
    mainSpeed = param.asInt();
    turnSpeed = mainSpeed * 0.9; // Turn speed is 90% of main speed
    updateMotorOutputs();
}

BLYNK_WRITE(V8) { // Auxiliary motors speed control
    auxiliarySpeed = param.asInt();
    updateMotorOutputs();
}

void updateMotorOutputs() {
    // Update main motors
    uint8_t leftSpeed = 0;
    uint8_t rightSpeed = 0;
    
    // Set motor directions based on state
    if (directionState & STATE_FORWARD) {
        digitalWrite(M1_IN1, HIGH);
        digitalWrite(M1_IN2, LOW);
        digitalWrite(M1_IN3, HIGH);
        digitalWrite(M1_IN4, LOW);
        
        if (directionState & STATE_RIGHT) {
            leftSpeed = mainSpeed;
            rightSpeed = turnSpeed;
        } else if (directionState & STATE_LEFT) {
            leftSpeed = turnSpeed;
            rightSpeed = mainSpeed;
        } else {
            leftSpeed = rightSpeed = mainSpeed;
        }
    } else if (directionState & STATE_BACKWARD) {
        digitalWrite(M1_IN1, LOW);
        digitalWrite(M1_IN2, HIGH);
        digitalWrite(M1_IN3, LOW);
        digitalWrite(M1_IN4, HIGH);
        
        if (directionState & STATE_RIGHT) {
            leftSpeed = mainSpeed;
            rightSpeed = turnSpeed;
        } else if (directionState & STATE_LEFT) {
            leftSpeed = turnSpeed;
            rightSpeed = mainSpeed;
        } else {
            leftSpeed = rightSpeed = mainSpeed;
        }
    } else if ((directionState & STATE_RIGHT) && !(directionState & (STATE_FORWARD | STATE_BACKWARD))) {
        // In-place right turn
        digitalWrite(M1_IN1, HIGH);
        digitalWrite(M1_IN2, LOW);
        digitalWrite(M1_IN3, LOW);
        digitalWrite(M1_IN4, HIGH);
        leftSpeed = rightSpeed = turnSpeed;
    } else if ((directionState & STATE_LEFT) && !(directionState & (STATE_FORWARD | STATE_BACKWARD))) {
        // In-place left turn
        digitalWrite(M1_IN1, LOW);
        digitalWrite(M1_IN2, HIGH);
        digitalWrite(M1_IN3, HIGH);
        digitalWrite(M1_IN4, LOW);
        leftSpeed = rightSpeed = turnSpeed;
    } else {
        // Stop motors
        digitalWrite(M1_IN1, LOW);
        digitalWrite(M1_IN2, LOW);
        digitalWrite(M1_IN3, LOW);
        digitalWrite(M1_IN4, LOW);
    }
    
    analogWrite(M1A_EN, leftSpeed);
    analogWrite(M1B_EN, rightSpeed);
    
    // Update auxiliary motors
    if (directionState & STATE_AUX_FORWARD) {
        digitalWrite(M2_IN1, HIGH);
        digitalWrite(M2_IN2, LOW);
        digitalWrite(M2_IN3, HIGH);
        digitalWrite(M2_IN4, LOW);
        analogWrite(M2A_EN, auxiliarySpeed);
        analogWrite(M2B_EN, auxiliarySpeed);
    } else if (directionState & STATE_AUX_BACKWARD) {
        digitalWrite(M2_IN1, LOW);
        digitalWrite(M2_IN2, HIGH);
        digitalWrite(M2_IN3, LOW);
        digitalWrite(M2_IN4, HIGH);
        analogWrite(M2A_EN, auxiliarySpeed);
        analogWrite(M2B_EN, auxiliarySpeed);
    } else {
        digitalWrite(M2_IN1, LOW);
        digitalWrite(M2_IN2, LOW);
        digitalWrite(M2_IN3, LOW);
        digitalWrite(M2_IN4, LOW);
        analogWrite(M2A_EN, 0);
        analogWrite(M2B_EN, 0);
    }
}

void setup() {
    // Configure L298N pins all at once
    const uint8_t output_pins[] = {
        M1A_EN, M1B_EN, M1_IN1, M1_IN2, M1_IN3, M1_IN4,
        M2A_EN, M2B_EN, M2_IN1, M2_IN2, M2_IN3, M2_IN4
    };
    
    for (uint8_t i = 0; i < sizeof(output_pins); i++) {
        pinMode(output_pins[i], OUTPUT);
    }
    
    // Set initial state for all motors (stopped)
    digitalWrite(M1_IN1, LOW);
    digitalWrite(M1_IN2, LOW);
    digitalWrite(M1_IN3, LOW);
    digitalWrite(M1_IN4, LOW);
    digitalWrite(M2_IN1, LOW);
    digitalWrite(M2_IN2, LOW);
    digitalWrite(M2_IN3, LOW);
    digitalWrite(M2_IN4, LOW);
    
    // Start serial and Blynk
    Serial.begin(115200); // Faster baud rate
    Blynk.begin(auth, ssid, pass);
}

void loop() {
    Blynk.run();
    // No need to call updateMotorOutputs() in loop - it's called directly from each Blynk handler
}