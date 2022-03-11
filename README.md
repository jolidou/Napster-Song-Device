# Napster - Joli Dou

---

# Overview

## Demo Video:

[demonstration video](https://youtu.be/ti0t3enVFUs)

## Implementing Specifications:

1. Producing and Consuming Structures
   I imported the Riff structure from Lab 03B and created a Riff called song_to_play. I used the getSong() method to retrieve a song from the database, and used string parsing and float conversions (atof) to extract duration and note frequencies, which I could then store in the appropriate song_to_play fields. Then, play_riff() plays the Riff, as it did in the original Lab 03B. I implemented song production by hard-coding the username for a given Arduino, then using string parsing when creating the JSON body for the POST request.

2. Two Modes:
   In the Playback Mode, I display the mode and which song (by ID) we are currently retrieving. If the button on pin 38 is short-pressed, this increments the song ID to find. If the button on pin 39 is pressed, this submits an HTTP request with the song ID as a query argument. There is a Reset button on pin 34, which can reset the song ID to 0 and also stop a Riff while it plays. Otherwise, the Riff loops, as per specification. Toggle to Record mode using the button on pin 45.
   In the Record Mode, I modeled the code off of my Wikipedia Interfacer code from Week 5 Reading Exercises. I created a SongGetter with the same update() state machine, so when the button on pin 38 was long-pressed, we enter data entry mode. I used the IMU for user input, so one can scroll to the desired value by tilting the ESP left (smaller) or right(larger). Short-pressing concatenates the selected value to previously selected digits. Long-pressing again sends a request, using postSong(). postSong() calls a helper parseSong() to extract duration and note frequencies from the user entry, then makes a JSON_body, and sends a POST request. At each step, the selected digits or a message (like the output from POST-ing) is printed on the LCD Display.

3. Controlling the Instrument
   You can add non-adjacent notes by scrolling to a non-adjacent value, then selecting it. Because the selection and submission of a value are 2 distinct steps, as opposed to in lab 03b, the IMU-entered notes do not have to be adjacent. To add silences, we can just write a 0 value as our frequency, as writing 0 to the PWM creates silence.

## Design Strategies:

1. Parsing
   I used strtok() for parsing. First, I used it to extract duration and note frequencies from HTTP GET responses, with the delimiters "&" and ",". To make POST requests, I also used strtok() to separate user-entered query strings, because "," indicates a separate entry. For conversions, I used atof() instead of atoi() because the values were floats.

2. Setting Values
   I used an IMU to scroll through values and the button on Pin 38 to add values. Based on IMU angle measurements, if the ESP was tilted left, the selected value would be smaller out of the digits array, and if tilted right, the selected value would be larger. This indexing wrapped around.

3. Playback Looping
   To play a song on loop, play_riff runs each time in loop(). However, by adding a Reset Button on pin 34, the song loop can be stopped. This has the dual functionality of being able to reset a song ID query before it is made to be 0.

## Resources

1. Parsing Example

```cpp
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
```

2. Creating POST request: Parsing, creating JSON body, sending request

```cpp
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

```

3. Riff: looping and reset

```cpp
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
```

---

# Summary

In this project, I implemented Napster, a system to get and post song Riffs to an online database. I defined data structures for the Riffs, Buttons, and SongGetter for post requests. This helped with variable scoping and maintaining state. Using parsing to break character arrays into their necessary components, I then formatted them into HTTP GET, HTTP POST, or JSON formatting. To get user input and allow non-adjacent values, I used an IMU-button system to scroll through values and select ones. Some key concepts were (1)HTTP requests, (2)classes, and (3)Parsing. Hardware concepts included the IMU, buttons, and the buzzer.
