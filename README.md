# Aura2

The Aura2 project is a landscape layout weather widget inspired by the [Aura project](https://github.com/Surrey-Homeware/Aura).

That version used the Arduino IDE, which is OK for smaller projects but I'm more accustomed to IDEs like Visual Studio and Jetbrains Rider.
Neither of them support use with controllers like ESP32. Visual Studio Code with PlatformIO does.

The project also uses LVGL to render the UI. The original version uses a lot of C that manually creates and places widgets. This project uses
EEZ Studio to visually layout the UI as much as possible.

Since I was in learning mode, I may as well go overboard. This project also captures experiments with:

* MQTT
* WiFi provisioning with SoftAP
* Integration with Home Assistant via MQTT

## How to build

### Prerequisites

* Visual Studio Code
* PlatformIO
* A CYD

### Steps

1. Open the folder containing platformio.ini
2. Wait for PlatformIO to initialize
3. Have the CYD attached via USB
4. Upload (via PlatformIO) to the device

## Thanks & Credits

I'd like to extend the thanks given to all of the work that predates this project and would not have been feasible without it.

* [Aura](https://github.com/Surrey-Homeware/Aura)
* [PlatformIO](https://platformio.org/)
* [EEZ Studio](https://www.envox.eu/studio/studio-introduction/)
* [Random Nerd Tutorials](https://randomnerdtutorials.com/)
* [Aura's Thanks and Credits](https://github.com/Surrey-Homeware/Aura?tab=readme-ov-file#thanks--credits)
* [Tabler Icons](https://tabler.io/icons)