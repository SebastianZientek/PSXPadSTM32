#include <SPI.h>
#include <Wire.h>

#include <cstdint>

#define I2C_RPI_SLAVE_ADDR 0x6A
#define ACK PA3
#define ATT PA4
#define LED_PIN PC13

uint8_t cmd[9];
uint8_t data[6] = {0xFF, 0xFF, 0x08, 0x08, 0x08, 0x08};
uint8_t smallMotorForce = 0;
uint8_t largeMotorForce = 0;

bool configMode = false;
bool analogMode = false;

uint8_t buttons[6] = {0xFF, 0xFF, 0x7F, 0x7F, 0x7F, 0x7F};
uint8_t buttonsIndex = 0;


void setup()
{
    Serial1.begin(115200);
    Serial1.println("STARTED");

    Wire.begin();
    delay(100);
    // pinMode(ACK, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, analogMode ? LOW : HIGH);

    // SPI.beginTransactionSlave(SPISettings(50000000, LSBFIRST, SPI_MODE3, DATA_SIZE_8BIT));
}

inline static int8_t ack()
{
    // pinMode(ACK, OUTPUT);
    // digitalWrite(ACK, HIGH);
    delayMicroseconds(5);
    digitalWrite(ACK, LOW);
    delayMicroseconds(3);
    digitalWrite(ACK, HIGH);
    // pinMode(ACK, INPUT);
    return 0;
}

bool knownCommand(const uint8_t &cmd)
{
    return (cmd == 0x42) || (cmd == 0x43) || (cmd == 0x44) || (cmd == 0x45) || (cmd == 0x46) ||
           (cmd == 0x47) || (cmd == 0x4C) || (cmd == 0x4D);
}

void exchangeCmdData(uint8_t &cmd, const uint8_t &data)
{
    cmd = SPI.transfer(data);
}

void loop()
{
    delay(14);
    Wire.beginTransmission(I2C_RPI_SLAVE_ADDR);
    uint8_t i2cData[3] = {'r', smallMotorForce, largeMotorForce};
    Wire.write(i2cData, 3);
    Wire.endTransmission();

    Wire.requestFrom(I2C_RPI_SLAVE_ADDR, 6);
    unsigned i = 0;
    while (Wire.available())
    {
        char c = Wire.read();
        buttons[i++] = (uint8_t)c;
    }

    static bool analogChanged = false;
    if (buttons[0] == 0xFC)  // SELECT + L3
    {
        if (!analogChanged)
        {
            analogMode = !analogMode;
            analogChanged = true;

            digitalWrite(LED_PIN, analogMode ? LOW : HIGH);
        }
    }
    else
    {
        analogChanged = false;
    }

    uint8_t numButtonsToSend = analogMode ? 6 : 2;
    pinMode(ACK, OUTPUT);
    SPI.beginTransactionSlave(SPISettings(50000000, LSBFIRST, SPI_MODE3, DATA_SIZE_8BIT));

    exchangeCmdData(cmd[0], 0xFF);
    if (cmd[0] != 0x01) goto TERMINATE_LOOP;
    ack();

    if (configMode)
    {
        exchangeCmdData(cmd[1], 0xF3);
    }
    else if (analogMode)
    {
        exchangeCmdData(cmd[1], 0x73);
    }
    else if (!analogMode)
    {
        exchangeCmdData(cmd[1], 0x41);
    }

    if (!knownCommand(cmd[1]))
    {
        goto TERMINATE_LOOP;
    }
    ack();

    exchangeCmdData(cmd[2], 0x5A);

    switch (cmd[1])
    {
    case 0x43:
    case 0x42:
    {
        for (int i = 0; i < numButtonsToSend; ++i)
        {
            ack();
            exchangeCmdData(cmd[3 + i], buttons[i]);
        }
        if (cmd[1] == 0x42)
        {
            smallMotorForce = cmd[3];
            largeMotorForce = cmd[4];
        }

        configMode = (cmd[3] == 0x01) && (cmd[1] == 0x43);
    }
    break;
    case 0x45:
    {
        uint8_t data[] = {0x01, 0x02, 0x00, 0x02, 0x01, 0x00};

        for (const uint8_t &d : data)
        {
            ack();
            exchangeCmdData(cmd[3], d);
        }
    }
    break;
    case 0x47:
    {
        uint8_t data[] = {0x00, 0x00, 0x02, 0x00, 0x01, 0x00};

        for (const uint8_t &d : data)
        {
            ack();
            exchangeCmdData(cmd[3], d);
        }
    }
    break;
    case 0x4C:
    {
        uint8_t dataFirst[] = {0x00, 0x00, 0x04, 0x00, 0x00};
        uint8_t dataSecond[] = {0x00, 0x00, 0x07, 0x00, 0x00};

        ack();
        exchangeCmdData(cmd[3], 0x00);

        if (cmd[3] == 0x00)
        {
            for (int i = 0; i < 5; ++i)
            {
                ack();
                exchangeCmdData(cmd[3], dataFirst[i]);
            }
        }
        else if (cmd[3] == 0x01)
        {
            for (int i = 0; i < 5; ++i)
            {
                ack();
                exchangeCmdData(cmd[3], dataSecond[i]);
            }
        }
    }
    break;
    case 0x46:
    {
        uint8_t dataFirst[] = {0x00, 0x01, 0x02, 0x00, 0x0a};
        uint8_t dataSecond[] = {0x00, 0x01, 0x01, 0x01, 0x14};

        ack();
        exchangeCmdData(cmd[3], 0x00);

        if (cmd[3] == 0x00)
        {
            for (int i = 0; i < 5; ++i)
            {
                ack();
                exchangeCmdData(cmd[3], dataFirst[i]);
            }
        }
        else if (cmd[3] == 0x01)
        {
            for (int i = 0; i < 5; ++i)
            {
                ack();
                exchangeCmdData(cmd[3], dataSecond[i]);
            }
        }
    }
    break;
    case 0x4D:
        for (int i = 0; i < 6; ++i) // always 6 bytes
        {
            ack();
            exchangeCmdData(cmd[3 + i], 0xFF);
        }
        Serial1.println("Map vibration motors");
    break;
    }

TERMINATE_LOOP:
    pinMode(ACK, INPUT);
    SPI.endTransaction();
    SPI.end();
}
