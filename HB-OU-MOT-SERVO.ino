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

// timer values for servo motor
#define TIMER_0DEG   22
#define TIMER_180DEG 80

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

DEFREGISTER(ServoReg0,MASTERID_REGS, DREG_INTKEY, 0x2e, 0x2f)

class ServoList0 : public RegList0<ServoReg0> {
public:
  ServoList0 (uint16_t addr) : RegList0<ServoReg0>(addr) {}
  bool releaseAfterMove (bool value) const { return this->writeRegister(0x2f, value & 0xff); }
  bool releaseAfterMove ()           const { return this->readRegister (0x2f, 0);            }
  bool powerUpAction (uint8_t value) const { return this->writeRegister(0x2e, value & 0xff); }
  bool powerUpAction ()              const { return this->readRegister (0x2e, 0);            }
  void defaults () {
    clear();
    releaseAfterMove(false);
    intKeyVisible(true);
    powerUpAction(0);
  }
};

typedef AskSin<StatusLed<LED_PIN>,NoBattery,Radio<AvrSPI<10,11,12,13> ,2>> Hal;
typedef DimmerChannel<Hal, PEERS_PER_CHANNEL,ServoList0> ServoChannelType;
typedef DimmerDevice<Hal, ServoChannelType, 1, 1, ServoList0> ServoDevice;
Hal hal;


class NoPWM {
public:
  void init(uint8_t __attribute__ ((unused)) pwm) {}
  void set(uint8_t __attribute__ ((unused)) pwm) {}
};

template<class HalType,class DimmerType,class PWM>
class ServoControl : public DimmerControl<HalType,DimmerType,PWM>, public Alarm {
private:
  uint8_t lastphys;
  bool    first;
public:
  typedef DimmerControl<HalType,DimmerType,PWM> BaseControl;
  ServoControl (DimmerType& dim) : BaseControl(dim), Alarm(2), lastphys(0), first(true) { }

  virtual ~ServoControl () {}

  void activateTCCR2() {
    TCCR2A = /*_BV(COM2A1) | */ _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(WGM21) | _BV(CS22);  // Prescaler 64
  }

  void deactivateTCCR2() {
    TCCR2B = 0;
  }

  bool isActive() { return TCCR2B > 0; }

  void initTimer() {
    DDRD |= _BV(PD3);
    activateTCCR2();
    if (BaseControl::dimmer.getList0().powerUpAction() == 0)
      deactivateTCCR2();
  }

  virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
    deactivateTCCR2();
  }

  virtual void updatePhysical () {
    // first calculate all physical values of the dimmer channels
    BaseControl::updatePhysical();
    ServoList0 l0 = BaseControl::dimmer.getList0();
    uint8_t phys = this->physical[0];
    int pos = map(phys, 0, 200, TIMER_0DEG, TIMER_180DEG);
    if ((lastphys != phys) || (first && l0.powerUpAction() > 0 && phys == 0)) {

      if (!isActive()) activateTCCR2();
      OCR2B = pos;

      //DPRINT("UPDATE WITH POS");DDEC(pos);DPRINT(" FROM PHYS ");DDECLN(phys);

      if (l0.releaseAfterMove() == true) {
        sysclock.cancel(*this);
        Alarm::set(millis2ticks(800));
        sysclock.add(*this);
      }

      first = false;

    }
    lastphys = phys;
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
  control.initTimer();
  buttonISR(cfgBtn,CONFIG_BUTTON_PIN);

  sdev.initDone();

  DDEVINFO(sdev);
}

void loop() {
  hal.runready();
  sdev.pollRadio();
}
