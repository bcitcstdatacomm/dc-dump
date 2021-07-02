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


#include "dump.h"
#include <dc_application/command_line.h>
#include <dc_application/environment.h>
#include <dc_application/config.h>
#include <dc_application/defaults.h>
#include <dc_application/options.h>
#include <dc_util/streams.h>
#include <dc_util/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"
#define __USE_POSIX 1
#pragma GCC diagnostic pop
#include <string.h>


struct application_settings
{
    struct dc_opt_settings opts;
    struct dc_setting_bool *verbose;
    struct dc_setting_path *input_path;
    struct dc_setting_path *output_path;
    struct dc_setting_path *dump_path;
};


static struct dc_application_lifecycle *create_lifecycle(const struct dc_posix_env *env);
static struct dc_application_settings *create_settings(const struct dc_posix_env *env);
static int destroy_settings(struct dc_application_settings **psettings);
static int run(struct dc_application_settings *settings);
static void error_reporter(const char *file_name, const char *function_name, size_t line_number, int err);
static void trace(const char *file_name, const char *function_name, size_t line_number);
static off_t link_stdin(struct dc_setting_path *path);
static int link_stdout(struct dc_setting_path *path);
static int open_out(struct dc_setting_path *path);


int main(int argc, char *argv[])
{
    struct dc_posix_env         env;
    struct dc_application_info *info;
    int                         ret_val;

    dc_posix_env_init(&env, error_reporter);
//    env.tracer = trace;
    info      = dc_application_info_create("Test Application", NULL, &env);
    ret_val   = dc_application_run(info, create_lifecycle, "~/.dcdump.conf", argc, argv);
    dc_application_info_destroy(&info);

    return ret_val;
}


static struct dc_application_lifecycle *create_lifecycle(const struct dc_posix_env *env)
{
    struct dc_application_lifecycle *lifecycle;

    lifecycle = dc_application_lifecycle_create(env, create_settings, destroy_settings, run);
    dc_application_lifecycle_set_parse_command_line(lifecycle, dc_default_parse_command_line);
    dc_application_lifecycle_set_read_env_vars(lifecycle, dc_default_read_env_vars);
    dc_application_lifecycle_set_read_config(lifecycle, dc_default_load_config);
    dc_application_lifecycle_set_set_defaults(lifecycle, dc_default_set_defaults);

    return lifecycle;
}

static struct dc_application_settings *create_settings(const struct dc_posix_env *env)
{
    static bool                  default_verbose = false;
    struct application_settings *settings;
    int                          err;

    settings = dc_malloc(env, &err, sizeof(struct application_settings));

    if(settings == NULL)
    {
        return NULL;
    }

    settings->opts.parent.env         = env;
    settings->opts.parent.config_path = dc_setting_path_create();
    settings->verbose                 = dc_setting_bool_create();
    settings->input_path              = dc_setting_path_create();
    settings->output_path             = dc_setting_path_create();
    settings->dump_path               = dc_setting_path_create();

    struct options opts[] =
    {
        { (struct dc_setting *)settings->opts.parent.config_path, dc_options_set_path, "config",  required_argument, 'c', "CONFIG",  dc_string_from_string, NULL,      dc_string_from_config, NULL             },
        { (struct dc_setting *)settings->verbose,                 dc_options_set_bool, "verbose", no_argument,       'v', "VERBOSE", dc_flag_from_string,   "verbose", dc_flag_from_config,   &default_verbose },
        { (struct dc_setting *)settings->input_path,              dc_options_set_path, "in",      required_argument, 'i', "IN",      dc_string_from_string, "in",      dc_string_from_config, NULL             },
        { (struct dc_setting *)settings->output_path,             dc_options_set_path, "out",     required_argument, 'o', "OUT",     dc_string_from_string, "out",     dc_string_from_config, NULL             },
        { (struct dc_setting *)settings->dump_path,               dc_options_set_path, "dump",    required_argument, 'd', "DUMP",    dc_string_from_string, "dump",    dc_string_from_config, NULL             },
    };

    // note the trick here - we use calloc and add 1 to ensure the last line is all 0/NULL
    settings->opts.opts = dc_calloc(env, &err, (sizeof(opts) / sizeof(struct options)) + 1, sizeof(struct options));
    memcpy(settings->opts.opts, opts, sizeof(opts));
    settings->opts.flags = "c:vi:o:d:";
    settings->opts.env_prefix = "DC_DUMP_";

    return (struct dc_application_settings *)settings;
}

static int destroy_settings(struct dc_application_settings **psettings)
{
    struct application_settings *app_settings;

    app_settings = (struct application_settings *)*psettings;
    dc_setting_bool_destroy(&app_settings->verbose);
    dc_setting_path_destroy(&app_settings->input_path);
    dc_setting_path_destroy(&app_settings->output_path);
    dc_setting_path_destroy(&app_settings->dump_path);
    free(app_settings->opts.opts);
    memset(*psettings, 0, sizeof(struct application_settings));
    free(*psettings);
    *psettings = NULL;

    return 0;
}

static int run(struct dc_application_settings *settings)
{
    struct application_settings *app_settings;
    int                          ret_val;
    off_t                        max_position;
    int                          fd_dump;
    int                          fd_out;
    struct dc_stream_copy_info  *copy_info;
    struct dc_dump_info         *info;


    app_settings = (struct application_settings *)settings;
    ret_val      = 0;
    max_position = link_stdin(app_settings->input_path);

    if(max_position < 0)
    {
        fprintf(stderr, "Can't open file %s\n", dc_setting_path_get(app_settings->input_path));
        return -1;
    }

    fd_dump = link_stdout(app_settings->dump_path);

    if(fd_dump < 0)
    {
        fprintf(stderr, "Can't open file %s\n", dc_setting_path_get(app_settings->dump_path));
        return -1;
    }

    fd_out = open_out(app_settings->output_path);

    if(fd_out < 0)
    {
        fprintf(stderr, "Can't open file %s\n", dc_setting_path_get(app_settings->input_path));
        return -1;
    }

    info = dc_dump_dump_info_create(settings->env, STDOUT_FILENO, max_position);
    copy_info = dc_stream_copy_info_create(settings->env, NULL, dc_dumper, info, NULL, NULL);
    dc_stream_copy(settings->env, STDIN_FILENO, fd_out, 1024, copy_info);
    dc_stream_copy_info_destroy(settings->env, &copy_info);
    dc_dump_dump_info_destroy(&info);

    return ret_val;
}

static off_t link_stdin(struct dc_setting_path *setting)
{
    off_t max_position;

    if(dc_setting_is_set((struct dc_setting *)setting))
    {
        const char *path;
        int         fd;
        struct stat file_info;

        path = dc_setting_path_get(setting);
        fd   = open(path, O_RDONLY);

        if(fd < 0)
        {
            max_position = -1;
        }
        else
        {
            dup2(fd, STDIN_FILENO);
            fstat(fd, &file_info);
            max_position = file_info.st_size;
        }
    }
    else
    {
        max_position = dc_max_off_t();
    }

    return max_position;
}


static int link_stdout(struct dc_setting_path *setting)
{
    int ret_val;

    ret_val = 0;

    if(dc_setting_is_set((struct dc_setting *)setting))
    {
        const char *path;

        path = dc_setting_path_get(setting);
        ret_val = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);

        if(ret_val > 0)
        {
            dup2(ret_val, STDOUT_FILENO);
        }
    }

    return ret_val;
}

static int open_out(struct dc_setting_path *setting)
{
    const char *path;
    int         fd;

    if(dc_setting_is_set((struct dc_setting *)setting))
    {
        path = dc_setting_path_get(setting);
    }
    else
    {
        path = "/dev/null";
    }

    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);

    return fd;
}

static void error_reporter(const char *file_name, const char *function_name, size_t line_number, int err)
{
    fprintf(stderr, "ERROR: %s : %s : @ %zu : %d\n", file_name, function_name, line_number, err);
}

__attribute__ ((unused)) static void trace(const char *file_name, const char *function_name, size_t line_number)
{
    fprintf(stderr, "TRACE: %s : %s : @ %zu\n", file_name, function_name, line_number);
}
