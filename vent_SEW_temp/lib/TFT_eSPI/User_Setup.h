// User_Setup.h - Poenostavljene nastavitve za knjižnico TFT_eSPI za projekt Display-S3_sens
// Ta datoteka prilagodi TFT_eSPI za ESP32-S3 na LILYGO T-Display S3 z zaslonom 320x170 pikslov
// in podporo za dotik CST816 ter standardne vgrajene fonte.

// Osnovne nastavitve za ESP32-S3 in LILYGO T-Display S3
#define ILI9341_DRIVER     // Gonilnik za ILI9341, primeren za LILYGO T-Display S3
#define TFT_WIDTH  170     // Širina zaslona v pikselih (po rotaciji 0)
#define TFT_HEIGHT 320     // Višina zaslona v pikselih (po rotaciji 0)

// Nastavitve pinov za ESP32-S3 na LILYGO T-Display S3 (usklajeno s senzor.cpp)
#define TFT_MOSI  47       // MOSI pin za SPI (D6 v senzor.cpp)
#define TFT_SCLK  14       // SCLK pin za SPI (PIN_BUTTON_2 v senzor.cpp, preveri!)
#define TFT_CS    6        // Pin za izbiro čipa (PIN_LCD_CS v senzor.cpp)
#define TFT_DC    7        // Pin za kontroliranje podatkov/ukazov (PIN_LCD_DC v senzor.cpp)
#define TFT_RST   5        // Pin za reset (PIN_LCD_RES v senzor.cpp)
#define TFT_BL    38       // Pin za osvetlitev ozadja (PIN_LCD_BL v senzor.cpp)
#define TFT_MISO  -1       // MISO pin ni uporabljen (nastavi na -1)
#define TFT_WR    8        // Pin za zapis (PIN_LCD_WR v senzor.cpp)
#define TFT_RD    9        // Pin za branje (PIN_LCD_RD v senzor.cpp)

// Dodatni podatkovni pini za 8-bitno vzporedno komunikacijo (usklajeno s senzor.cpp)
#define TFT_D0    39       // Podatkovni pin D0
#define TFT_D1    40       // Podatkovni pin D1
#define TFT_D2    41       // Podatkovni pin D2
#define TFT_D3    42       // Podatkovni pin D3
#define TFT_D4    45       // Podatkovni pin D4
#define TFT_D5    46       // Podatkovni pin D5
#define TFT_D6    47       // Podatkovni pin D6
#define TFT_D7    48       // Podatkovni pin D7

// Rotacija zaslona
#define TFT_ROTATION 0     // Nastavi pokončno orientacijo (0 stopinj)

// Hitrost SPI komunikacije
#define SPI_FREQUENCY  40000000  // SPI hitrost 40 MHz (prilagodi glede na zaslon)

// Podpora za dotik (CST816 za LILYGO T-Display S3)
#define TOUCH_CST816       // Uporaba CST816 kot dotikalnega vmesnika
#define TOUCH_SDA  18      // I2C SDA pin za dotik (BOARD_I2C_SDA v senzor.cpp)
#define TOUCH_SCL  17      // I2C SCL pin za dotik (BOARD_I2C_SCL v senzor.cpp)
#define TOUCH_RST  21      // Reset pin za dotik (BOARD_TOUCH_RST v senzor.cpp)
#define TOUCH_IRQ  16      // Prekinitev pin za dotik (BOARD_TOUCH_IRQ v senzor.cpp)

// Vključitev standardnih fontov, ki jih uporabljaš v projektu
#define LOAD_FONT2         // Vključi standardni font 2 (majhen font za srednje dele)
#define LOAD_FONT4         // Vključi standardni font 4 (večji font za zgornje dele)

// Izključitev nepotrebnih fontov in nastavitev
#undef LOAD_GLCD          // Izključi privzeti GLCD font (5x7)
#undef LOAD_FONT6         // Izključi nepotrebne fonte (npr. Font6)
#undef LOAD_FONT7         // Izključi Font7srle.h (ni več v uporabi)
#undef LOAD_FONT8         // Izključi nepotrebne fonte (npr. Font8)
#undef LOAD_GFXFF         // Izključi podporo za Adafruit_GFX free fonte (ni potrebno)
#undef SMOOTH_FONT        // Izključi podporo za gladke fonte (ni potrebno za tvoj projekt)

// Barvna globina in optimizacije
#define TFT_RGB_ORDER TFT_RGB  // Nastavi RGB zaporedje za barve
#define SPI_READ_FREQUENCY 20000000  // Zmanjšaj hitrost branja SPI za stabilnost

// Vključitev 8-bitne vzporedne komunikacije (glede na pine v senzor.cpp)
#define TFT_PARALLEL_8_BIT    // Omogoči 8-bitno vzporedno komunikacijo

// Izključitev naprednih funkcij, ki jih ne uporabljaš
#undef SUPPORT_TRANSACTIONS  // Izključi podporo za SPI transakcije (ni potrebno pri vzporedni povezavi)