# P1 port reader for belgian electricity meters Sagecom T211 and S211
Software for the ESP2866 / ESP32 that sends P1 smart meter data as a string to a TCP port (with CRC checking).
These meters have an open collector output, thus they require a pullup to 3.3V / 5V and an inversion of the signal (see included schematic).

### Installation instrucions
- Make sure that your ESP8266 / ESP32 can be flashed from the Arduino environnment.
- Place all files from this repository in a directory. Open the required .ino file.
- Adjust WIFI settings
- Adjust TCP server and port
- Add / delete messages as required for output string
- Compile and flash

### Connection of the P1 meter to the ESP
You need to connect the smart meter with a RJ11 (4 pin) or RJ12 (6 pin with +5V and GND) connector.
More information here: https://www.cdem.be/13_technical/#what-information-is-provided-with-the-p1-port

Invert the input to 3.3V logic for ESP using a BC547B NPN transistor or similar:
![image](https://user-images.githubusercontent.com/56192644/223165216-14dac1c8-cbf8-4372-b32e-bdf38041cfcc.png)

Connect GND->GND, RTS->REQ_PIN, and RxD->RX pin.
By default, the req pin and the npn base input are on 3.3V. If this does not work, then supply 5V for these by using a level shifter from the output of the ESP for the REQ_PIN and connect the R2 to +5V.
