#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <librdkafka/rdkafka.h>
#include "kafkautils.h"
#include "ssd1331.h"
#include "timeops.h"
#include "gui.h"

#define SHOW_TOP_DEBUG 0
#define SHOW_BOTTOM_DEBUG 1
#define BOTTOM_DEBUG_RGB (RGB(60,60,200))
#define MS_PER_UPDATE_GRAPHICS 16
#define MS_PER_UPDATE_LOGIC 1000 
#define AMOUNT_PARTICLES 48
#define AMOUNT_STARS 48
#define AMOUNT_DEVICES 4
#define TYPE_1_STAR 1
#define TYPE_2_STAR 2
#define TYPE_3_STAR 3
#define TYPE_1_STAR_SPEED 0.7f
#define TYPE_2_STAR_SPEED 0.07f
#define TYPE_3_STAR_SPEED 0.007f
#define TYPE_1_STAR_CHANCE 100
#define TYPE_3_STAR_CHANCE 500
#define MIN_TEMP_Y 12
#define MAX_TEMP_Y 48
#define TEMP_SCALE_MIN 47
#define TEMP_SCALE_MAX 57 

#define DEVICE_LETO_KEY "leto"
#define DEVICE_DUNCAN_KEY "duncan"
#define DEVICE_CHANI_KEY "chani"
#define DEVICE_MUADDIB_KEY "muaddib"
#define DEVICE_LETO_NAME_SHORT "LE"
#define DEVICE_DUNCAN_NAME_SHORT "DU"
#define DEVICE_CHANI_NAME_SHORT "CH"
#define DEVICE_MUADDIB_NAME_SHORT "MU"

/** 
 * Global variable determining if main loop should run 
 */
static volatile sig_atomic_t program_is_running = 1;

/** 
 * Stops the program from running 
 */
static void stop (int sig) {
	program_is_running = 0;
}

/** 
 * Struct describing a particle in the background (stars) 
 */
typedef struct PARTICLE {
	float x, y;
	unsigned int rgb;
	int type;
} PARTICLE;

typedef struct DEVICE {
	char name[2];
	float temperature;
	unsigned int rgb;
	PARTICLE *temperature_particles;
} DEVICE;

typedef struct BACKGROUND {
	PARTICLE *stars;
} BACKGROUND;

typedef struct INSTANCE {
	BACKGROUND *background;
	PARTICLE *particles;
	DEVICE *devices;
	rd_kafka_t *kafka_handler;
	float temperature;
} INSTANCE;

typedef struct KAFKA_CONSUMER_ARGS {
	rd_kafka_t *rk; // pointer to kafka consumer instance
	char *payload; // the latest message text from the topic will be stored in this pointer
	DEVICE *devices;
} KAFKA_CONSUMER_ARGS;

/** 
 * Converts a float to screen y position, determined by float min/max values & oled height 
 */
static int float_to_screen_y(const float temperature) 
{
	return 64 - (((temperature - TEMP_SCALE_MIN) / (TEMP_SCALE_MAX - TEMP_SCALE_MIN)) * (MAX_TEMP_Y - MIN_TEMP_Y)) - MIN_TEMP_Y;
}

/** 
 * Returns a random int from min/max range 
 */
static int rand_range(int min_n, int max_n)
{
	return rand() % (max_n - min_n + 1) + min_n;
}

/** 
 * Initializes the backround structure 
 */
static int init_background(BACKGROUND *background) 
{
	/* allocates memory for background stars of a given amount */
	background->stars = malloc(AMOUNT_STARS * sizeof(PARTICLE));
	if (!background->stars) return -1;

	/* initializes a random star type & position for each allocated particle */
	for (int i = 0; i < AMOUNT_STARS; i++) {
		PARTICLE *star = &background->stars[i];
		star->x = rand_range(0, OLED_WIDTH);
		star->y = rand_range(0, OLED_HEIGHT);
		int range = rand_range(0,1000);
		if (range < TYPE_1_STAR_CHANCE) star->type = TYPE_1_STAR;
		if (range >= TYPE_1_STAR_CHANCE && range < TYPE_3_STAR_CHANCE) star->type = TYPE_2_STAR;
		if (range >= TYPE_3_STAR_CHANCE) star->type = TYPE_3_STAR;
			
		if (TYPE_1_STAR == star->type) star->rgb = RGB(255,255,255);		
		if (TYPE_2_STAR == star->type) star->rgb = RGB((255>>1),(255>>1),(255>>1));		
		if (TYPE_3_STAR == star->type) star->rgb = RGB((255>>3),(255>>3),(255>>3));		
	}
	return 1;
}

int init_devices(DEVICE *devices)
{	
	DEVICE *dev0 = &devices[0];
	DEVICE *dev1 = &devices[1];
	DEVICE *dev2 = &devices[2];
	DEVICE *dev3 = &devices[3];

	strcpy(dev0->name, DEVICE_LETO_NAME_SHORT);
   	strcpy(dev1->name, DEVICE_DUNCAN_NAME_SHORT);
	strcpy(dev2->name, DEVICE_CHANI_NAME_SHORT);
	strcpy(dev3->name, DEVICE_MUADDIB_NAME_SHORT);

	dev0->temperature = 45.0f;
	dev1->temperature = 45.0f;
	dev2->temperature = 45.0f;
	dev3->temperature = 45.0f;

	// define colors for devices 0-3 (enum definition in ssd1331.h)
	enum Color colors[] = {RED, BLUE, GREEN, YELLOW};
	devices[0].rgb = colors[0]; //RGB(18,127,169); //colors[0]; // 18,127,169
	devices[1].rgb = colors[1]; //RGB(75, 36, 143); //colors[1]; // 75, 36, 143
	devices[2].rgb = colors[2]; //RGB(19, 27, 76); //colors[2]; // 19, 27, 76
	devices[3].rgb = colors[3]; //RGB(0, 243, 197); //RGB(127, 64, 122); //colors[3]; // 0, 243, 197 || 127, 64, 122
   
	dev0->temperature_particles = malloc(AMOUNT_PARTICLES * sizeof(PARTICLE)); //dev0particles;
	dev1->temperature_particles = malloc(AMOUNT_PARTICLES * sizeof(PARTICLE)); // dev1particles;
	dev2->temperature_particles = malloc(AMOUNT_PARTICLES * sizeof(PARTICLE)); // dev1particles;
	dev3->temperature_particles = malloc(AMOUNT_PARTICLES * sizeof(PARTICLE)); // dev1particles;

	return 0;
}


/**
 * Update the particles responsible for showing the temperature chart.
 * TODO implement this for each device registering in the kafka alive topic.
 */
int update_temperature (DEVICE *device, float temperature) 
{
	PARTICLE *current_temperature;
	PARTICLE *previous_temperature;

	/* Code responsible for adjusting the line chart when time elapses */
	for (int i = AMOUNT_PARTICLES - 1; i > 0; i--) {
		current_temperature = &device->temperature_particles[i];
		previous_temperature = &device->temperature_particles[i-1];
		current_temperature->y = previous_temperature->y;
	}
	device->temperature_particles[0].y = float_to_screen_y(device->temperature);
//	sprintf(instance->debug_info.bottom, "%.1f'C", instance->temperature);
	
	return 1;	
}
/**
 * Initializes the program instance
 */
int init(INSTANCE *instance) 
{
	/* Setup wiring of GPIO */
	if(wiringPiSetup() < 0) return -1;

	/* Randomize seed */
	srand(time(NULL));		

	/* Allocate memory for the background structure */	
	BACKGROUND *background = malloc(sizeof *background);
	if (!background || !init_background(background)) return -1;
	
	/* Allocate memory for stars in the background*/
	PARTICLE *particles = malloc(AMOUNT_PARTICLES * sizeof *particles);
	if (!particles) return -1;	

	DEVICE *devices = malloc(AMOUNT_DEVICES * sizeof *devices)	;
	if (!devices || init_devices(devices)) return -1;

	instance->background = background;
	instance->particles = particles;
	instance->devices = devices;
	instance->temperature = 30.0f;

	/* Place temperature particles in their initial position */
	for (int dev_idx = 0; dev_idx < AMOUNT_DEVICES; dev_idx ++) {
		DEVICE *device = &instance->devices[dev_idx];
		for (int i = 0; i < AMOUNT_PARTICLES; i++) {
			PARTICLE *current_particle = &device->temperature_particles[i];
			current_particle->x = OLED_WIDTH - i*(OLED_WIDTH/AMOUNT_PARTICLES);
			current_particle->y = float_to_screen_y(device->temperature);
			int mod = i * (255 / AMOUNT_PARTICLES);
			current_particle->rgb = RGB((255-mod),mod,mod);
			current_particle->type = 0;
		}	
		update_temperature(device, device->temperature);
	}

	/* Write initial debug info */
//	sprintf(instance->debug_info.bottom, "[]");

	/* Turn on the OLED screen */
	SSD1331_begin();
	
	return 1;
}

/**
 * Move stars according to lag between each program loop
 */
int update_background(INSTANCE *instance, const float lag_ms) 
{
	for (int i = 0; i < AMOUNT_STARS; i++) {
		PARTICLE *star = &instance->background->stars[i];
		if (TYPE_1_STAR == star->type) star->x += TYPE_1_STAR_SPEED;
		if (TYPE_2_STAR == star->type) star->x += TYPE_2_STAR_SPEED;
		if (TYPE_3_STAR == star->type) star->x += TYPE_3_STAR_SPEED;

		/* If star goes out of the visible screen, put it somewhere on the left.
		   Stars are being reused this way instead of deleting/creating them */
		 
		if (star->x > OLED_WIDTH) {
			star->x = rand_range(-15, 0);
			star->y = rand_range(0, OLED_HEIGHT);
		}
	}
	
	return 1;
}


/**
 * Draw stars in the background.
 */
static int render_background(const INSTANCE *instance) 
{
	for (int i = 0; i < AMOUNT_STARS; i++) {
		PARTICLE *star = &instance->background->stars[i];
		if (star->x >= 0) SSD1331_draw_point(star->x, star->y, star->rgb);
	}
	
	return 1;
}

/**
 * Draw temperature chart.
 * TODO implement for devices sending messages to kafka.
 */
static int render_termometer(const DEVICE *device) 
{
	PARTICLE *current_temperature;
	PARTICLE *previous_temperature;
	for (int i = 0; i < AMOUNT_PARTICLES; i++) {
		current_temperature = &device->temperature_particles[i];
//		fprintf(stderr, "cur=[%0.1f]", current_temperature->x);
	    previous_temperature = &device->temperature_particles[i < AMOUNT_PARTICLES - 1 ? i+1 : 0];
		if (i < AMOUNT_PARTICLES - 1) {
			SSD1331_line(current_temperature->x, current_temperature->y, 
						previous_temperature->x, previous_temperature->y, 
						device->rgb);
		}
	}

	return 1;
}

/**
 * Draw the debug text.
 */
static int render_debug(const INSTANCE *instance) 
{
	DEVICE *device0 = &instance->devices[0];
	DEVICE *device1 = &instance->devices[1];
	DEVICE *device2 = &instance->devices[2];
	DEVICE *device3 = &instance->devices[3];

	char display_text[9];

	sprintf(display_text, "%s[%.1f]", device0->name, device0->temperature);
	SSD1331_string(0, TOP_DEBUG_STRING_Y, display_text, 12, 1, device0->rgb);

	sprintf(display_text, "%s[%.1f]", device1->name, device1->temperature);
	SSD1331_string(48, TOP_DEBUG_STRING_Y, display_text, 12, 1, device1->rgb);

	sprintf(display_text, "%s[%.1f]", device2->name, device2->temperature);
	SSD1331_string(0, BOTTOM_DEBUG_STRING_Y, display_text, 12, 1, device2->rgb);

	sprintf(display_text, "%s[%.1f]", device3->name, device3->temperature);
	SSD1331_string(48, BOTTOM_DEBUG_STRING_Y, display_text, 12, 1, device3->rgb);

	return 1;
}

/**
 * Draw all screen components
 */
int render(const INSTANCE *instance, const float ms) 
{
	SSD1331_clear();

	render_background(instance);
	for (int i = 0; i < AMOUNT_DEVICES; i++) {
		render_termometer(&instance->devices[i]);
	}
	render_debug(instance);	
	
	SSD1331_display();
	
	return 1;
}

/**
 * Free memory used by program instance.
 */
int deallocate_instance_from_memory(INSTANCE *instance) 
{
	free(instance->background->stars);
	free(instance->background);
	free(instance->particles);
	free(instance->devices->temperature_particles);
	free(instance->devices);

	/* Close the consumer: commit final offsets and leave the group. */
	fprintf(stderr, "%% Closing consumer\n");
	rd_kafka_consumer_close(instance->kafka_handler);
	/* Destroy the consumer */
	rd_kafka_destroy(instance->kafka_handler);

	free(instance);

	return 1;
}

/**
 * @returns 1 if all bytes are printable, else 0.
 */
static int is_printable (const char *buf, size_t size) {
        size_t i;
        for (i = 0 ; i < size ; i++)
                if (!isprint((int)buf[i]))
                        return 0;
        return 1;
}

/** 
 * Run kafka message consumer in a separate thread.
 *
 * @param args - pointer to a KAFKA_CONSUMER_ARGS struct.
 *
 * The handler *rk inside args needs to be already initialized and subscribed to a topic.
 * The text of the latest received message will be stored under the args->payload pointer.
 */
void *consume_kafka_messages(void *vargp) {

	struct KAFKA_CONSUMER_ARGS *args = (struct KAFKA_CONSUMER_ARGS *) vargp;
	rd_kafka_t *rk = args->rk;
	char *payload = args->payload;
	DEVICE *devices = args->devices;	
	signal(SIGINT, stop);
	
	while (program_is_running) {
		rd_kafka_message_t *rkm;

		rkm = rd_kafka_consumer_poll(rk, 100);
		if (!rkm)
				continue; /* Timeout: no message within 100ms,
						   *  try again. This short timeout allows
						   *  checking for `run` at frequent intervals.
						   */

		/* consumer_poll() will return either a proper message
		 * or a consumer error (rkm->err is set). */
		if (rkm->err) {
				/* Consumer errors are generally to be considered
				 * informational as the consumer will automatically
				 * try to recover from all types of errors. */
				fprintf(stderr,
						"%% Consumer error: %s\n",
						rd_kafka_message_errstr(rkm));
				rd_kafka_message_destroy(rkm);
				continue;
		}

		/* Proper message. */
		printf("Message on %s [%"PRId32"] at offset %"PRId64":\n",
		rd_kafka_topic_name(rkm->rkt), rkm->partition,
			   rkm->offset);

		/* Print the message key. */
		if (rkm->key && is_printable(rkm->key, rkm->key_len))
				printf(" Key: %.*s\n",
					   (int)rkm->key_len, (const char *)rkm->key);
		else if (rkm->key)
				printf(" Key: (%d bytes)\n", (int)rkm->key_len);

		/* Print the message value/payload. */
		if (rkm->key && is_printable(rkm->key, rkm->key_len) && rkm->payload && is_printable(rkm->payload, rkm->len)) {
			printf(" Value: %.*s\n", (int)rkm->len, (const char *)rkm->payload);
			char key[4];
			sprintf(key, "%.*s", (int)rkm->key_len, (const char *)rkm->key);
			for (int i = 0; i < AMOUNT_DEVICES; i++) {
				DEVICE *device = &devices[i];
				fprintf(stderr, "KEY:[%.*s] DEV[%s]\n", (int)rkm->key_len, (const char *)rkm->key, device->name);
				if (strcmp(device->name, key) == 0) {
					device->temperature= strtof(rkm->payload, NULL);
					fprintf(stderr, "[%s]=[%0.1f]\n", device->name, device->temperature);
				}
			}
		}
		else if (rkm->key)
				printf(" Value: (%d bytes)\n", (int)rkm->len);

		rd_kafka_message_destroy(rkm);
	}
	pthread_exit(NULL);
}

/**
 * Main program
 * 
 * 1. Initialize state
 * 2. Run update-render loop
 * 3. On exit free memory & turn off OLED screen
 *
 */  
int main(int argc, char **argv)
{
	const char *brokers;     /* Argument: broker list */
	const char *groupid;     /* Argument: Consumer group id */
	char **topics;           /* Argument: list of topics to subscribe to */
	int topic_cnt;           /* Number of topics to subscribe to */

	/*
	 * Program argument validation
	 */
	if (argc < 4)
	{
		fprintf(stderr,
				"%% Usage: "
				"%s <broker> <group.id> <topic1> <topic2>..\n",
				argv[0]);
		return 1;
	}

	brokers   = argv[1];
	groupid   = argv[2];
	topics    = &argv[3];
	topic_cnt = argc - 3;

	long previous_ms = 0, current_ms = 0, elapsed_ms = 0, lag_ms = 0, count_ms = 0;
	
	/* Reserve memory for instance struct */
	INSTANCE *instance = malloc(sizeof *instance);
	if (!instance || !init(instance)) return -1;
	
	/* Initialize Kafka handler and assign pointer to instance struct */
	instance->kafka_handler = init_kafka_handler(brokers, groupid, topic_cnt, topics);	
	if (!instance->kafka_handler) {		
		fprintf(stderr, "Failed to initialize Kafka handler.");
		return 1;
	}
	
	previous_ms = get_current_time();

	/* Reserve memory for Kafka message text and make it empty at first */
	char latest_message_text[20];
	sprintf(latest_message_text, "%s", "");
	
	/* Prepare args for consumer threads - Kafka handler & pointer to latest message text */
	KAFKA_CONSUMER_ARGS *args = malloc(sizeof *args);
	args->rk = instance->kafka_handler;
	args->payload = latest_message_text;
	args->devices = instance->devices;
	
	/* Start thread with message consumer */
	pthread_t consumer_thread;
	pthread_create(&consumer_thread, NULL, consume_kafka_messages, (void*) args);

	/* Stop program on CTRL+c */
	signal(SIGINT, stop);

	/*
	 * Main program loop
	 */
	while(program_is_running)
	{
		current_ms = get_current_time();

		/* Update if enough time elapsed */
		if (count_ms > MS_PER_UPDATE_LOGIC) {

			/* Write latest Kafka message */
			//sprintf(instance->debug_info.bottom, "[%s]", latest_message_text);
			count_ms = 0;
		}

		/* FPS calculations */
		elapsed_ms = current_ms - previous_ms;
		previous_ms = current_ms;
		count_ms += elapsed_ms;
		lag_ms += elapsed_ms;
		for (int i = 0; i < AMOUNT_DEVICES; i++) {
			DEVICE *device = &instance->devices[i];
			if (!update_temperature(device, 10.0f)) return -1;
		}
		/* Update the background according to lag */
		while (lag_ms >= MS_PER_UPDATE_GRAPHICS) 
		{
			if (!update_background(instance, lag_ms)) return -1;
			lag_ms -= MS_PER_UPDATE_GRAPHICS;
		}

		/* Render instance state to screen */
		if (!render(instance, lag_ms / (float) MS_PER_UPDATE_GRAPHICS)) return -1;
	}

	/* Exit program */

	/* Clear and turn off display*/
	SSD1331_clear();
	command(DISPLAY_OFF);
	
	/* Free memory */
	deallocate_instance_from_memory(instance);

	/* Disable running threads */
	pthread_exit(NULL);

	/* Exit */	
	return 0;
}
