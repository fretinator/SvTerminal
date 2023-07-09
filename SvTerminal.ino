#include <WiFi.h>
#include <WiFiClient.h>
#include <M5Stack.h>
#include "secrets.h"

#define KEYBOARD_I2C_ADDR 0X08
#define KEYBOARD_INT      5
uint8_t key_read = 0;
uint8_t byte_read = 0;




#define LINE_CHAR_LENGTTH 50

#define NUM_LINES 20
#define LEFT_PAD 4

// calculate later
int16_t screen_w = 0;
int16_t screen_h = 0;
int text_height = 0;
int text_width = 0; 

enum VK_KEYS {
  VK_BACKSPACE = 8,
  VK_TAB = 9,
  VK_LF = 10,
  VK_CR = 13,
  VK_ESCAPE = 27,
  VK_DELETE = 127,
};

#define ESC_FINAL_BYTE_MIN 0x40 
#define ESC_FINAL_BYTE_MAX 0x7E
#define MAX_ESC_SEQ_LEN 32 // For safety
const char esc_start_char = '[';
const String clear_screen = "H";


const char* ESC_COLOR_IDENTIFIER = "m";
const uint8_t  ESC_RESET = 0;
const uint8_t  ESC_BLACK = 30;
const uint8_t  ESC_RED = 31;
const uint8_t  ESC_GREEN = 32;
const uint8_t  ESC_YELLOW = 33;
const uint8_t  ESC_BLUE = 34;
const uint8_t  ESC_MAGENTA = 35;
const uint8_t  ESC_CYAN = 36;
const uint8_t  ESC_WHITE = 37;
const uint8_t  ESC_DEFAULT = 39;


uint8_t current_color = ESC_WHITE;

const uint16_t COLOR_MASK = 0b1111111100000000;
const uint16_t CHAR_MASK =  0b0000000011111111;




// + 1 for null character
char esc_sequence[MAX_ESC_SEQ_LEN + 1]; 

#define MAX_GET_NEXT_KEY_ATTEMPTS 5
#define GET_NEXT_KEY_DELAY 100

struct char_line {
  uint16_t chars[LINE_CHAR_LENGTTH + 1];
  char_line* prev;
  char_line* next;
};

char_line* root_line = NULL;
char_line* current_line = NULL;
uint8_t cur_char = 0;
uint8_t last_key = 0;

WiFiClient netClient;
#define MAX_WIFI_ATTEPTS 20
const int net_retries = 3;

uint16_t combineCharAndColor( char c, uint8_t color) {
  return 256 * color + c;
}

char getCharFromInt(uint16_t val) {
  return char(val & CHAR_MASK);
}

uint8_t getColorFromInt(uint16_t val) {
  return (val & COLOR_MASK) / 256;
}


void initEscSequence() {
  for(int x = 0; 0 < MAX_ESC_SEQ_LEN;x++) {
    esc_sequence[x] = '\0';
  }
  esc_sequence[MAX_ESC_SEQ_LEN] = '\0';
}


bool setupWiFi() {
  uint8_t curAttempts = 0;
   
  WiFi.begin(my_ssid, my_password);
  
  while (WiFi.status() != WL_CONNECTED &&
      curAttempts < MAX_WIFI_ATTEPTS) {
    Serial.print(WiFi.status());
    delay(500);
    curAttempts++;
    //Serial.print(".");
    }

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed.");
    return false;
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); 
  return true; 
}

bool connectToHost() {
  IPAddress addr;
  addr.fromString(host);
  return netClient.connect(addr, port);
}

char_line* createLine() {
  char_line* the_line = new char_line();
  the_line->prev = NULL;
  the_line->next = NULL;
  for(int x = 0; x < LINE_CHAR_LENGTTH;x++) {
    the_line->chars[x] = combineCharAndColor(' ', current_color);
  }
  the_line->chars[LINE_CHAR_LENGTTH] = combineCharAndColor('\0', current_color);
  return the_line;
}

char_line* getLastLine() {
  char_line* my_line = root_line;
  while(NULL != my_line->next) {
    my_line = my_line->next;
  }

  return my_line;
}

void sendByte(uint8_t byte) {
  netClient.write(byte);
}

uint8_t getByte() {
  if(netClient.available()) {
    return uint8_t(netClient.read());
  }

  return 0;
}

uint8_t getNumberOfLines() {
  char_line* my_line = root_line;
  uint8_t line_count = 1; // Root is one
  while(NULL != my_line->next) {
    my_line = my_line->next;
    line_count++;
  }

  return line_count;
}

void setup() {
  Serial.begin(115200);

  M5.begin();
  M5.Power.begin();
  // correctly
  M5.Lcd.fillScreen(TFT_BLACK);

  if(!setupWiFi()) {
    M5.lcd.println("Unable to connect to WiFi");
    delay(100);
    while(true);
  }

  if(!connectToHost()) {
    M5.lcd.println("Unable to connect to Host");
    delay(100);
    while(true);
  }

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.drawCentreString("Net terminal ", 320 / 2, 0, 2);


  delay(3000);

  M5.lcd.clear();
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  screen_w = M5.lcd.width();
  screen_h = M5.lcd.height();

  text_width = M5.lcd.textWidth("W");
  //text_width -= 2;
  Serial.print("Text Width: ");
  Serial.println(text_width);

  text_height = screen_h / NUM_LINES;

  // Change colour for scrolling zone text
  root_line = createLine();
  current_line = root_line;
  drawCursor();
}

bool getKeyboardInput(uint8_t& key) {
  if (digitalRead(KEYBOARD_INT) == LOW) {
    Wire.requestFrom(KEYBOARD_I2C_ADDR, 1);  // request 1 byte from keyboard
    if (Wire.available()) {
        uint8_t key_val = Wire.read();  // receive a byte as character
        //Serial.print("Key read: ");
        //+Serial.println(key_val);
        if(key_val > 0) {
          key = key_val;
          return true;
        }       
    }
  }

  return false;
}

// Will try to get next key for sequence with retries
uint8_t getNextByte() {
  uint8_t cur_attempts = 0;
  bool byte_retrieved = false;
  uint8_t the_byte = 0;

  while(!byte_retrieved && cur_attempts < MAX_GET_NEXT_KEY_ATTEMPTS) {
    cur_attempts++;
    the_byte = getByte();

    byte_retrieved = the_byte > 0;

    if(!the_byte) {
      delay(GET_NEXT_KEY_DELAY);
    }
  }
  
  return the_byte;
}

// Lines are 1-based
uint8_t getCurrentLineY() {
  return (getCurrentLineNumber() - 1) * text_height;
}

// 1-based
uint8_t getCurrentLineNumber() {
  uint8_t cur_line_num = 1; // Root line
  char_line* test_line = root_line;

  while(current_line != test_line) {
    cur_line_num++;
    test_line = test_line->next; // live dangerously!
  }

  return cur_line_num;
}

// Characters are 0-based
uint8_t getCurrentCharX() {
  return LEFT_PAD + (cur_char * text_width);
}


void drawCursor() {
  uint8_t y = getCurrentLineY();
  uint8_t x = getCurrentCharX();
  M5.lcd.drawRect(x, y, text_width, text_height, TFT_BLUE);
}

void eraseCursor() {
  uint8_t y = getCurrentLineY();
  uint8_t x = getCurrentCharX();
  M5.lcd.drawRect(x, y, text_width, text_height, TFT_BLACK);
}



void printCurrentChar() {
  uint16_t int_val =  current_line->chars[cur_char];
  char the_char = getCharFromInt(int_val);
  uint8_t color = getColorFromInt(int_val);
  uint8_t ypos = getCurrentLineY();
  uint8_t xpos = getCurrentCharX();

  M5.lcd.fillRect(xpos, ypos, text_width, text_height, TFT_BLACK);
  M5.lcd.setCursor(xpos, ypos);
  
  if(current_color != color) {
    setTextColor(color);
    current_color = color;
  }
  
  M5.lcd.print(the_char);
}

void handleScroll() {
  if(getNumberOfLines() > NUM_LINES) {
     char_line* new_root = root_line->next;
     delete root_line;
     root_line = new_root;
     if(NULL != root_line->next) {
       root_line->next->prev = root_line;
     }
  }
  eraseCursor();
  printLines();
  //current_line = getLastLine();
  //cur_char = 0;
}

char_line* insertNewLine() {
  char_line* prior_next_line = current_line->next;
  char_line* new_line = createLine();
  
  current_line->next = new_line;
  new_line->prev = current_line;
  new_line->next = prior_next_line;
  return new_line;
}

void debugPrint(uint8_t key) {
  //Serial.println("Printing character:");
  //Serial.print("\tByte Value: ");
  //Serial.println(key);
  //Serial.print("\tChar Value:");
  //Serial.println(char(key));
}

void handlePrintingChar(uint8_t key) {
  debugPrint(key);
  eraseCursor();
  if(cur_char > LINE_CHAR_LENGTTH - 1) {
    current_line = insertNewLine();
    cur_char = 0;

    if(getNumberOfLines() > NUM_LINES) {
      handleScroll();
    } 
  }

  current_line->chars[cur_char] = combineCharAndColor(char(key), current_color);
  printCurrentChar();
  cur_char++;
  drawCursor();
}

void handleReturnKey() {
  char_line* new_line = createLine();
  char_line* next_line = current_line->next;

  eraseCursor();
  current_line->next = new_line;
  new_line->prev = current_line;

  if(NULL != next_line) {
    new_line->next = next_line;
    next_line->prev = new_line;
  }

  current_line = new_line;
  cur_char = 0;

  if(getNumberOfLines() > NUM_LINES) {
    handleScroll();
  }  else {
    printCurrentChar();
  }
  drawCursor();
}

void handleBackspace() {
  if(cur_char > 0) {
    eraseCursor();
    cur_char--;
    current_line->chars[cur_char] = ' ';
    printCurrentChar();
    drawCursor();
  }
}

bool isESCFinalByte(uint8_t key) {
  return key >= ESC_FINAL_BYTE_MIN && key <= ESC_FINAL_BYTE_MAX;
}

void handleEscapeSequence() {
  Serial.println("\t[ESC} Entering Escape Sequence");
  // Next character must be '{' or we are not Esc Sequence}
  uint8_t the_byte = 0;
  bool endSeqFound = false;
  uint8_t seq_bytes_read = 0;
  bool continueReading = true;

  the_byte = getNextByte();
  if(!the_byte > 0) {
    Serial.println("\t[ESC] Unable to retrieve next byte");
    return;
  } 

  if(esc_start_char != char(the_byte)) {
    Serial.print("\t[ESC] Next byte after escape is NOT '[',  ");
    Serial.print("Escape char read:  byte: ");
    Serial.print(the_byte);
    Serial.print(", char: ");
    Serial.println(char(the_byte));
    handleKey(the_byte);
    return;
  } else {
    Serial.print("\t[ESC] Escape char read:  byte: ");
    Serial.print(the_byte);
    Serial.print(", char: ");
    Serial.println(char(the_byte));
  }

  // read until we read final byte character
  while(continueReading && seq_bytes_read < MAX_ESC_SEQ_LEN &&
    (the_byte = getNextByte()) > 0 ){
      esc_sequence[seq_bytes_read] = char(the_byte);
      seq_bytes_read++;
      continueReading = !isESCFinalByte(the_byte);
  }

  esc_sequence[seq_bytes_read] = '\0';

  Serial.print("\tEscape sequence: ");
  Serial.print("ESC[");
  Serial.println(esc_sequence);
  processEscapeSequence(esc_sequence);
}

void clearScreen() {
  char_line* delete_line = root_line;
  char_line* next_line = root_line->next;

  delete root_line;

  while(NULL != next_line) {
    delete_line = next_line;
    next_line = next_line->next;
    delete delete_line;
  }

  cur_char = 0;
  root_line = createLine();
  current_line = root_line;
  m5.lcd.clear();
  drawCursor();
}

bool isColorCommand(String sequence) {
  return sequence.endsWith(ESC_COLOR_IDENTIFIER);
}

uint8_t setTextColor(uint8_t color) {
  switch(color) {
      case ESC_RESET:
        M5.lcd.setTextColor(TFT_WHITE);
        return ESC_RESET;
      case ESC_BLACK:
        M5.lcd.setTextColor(TFT_BLACK);
        return ESC_BLACK;
      case ESC_RED:
        M5.lcd.setTextColor(TFT_RED);
        return ESC_RED;
      case ESC_GREEN:
        M5.lcd.setTextColor(TFT_GREEN);
        return ESC_GREEN;
      case ESC_YELLOW:
        M5.lcd.setTextColor(TFT_YELLOW);
        return ESC_YELLOW;
      case ESC_BLUE:
        M5.lcd.setTextColor(TFT_BLUE);
        return ESC_BLUE;
      case ESC_MAGENTA:
        M5.lcd.setTextColor(TFT_MAGENTA);
        return ESC_MAGENTA;
      case ESC_CYAN:
        M5.lcd.setTextColor(TFT_CYAN);
        return ESC_CYAN;
      case ESC_WHITE:
        M5.lcd.setTextColor(TFT_WHITE);
        return ESC_WHITE;
      case ESC_DEFAULT:
        M5.lcd.setTextColor(TFT_WHITE);
        return ESC_DEFAULT;
      default:
         M5.lcd.setTextColor(TFT_WHITE);
        return ESC_WHITE;
    }
}

void handleColorCommand(String sequence) {
  Serial.print("***** Handling Color Command: " + sequence);
  int char_pos = sequence.indexOf(';');

  if(char_pos > 0) {
    // Should be something like 01;34m
    String my_color = sequence.substring(char_pos + 1, sequence.length() - 1);
    uint8_t esc_color = my_color.toInt();
    Serial.print(", COLOR = ");
    Serial.println(esc_color);
    
   
    current_color =  setTextColor(esc_color);
  } else {
    Serial.println(", RESET COLOR");
    M5.lcd.setTextColor(TFT_WHITE);
    current_color = ESC_WHITE;
  }
}

void processEscapeSequence(String sequence) {
  if(clear_screen == sequence) {
    clearScreen();
  } else {
    if(isColorCommand(sequence)) {
      handleColorCommand(sequence);
    }
  }
}

void handleControlChar(uint8_t key) {
  //("Handling control character: ");
  //Serial.print(key);

  switch(key) {
    case VK_ESCAPE:
      //Serial.println(" - Enter escape handler");
      handleEscapeSequence();
      break;
    case VK_BACKSPACE:
      //Serial.println(" - Handled");
      handleBackspace();
      break;
    //case VK_CR:
    case VK_LF:
      //Serial.println(" - Handled");
      handleReturnKey();
      break;
    default:
      Serial.println(" - Unhandled");
  }
}

bool isControlChar(uint8_t key) {
  return key < 32 || VK_DELETE == key;
}

void handleKey(uint8_t key) {
  Serial.print(key);
  Serial.print(",");
  Serial.println(char(key));

  if(isControlChar(key)) {
    handleControlChar(key);
  } else {
    handlePrintingChar(key);
  }

  last_key = key;
}

void printLine(char_line* line, uint8_t lineY) {
  int which_char = 0;
  uint16_t the_val;
  char c;
  uint8_t char_color;
  uint8_t last_color = ESC_WHITE;
  bool is_null_char = false;

  while(which_char <  LINE_CHAR_LENGTTH && !is_null_char) {
    the_val = line->chars[which_char];
    c = getCharFromInt(the_val);
    char_color = getColorFromInt(the_val);
    if(last_color != char_color) {
      setTextColor(char_color);
      last_color = char_color;

    }

    if('\0' == c) {
      is_null_char = true;
    } else {
      M5.lcd.setCursor(which_char * text_width, lineY);
      M5.Lcd.print(c);
      which_char++;      
    }
  }
}

void printLines() {
  M5.Lcd.clear();

  uint8_t lineY = 0;
  uint8_t lineX = LEFT_PAD;

  printLine(root_line, lineY);
  //M5.lcd.setCursor(lineX, lineY);
  //M5.Lcd.println(root_line->chars);

  char_line* next_line = root_line->next;

  while(NULL != next_line) {
    lineY += text_height;
    printLine(next_line, lineY);
    //M5.lcd.setCursor(lineX, lineY);
    //M5.Lcd.println(next_line->chars);
    next_line = next_line->next;
  }
}

void loop() {
  if((byte_read = getByte()) > 0) {
    handleKey(byte_read);
  }
  if(getKeyboardInput(key_read)) {
    sendByte(key_read);
  }
}
