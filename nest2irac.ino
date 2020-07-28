#include <ArduinoLog.h>

#include <IRLibSendBase.h>
#include <IRLib_P01_NEC.h>
#include <IRLibCombo.h>

#include <SPI.h>
#include "RF24.h"

#define LOG_LEVEL LOG_LEVEL_TRACE

#define OPTO_PIN 10
#define OPTO_DIR 0x1
#define OPTO_HZ  60

#define CALL_PIN 7

#define RADIO_DEVICE_ID 0
#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 8
#define RADIO_TIME_INTERVAL_BETWEEN_SEND 3000000
#define RADIO_RECEIVE_TIMEOUT 8000000
#define RADIO_CHANNEL 0x7B
#define RADIO_PA_LEVEL RF24_PA_MAX
#define RADIO_DATA_RATE RF24_250KBPS
#define RADIO_CRC_LENGTH RF24_CRC_8
//#define RADIO_PAYLOAD_SIZE sizeof(Data)

enum LGACRemoteCommand {
  POWER,
  TEMP_DOWN,
  TEMP_UP,
  FAN_SPEED,
  TIMER,
  MODE
};

enum LGACPowerState {
  OFF,
  ON,
  NUMBER_OF_POWER_STATES
};

enum LGACFanState {
  F1,
  F2,
  F3,
  NUMBER_OF_FAN_STATES
};

enum LGACModeState {
  ENERGY_SAVER,
  COOL,
  FAN,
  DRY,
  NUMBER_OF_MODES
};

class LGAC {
  public:
    LGAC(IRsend* ir_control) : __ir_control(ir_control) {}

    void send(const LGACRemoteCommand command)
    {
      switch (command) {
        case POWER:
          sendPower();
          break;
        case TEMP_DOWN:
          sendTempDown();
          break;
        case TEMP_UP:
          sendTempUp();
          break;
        case FAN_SPEED:
          sendFanSpeed();
          break;
        case TIMER:
          sendTimer();
          break;
        case MODE:
          sendMode();
          break;
      }

    }

    LGACPowerState power()
    {
      return __power;
    }

    LGACFanState fan()
    {
      return __fan;
    }

    LGACModeState mode()
    {
      return __mode;
    }

    int temperature()
    {
      return __temperature;
    }

    void powerOn()
    {
      if (__power == OFF)
      {
        send(POWER);
      }
    }

    void powerOff()
    {
      if (__power == ON)
      {
        send(POWER);
      }
    }

    void setFan(LGACFanState fan_state)
    {
      LGACPowerState prev_power = __power;
      powerOn();
      while (__fan != fan_state)
      {
        send(FAN_SPEED);
      }
      if (prev_power == OFF)
      {
        powerOff();
      }
    }

    void setMode(LGACModeState mode)
    {
      if (__power == OFF && (mode == COOL || mode == DRY))
      {
        Log.error("Cannot set %s mode while ac is off\n", mode == COOL ? "COOL" : "DRY");
        return;
      }

      LGACPowerState prev_power = __power;
      powerOn();
      while (__mode != mode)
      {
        send(MODE);
      }
      if (prev_power == OFF)
      {
        powerOff();
      }
    }

    void setMinimumTemperature()
    {
      LGACPowerState prev_power = __power;
      LGACModeState prev_mode = __mode;
      powerOn();
      if (__mode == FAN)
      {
        send(MODE);
      }
      while (__temperature != MINIMUM_TEMPERATURE)
      {
        send(TEMP_DOWN);
      }
      setMode(__mode);
      if (prev_power == OFF)
      {
        powerOff();
      }
    }
 
  private:
    void sendPower()
    {
      Log.notice("Sending POWER command...\n");
      __ir_control->send(NEC, IR_POWER);
      delay(COMMAND_DELAY);
      __power = (__power + 1) % NUMBER_OF_POWER_STATES;
      if (__power == OFF)
      {
        if (__mode != FAN) {
          __mode = ENERGY_SAVER;
        }
        Log.trace("Power is OFF.\n");
      } else {
        Log.trace("Power is ON.\n");
      }
    }

    void sendTempDown()
    {
      if (__power == OFF) {
        Log.warning("AC is off. Notheng to do.\n");
        return;
      }
      if (__mode == FAN) {
        Log.warning("Cannot adjust temperature in fan mode.\n");
        return;
      }
      Log.notice("Sending TEMPERATURE DOWN command...\n");
      __ir_control->send(NEC, IR_TEMP_DOWN, 32);
      delay(COMMAND_DELAY);
      --__temperature;
      if (__temperature < MINIMUM_TEMPERATURE)
      {
        __temperature = MINIMUM_TEMPERATURE;
      }
      Log.trace("Current temperature is %d\n", __temperature);
    }

    void sendTempUp()
    {
      if (__power == OFF) {
        Log.warning("AC is off. Notheng to do.\n");
        return;
      }
      if (__mode == FAN) {
        Log.warning("Cannot adjust temperature in fan mode.\n");
        return;
      }
      Log.notice("Sending TEMPERATURE UP command...\n");
      __ir_control->send(NEC, IR_TEMP_UP, 32);
      delay(COMMAND_DELAY);
      ++__temperature;
      if (__temperature > MAXIMUM_TEMPERATURE)
      {
        __temperature = MAXIMUM_TEMPERATURE;
      }
      Log.trace("Current temperature is %d\n", __temperature);
    }

    void sendFanSpeed()
    {
      if (__power == OFF) {
        Log.warning("AC is off. Notheng to do.\n");
        return;
      }
      Log.notice("Sending FAN SPEED command...\n");
      __ir_control->send(NEC, IR_FAN_SPEED, 32);
      delay(COMMAND_DELAY);
      __fan = (__fan + 1) % NUMBER_OF_FAN_STATES;
      Log.trace("Current fan speed is %s\n",
          __fan == F1 ? "F1" :
          __fan == F2 ? "F2" :
          __fan == F3 ? "F3" : "???");
    }

    void sendTimer()
    {
      if (__power == OFF) {
        Log.warning("AC is off. Notheng to do.\n");
        return;
      }
      Log.warning("TIMER function is not implemented yet.\n");
    }

    void sendMode()
    {
      if (__power == OFF) {
        Log.warning("AC is off. Notheng to do.\n");
        return;
      }
      Log.notice("Sending MODE command...\n");
      __ir_control->send(NEC, IR_MODE, 32);
      delay(COMMAND_DELAY);
      __mode = (__mode + 1) % NUMBER_OF_MODES;
      Log.trace("Current mode is %s\n", 
          __mode == ENERGY_SAVER ? "ENERGY_SAVER" :
          __mode == COOL ? "COOL" :
          __mode == FAN ? "FAN" :
          __mode == DRY ? "DRY" : "???");
    }

    IRsend* __ir_control;

    LGACPowerState __power{OFF};
    LGACFanState   __fan{F1};
    LGACModeState  __mode{FAN};
    int __temperature{60};

    static const int MINIMUM_TEMPERATURE = 60;
    static const int MAXIMUM_TEMPERATURE = 86;
    static const uint32_t IR_POWER     = 0x8166817E;
    static const uint32_t IR_TEMP_DOWN = 0x816651AE;
    static const uint32_t IR_TEMP_UP   = 0x8166A15E;
    static const uint32_t IR_FAN_SPEED = 0x81669966;
    static const uint32_t IR_TIMER     = 0x8166F906;
    static const uint32_t IR_MODE      = 0x8166D926;
    static const unsigned long COMMAND_DELAY = 500;
};

class CallAC {
  public:
    CallAC(LGAC* ac)
      : __ac(ac)
    {
    }

    void on()
    {
      const unsigned long time_between_calls = millis() - prev_call_time;

      if (prev_call == 1) {
        return;
      }

      if (time_between_calls < CALL_DELAY)
      {
        Log.warning("Time between calls (%l) is too short (%l)\n", time_between_calls, CALL_DELAY);
        return;
      }

      Log.notice("Calling on\n");

      __ac->powerOn();
      __ac->setMode(COOL);
      __ac->setFan(F3);
      __ac->setMinimumTemperature();

      prev_call = 1;
      prev_call_time = millis();
    }

    void off()
    {
      const unsigned long time_between_calls = millis() - prev_call_time;

      if (prev_call == -1) {
        __ac->powerOff();
        return;
      }

      if (prev_call == 0) {
        if (time_between_calls > POWER_OFF)
          __ac->powerOff();
        return;
      }

      if (time_between_calls < CALL_DELAY)
      {
        Log.warning("Time between calls (%l) is too short (%l)\n", time_between_calls, CALL_DELAY);
        return;
      }

      Log.notice("Calling off\n");

      __ac->setMode(FAN);
      __ac->setFan(F1);

      prev_call = 0;
      prev_call_time = millis();
    }

  private:
    LGAC* __ac;

    int prev_call{ -1};
    unsigned long prev_call_time{ -CALL_DELAY};

    static const unsigned long POWER_OFF  = 900000;
    static const unsigned long CALL_DELAY = 300000;
};

class Opto {
  public:
    Opto(unsigned int opto_pin, unsigned int opto_dir, unsigned int opto_hz)
      : __opto_pin(opto_pin)
      , __opto_dir(opto_dir)
      , __opto_hz(opto_hz)
    {
        __interval = 125000 / opto_hz; // 8 samples per cycle
        __last_read = - __interval;
    }

    void receive()
    {
        if (micros() - __last_read < __interval)
        {
            return;
        }
        bitWrite(__opto_buf, __read_position, (digitalRead(__opto_pin) ^ __opto_dir));
        __read_position = (__read_position + 1) & 0x7;
        __last_read = micros();
        Log.verbose("opto_buf is %d, read position is %d\n", __opto_buf, __read_position);
    }
    
    int call()
    {        
        return __opto_buf;
    }
    
  private:
    unsigned int __opto_pin{0};
    unsigned int __opto_dir{0};
    unsigned int __opto_hz{0};
    unsigned int __interval{0};
    unsigned long __last_read{0};
    unsigned int __read_position{0};
    unsigned int __opto_buf{0};
};

struct Data
{
    unsigned long _time{0};
    int call{0};
};

const unsigned int n_devices{2};
byte addresses[n_devices][5] = {{0x00, 0x55, 0x0A, 0xAD, 0x0B},
                                {0x01, 0x55, 0x0A, 0xAD, 0x0B}};
unsigned long last_sent{0};

IRsend ir_control;
LGAC ac(&ir_control);
CallAC call_ac(&ac);
Opto opto(OPTO_PIN, OPTO_DIR, OPTO_HZ);
RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);

void setup_radio() {
    radio.begin();
    if (RADIO_DEVICE_ID)
    {
        radio.openReadingPipe(1, addresses[1]);
        radio.startListening();
    }
    else
    {
        radio.openWritingPipe(addresses[1]);
    }
#ifdef RADIO_CHANNEL
    radio.setChannel(RADIO_CHANNEL);
#endif
#ifdef RADIO_PA_LEVEL
    radio.setPALevel(RADIO_PA_LEVEL);
#endif
#ifdef RADIO_DATA_RATE
    radio.setDataRate(RADIO_DATA_RATE);
#endif
#ifdef RADIO_CRC_LENGTH
    radio.setCRCLength(RADIO_CRC_LENGTH);
#endif
#ifdef RADIO_PAYLOAD_SIZE
    radio.setPayloadSize(RADIO_PAYLOAD_SIZE);
#endif
    Log.notice("Radio address %x%x%x%x%x\n", addresses[1][4], addresses[1][3], addresses[1][2], addresses[1][1], addresses[1][0]);
    Log.notice("Radio channel %x\n", radio.getChannel());
    Log.notice("Radio power level %d\n", radio.getPALevel());
    Log.notice("Radio data rate %d\n", radio.getDataRate());
    Log.notice("Radio payload size %d\n", radio.getPayloadSize());
    Log.notice("Radio CRC length %d\n", radio.getCRCLength());
    Log.notice("Radio is plus%s compatible\n", radio.isPVariant() ? "" : " NOT");
}

void setup() {
    Serial.begin(115200);
    delay(2000); while (!Serial);
    Log.begin(LOG_LEVEL, &Serial, true);

    Log.notice("Initializing pins...\n");
    pinMode(OPTO_PIN, INPUT);
    digitalWrite(OPTO_PIN, HIGH);

    pinMode(CALL_PIN, OUTPUT);
    digitalWrite(CALL_PIN, LOW);

    Log.notice("Initializing radio...\n");
    setup_radio();
    Log.notice("Initialization complete!\n");
}

//long unsigned last_test{0};
//int last_call{0};

void loop() {
    Data data;

    if (radio.failureDetected) {
        Log.error("Radio failed!\n");
        Log.notice("Reinitializing radio...\n");
        setup_radio();
        Log.notice("Reinitialization complete!\n");
    }
    
    if (RADIO_DEVICE_ID)
    {
        unsigned long started_waiting_at = micros();
        boolean timeout = false;
    
        while ( ! radio.available() )
        {
              if (micros() - started_waiting_at > RADIO_RECEIVE_TIMEOUT)
              {
                  timeout = true;
                  break;
              }      
        }

        if (timeout)
        {
            Log.verbose("Radio is unavailable\n");
        }
        else
        {
            radio.read(&data, sizeof(data));
            if (data.call)
            {
                digitalWrite(CALL_PIN, HIGH);
                call_ac.on();
            }
            else
            {
                digitalWrite(CALL_PIN, LOW);
                call_ac.off();
            }      
            Log.trace("Received {%l, %d}\n", data._time, data.call);
        }
    }
    else
    {
        opto.receive();
        data._time = millis();
        data.call = opto.call();
//        data.call = last_call;
//        if (millis() - last_test > 5000)
//        {
//            last_test = millis();
//            last_call = 1 - last_call;
//        }
        if (data.call)
        {
            digitalWrite(CALL_PIN, HIGH);
        }
        else
        {
            digitalWrite(CALL_PIN, LOW);
        }
        if (micros() - last_sent > RADIO_TIME_INTERVAL_BETWEEN_SEND) {
            last_sent = micros();
            if (!radio.write(&data, sizeof(data))){
                Log.error("Failed to send {%l, %d}\n", data._time, data.call);
            }
            Log.trace("Sent {%l, %d}\n", data._time, data.call);
        }
    }
}
