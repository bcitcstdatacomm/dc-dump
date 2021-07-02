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


#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <dc_util/bits.h>
#include "dump.h"


static const char *lookup_control(uint8_t c);


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
struct dc_dump_info
{
    int fd;
    size_t max_position;
    size_t line_number;
    size_t line_position;
    char *line_format;
    char *line_buffer;
};
#pragma GCC diagnostic pop



struct dc_dump_info *dc_dump_dump_info_create(const struct dc_posix_env *env, int fd, off_t file_size)
{
    struct dc_dump_info *info;
    size_t               format_size;
    const char          *format;
    int                  err;

    printf("%lld\n", file_size);

    info = dc_calloc(env, &err, 1, sizeof(struct dc_dump_info));

    if(info == NULL)
    {
    }

    info->fd            = fd;
    info->line_number   = 1;
    info->line_position = 1;

    // TODO - breaks for 999999999999999997 - 999999999999999999

    printf("log10l       = %Lf\n", log10l(file_size));
    printf("log10l + 1   = %Lf\n", (log10l(file_size) + 1.0l));
    info->max_position = (size_t)(log10l(file_size) + 1.0l);
    printf("max_position = %zu\n", info->max_position);

    // NOTE: this will be controlled by options in the future
    // file pos line # line pos : binary : octal : decimal : hex : ascii or
    // max_digits max_digits max_digits : 11111111 : 0377 : 255 : 0xFF : ????
    format = "%*d %*d %*d : %08s : 0%03o : %03d : 0x%02X : %-4s";
    info->line_format = dc_malloc(env, &err, strlen(format) + 1);

    if(info->line_format == NULL)
    {
    }

    strcpy(info->line_format, format);

    // 3 * "%*d " where * is info->max_position
    // ": 11111111 " for binary (11)
    // ": 0### " for octal (7)
    // ": ### " for decimal (6)
    // ": 0x### " for hex (8)
    // ": ????" for the ASCII value (6)
    // '\0' + 1
    format_size = (3 * (info->max_position + 1)) + 11 + 7 + 6 + 8 + 6 + 1;
printf("%lu\n", format_size);
    info->line_buffer = dc_malloc(env, &err, format_size);

    if(info->line_buffer == NULL)
    {
    }

    return info;
}

void dc_dump_dump_info_destroy(struct dc_dump_info **pinfo)
{
    free((*pinfo)->line_format);
    free((*pinfo)->line_buffer);
    memset(*pinfo, 0, sizeof(struct dc_dump_info));
    free(*pinfo);
    *pinfo = NULL;
}

void dc_dumper(uint8_t item, __attribute__((unused)) size_t line_position, __attribute__((unused)) size_t count, size_t file_position, void *arg)
{
    struct dc_dump_info *info;
    bool                 bits[8];
    char                 binary[9];
    char                 printable[4];

    dc_to_binary8(item, bits);
    dc_to_printable_binary8(bits, binary);
    info = arg;

    if(isprint(item))
    {
        printable[0] = (char)item;
        printable[1] = '\0';
    }
    else if(iscntrl(item))
    {
        const char *temp;

        temp = lookup_control(item);
        strcpy(printable, temp);
    }
    else
    {
        printable[0] = '\0';
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    sprintf(info->line_buffer, info->line_format,
            info->max_position, file_position,
            info->max_position, info->line_number,
            info->max_position, info->line_position,
            binary, item, item, item, printable);
#pragma GCC diagnostic pop
    write(info->fd, info->line_buffer, strlen(info->line_buffer));
    write(info->fd, "\n", 1);

    if(item == '\n')
    {
        info->line_number++;
        info->line_position = 1;
    }
    else
    {
        info->line_position++;
    }
}


static const char *lookup_control(uint8_t c)
{
    // https://en.wikipedia.org/wiki/List_of_Unicode_characters#Control_codes

    static const char *LOW_VALUES[] =
            {
                    "NUL",  // 0
                    "SOH",  // 1
                    "STX",  // 2
                    "ETX",  // 3
                    "EOT",  // 4
                    "ENQ",  // 5
                    "ACK",  // 6
                    "BEL",  // 7
                    "BS",   // 8
                    "\\t",  // 9
                    "\\n",  // 10
                    "VT",   // 11
                    "FF",   // 12
                    "\\r",  // 13
                    "SO",   // 14
                    "SI",   // 15
                    "DLE",  // 16
                    "DC1",  // 17
                    "DC2",  // 18
                    "DC3",  // 19
                    "DC4",  // 20
                    "NAK",  // 21
                    "SYN",  // 22
                    "ETB",  // 23
                    "CAN",  // 24
                    "EM",   // 25
                    "SUB",  // 26
                    "ESC",  // 27
                    "FS",   // 28
                    "GS",   // 29
                    "RS",   // 30
                    "US",   // 31
            };
    static const char *HIGH_VALUES[] =
            {
                    "DEL",  // 127
                    "PAD",  // 128
                    "HOP",  // 129
                    "BPH",  // 130
                    "NBH",  // 131
                    "IND",  // 132
                    "NEL",  // 133
                    "SSA",  // 134
                    "ESA",  // 135
                    "HTS",  // 136
                    "HTJ",  // 137
                    "VTS",  // 138
                    "PLD",  // 139
                    "PLU",  // 140
                    "RI",   // 141
                    "SS2",  // 142
                    "SS3",  // 143
                    "DCS",  // 144
                    "PU1",  // 145
                    "PU2",  // 146
                    "STS",  // 147
                    "CCH",  // 148
                    "MW",   // 159
                    "SPA",  // 150
                    "EPA",  // 151
                    "SOS",  // 152
                    "SGCI", // 153
                    "SCI",  // 154
                    "CSI",  // 155
                    "ST",   // 156
                    "OCS",  // 157
                    "PM",   // 158
                    "APC",  // 159
            };
    const char *value;

    if(c <= 31)
    {
        value = LOW_VALUES[c];
    }
    else if(c >= 127 && c <= 159)
    {
        value = HIGH_VALUES[c];
    }
    else
    {
        value = "????";
    }

    return value;
}
