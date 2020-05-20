# rpi-kafka-oled
Raspberry Pi project - consumes messages from a Kafka broker and displays the latest message text to a OLED display.

## Getting Started

```
git clone https://github.com/bredlej/rpi-kafka-oled.git
cd rpi-kafka-oled
make
./rpi-kafka-oled <broker:port> <group-id> <topic 1> <topic 2> ... <topic N>
```
The program will run until it receives an interrupt signal (CTRL+c), which will cause the Kafka consumer to stop and turn off the display.

## Prerequisites:
* WiringPi C library
`sudo apt-get install wiringpi`
* librdkafka-dev 
`sudo apt-get install librdkafka-dev"`
* an OLED display connected to RPI - tested with [Waveshare 0.95 RGB OLED (A)](https://www.waveshare.com/wiki/0.95inch_RGB_OLED_(A))
* a running [Apache Kafka](https://kafka.apache.org/) broker with a topic (or more), that the program can connect and subscribe to.

### RPI GPIO  -> OLED connection
See [GPIO Pinout guide](https://pinout.xyz/#) for reference.
| GPIO      | OLED |
|-----------|------|
| 3v3       | VCC  |
| GND       | GND  |
| MOSI (10) | DIN  |
| SCLK (11) | CLK  |
| CE0  (8)  | CS   |
| BCM  (16) | D/C  |
| MISO (19) | RES  |

## Use-case examples:

### temperature-oled.c
![Picture of temperature-oled](/image.jpg)

Displays line chart of CPU temperatures of four devices in the network.
The devices send their data as key-value pair to a Kafka topic, here: `device_name:cpu_temperature`.

If you want to use it you'd need to adjust the code to match your devices:
* temperature-oled.c
```
Run with:
./temperature-oled <broker:port> <group-id> <topic 1> <topic 2> ... <topic N>

The DEVICE_[0..4]_KEY definitions are names of devices displayed on-screen and also keys of the kafka messages.
If you change the number of devices make sure to adjust code in functions init_devices() and render_debug().

```
* temperature-send.py
```
Python script to send messages with CPU temperature data in 5 second intervals to a given topic.
Needs python3-kafka library (sudo apt-get install python3-kafka).

Run with:
python3 ./temperature-send.py <broker:port> <topic> <device_name>

Make sure the <device_name> matches the keys defined in temperature-oled.c
```

## Demo:
[YouTube video demonstration of temperature-oled](https://www.youtube.com/watch?v=tcb1ZKKq0Ag&t)

[Video of base rpi-kafka-oled display](https://www.youtube.com/watch?v=azisqt7O75k)

## Author

**Patryk Szczypień** (https://github.com/bredlej) - patryk.szczypien@gmail.com

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

## Picture

![Picture of running program](/image.jpg)
