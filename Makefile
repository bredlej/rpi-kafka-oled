all: rpi-kafka-oled
rpi-kafka-oled: rpi-kafka-oled.o ssd1331.o kafkautils.o
	gcc -Wall -o rpi-kafka-oled rpi-kafka-oled.o ssd1331.o kafkautils.o -lwiringPi -lpthread -lrdkafka
rpi-kafka-oled.o: rpi-kafka-oled.c gui.h ssd1331.h kafkautils.h
	gcc -Wall -c rpi-kafka-oled.c gui.h kafkautils.h -lwiringPi -lpthread -lrdkafka
kafkautils.o: kafkautils.c kafkautils.h
	gcc -Wall -c kafkautils.c -lrdkafka
ssd1331.o: ssd1331.c ssd1331.h
	gcc -Wall -c ssd1331.c -lwiringPi
clean:
	rm *.o
