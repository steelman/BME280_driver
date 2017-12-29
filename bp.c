/*
 * Access I²C devices through a BusPirate device
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "log.h"

#define BP_I2C_OK       (0)
#define BP_I2C_ERROR    (-1)

#define min(a,b) (((a) < (b)) ? (a) : (b))

static int port_fd = -1;

int bp_i2c_close() {
	write(port_fd,"\x00\x0f",2);
	return close(port_fd);
}

int bp_i2c_init(const char* path)
{
	int fd=-1;
	int ret = BP_I2C_ERROR;
	char buf[64];
	struct termios term;

	fd = open(path, O_RDWR);
	if (fd < 0) {
		log_error_errno(errno, "Unable to open \"%s\": %m", path);
		return ret;
	}

	ret = tcgetattr(fd, &term);
	cfsetospeed(&term, B115200);
	cfsetispeed(&term, B115200);
	term.c_cflag &= ~CRTSCTS;
	ret = tcsetattr(fd, TCSANOW, &term);
	
	ret=write(fd, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 20);
	if (ret < 0) {
		log_error_errno(errno, "Error sending command: %m");
		goto error;
	} else if (ret != 20) {
		log_error("Error entering BusPirate binary mode.");
		goto error;
	}
	
	ret=read(fd, buf, 5);
	if (ret < 0) {
		log_error_errno(errno, "Error reading response: %m");
		goto error;
	} else if (ret != 5 || (memcmp("BBIO1", buf, 5) != 0)) {
		log_error("Invalid response");
		goto error;
	}
	
	ret=write(fd, "\x02\x48\x60", 3);
	if (ret < 0) {
		log_error_errno(errno, "Error sending command: %m");
		goto error;
	} else if (ret != 3) {
		log_error("Error entering I²C mode.");
		goto error;
	}

	ret=read(fd, buf, 6);
	if (ret < 0) {
		log_error_errno(errno, "Error reading response: %m");
		goto error;
	} else if (ret != 6 || (memcmp("I2C1\x01\x01", buf, 6) != 0)) {
		log_error("Invalid response");
		goto error;
	}

	port_fd = fd;
	return BP_I2C_OK;
error:
	close(fd);
	return BP_I2C_ERROR;
}

#ifndef BP_WRITE_AND_READ
int8_t bp_i2c_write(uint8_t dev_id, uint8_t reg_addr,
		    uint8_t *data, uint16_t len)
{
	uint8_t buf[16];
	int ret;

	if (len > 126) {
		log_error("Too much data to write.");
		return BP_I2C_ERROR;
	}

	memcpy(buf, "\x02\x11\xAA\xBB", 4);
	buf[2] = dev_id << 1;
	buf[3] = reg_addr;
	ret = write(port_fd, buf, 4);
	if (ret < 0 || ret != 4)
		return BP_I2C_ERROR;
	ret = read(port_fd, buf, sizeof(buf));
	if (ret < 0 || ret != 4)
		return BP_I2C_ERROR;

	do {
		int l = min(15, len);
		buf[0] = 0x10 | (l & 0x0f);
		ret = write(port_fd, buf, 1);
		ret = read(port_fd, buf, sizeof(buf));
		ret = write(port_fd, data, l);
		ret = read(port_fd, buf, sizeof(buf));
		data += l;
		len -= l;
	} while (len);
	ret = write(port_fd, "\x03", 1);
	if (ret < 0 || ret != 1)
		return BP_I2C_ERROR;
	ret = read(port_fd, buf, sizeof(buf));
	if (ret < 0 || ret != 1)
		return BP_I2C_ERROR;

	return BP_I2C_OK;
}

int8_t bp_i2c_read(uint8_t dev_id, uint8_t reg_addr,
			uint8_t *data, uint16_t len)
{
	uint8_t buf[16];
	int ret;

	memcpy(buf, "\x02\x11\xAA\xBB\x03",5);
	buf[2] = (dev_id << 1);
	buf[3] = reg_addr;
	ret = write(port_fd, buf, 5);
	if (ret < 0 || ret != 5)
		return BP_I2C_ERROR;
	ret = read(port_fd, buf, sizeof(buf));
	if (ret < 0 || ret != 5)
		return BP_I2C_ERROR;
  
	memcpy(buf, "\x02\x10\xAA", 3);
	buf[2] = (dev_id << 1) | 1;
	
	ret = write(port_fd, buf, 3);
	if (ret < 0 || ret != 3)
		return BP_I2C_ERROR;
	ret = read(port_fd, buf, sizeof(buf));
	if (ret < 0 || ret != 3)
		return BP_I2C_ERROR;

	for (; len; len--) {
		ret = write(port_fd, "\x04\x06", 2); /* read byte */
		ret = read(port_fd, buf, sizeof(buf));
		*data = buf[0];
		data += 1;
	} while (len);
	ret = write(port_fd, "\x04\x07\03", 3); /* read no more */
	if (ret < 0 || ret != 3)
		return BP_I2C_ERROR;
	ret = read(port_fd, buf, sizeof(buf));
	if (ret < 0 || ret != 3)
		return BP_I2C_ERROR;

	return BP_I2C_OK;
}

#else  /* BP_WRITE_AND_READ */

static int8_t do_i2c_write_and_read(uint8_t *out, uint16_t olen,
				    uint8_t *in, uint16_t ilen)
{
	int ret;
	uint8_t t;
	uint8_t _len[2];

	if (ilen > 0 && in == NULL)
		return BP_I2C_ERROR;
	
	ret = write(port_fd, "\x08", 1);
	if (ret < 0) {
		log_error_errno(errno, "Error sending 0x08 command: %m");
		return BP_I2C_ERROR;
	} else if (ret != 1) {
		log_error("Error sending 0x08 command.");
		return BP_I2C_ERROR;
	}

	_len[0] = (olen >> 8);
	_len[1] = (olen & 0xff);
	
	ret = write(port_fd, _len, 2);
	if (ret < 0) {
		log_error_errno(errno, "Error writing output length: %m");
		return BP_I2C_ERROR;
	} else if (ret != 2) {
		log_error("Error writing output length: %m");
		return BP_I2C_ERROR;
	}

	_len[0] = (ilen >> 8);
	_len[1] = (ilen & 0xff);

	ret = write(port_fd, _len, 2);
	if (ret < 0) {
		log_error_errno(errno, "Error writing output length: %m");
		return BP_I2C_ERROR;
	} else if (ret != 2) {
		log_error("Error writing output length: %m");
		return BP_I2C_ERROR;
	}
	
	if (olen > 0) {
		ret = write(port_fd, out, olen);
		if (ret < 0) {
			log_error_errno(errno, "Error writing data: %m");
			return BP_I2C_ERROR;
		} else if (ret != olen) {
			log_error("Error writing data.");
			return BP_I2C_ERROR;
		}
	}

	ret = read(port_fd, &t, 1);
	if (ret < 0) {
		log_error_errno(errno, "Error reading response: %m");
		return BP_I2C_ERROR;
	} else if (ret != 1) {
		log_error("Error reading response.");
		return BP_I2C_ERROR;
	}

	if (t == 0) {
		log_error("BP command 0x08 failed.");
		return BP_I2C_ERROR;
	}
	
	if (ilen > 0) {
		do {
			int i=min(16,ilen);
			ret = read(port_fd, in, i);
			if (ret < 0) {
				log_error_errno(errno, "Error reading data: %m");
				return BP_I2C_ERROR;
			}
			ilen -= ret;
		} while (ilen);
	}

	return BP_I2C_OK;
}

static int8_t i2c_read(uint8_t dev_id, uint8_t reg_addr,
		       uint8_t *data, uint16_t len)
{
	int ret;
	int i;
	uint8_t buf[3];
	
	if (data == NULL)
		return BP_

	buf[0] = (dev_id << 1);
	buf[1] = reg_addr;
	ret = do_i2c_write_and_read(buf, 2, NULL, 0);
	if (ret != BP_I2C_OK)
		return ret;
	buf[0] = (dev_id << 1) | 1;
	do {
		int t = min(16, len);
		do_i2c_write_and_read(buf, 1, data, t);
		len -= t;
		data += t;
	} while (len);
	return BP_I2C_OK;
}

static int8_t i2c_write(uint8_t dev_id, uint8_t reg_addr,
			uint8_t *data, uint16_t len)
{
	int ret;
	int i;
	uint8_t t;
	uint8_t buf[4096];

	if (data == NULL && len !=0)
		return BME280_E_NULL_PTR;

	buf[0] = (dev_id << 1);
	buf[1] = reg_addr;
	memcpy(&buf[2], data, min(len, 4094));

	return do_i2c_write_and_read(buf, len + 2, NULL, 0);	
}
#endif
