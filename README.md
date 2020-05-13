# alivetest
Raspberry Pi project - consumes messages from a Kafka broker and displays the latest message text to a OLED display.

## Getting Started

```
git clone https://github.com/bredlej/alivetest.git
cd alivetest
make
./alivetest <broker:port> <group-id> <topic 1> <topic 2> ... <topic N>
```
The program will run until it receives an interrupt signal (CTRL+c), which will cause the Kafka consumer to stop and turn off the display.

## Prerequisites:
* WiringPi C library
`sudo apt-get install wiringpi`
* librdkafka-dev 
`sudo apt-get install librdkafka-dev"`
* an OLED display connected to RPI - tested with [Waveshare 0.95 RGB OLED (A)](https://www.waveshare.com/wiki/0.95inch_RGB_OLED_(A))
* a running [Apache Kafka](https://kafka.apache.org/) broker with a topic (or more), that the program can connect and subscribe to.

RPI GPIO  -> OLED connection: (BCM numbers corresponding to https://pinout.xyz/#)
| GPIO      | OLED |
|-----------|------|
| 3v3       | VCC  |
| GND       | GND  |
| MOSI (10) | DIN  |
| SCLK (11) | CLK  |
| CE0  (8)  | CS   |
| BCM  (16) | D/C  |
| MISO (19) | RES  |

## Demo:

[YouTube video showing what it looks like in action](https://www.youtube.com/watch?v=azisqt7O75k)

## Author

**Patryk Szczypień** (https://github.com/bredlej) - patryk.szczypien@gmail.com

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

## Picture

![Picture of running program](/image.jpg)
