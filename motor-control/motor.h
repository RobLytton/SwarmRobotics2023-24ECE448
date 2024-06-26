#include <pigpio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>

#define PI 3.14159

// Encoder parameters (time in microseconds)
#define TICKS_PER_ROTATION 20
#define MINIMUM_ENCODER_INTERVAL 10000UL
#define MAXIMUM_ENCODER_INTERVAL 200000UL
#define ENCODER_P 1

// Struct containing motor settings and values
struct Motor {
    // GPIO pins for PWM, direction, and the encoder
    const int gpioPWM; // not to be confused with pigpio's gpioPWM() function
    const int gpioDir;
    const int gpioEncoder;
    
    volatile int encoderTick;
    volatile uint32_t encoderTime;

    float velocityTarget;
    float velocityOutput;
    volatile float velocityMeasured;
    float speedMax;

    uint8_t powerMin;
    uint8_t powerStart;
};

// Handles encoder interrupt
void motorOnEncoderTick(struct Motor* m, int level, uint32_t now) {
    uint32_t delta = now - m->encoderTime;
    
    // Ignore tick if too fast or wrong level
    if (level || delta < MINIMUM_ENCODER_INTERVAL) return;

    // Handle tick
    int8_t direction = (m->velocityOutput < 0 ? -1 : 1);
    m->velocityMeasured = 1000000.0/delta*direction;
    m->encoderTick += direction;
    m->encoderTime = now;
}

void motorInitialize(struct Motor* m, gpioISRFunc_t onEncoderTick) {
    // Initialize piGPIO if needed
    static int gpioInitialized = 0;
    if (!gpioInitialized) {
        // Initialize piGPIO with signal handlers disabled
        int cfg = gpioCfgGetInternals();
        cfg |= PI_CFG_NOSIGHANDLER;  // (1<<10)
        gpioCfgSetInternals(cfg);
        if (gpioInitialise() < 0) {
            printf("PiGPIO Initialization failed. Make sure to run with sudo\n");
            return;
        }
        
        // Set initialization flag
        gpioInitialized = 1;
    }
    
    // Setup PWM
    gpioSetMode(m->gpioPWM, PI_OUTPUT);
    gpioSetPWMfrequency(m->gpioPWM, 8000);
    gpioPWM(m->gpioPWM, 0);
    
    // Setup motor direction
    gpioSetMode(m->gpioDir, PI_OUTPUT);
    gpioWrite(m->gpioDir, 0);
    
    // Setup encoder interrupt
    gpioSetMode(m->gpioEncoder, PI_INPUT);
    gpioSetPullUpDown(m->gpioEncoder, PI_PUD_DOWN);
    gpioSetAlertFunc(m->gpioEncoder, onEncoderTick);
    
    // Set default values
    m->encoderTick = 0;
    m->encoderTime = 0;
    m->velocityTarget = 0;
    m->velocityOutput = 0;
    m->velocityMeasured = 0;
    m->speedMax = 0;
    m->powerMin = 0;
    m->powerStart = 0;
}

void motorSetDuty(struct Motor* m, int duty) {
    // Set PWM and direction
    int dir = (duty < 0);
    gpioPWM(m->gpioPWM, duty*(dir ? -1 : 1));
    gpioWrite(m->gpioDir, dir);
}

void motorTerminate(struct Motor* m) {
    // Clear motor output and encoder interrupt
    gpioPWM(m->gpioPWM, 0);
    gpioWrite(m->gpioPWM, 0);
    gpioSetAlertFunc(m->gpioEncoder, NULL);
}

// Calculates powerStart, powerMin, and speedMax for a motor
void motorCalibrate(struct Motor* m) {
    // Reset measured velocity
    m->velocityMeasured = 0;

    // Increase power until motor moves and store result
    for (uint8_t power = 0; power < 255; power++) {
        if (m->velocityMeasured != 0) {
            m->powerStart = fmin(power + 10, 255);
            break;
        }
        motorSetDuty(m, power);
        usleep(16000);
    }

    // Drive at full power and measure speed
    motorSetDuty(m, 200);
    usleep(900000);

    float speed = 0;
    for (uint8_t i = 0; i < 64; i++) {
        speed += m->velocityMeasured;
        usleep(16000);
    }

    m->speedMax = speed/64;

    // Decrease power until motor stops and store result
    for (uint8_t power = m->powerStart; power > 0; power--) {
        if (gpioTick() - m->encoderTime > MAXIMUM_ENCODER_INTERVAL) {
            m->powerMin = power + 10;
            break;
        }
        motorSetDuty(m, --power);
        usleep(50000);
    }

    motorSetDuty(m, 0);
    m->velocityTarget = 0;
}

// Determines motor speed and adjusts output to match target
void motorUpdate(struct Motor* m, float delta) {
    // Assume motor stopped if no encoder tick occurs in a certain interval
    if ((gpioTick() - m->encoderTime) > MAXIMUM_ENCODER_INTERVAL) m->velocityMeasured = 0;

    // Update PID loop (really just P)
    float error = m->velocityTarget - m->velocityMeasured;
    if (m->velocityTarget == 0) m->velocityOutput = 0;
    else m->velocityOutput += ENCODER_P*error*delta;

    // Limit speed range
    m->velocityOutput = fmax(-m->speedMax, fmin(m->velocityOutput, m->speedMax));

    // Calculate power output
    int16_t power = (int16_t) 255*m->velocityOutput/m->speedMax; // Speed -> power
    if (m->powerMin < abs(power) && abs(power) < m->powerStart && m->velocityMeasured == 0) power = m->powerStart*(power < 0 ? -1 : 1); // Start push

    // Set motor power
    //printf("%f %d\n",  m->velocityOutput, power);
    motorSetDuty(m, power);
}

// Sets the motor speed
void motorSetVelocity(struct Motor* m, float velocity) {
    float speed = fabs(velocity);
    
    // Zero power if below minimum
    if (speed < (m->speedMax*m->powerMin/255)) {
        m->velocityTarget = 0;
        return;
    }

    // Limit to max speed and set
    if (speed > m->speedMax) velocity = m->speedMax*(velocity < 0 ? -1 : 1);
    m->velocityTarget = velocity;
}
