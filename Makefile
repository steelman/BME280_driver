# Copyright (C) 2017  Łukasz Stelmach
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

BP_VERSION := -DBP_I2C_VERSION=\"$(shell git describe --tags --dirty)\"
CFLAGS = -g -O2 $(BP_VERSION) -Wall -Werror

bme280: reader.o bp.o log.o bme280.o
	$(CC) -o $@ $^

clean:
	@rm -f *.o
