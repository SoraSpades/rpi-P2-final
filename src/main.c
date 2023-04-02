/*
 *  main.c - Main file of the project
 *
 *  Created on: Mar 21=02023
 *      Author: David Andrino & Fernando Sanz
 */
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include "Accelerometer/accelerometer.h"
#include "ColorSensor/colorSensor.h"

#define DEBUG

static volatile int gb_stop = 0;
static pthread_mutex_t g_stop_mutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_acc_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_color_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_data_mutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_stop_cond    = PTHREAD_COND_INITIALIZER;
static pthread_cond_t g_acc_cond     = PTHREAD_COND_INITIALIZER;
static pthread_cond_t g_color_cond   = PTHREAD_COND_INITIALIZER;
static pthread_cond_t g_data_cond    = PTHREAD_COND_INITIALIZER;

static acc_t            g_acceleration;
static rgb_color_t      g_rgb_color;
static complete_color_t g_complete_color;
static volatile int gb_data_ready = 0;

static sigset_t g_stopsignals;

/**
 * Thread safe get of the g_stop variable
*/
static inline int get_stop() {
	pthread_sigmask(SIG_BLOCK, &g_stopsignals, NULL);
	pthread_mutex_lock(&g_stop_mutex);
	int tmp_stop = gb_stop;
	pthread_mutex_unlock(&g_stop_mutex);
	pthread_sigmask(SIG_UNBLOCK, &g_stopsignals, NULL);
	return tmp_stop;
}

void sigint_isr(int signal) {
	#ifdef DEBUG
		printf("[DEBUG] SIGINT received");
	#endif
	gb_stop = 1;
}

void* acc_thread_fn(void *ptr) {
	#ifdef DEBUG
		printf("[DEBUG] Begin of acceleration thread");
	#endif
	acc_init();
	#ifdef DEBUG
		printf("[DEBUG] Acceleration sensor inited");
	#endif
	while (!get_stop()) {
		// Read acceleration from the sensor
		pthread_mutex_lock(&g_acc_mutex);
			acc_read(&g_acceleration);
		pthread_mutex_unlock(&g_acc_mutex);

		// Change data_ready flag to indicate the display thread to read
		pthread_mutex_lock(&g_data_mutex);
			gb_data_ready = 1;
			pthread_cond_signal(&g_data_cond);
		pthread_mutex_unlock(&g_data_mutex);
		usleep(500000);
	}
	#ifdef DEBUG
		printf("[DEBUG] Closing acceleration sensor");
	#endif
	acc_close();
	#ifdef DEBUG
		printf("[DEBUG] End of acceleration thread");
	#endif
	pthread_exit(NULL);
}

void* color_thread_fn(void *ptr) {
	#ifdef DEBUG
		printf("[DEBUG] Begin of color thread");
	#endif
	cs_init();
	#ifdef DEBUG
		printf("[DEBUG] Color sensor inited");
	#endif
	while (!get_stop()) {
		// Read acceleration from the sensor
		pthread_mutex_lock(&g_acc_mutex);
			cs_read_clear_corrected(&g_rgb_color);
		pthread_mutex_unlock(&g_acc_mutex);

		// Change data_ready flag to indicate the display thread to read
		pthread_mutex_lock(&g_data_mutex);
			gb_data_ready = 1;
			pthread_cond_signal(&g_data_cond);
		pthread_mutex_unlock(&g_data_mutex);
		usleep(500000);
	}
	#ifdef DEBUG
		printf("[DEBUG] Closing color sensor");
	#endif
	cs_close();
	#ifdef DEBUG
		printf("[DEBUG] End of color thread");
	#endif
	pthread_exit(NULL);
}

void* display_thread_fn(void *ptr) {
	#ifdef DEBUG
		printf("[DEBUG] Begin of display thread");
	#endif

	printf("Raspberry Pi sensing application - By David Andrino and Fernando Sanz\n");
	while (!get_stop()) {
		pthread_mutex_lock(&g_data_mutex);
			while (!gb_data_ready) pthread_cond_wait(&g_data_cond, &g_data_mutex);

			pthread_mutex_lock(&g_acc_mutex);
			pthread_mutex_lock(&g_color_mutex);
				printf("\033[F\033[F\033[F\033[F\033[F\033[F\033[F\033[F\rAcceleration:\n\tX: %.02f\n\tY: %.02f\n\tZ: %.02f\nColor:\n\tR: %03d\n\tG: %03d\n\tB: %03d",
				g_acceleration.x, g_acceleration.y, g_acceleration.z,
				g_rgb_color.r, g_rgb_color.g, g_rgb_color.b);
			pthread_mutex_unlock(&g_acc_mutex);
			pthread_mutex_unlock(&g_color_mutex);

		pthread_mutex_unlock(&g_data_mutex);
	}
	#ifdef DEBUG
		printf("[DEBUG] End of display thread");
	#endif
	pthread_exit(NULL);
}

void* input_thread_fn(void *ptr) {
	#ifdef DEBUG
		printf("[DEBUG] Begin of input thread");
	#endif

	while (!get_stop()) {
		/* char c = getc(stdin);
		switch (c) {
			case 'q': 
				pthread_sigmask(SIG_BLOCK, &g_stopsignals, NULL);
				pthread_mutex_lock(&g_stop_mutex);
				int gb_stop = 1;
				pthread_mutex_unlock(&g_stop_mutex);
				pthread_sigmask(SIG_UNBLOCK, &g_stopsignals, NULL);
				break;
			default:
				printf("\033[F\033[F\033[F\033[F\033[F\033[F\033[F\033[F\rPressed %c\n\n\n\n\n\n\n", c);
				break;
		} */
		sleep(0.5);
	}

	#ifdef DEBUG
		printf("[DEBUG] End of input thread");
	#endif

	pthread_exit(NULL);
}

int main(int argc, char** argv) {
	sigemptyset(&g_stopsignals);
	sigaddset(&g_stopsignals, SIGINT);

	pthread_t acc_tid, color_tid, display_tid, input_tid; 

	signal(SIGINT, sigint_isr);

	#ifdef DEBUG
		printf("[DEBUG] Creating Threads");
	#endif
	pthread_create(&acc_tid,     NULL, acc_thread_fn,     NULL);
	pthread_create(&color_tid,   NULL, color_thread_fn,   NULL);
	pthread_create(&input_tid,   NULL, input_thread_fn,   NULL);
	pthread_create(&display_tid, NULL, display_thread_fn, NULL);


	// Wait for stop to be activated
	pthread_mutex_lock(&g_stop_mutex);
		while (gb_stop == 0) pthread_cond_wait(&g_stop_cond, &g_stop_mutex);
	pthread_mutex_unlock(&g_stop_mutex);

	#ifdef DEBUG
		printf("[DEBUG] Exiting threads");
	#endif
	// Wait for the threads to exit
	pthread_join(acc_tid,     NULL);
	pthread_join(color_tid,   NULL);
	pthread_join(input_tid,   NULL);
	pthread_join(display_tid, NULL);
	#ifdef DEBUG
		printf("[DEBUG] All threads finished, exiting program");
	#endif

	exit(0);
}
