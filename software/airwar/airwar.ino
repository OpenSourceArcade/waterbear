#include <avr/io.h>
#include <avr/pgmspace.h>

#define I2C_SDA PB0 // serial data pin
#define I2C_SCL PB2 // serial clock pin

#define I2C_SDA_HIGH() DDRB &= ~(1 << I2C_SDA) // release SDA   -> pulled HIGH by resistor
#define I2C_SDA_LOW() DDRB |= (1 << I2C_SDA)   // SDA as output -> pulled LOW  by MCU
#define I2C_SCL_HIGH() DDRB &= ~(1 << I2C_SCL) // release SCL   -> pulled HIGH by resistor
#define I2C_SCL_LOW() DDRB |= (1 << I2C_SCL)   // SCL as output -> pulled LOW  by MCU

// I2C transmit one data byte to the slave, ignore ACK bit, no clock stretching allowed
void I2C_write(uint8_t data)
{
  for (uint8_t i = 8; i; i--)
  {                // transmit 8 bits, MSB first
    I2C_SDA_LOW(); // SDA LOW for now (saves some flash this way)
    if (data & 0x80)
      I2C_SDA_HIGH(); // SDA HIGH if bit is 1
    I2C_SCL_HIGH();   // clock HIGH -> slave reads the bit
    data <<= 1;       // shift left data byte, acts also as a delay
    I2C_SCL_LOW();    // clock LOW again
  }
  I2C_SDA_HIGH(); // release SDA for ACK bit of slave
  I2C_SCL_HIGH(); // 9th clock pulse is for the ACK bit
  asm("nop");     // ACK bit is ignored, just a delay
  I2C_SCL_LOW();  // clock LOW again
}

// I2C start transmission
void I2C_start(uint8_t addr)
{
  I2C_SDA_LOW();   // start condition: SDA goes LOW first
  I2C_SCL_LOW();   // start condition: SCL goes LOW second
  I2C_write(addr); // send slave address
}

// I2C stop transmission
void I2C_stop(void)
{
  I2C_SDA_LOW();  // prepare SDA for LOW to HIGH transition
  I2C_SCL_HIGH(); // stop condition: SCL goes HIGH first
  I2C_SDA_HIGH(); // stop condition: SDA goes HIGH second
}

// ===================================================================================
// OLED Implementation
// ===================================================================================

// OLED definitions
#define OLED_ADDR 0x78     // OLED write address
#define OLED_CMD_MODE 0x00 // set command mode
#define OLED_DAT_MODE 0x40 // set data mode
#define OLED_INIT_LEN 14   // 12: no screen flip, 14: screen flip

// OLED init settings
const uint8_t OLED_INIT_CMD[] PROGMEM = {
    0xA8, 0x1F,       // set multiplex (HEIGHT-1): 0x1F for 128x32, 0x3F for 128x64
    0x22, 0x00, 0x03, // set min and max page
    0x20, 0x00,       // set vertical memory addressing mode
    0xDA, 0x02,       // set COM pins hardware configuration to sequential
    0x8D, 0x14,       // enable charge pump
    0xAF,             // switch on OLED
    0xA1, 0xC8        // flip the screen
};

// OLED init function
void OLED_init(void)
{
  I2C_start(OLED_ADDR);     // start transmission to OLED
  I2C_write(OLED_CMD_MODE); // set command mode and send command bytes ...
  for (uint8_t i = 0; i < OLED_INIT_LEN; i++)
    I2C_write(pgm_read_byte(&OLED_INIT_CMD[i]));
  I2C_stop(); // stop transmission
}

const uint8_t imghero[] PROGMEM = {
    0xe0, 0x70, 0xd8, 0x5c, 0xf8, 0x5c, 0xd8, 0x70, 0xe0};
const uint8_t imgfire[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00};
const uint8_t imgenemy[] PROGMEM = {
    0x70, 0x18, 0x7D, 0xB2, 0xBC, 0x3C, 0xBC, 0xB2, 0x7D, 0x18, 0x70};

const uint8_t OLED_FONT[] PROGMEM = {
    0x3E, 0x41, 0x49, 0x49, 0x7A, // G
    0x7C, 0x12, 0x11, 0x12, 0x7C, // A
    0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    //  0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    //  0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    //  0x42, 0x61, 0x51, 0x49, 0x46, // 2
    //  0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    //  0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    //  0x27, 0x45, 0x45, 0x45, 0x39, // 5
    //  0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    //  0x01, 0x71, 0x09, 0x05, 0x03, // 7
    //  0x36, 0x49, 0x49, 0x49, 0x36, // 8
    //  0x06, 0x49, 0x49, 0x29, 0x1E, // 9

};

struct sprite
{
  uint8_t x;
  uint8_t y;
  uint8_t w;
  uint8_t h;
  uint8_t life;
};

// OLED set the cursor
void OLED_cursor(uint8_t xpos, uint8_t ypos)
{
  I2C_start(OLED_ADDR);            // start transmission to OLED
  I2C_write(OLED_CMD_MODE);        // set command mode
  I2C_write(xpos & 0x0F);          // set low nibble of start column
  I2C_write(0x10 | (xpos >> 4));   // set high nibble of start column
  I2C_write(0xB0 | (ypos & 0x07)); // set start page
  I2C_stop();                      // stop transmission
}

// OLED clear screen
void OLED_clear(void)
{
  OLED_cursor(0, 0);        // set cursor at upper left corner
  I2C_start(OLED_ADDR);     // start transmission to OLED
  I2C_write(OLED_DAT_MODE); // set data mode
  for (uint16_t i = 512; i; i--)
    I2C_write(0x00); // clear the screen
  I2C_stop();        // stop transmission
}

void drawhero(struct sprite who)
{
  uint8_t x = who.x;
  uint8_t y = who.y;
  OLED_cursor(x, 3);
  I2C_start(OLED_ADDR);
  I2C_write(OLED_DAT_MODE);
  for (uint8_t i = 0; i < who.w; i++)
  {
    I2C_write(pgm_read_byte(&imghero[i]));
  }
  I2C_stop();
}

void drawfire(struct sprite who)
{
  uint8_t x = who.x;
  uint8_t y = who.y;
  OLED_cursor(x, y);
  I2C_start(OLED_ADDR);
  I2C_write(OLED_DAT_MODE);
  for (uint8_t i = 0; i < who.w; i++)
  {
    I2C_write(pgm_read_byte(&imgfire[i]));
  }
  I2C_stop();
}

void drawenemy(struct sprite who)
{
  uint8_t x = who.x;
  uint8_t y = who.y;
  //uint8_t page = y / 8;
  OLED_cursor(x, y);
  I2C_start(OLED_ADDR);
  I2C_write(OLED_DAT_MODE);
  for (uint8_t i = 0; i < who.w; i++)
  {
    uint32_t ch = pgm_read_byte(&imgenemy[i]);
    I2C_write(ch);
  }
  I2C_stop();
}

// OLED print a string from program memory
void OLED_gameover()
{
  I2C_start(OLED_ADDR);     // start transmission to OLED
  I2C_write(OLED_DAT_MODE); // read first character from program memory
  for (uint8_t i = 0; i < 40; i++)
  {
    if (i % 5 == 0)
    {
      I2C_write(0x00);
      I2C_write(0x00);
      I2C_write(0x00);
    }
    I2C_write(pgm_read_byte(&OLED_FONT[i]));
  }
  I2C_stop(); // stop transmission
}

const char text[] PROGMEM = "GAME OVER";

int main(void)
{
  DDRB &= ~(1 << PB4); //set PB4 input (leftBtn)
  DDRB &= ~(1 << PB3); //set PB3 input (rightBtn)

  PORTB |= 1 << PB4; //pull up PB4
  PORTB |= 1 << PB3; //pull up PB3

  OLED_init(); // initialize the OLED
  OLED_clear();

  struct sprite hero = {
      60, 3, 9, 8, 1};

  struct sprite fire = {
      60, 3, 9, 8, 0};

  struct sprite enemys[5];
  for (uint8_t i = 0; i < 4; i++)
  {
    enemys[i] = {i * 32, 0, 11, 8, 1};
  }

  uint8_t t = 0;

  uint8_t dir = 0;

  while (1)
  {
    OLED_clear();
    drawhero(hero);
    if (fire.life > 0)
    {
      //drawfire(fire);
    }
    if (hero.life < 1)
    {
      OLED_cursor(20, 1);
      OLED_gameover();
      if ((PINB & (1 << PB3)) == 0)
      {
        for (uint8_t i = 0; i < 4; i++)
        {
          enemys[i].y = 0;
        }
        hero.life++;
      }
      //return 0;
    }

    if (hero.life > 0)
    {
      t++;
      if (t % 30 == 0)
      {
        for (uint8_t i = 0; i < 4; i++)
        {
          if (enemys[i].life > 0)
          {
            enemys[i].y++;
            if (enemys[i].y == 4)
            {
              enemys[i].y = -2;
            }
          }
        }
      }

      if (t % 10 == 0)
      {
        if (fire.life > 0)
        {
          fire.y--;
        }
      }
      for (uint8_t i = 0; i < 5; i++)
      {
        if (enemys[i].life > 0)
        {
          drawenemy(enemys[i]);
          //              if(enemys[i].y==fire.y){
          //                if(fire.x-enemys[i].x<8 || enemys[i].x-fire.x<8){
          //                    fire.life--;
          //                    enemys[i].life--;
          //                    break;
          //                 }
          //              }
          if (enemys[i].y == 3 && enemys[i].life > 0)
          {
            if (hero.x > enemys[i].x && hero.x < enemys[i].x + enemys[i].w)
            {
              hero.life--;
              break;
            }
            if (hero.x + hero.w > enemys[i].x && hero.x + hero.w < enemys[i].x + enemys[i].w)
            {
              hero.life--;
              break;
            }
          }
        }
      }

      if (fire.y == 0)
      {
        fire.life = 0;
      }

      if ((PINB & (1 << PB4)) == 0)
      {
        if (dir == 0)
        {
          if (hero.x - 5 > 0)
          {
            hero.x -= 5;
          }
          else
          {
            dir = 1;
          }
        }
        if (dir == 1)
        {
          if (hero.x + 5 < 127 - 6)
          {
            hero.x += 5;
          }
          else
          {
            dir = 0;
          }
        }
      }

      if ((PINB & (1 << PB3)) == 0)
      {
        if (fire.life == 0)
        {
          fire.x = hero.x;
          fire.y = 3;
          fire.life = 1;
        }
      }
    }
    //_delay_ms(10);
  }
}