/*
 * Copyright (c) 2022 Hendrik Hagendorn
 * Copyright (c) 2016 pa-pa
 *
 * Creative Commons
 * http://creativecommons.org/licenses/by-nc-sa/3.0/
 *
 * AskSin++ HM-ES-TX-WM energy meter with SML support
 */

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#include <AskSinPP.h>
#include <MultiChannelDevice.h>

#include <SoftwareSerial.h>

#define LED_PIN             4
#define CONFIG_BUTTON_PIN   5

#define IR_RX_PIN           7
#define IR_TX_PIN           6

// SML communication
#define SERIAL_READ_TIMEOUT_MS  100
#define SML_MSG_BUFFER_SIZE     500

#define SML_TYPE_STRING             0
#define SML_TYPE_BOOLEAN            4
#define SML_TYPE_SIGNED_INTEGER     5
#define SML_TYPE_UNSIGNED_INTEGER   6
#define SML_TYPE_LIST               7

#define SML_OK                      0
#define SML_ERROR_UNKNOWN           1
#define SML_ERROR_UNEXPECTED_TYPE   2
#define SML_ERROR_UNSUPPORTED       3
#define SML_ERROR_OUT_OF_BOUNDS     4

// we send the counter every 2 minutes
#define MSG_CYCLE seconds2ticks(2*60)

// number of available peers per channel
#define PEERS_PER_CHANNEL 2

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
    { 0x90, 0x12, 0x34 },       // Device ID
    "NVG0M00001",               // Device Serial
    { 0x00, 0xde },             // Device Model
    0x20,                       // Firmware Version
    as::DeviceType::PowerMeter, // Device Type
    { 0x01, 0x00 }              // Info Bytes
};

// SML communication
const uint8_t SEQUENCE_METER_READING[] = { 0x77, 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF };
const uint8_t SEQUENCE_POWER_READING[] = { 0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF };

SoftwareSerial smlSerial(IR_RX_PIN, IR_TX_PIN);

uint16_t smlBufferSize = 0;
uint8_t smlBuffer[SML_MSG_BUFFER_SIZE];

/**
 * Configure the used hardware
 */
typedef AvrSPI<10, 11, 12, 13> SPIType;
typedef Radio<SPIType, 2> RadioType;
typedef StatusLed<LED_PIN> LedType;
typedef AskSin<LedType, BatterySensor, RadioType> HalType;

class MeterList0Data : public List0Data {
    uint8_t LocalResetDisable       : 1;   // 0x18 - 24
    uint8_t Baudrate                : 8;   // 0x23 - 35
    uint8_t SerialFormat            : 8;   // 0x24 - 36
    uint8_t MeterPowerMode          : 8;   // 0x25 - 37
    uint8_t MeterProtocolMode       : 8;   // 0x26 - 38
    uint8_t SamplesPerCycle         : 8;   // 0x27 - 39
    uint8_t DzgCompatibilityMode    : 1;   // 0x28 - 40
    uint8_t ElsterCompatibilityMode : 1;   // 0x28 - 40.1

  public:
    static uint8_t getOffset(uint8_t reg) {
        switch (reg) {
            case 0x18: return sizeof(List0Data) + 0;
            case 0x23: return sizeof(List0Data) + 1;
            case 0x24: return sizeof(List0Data) + 2;
            case 0x25: return sizeof(List0Data) + 3;
            case 0x26: return sizeof(List0Data) + 4;
            case 0x27: return sizeof(List0Data) + 5;
            case 0x28: return sizeof(List0Data) + 6;
            default:   break;
        }

        return List0Data::getOffset(reg);
    }

    static uint8_t getRegister(uint8_t offset) {
        switch (offset) {
            case sizeof(List0Data) + 0:  return 0x18;
            case sizeof(List0Data) + 1:  return 0x23;
            case sizeof(List0Data) + 2:  return 0x24;
            case sizeof(List0Data) + 3:  return 0x25;
            case sizeof(List0Data) + 4:  return 0x26;
            case sizeof(List0Data) + 5:  return 0x27;
            case sizeof(List0Data) + 6:  return 0x28;
            default: break;
        }

        return List0Data::getRegister(offset);
    }
};

class MeterList0 : public ChannelList<MeterList0Data> {
  public:
    MeterList0(uint16_t a) : ChannelList(a) {}

    operator List0& () const { return *(List0 *) this; }

    // from List0
    HMID masterid() { return ((List0 *) this)->masterid(); }
    void masterid(const HMID& mid) { ((List0 *) this)->masterid(mid); }
    bool aesActive() const { return ((List0 *) this)->aesActive(); }

    bool localResetDisable() const { return isBitSet(sizeof(List0Data) + 0, 0x01); }
    bool localResetDisable(bool value) const { return setBit(sizeof(List0Data) + 0, 0x01, value); }
    uint8_t baudrate() const { return getByte(sizeof(List0Data) + 1); }
    bool baudrate(uint8_t value) const { return setByte(sizeof(List0Data) + 1, value); }
    uint8_t serialFormat() const { return getByte(sizeof(List0Data) + 2); }
    bool serialFormat(uint8_t value) const { return setByte(sizeof(List0Data) + 2, value); }
    uint8_t powerMode() const { return getByte(sizeof(List0Data) + 3); }
    bool powerMode(uint8_t value) const { return setByte(sizeof(List0Data) + 3, value); }
    uint8_t protocolMode() const { return getByte(sizeof(List0Data) + 4); }
    bool protocolMode(uint8_t value) const { return setByte(sizeof(List0Data) + 4, value); }
    uint8_t samplesPerCycle() const { return getByte(sizeof(List0Data) + 5); }
    bool samplesPerCycle(uint8_t value) const { return setByte(sizeof(List0Data) + 5, value); }
    bool dzgCompatibilityMode() const { return isBitSet(sizeof(List0Data) + 6, 0x01); }
    bool dzgCompatibilityMode(bool value) const { return setBit(sizeof(List0Data) + 6, 0x01, value); }
    bool elsterCompatibilityMode() const { return isBitSet(sizeof(List0Data) + 6, 0x02); }
    bool elsterCompatibilityMode(bool value) const { return setBit(sizeof(List0Data) + 6, 0x02, value); }

    uint8_t transmitDevTryMax() const { return 6; }
    uint8_t ledMode() const { return 1; }

    void defaults() {
        ((List0 *) this)->defaults();
    }
};

class MeterList1Data {
  public:
    uint8_t  AesActive          : 1;  // 0x08, s:0, e:1
    uint32_t TxThresholdPower   : 24; // 0x7C - 0x7E
    uint8_t  PowerString[16];         // 0x36 - 0x46 : 06 - 21
    uint8_t  EnergyCounterString[16]; // 0x47 - 0x57 : 22 - 37

    static uint8_t getOffset(uint8_t reg) {
        switch (reg) {
            case 0x08: return 0;
            case 0x7c: return 3;
            case 0x7d: return 4;
            case 0x7e: return 5;
            default: break;
        }

        if (reg >= 0x36 && reg <= 0x57) {
            return reg - 0x36 + 6;
        }

        if (reg >= 0x96 && reg <= 0x9b) {
            return reg - 0x96 + 38;
        }

        return 0xff;
    }

    static uint8_t getRegister(uint8_t offset) {
        switch (offset) {
            case 0:  return 0x08;
            case 3:  return 0x7c;
            case 4:  return 0x7d;
            case 5:  return 0x7e;
            default: break;
        }

        if (offset >= 6 && offset <= 37) {
            return offset - 6 + 0x36;
        }

        if (offset >= 38 && offset <= 43) {
            return offset - 38 + 0x96;
        }

        return 0xff;
    }
};

class MeterList1 : public ChannelList<MeterList1Data> {
  public:
    MeterList1(uint16_t a) : ChannelList(a) {}

    bool aesActive() const { return isBitSet(0, 0x01); }
    bool aesActive(bool s) const { return setBit(0, 0x01, s); }
    uint32_t thresholdPower() const { return ((uint32_t) getByte(3) << 16) + ((uint16_t) getByte(4) << 8) + getByte(5); }
    bool thresholdPower(uint32_t value) const { return setByte(3, (value >> 16) & 0xff) && setByte(4, (value >> 8) & 0xff) && setByte(5, value & 0xff); }

    void defaults() {
        aesActive(false);
        thresholdPower(100 * 100);
    }
};

class IECEventMsg : public Message {
  public:
    void init(uint8_t msgcnt, uint8_t channel, const uint64_t& counter, const uint32_t& power, bool lowbat) {
        uint8_t cnt1 = channel & 0x3f;
        if (lowbat == true) {
            cnt1 |= 0x40;
        }

        Message::init(0x15, msgcnt, 0x61, BIDI | WKMEUP, cnt1, 0x00);
        pload[0] = (counter >> 32) & 0xff;
        pload[1] = (counter >> 24) & 0xff;
        pload[2] = (counter >> 16) & 0xff;
        pload[3] = (counter >>  8) & 0xff;
        pload[4] = counter & 0xff;
        pload[5] = 0x00;
        pload[6] = (power >> 24) & 0xff;
        pload[7] = (power >> 16) & 0xff;
        pload[8] = (power >>  8) & 0xff;
        pload[9] = power & 0xff;
    }
};

class IECEventCycleMsg : public IECEventMsg {
  public:
    void init(uint8_t msgcnt, uint8_t channel, const uint64_t& counter, const uint32_t& power, bool lowbat) {
        IECEventMsg::init(msgcnt, channel, counter, power, lowbat);
        typ = 0x60;
    }
};

class MeterChannel : public Channel<HalType, MeterList1, EmptyList, List4, PEERS_PER_CHANNEL, MeterList0>, public Alarm {

    uint64_t counter;
    uint32_t power;
    Message  msg;
    uint8_t  msgcnt;
    bool     boot;

  private:

  public:
    MeterChannel() : Channel(), Alarm(MSG_CYCLE), counter(0), power(0), msgcnt(0), boot(true) {}
    virtual ~MeterChannel() {}

    void firstinit() {
        Channel<HalType, MeterList1, EmptyList, List4, PEERS_PER_CHANNEL, MeterList0>::firstinit();
    }

    uint8_t status() const {
        return 0;
    }

    uint8_t flags() const {
        return device().battery().low() ? 0x80 : 0x00;
    }

    void setCounter(uint64_t newCounter) {
        counter = newCounter;

        DPRINT(F("counter = "));
        DHEX((uint32_t) (counter >> 32));
        DHEXLN((uint32_t) counter);
    }

    void setPower(uint32_t newPower) {
        power = newPower;

        DPRINT(F("power = "));
        DHEXLN(power);
    }

    virtual void trigger(AlarmClock& clock) {
        tick = MSG_CYCLE;
        clock.add(*this);

        ((IECEventCycleMsg&) msg).init(msgcnt++, number(), counter, power, device().battery().low());

        device().sendPeerEvent(msg, *this);
        boot = false;
    }
};

bool isValidSMLHeader() {
    return ((smlBuffer[0] == 0x1b) &&
            (smlBuffer[1] == 0x1b) &&
            (smlBuffer[2] == 0x1b) &&
            (smlBuffer[3] == 0x1b) &&
            (smlBuffer[4] == 0x01) &&
            (smlBuffer[5] == 0x01) &&
            (smlBuffer[6] == 0x01) &&
            (smlBuffer[7] == 0x01));
}

uint8_t getListEntry(uint16_t position, uint8_t entryNum, uint8_t *type, uint16_t *start, uint16_t *length) {
    uint8_t tlList = smlBuffer[position++];

    if ((tlList >> 7) == 1) {
        return SML_ERROR_UNSUPPORTED;
    }

    if ((tlList >> 4) != SML_TYPE_LIST) {
        return SML_ERROR_UNEXPECTED_TYPE;
    }

    if (entryNum >= (tlList & 0xF)) {
        return SML_ERROR_OUT_OF_BOUNDS;
    }

    for (uint8_t i = 0; i <= entryNum; i++) {
        uint8_t tl = smlBuffer[position];

        if (tl >> 7 == 1) {
            return SML_ERROR_UNSUPPORTED;
        }

        if (i == entryNum) {
            *type = tl >> 4;
            *start = position + 1;
            *length = (tl & 0xF) - 1;

            return SML_OK;
        } else {
            if (tl >> 4 == SML_TYPE_LIST) {
                position++;
            }

            position += tl & 0xF;
        }
    }

    return SML_ERROR_UNKNOWN;
}

int64_t parseMeterReading() {
    int8_t scaler;
    uint8_t status, type;
    uint16_t start, length;
    int64_t reading;

    for (uint16_t i = 0; i < smlBufferSize; i++) {
        for (uint8_t j = 0; j < sizeof(SEQUENCE_METER_READING); j++) {
            if (smlBuffer[i + j] != SEQUENCE_METER_READING[j]) {
                goto meter_next;
            }
        }

        status = getListEntry(i, 4, &type, &start, &length);
        if (status != SML_OK) {
            DPRINT(F("[parseMeterReading] SML_ERROR: entry = 4, status = "));
            DHEXLN(status);
            return -2;
        }

        if (type != SML_TYPE_SIGNED_INTEGER) {
            DPRINT(F("[parseMeterReading] SML_ERROR: entry = 4, type = "));
            DHEXLN(status);
            return -3;
        }

        if (length != 1) {
            DPRINT(F("[parseMeterReading] SML_ERROR: entry = 4, length = "));
            DHEXLN(length);
            return -4;
        }

        scaler = smlBuffer[start];

        status = getListEntry(i, 5, &type, &start, &length);
        if (status != SML_OK) {
            DPRINT(F("[parseMeterReading] SML_ERROR: entry = 5, status = "));
            DHEXLN(status);
            return -2;
        }

        if (type != SML_TYPE_SIGNED_INTEGER) {
            DPRINT(F("[parseMeterReading] SML_ERROR: entry = 5, type = "));
            DHEXLN(status);
            return -3;
        }

        if (length != 8) {
            DPRINT(F("[parseMeterReading] SML_ERROR: entry = 5, length = "));
            DHEXLN(length);
            return -4;
        }

        reading =   ((int64_t) smlBuffer[start + 7])
                  | ((int64_t) smlBuffer[start + 6] << 0x08)
                  | ((int64_t) smlBuffer[start + 5] << 0x10)
                  | ((int64_t) smlBuffer[start + 4] << 0x18)
                  | ((int64_t) smlBuffer[start + 3] << 0x20)
                  | ((int64_t) smlBuffer[start + 2] << 0x28)
                  | ((int64_t) smlBuffer[start + 1] << 0x30)
                  | ((int64_t) smlBuffer[start]     << 0x38);

        return reading * pow(10, scaler);

meter_next:;
    }

    return -1;
}

int32_t parsePowerReading() {
    int8_t scaler;
    uint8_t status, type;
    uint16_t start, length;
    int64_t reading;

    for (uint16_t i = 0; i < smlBufferSize; i++) {
        for (uint8_t j = 0; j < sizeof(SEQUENCE_POWER_READING); j++) {
            if (smlBuffer[i + j] != SEQUENCE_POWER_READING[j]) {
                goto power_next;
            }
        }

        status = getListEntry(i, 4, &type, &start, &length);
        if (status != SML_OK) {
            DPRINT(F("[parsePowerReading] SML_ERROR: entry = 4, status = "));
            DHEXLN(status);
            return -2;
        }

        if (type != SML_TYPE_SIGNED_INTEGER) {
            DPRINT(F("[parsePowerReading] SML_ERROR: entry = 4, type = "));
            DHEXLN(status);
            return -3;
        }

        if (length != 1) {
            DPRINT(F("[parsePowerReading] SML_ERROR: entry = 4, length = "));
            DHEXLN(length);
            return -4;
        }

        scaler = smlBuffer[start];

        status = getListEntry(i, 5, &type, &start, &length);
        if (status != SML_OK) {
            DPRINT(F("[parsePowerReading] SML_ERROR: entry = 5, status = "));
            DHEXLN(status);
            return -2;
        }

        if (type != SML_TYPE_SIGNED_INTEGER) {
            DPRINT(F("[parsePowerReading] SML_ERROR: entry = 5, type = "));
            DHEXLN(status);
            return -3;
        }

        if (length != 4) {
            DPRINT(F("[parsePowerReading] SML_ERROR: entry = 5, length = "));
            DHEXLN(length);
            return -4;
        }

        reading =   ((int32_t) smlBuffer[start + 3])
                  | ((int32_t) smlBuffer[start + 2] << 0x08)
                  | ((int32_t) smlBuffer[start + 1] << 0x10)
                  | ((int32_t) smlBuffer[start]     << 0x18);

        return reading * pow(10, scaler);

power_next:;
    }

    return -1;
}

typedef MultiChannelDevice<HalType, MeterChannel, 2, MeterList0> MeterType;

HalType hal;
MeterType sdev(devinfo, 0x20);

ConfigButton<MeterType> cfgBtn(sdev);

void setup() {
    DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);

    sdev.init(hal);

    buttonISR(cfgBtn, CONFIG_BUTTON_PIN);

    // add channel 1 to timer to send event
    sysclock.add(sdev.channel(1));

    sdev.initDone();

    // SML communication
    pinMode(IR_RX_PIN, INPUT);
    pinMode(IR_TX_PIN, OUTPUT);

    smlSerial.begin(9600);

    DPRINTLN(F("Booted!"));
}

void loop() {
    bool asp_run = hal.runready();
    bool poll = sdev.pollRadio();

    if (asp_run == false && poll == false) {
        smlSerial.overflow();
        smlSerial.listen();

        int available = smlSerial.available();
        if (!available) {
            return;
        }

        smlBufferSize = 0;
        memset(smlBuffer, 0, sizeof(smlBuffer));

        uint64_t lastReadTime = millis();
        while (millis() - lastReadTime < SERIAL_READ_TIMEOUT_MS) {
            if (smlSerial.available()) {
                smlBuffer[smlBufferSize++] = (uint8_t) smlSerial.read();
                lastReadTime = millis();

                if (smlBufferSize >= SML_MSG_BUFFER_SIZE) {
                    smlSerial.stopListening();
                    DPRINTLN(F("SML: Buffer overflow"));

                    return;
                }
            }
        }

        if (smlSerial.overflow()) {
            smlSerial.stopListening();
            DPRINTLN(F("SML: Serial overflow"));
        } else {
            smlSerial.stopListening();

            if (isValidSMLHeader()) {
                int64_t counter = parseMeterReading();
                if (counter >= 0) {
                    sdev.channel(1).setCounter(counter * 10);
                }

                int32_t power = parsePowerReading();
                if (power >= 0) {
                    sdev.channel(1).setPower(power * 100);
                }
            } else {
                DPRINTLN(F("SML: Invalid header"));
            }
        }
    }
}
