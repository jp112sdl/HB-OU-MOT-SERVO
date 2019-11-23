//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2019-03-05 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2019-11-23 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <LowPower.h>

#include <Dimmer.h>

// We use a Pro Mini
// PIN for Status LED
#define LED_PIN 4
// PIN for ConfigToggleButton
#define CONFIG_BUTTON_PIN 8

// number of available peers per channel
#define PEERS_PER_CHANNEL 6

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
    {0xf3,0x48,0x00},      // Device ID
    "JPSERV0000",           // Device Serial
    {0xf3,0x48},            // Device Model: HM-DW-WM 2-Ch LED dimmer
    0x10,                   // Firmware Version
    as::DeviceType::Dimmer, // Device Type
    {0x01,0x00}             // Info Bytes
};

/**
 * Configure the used hardware
 */
typedef AskSin<StatusLed<LED_PIN>,NoBattery,Radio<AvrSPI<10,11,12,13> ,2>> Hal;
Hal hal;

typedef DimmerChannel<Hal, PEERS_PER_CHANNEL,List0> ServoChannelType;
typedef DimmerDevice<Hal, ServoChannelType,1,1,List0> ServoDevice;

class NoPWM {
public:
  void init(uint8_t __attribute__ ((unused)) pwm) {}
  void set(uint8_t __attribute__ ((unused)) pwm) {}
};

template<class HalType,class DimmerType,class PWM>
class ServoControl : public DimmerControl<HalType,DimmerType,PWM> {
public:
  typedef DimmerControl<HalType,DimmerType,PWM> BaseControl;
  ServoControl (DimmerType& dim) : BaseControl(dim) {
    DDRD |= _BV(PD3);
    TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(WGM21);
    TCCR2B |= _BV(CS22) ; // Prescaler 64
   // TCCR2B |= _BV(CS21) | _BV(CS22); // Prescaler 256
  }

  virtual ~ServoControl () {}

  virtual void updatePhysical () {
    // first calculate all physical values of the dimmer channels
    BaseControl::updatePhysical();
    int pos = map(this->physical[0], 0, 200, 22, 80);
    OCR2B = pos;
  }
};

ServoDevice sdev(devinfo, 0x20);
ServoControl<Hal,ServoDevice,NoPWM > control(sdev);
ConfigToggleButton<ServoDevice> cfgBtn(sdev);

void setup () {
  DINIT(57600,ASKSIN_PLUS_PLUS_IDENTIFIER);
  if( control.init(hal,0) ) {
    sdev.channel(1).peer(cfgBtn.peer());
  }
  buttonISR(cfgBtn,CONFIG_BUTTON_PIN);

  sdev.initDone();

  DDEVINFO(sdev);
}

void loop() {
  hal.runready();
  sdev.pollRadio();
}
