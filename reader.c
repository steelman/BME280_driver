/*
 * Read measurements from BME280 sensor with a BusPirate.
 *
 * Copyright (C) 2017 Łukasz Stelmach
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BME280_64BIT_ENABLE
#include "bme280.h"
#include "bp.h"
#include "log.h"

static int keep_running = 1;
static int delay = 5000;	/* milliseconds */

static void delay_ms(uint32_t period)
{
	struct timespec t;
	t.tv_sec = period / 1000;
	t.tv_nsec = (period % 1000) * 1000000;
	nanosleep(&t, NULL);
}

void stop_running(int signum, siginfo_t *si, void *uctx)
{
	log_debug("Signal %d received.", signum);
	keep_running = 0;
}

int help(const char *prog, int ret)
{
	fprintf(stderr, "Usage: %s [OPTION] DEVICE\n", prog);
	return ret;
}

int version()
{
	fprintf(stderr, "BusPirate BMP280 reader " BP_I2C_VERSION "\n"
		"Copyright © 2016 - 2017 Bosch Sensortec GmbH\n"
		"Copyright © 2017 Łukasz Stelmach\n"
		"Copyright © 2017 Samsung Electronics\n\n"
		"License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n");
	return 0;
}

int main(int ac, char *av[])
{
	struct bme280_dev sensor;
	struct bme280_data data;
	struct sigaction act;
	int ret = -1;
	uint8_t settings_sel;
	int c;

	enum {
		ARG_VERSION = 0x100,
		ARG_DELAY,
	};
	static const struct option options[] = {
		{"help",    no_argument, NULL, 'h' },
		{"version", no_argument, NULL, ARG_VERSION },
		{"oneshot", no_argument, NULL, '1' },
		{"debug",   no_argument, NULL, 'd' },
		{"delay",   required_argument, NULL, ARG_DELAY }
	};

	while (1) {
		c = getopt_long(ac, av, "1dh", options, NULL);
		if (c == -1)
			break;
		switch(c) {
		case 'h':
			return help(av[0], 0);
		case ARG_VERSION:
			return version();
		case 'd':
			log_set_max_level(LOG_DEBUG);
			break;
		case '1':
			keep_running = 0;
			delay = 0;
			break;
		case ARG_DELAY: {
			char *p;
			delay = strtol(optarg, &p, 10);
			if (*p != '\0')
				return help(av[0], -1);
			delay = (delay < 100) ? 100 : delay;
		}

		}
	}

	if (optind >= ac) {
		return help(av[0], -1);
	}

	ret = bp_i2c_init(av[optind]);
	if (ret < 0) {
		log_error("bp_i2c_init failed.");
		goto out;
	}

	sensor.dev_id = 0x77;
	sensor.intf = BME280_I2C_INTF;
	sensor.read = bp_i2c_read;
	sensor.write = bp_i2c_write;
	sensor.delay_ms = delay_ms;

	ret = bme280_init(&sensor);
	log_debug("chip_id: 0x%x", sensor.chip_id);
	if (ret != BME280_OK) {
		log_error("bme280_init failed: %d", ret);
		goto error;
	}

	sensor.settings.osr_h = BME280_OVERSAMPLING_1X;
	sensor.settings.osr_p = BME280_OVERSAMPLING_16X;
	sensor.settings.osr_t = BME280_OVERSAMPLING_1X;
	sensor.settings.filter = BME280_FILTER_COEFF_OFF;

	settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

	ret = bme280_set_sensor_settings(settings_sel, &sensor);

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = stop_running;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &act, NULL);

	do {
		uint8_t reg_data[ BME280_P_T_H_DATA_LEN];
		ret = bme280_set_sensor_mode(BME280_FORCED_MODE, &sensor);
		if (ret != BME280_OK) {
			log_error("Error setting forced mode: %d", ret);
			break;
		}
		sensor.delay_ms(50);
		memset(&data, 0, sizeof(data));
		ret = bme280_get_regs(BME280_DATA_ADDR, reg_data, BME280_P_T_H_DATA_LEN, &sensor);
		log_debug("%x%x%x%x%x%x%x%x",
		       reg_data[0],reg_data[1],reg_data[2],reg_data[3],
		       reg_data[4],reg_data[5],reg_data[6],reg_data[7]);
		ret = bme280_get_sensor_data(BME280_ALL, &data, &sensor);
		log_info("pressure: %d    temperature: %d    humidity: %d",
			 data.pressure, data.temperature, data.humidity);
		fprintf(stdout, "pressure %d\ntemperature %d\nhumidity %d\n",
			data.pressure, data.temperature, data.humidity);
		sensor.delay_ms(delay);
	} while (keep_running);

error:
	bp_i2c_close();
out:
	return ret;
}
