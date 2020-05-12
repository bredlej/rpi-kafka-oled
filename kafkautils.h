#ifndef _KAFKAUTILS_H_
#define _KAFKAUTILS_H_
#include <librdkafka/rdkafka.h>

rd_kafka_t *init_kafka_handler(const char *, const char *, int , char **);
#endif
