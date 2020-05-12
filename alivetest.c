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
#define TYPE_1_STAR 1
#define TYPE_2_STAR 2
#define TYPE_3_STAR 3
#define TYPE_1_STAR_SPEED 0.7f
#define TYPE_2_STAR_SPEED 0.07f
#define TYPE_3_STAR_SPEED 0.007f
#define TYPE_1_STAR_CHANCE 100
#define TYPE_3_STAR_CHANCE 500
#define TEMP_SCALE_MAX 30 
#define TEMP_SCALE_MIN -10

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

typedef struct DEBUG_INFO {
	char top[20];
	char bottom[20];
} DEBUG_INFO;

typedef struct BACKGROUND {
	PARTICLE *stars;
} BACKGROUND;

typedef struct INSTANCE {
	BACKGROUND *background;
	PARTICLE *particles;
	DEBUG_INFO debug_info;
	rd_kafka_t *kafka_handler;
	float temperature;
} INSTANCE;

typedef struct KAFKA_CONSUMER_ARGS {
	rd_kafka_t *rk; // pointer to kafka consumer instance
	char *payload; // the latest message text from the topic will be stored in this pointer
} KAFKA_CONSUMER_ARGS;

/** 
 * Converts a float to screen y position, determined by float min/max values & oled height 
 */
static int float_to_screen_y(const float temperature) 
{
	return OLED_HEIGHT - ((temperature - TEMP_SCALE_MIN) / (TEMP_SCALE_MAX - TEMP_SCALE_MIN) * OLED_HEIGHT);
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
	
	instance->background = background;
	instance->particles = particles;
	instance->temperature = 30.0f;

	/* Place temperature particles in their initial position */
	for (int i = 0; i < AMOUNT_PARTICLES; i++) {
		PARTICLE *current_particle = &instance->particles[i];
		current_particle->x = OLED_WIDTH - i*(OLED_WIDTH/AMOUNT_PARTICLES);
		current_particle->y = float_to_screen_y(instance->temperature);
		int mod = i * (255 / AMOUNT_PARTICLES);
		current_particle->rgb = RGB((255-mod),mod,mod);
		current_particle->type = 0;
	}

	/* Write initial debug info */
	sprintf(instance->debug_info.bottom, "[]");

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
 * Update the particles responsible for showing the temperature chart.
 * TODO implement this for each device registering in the kafka alive topic.
 */
int update_temperature (INSTANCE *instance, float temperature) 
{
	PARTICLE *current_temperature;
	PARTICLE *previous_temperature;

	instance->temperature = temperature;

	/* Code responsible for adjusting the line chart when time elapses */
	for (int i = AMOUNT_PARTICLES - 1; i > 0; i--) {
		current_temperature = &instance->particles[i];
		previous_temperature = &instance->particles[i-1];
		current_temperature->y = previous_temperature->y;
	}
	instance->particles[0].y = float_to_screen_y(instance->temperature);
	sprintf(instance->debug_info.bottom, "%.1f'C", instance->temperature);
	
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
static int render_termometer(const INSTANCE *instance) 
{
	PARTICLE *current_temperature;
	PARTICLE *previous_temperature;
	for (int i = 0; i < AMOUNT_PARTICLES; i++) {
		current_temperature = &instance->particles[i];
	    previous_temperature = &instance->particles[i < AMOUNT_PARTICLES - 1 ? i+1 : 0];
		if (i < AMOUNT_PARTICLES - 1) {
			SSD1331_line(current_temperature->x, current_temperature->y, 
						previous_temperature->x, previous_temperature->y, 
						current_temperature->rgb);
		}
	}

	return 1;
}

/**
 * Draw the debug text.
 */
static int render_debug(const INSTANCE *instance) 
{
	if (SHOW_TOP_DEBUG) { 
		SSD1331_string(0, TOP_DEBUG_STRING_Y, instance->debug_info.top, 12, 1, RGB(255,255,0));
	}
	if (SHOW_BOTTOM_DEBUG) {
		SSD1331_string(0, BOTTOM_DEBUG_STRING_Y, instance->debug_info.bottom, 12, 1, BOTTOM_DEBUG_RGB);
	}

	return 1;
}

/**
 * Draw all screen components
 */
int render(const INSTANCE *instance, const float ms) 
{
	SSD1331_clear();

	render_background(instance);	
	render_termometer(instance);
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
		if (rkm->payload && is_printable(rkm->payload, rkm->len)){
				printf(" Value: %.*s\n",
					   (int)rkm->len, (const char *)rkm->payload);
		/* Copy message text into the payload pointer */
				sprintf(payload, (const char *)rkm->payload);
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
			sprintf(instance->debug_info.bottom, "[%s]", latest_message_text);
			count_ms = 0;
		}

		/* FPS calculations */
		elapsed_ms = current_ms - previous_ms;
		previous_ms = current_ms;
		count_ms += elapsed_ms;
		lag_ms += elapsed_ms;

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
