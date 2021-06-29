#ifndef DC_DUMP_DUMP_H
#define DC_DUMP_DUMP_H


/*
 * This file is part of dc_dump.
 *
 *  dc_dump is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Foobar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with dc_dump.  If not, see <https://www.gnu.org/licenses/>.
 */


#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <dc_posix/stdlib.h>


struct dc_dump_info;


/**
 *
 * @param fd
 * @param file_size
 * @return
 */
struct dc_dump_info *dc_dump_dump_info_create(const struct dc_posix_env *env, int fd, off_t file_size);

/**
 *
 * @param pinfo
 */
void dc_dump_dump_info_destroy(struct dc_dump_info **pinfo);

/**
 *
 * @param item
 * @param line_position
 * @param count
 * @param file_position
 * @param arg
 */
void dc_dumper(uint8_t item, size_t line_position, size_t count, size_t file_position, void *arg);


#endif // DC_DUMP_DUMP_H
