#include <WiFi.h> //Connect to WiFi Network
#include <SPI.h>
#include <TFT_eSPI.h>
#include <mpu6050_esp32.h>
#include<math.h>
#include<string.h>

//DECLARATIONS:

TFT_eSPI tft = TFT_eSPI();
const int SCREEN_HEIGHT = 160;
const int SCREEN_WIDTH = 128;
const int LOOP_PERIOD = 40;

MPU6050 imu; //imu object called, appropriately, imu

char network[] = "MIT";  //SSID for 6.08 Lab`
char password[] = ""; //Password for 6.08 Lab
int song_id = 0;
int setVal = 0; //temp counter for entering song numbers
char server[] = "iesc-s3.mit.edu";

//Some constants and some resources:
const int RESPONSE_TIMEOUT = 6000; //ms to wait for response from host
const uint16_t IN_BUFFER_SIZE = 3500; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
char old_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request
const uint16_t JSON_BODY_SIZE = 3000;
char request[IN_BUFFER_SIZE];
char json_body[JSON_BODY_SIZE];
char old_post_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request
char post_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request

uint32_t primary_timer;

int old_val;

const uint32_t READING_PERIOD = 150; //milliseconds
uint32_t last_read = 0;
double MULT = 1.059463094359; //12th root of 2 (precalculated) for note generation
double A_1 = 55; //A_1 55 Hz  for note generation
const uint8_t NOTE_COUNT = 97; //number of notes set at six octaves from
double value;
float acc = 0; //acceleration reading

//buttons for today 
uint8_t BUTTON1 = 45; // toggle mode : playback , record
uint8_t BUTTON2 = 39; //add silence: long press
uint8_t BUTTON3 = 38; //long press = pause, new note = increment by 1
uint8_t BUTTON4 = 34;

int mode = 0;
uint32_t last_entry_time = 0;
uint16_t entry_timeout = 1500;
uint32_t last_set_time = 0; //POST mode: setting update type
uint16_t set_timeout = 2000;

/* CONSTANTS */
//Prefix to POST request:
const char PREFIX[] = "{\"artist\": "; //beginning of json body
const char SUFFIX[] = "\"}"; //suffix to POST request
const char user[] = "\"frogDuino\""; //user for POST

//pins for LCD and AUDIO CONTROL
uint8_t LCD_CONTROL = 21;
uint8_t AUDIO_TRANSDUCER = 14;

//PWM Channels. The LCD will still be controlled by channel 0, we'll use channel 1 for audio generation
uint8_t LCD_PWM = 0;
uint8_t AUDIO_PWM = 1;

//arrays you need to prepopulate for use in the run_instrument() function
double note_freqs[NOTE_COUNT];
float accel_thresholds[NOTE_COUNT + 1];

//global variables to help your code remember what the last note was to prevent double-playing a note which can cause audible clicking
float new_note = 0;
float old_note = 0;
float duration;

//global variables
char curr_artist[100];
char curr_duration[100];
char curr_note[100];
float post_duration;
double post_notes[300];
int note_index = 0;
int update_state = 0;
int query_string_length = 0;

//enum for button states
enum button_state {S0, S1, S2, S3, S4};


//HELPER FUNCTIONS
/*----------------------------------
  char_append Function:
  Arguments:
     char* buff: pointer to character array which we will append a
     char c:
     uint16_t buff_size: size of buffer buff

  Return value:
     boolean: True if character appended, False if not appended (indicating buffer full)
*/
uint8_t char_append(char* buff, char c, uint16_t buff_size) {
  int len = strlen(buff);
  if (len > buff_size) return false;
  buff[len] = c;
  buff[len + 1] = '\0';
  return true;
}

//HELPER FUNCTIONS

/*----------------------------------
   do_http_request Function:
   Arguments:
      char* host: null-terminated char-array containing host to connect to
      char* request: null-terminated char-arry containing properly formatted HTTP request
      char* response: char-array used as output for function to contain response
      uint16_t response_size: size of response buffer (in bytes)
      uint16_t response_timeout: duration we'll wait (in ms) for a response from server
      uint8_t serial: used for printing debug information to terminal (true prints, false doesn't)
   Return value:
      void (none)
*/
void do_http_request(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial) {
  WiFiClient client; //instantiate a client object
  if (client.connect(host, 80)) { //try to connect to host on port 80
    if (serial) Serial.print(request);//Can do one-line if statements in C without curly braces
    client.print(request);
    memset(response, 0, response_size); //Null out (0 is the value of the null terminator '\0') entire buffer
    uint32_t count = millis();
    while (client.connected()) { //while we remain connected read out data coming back
      client.readBytesUntil('\n', response, response_size);
      if (serial) Serial.println(response);
      if (strcmp(response, "\r") == 0) { //found a blank line!
        break;
      }
      memset(response, 0, response_size);
      if (millis() - count > response_timeout) break;
    }
    memset(response, 0, response_size);
    count = millis();
    while (client.available()) { //read out remaining text (body of response)
      char_append(response, client.read(), OUT_BUFFER_SIZE);
    }
    if (serial) Serial.println(response);
    client.stop();
    if (serial) Serial.println("-----------");
  } else {
    if (serial) Serial.println("connection failed :/");
    if (serial) Serial.println("wait 0.5 sec...");
    client.stop();
  }
}

//used to get x,y values from IMU accelerometer!
void get_angle(float* x, float* y) {
  imu.readAccelData(imu.accelCount);
  *x = imu.accelCount[0] * imu.aRes;
  *y = imu.accelCount[1] * imu.aRes;
}

void postSong(char* songValues, char* response, int response_size) {
  //HTTP response string parsing
  char * p;
  int i = 0;
  p = strtok (songValues,",");
  post_duration = atof(p);
  while (p != NULL)
  {
    if (i >= 1) {
      post_notes[i-1] = atof(p);
    }
    p = strtok (NULL, ",");
    i++;
  }
  char printlen[200];
  sprintf(printlen, "%lf", post_duration);
  Serial.println(printlen);
  int offset = 0;
  offset = sprintf(json_body, "%s", PREFIX);
  offset += sprintf(json_body + offset, "%s, ", user);
  //TO CHANGE
  offset += sprintf(json_body + offset, "\"song\":\"%lf&", post_duration);
  for (int i = 1; i < query_string_length; ++i) {
    offset += sprintf(json_body + offset, "%lf,", post_notes[i]);
  }
  offset += sprintf(json_body + offset, "%lf", post_notes[query_string_length-1]);
  sprintf(json_body + offset, "%s", SUFFIX);
  int len = strlen(json_body);
  request[0] = '\0'; //set 0th byte to null
  offset = 0; //reset offset variable for sprintf-ing
  offset += sprintf(request + offset, "POST http://iesc-s3.mit.edu/esp32test/limewire HTTP/1.1\r\n");
  offset += sprintf(request + offset, "Host: %s\r\n", server);
  offset += sprintf(request + offset, "Content-Type: application/json\r\n");
  offset += sprintf(request + offset, "Content-Length: %d\r\n\r\n", len); //ADD VARIABLE: content_length
  offset += sprintf(request + offset, "%s\r\n", json_body);
  do_http_request(server, request, response, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, false);
  Serial.println("-----------");
  Serial.println(response);
  Serial.println("-----------");
}

// Minimalist C Structure for containing a musical "riff"
//can be used with any "type" of note. Depending on riff and pauses,
// you may need treat these as eighth or sixteenth notes or 32nd notes...sorta depends
struct Riff {
  double notes[1024]; //the notes (array of doubles containing frequencies in Hz. I used https://pages.mtu.edu/~suits/notefreqs.html
  int length; //number of notes (essentially length of array.
  float note_period; //the timing of each note in milliseconds (take bpm, scale appropriately for note (sixteenth note would be 4 since four per quarter note) then
};

class Button {
  public:
  uint32_t S2_start_time;
  uint32_t button_change_time;    
  uint32_t debounce_duration;
  uint32_t long_press_duration;
  uint8_t pin;
  uint8_t flag;
  uint8_t button_pressed;
  button_state state; // This is public for the sake of convenience
  Button(int p) {
  flag = 0;  
    state = S0;
    pin = p;
    S2_start_time = millis(); //init
    button_change_time = millis(); //init
    debounce_duration = 7;
    long_press_duration = 1000;
    button_pressed = 0;
  }
  void read() {
    uint8_t button_val = digitalRead(pin);  
    button_pressed = !button_val; //invert button
  }
  int update() {
    read();
    flag = 0;
    if (state == S0) {
      if (button_pressed) {
        state = S1;
        button_change_time = millis();
      }
    } else if (state==S1) {
      if (button_pressed && millis() - button_change_time >= debounce_duration) {
        state = S2;
        S2_start_time = millis();
      } else if (!button_pressed) {
        state = S0;
        button_change_time = millis();
      }
    } else if (state==S2) {
      if (button_pressed && millis() - S2_start_time >= long_press_duration) {
        state = S3;
      } else if (!button_pressed) {
        button_change_time = millis();
        state = S4;
      }
    } else if (state==S3) {
      if (!button_pressed) {
        button_change_time = millis();
        state = S4;
      }
    } else if (state==S4) {      	
      if (!button_pressed && millis() - button_change_time >= debounce_duration) {
        if (millis() - S2_start_time < long_press_duration) {
          flag = 1;
        }
        if (millis() - S2_start_time >= long_press_duration) {
          flag = 2;
        }
        state = S0;
      } else if (button_pressed && millis() - S2_start_time < long_press_duration) {
        button_change_time = millis();
        state = S2;
      } else if (button_pressed && millis() - S2_start_time >= long_press_duration) {
        button_change_time = millis();
        state = S3;
      }
    }
      return flag;
  }
    
};

void toggleMode() {
  //Reset TFT graphics
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (mode == 1) {
    mode = 0;
  } else {
    mode = 1;
  }
}

//CLASS INSTANCES
Button ToggleButton(BUTTON1); //button object!
Button SubmitButton(BUTTON2);
Button UserEntryButton(BUTTON3);
Button ResetButton(BUTTON4);

class SongGetter {
    char digits[50] = ",.0123456789";
    char msg[400] = {0}; //contains previous query response
    char toSend[100];
    char query_string[50] = {0};
    int char_index;
    int state;
    uint32_t scroll_timer;
    const int scroll_threshold = 150;
    const float angle_threshold = 0.3;
  public:

    SongGetter() {
      state = 0;
      memset(msg, 0, sizeof(msg));//empty it.
      strcat(msg, "Long Press to Start!");
      char_index = 0;
      scroll_timer = millis();
    }
    void update(float angle, int button, char* output) {
          //your code here
      //S0: if long press, transition to "Text Entry" state
      switch (state) {
        case 0 :
          if (button == 2) {
            state = 1;
            scroll_timer = millis();
            sprintf(output, "%s", msg);
          } else {
            sprintf(output, "%s", msg);
          }
          break;
        case 1:
          if (button == 1) { //short button press
            query_string[strlen(query_string)] = digits[char_index];
            if (digits[char_index] == ',') {
              query_string_length++;
            }
            sprintf(output, "%s", query_string);
            char_index = 0;
          } else if (button == 2) {
            memset(output, 0, sizeof(output));
            state = 2;
          } else {
            if (abs(angle) > angle_threshold && (millis() - scroll_timer >= scroll_threshold)) {
              if (angle < 0){ //left tilt
                char_index = ((char_index + 11) % 12);
                scroll_timer = millis();
              } else if (angle > 0) {
                char_index = ((char_index + 1) % 12);
                scroll_timer = millis();
              }
            }
            sprintf(output, "%s%c", query_string, digits[char_index]);
          }
          break;
        case 2:
          sprintf(output, "%s", "Sending Query");
          Serial.println(output);
          state = 3;
          break;
        case 3:
          char toSend[200];
          sprintf(toSend, "%s", query_string); 
          postSong(toSend, msg, OUT_BUFFER_SIZE);       
          memset(query_string, 0, sizeof(query_string));
          sprintf(output, "%s", msg);
          state = 0;
          query_string_length = 0;
          break;
      }
    }
  };

Riff song_to_play;
SongGetter sg;

//make the riff player. Play whatever the current riff is (specified by struct instance song_to_play)
//function can be blocking (just for this lab :) )
void play_riff() {
  Serial.println("PLAYING RIFF");
  for (int i = 0; i < song_to_play.length; i++) {
    // ledcWriteTone(AUDIO_PWM, song_to_play.notes[i]);
    old_note = new_note;
    new_note = song_to_play.notes[i];

    if (ResetButton.update() != 0) {
      song_id = 0;
      getSong();
      ledcWriteTone(AUDIO_PWM, 0);
      Serial.println("Reset song ID");
      break;
    }
    
    //update screen
    if (new_note > old_note) {
      tft.fillScreen(TFT_ORANGE);
      tft.setTextColor(TFT_BLACK, TFT_ORANGE);
      ledcWriteTone(AUDIO_PWM, new_note);
    } else if (new_note < old_note) {
      tft.fillScreen(TFT_BLUE);
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
      ledcWriteTone(AUDIO_PWM, new_note);
    }
    char output[100];
    tft.setCursor(0, 0, 1);
    sprintf(output, "Playback Mode \n Current Note:%d                      ", int(new_note));
    tft.println(output);
    delay(song_to_play.note_period);
  }
}

void parseSong () {
  //temp variables for new Riff
  memset(song_to_play.notes, 0, sizeof(song_to_play.notes));

  //HTTP response string parsing
  char * p;
  int i = 0;
  p = strtok (response,",&");
  duration = atof(p);
  while (p != NULL)
  {
    if (i >= 1) {
      song_to_play.notes[i-1] = atof(p);
    }
    p = strtok (NULL, ",&");
    i++;
  }
  song_to_play.length = i-1;
  song_to_play.note_period = duration;
  char printlen[20];
  sprintf(printlen, "%d", song_to_play.length);
  Serial.println(printlen);
}

void getSong() {
  request[0] = '\0'; //set 0th byte to null
  int offset = 0; //reset offset variable for sprintf-ing
  offset += sprintf(request + offset, "GET http://iesc-s3.mit.edu/esp32test/limewire?song_id=%d HTTP/1.1\r\n", song_id);
  offset += sprintf(request + offset, "Host: iesc-s3.mit.edu\r\n");
  offset += sprintf(request + offset, "Content-Type: application/json\r\n");
  offset += sprintf(request + offset, "\r\n");
  do_http_request(server, request, response, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, false);
  Serial.println("-----------");
  Serial.println(response);
  Serial.println("-----------");
  parseSong();
  Serial.flush();
}

void setup() {
  Serial.begin(115200); //for debugging if needed.
  WiFi.begin(network, password); //attempt to connect to wifi
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count < 12) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.printf("%d:%d:%d:%d (%s) (%s)\n", WiFi.localIP()[3], WiFi.localIP()[2],
                  WiFi.localIP()[1], WiFi.localIP()[0],
                  WiFi.macAddress().c_str() , WiFi.SSID().c_str());    delay(500);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }
  if (imu.setupIMU(1)) {
    Serial.println("IMU Connected!");
  } else {
    Serial.println("IMU Not Connected :/");
    Serial.println("Restarting");
    ESP.restart(); // restart the ESP (proper way)
  }
  tft.init();
  tft.setRotation(2);
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); //set color of font to green foreground, black background

  //RIFF SETUP
  Serial.printf("Frequencies:\n"); //print out your frequencies as you make them to help debugging
  double note_freq = A_1;
  //fill in note_freq with appropriate frequencies from 55 Hz to 55*(MULT)^{NOTE_COUNT-1} Hz
  note_freqs[0] = A_1;
  for (int i = 1; i < NOTE_COUNT; i++) {
    note_freqs[i] = note_freq * MULT;
    note_freq = note_freqs[i];
  }

  //print out your accelerometer boundaries as you make them to help debugging
  Serial.printf("Accelerometer thresholds:\n");
  //fill in accel_thresholds with appropriate accelerations from -1 to +1
  double accToNote = 2 / (double(NOTE_COUNT + 1));
  double current = -1;
  accel_thresholds[0] = current;
  for (int i = 1; i < NOTE_COUNT + 1; i++) {
    accel_thresholds[i] = current + accToNote;
    current = accel_thresholds[i];
    // accel_thresholds.push(-1 + accToNote * i);
  }
  //start new_note as at middle A or thereabouts.
  new_note = note_freqs[NOTE_COUNT - NOTE_COUNT / 2]; //set starting note to be middle of range.

  //four pins needed: two inputs, two outputs. Set them up appropriately:
  pinMode(BUTTON1, INPUT_PULLUP); // toggle (pin 45)
  pinMode(BUTTON2, INPUT_PULLUP); //submit button (pin 39)
  pinMode(BUTTON3, INPUT_PULLUP); // increment num entry (pin 38)
  pinMode(BUTTON4, INPUT_PULLUP); // helps with post mode (pin 34)
  pinMode(AUDIO_TRANSDUCER, OUTPUT);
  pinMode(LCD_CONTROL, OUTPUT);

  //set up AUDIO_PWM which we will control in this lab for music:
  ledcSetup(AUDIO_PWM, 200, 12);//12 bits of PWM precision
  ledcWrite(AUDIO_PWM, 0); //0 is a 0% duty cycle for the NFET
  ledcAttachPin(AUDIO_TRANSDUCER, AUDIO_PWM);

  //set up the LCD PWM and set it to 
  pinMode(LCD_CONTROL, OUTPUT);
  ledcSetup(LCD_PWM, 100, 12);//12 bits of PWM precision
  ledcWrite(LCD_PWM, 0); //0 is a 0% duty cycle for the PFET...increase if you'd like to dim the LCD.
  ledcAttachPin(LCD_CONTROL, LCD_PWM);

  delay(2000);

  primary_timer = millis();
}


void loop() {
  //CHECK MODE: 0 = PLAYBACK, 1 = RECORD
  //toggle by pressing button on 45 (long/short arbitrary)
  ToggleButton.update();
  if (ToggleButton.flag != 0) {
    toggleMode();
  }

  //if in playback mode, enter value and call getSong
  if (mode == 0) {
    tft.setCursor(0, 0, 1);
    char screenOutput[100];
    sprintf(screenOutput, "Playback Mode \n Lookup Song:%d         ", song_id);
    tft.println(screenOutput);
    if (UserEntryButton.update() == 1) {
      song_id++;   
      char currId[20];
      sprintf(currId, "%d", song_id);   
      Serial.println(currId); 
    }

    if (SubmitButton.update() == 1) { //get new song
      getSong();
    }

    if (ResetButton.update() != 0) {
      song_id = 0;
      getSong();
      ledcWriteTone(AUDIO_PWM, 0);
      Serial.println("Reset song ID");
    }

    play_riff();
  }

  //in record mode:
  if (mode == 1) {
    int bv = UserEntryButton.update();
    int updateMode = 0;
    float x, y;
    get_angle(&x, &y); //get angle values
    sg.update(y, bv, post_response); //input: angle and button, output String to display on this timestep
   
    if (strcmp(post_response, old_post_response) != 0) {//only draw if changed!
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0, 0, 1);
      tft.println(post_response);
    }
    memset(old_post_response, 0, sizeof(old_post_response));
    strcat(old_post_response, post_response);
  }

  while (millis() - primary_timer < LOOP_PERIOD); //wait for primary timer to increment
  primary_timer = millis();
}
