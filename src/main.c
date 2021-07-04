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


#include <dc_util/dump.h>
#include <dc_application/command_line.h>
#include <dc_application/environment.h>
#include <dc_application/config.h>
#include <dc_application/defaults.h>
#include <dc_application/options.h>
#include <dc_util/streams.h>
#include <dc_util/types.h>
#include <getopt.h>
// TODO get rid of the next two
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


static struct dc_application_lifecycle *create_lifecycle(const struct dc_posix_env *env, struct dc_error *err);
static struct dc_application_settings *create_settings(const struct dc_posix_env *env, struct dc_error *err);
static int destroy_settings(const struct dc_posix_env *env, struct dc_error *err, struct dc_application_settings **psettings);
static int run(const struct dc_posix_env *env, struct dc_error *err, struct dc_application_settings *settings);
static off_t link_stdin(const struct dc_posix_env *env, struct dc_setting_path *setting);
static int link_stdout(const struct dc_posix_env *env, struct dc_setting_path *setting);
static int open_out(const struct dc_posix_env *env, struct dc_setting_path *setting);
static void error_reporter(const struct dc_posix_env *env, const struct dc_error *err);
static void trace(const struct dc_posix_env *env, const char *file_name, const char *function_name, size_t line_number);


int main(int argc, char *argv[])
{
    struct dc_posix_env         env;
    struct dc_error             err;
    struct dc_application_info *info;
    int                         ret_val;

    dc_err_reset(&err);
    dc_posix_env_init(&env, error_reporter);
//    env.tracer = trace;
    info      = dc_application_info_create(&env, &err, "Test Application", NULL);
    ret_val   = dc_application_run(&env, &err, info, create_lifecycle, "~/.dcdump.conf", argc, argv);
    dc_application_info_destroy(&env, &info);

    return ret_val;
}


static struct dc_application_lifecycle *create_lifecycle(const struct dc_posix_env *env, struct dc_error *err)
{
    struct dc_application_lifecycle *lifecycle;

    DC_TRACE(env);
    lifecycle = dc_application_lifecycle_create(env, err, create_settings, destroy_settings, run);
    dc_application_lifecycle_set_parse_command_line(env, lifecycle, dc_default_parse_command_line);
    dc_application_lifecycle_set_read_env_vars(env, lifecycle, dc_default_read_env_vars);
    dc_application_lifecycle_set_read_config(env, lifecycle, dc_default_load_config);
    dc_application_lifecycle_set_set_defaults(env, lifecycle, dc_default_set_defaults);

    return lifecycle;
}

static struct dc_application_settings *create_settings(const struct dc_posix_env *env, struct dc_error *err)
{
    static bool                  default_verbose = false;
    struct application_settings *settings;

    DC_TRACE(env);
    settings = dc_malloc(env, err, sizeof(struct application_settings));

    if(settings == NULL)
    {
        return NULL;
    }

    settings->opts.parent.config_path = dc_setting_path_create(env, err);
    settings->verbose                 = dc_setting_bool_create(env, err);
    settings->input_path              = dc_setting_path_create(env, err);
    settings->output_path             = dc_setting_path_create(env, err);
    settings->dump_path               = dc_setting_path_create(env, err);

    struct options opts[] =
    {
        { (struct dc_setting *)settings->opts.parent.config_path, dc_options_set_path, "config",  required_argument, 'c', "CONFIG",  dc_string_from_string, NULL,      dc_string_from_config, NULL             },
        { (struct dc_setting *)settings->verbose,                 dc_options_set_bool, "verbose", no_argument,       'v', "VERBOSE", dc_flag_from_string,   "verbose", dc_flag_from_config,   &default_verbose },
        { (struct dc_setting *)settings->input_path,              dc_options_set_path, "in",      required_argument, 'i', "IN",      dc_string_from_string, "in",      dc_string_from_config, NULL             },
        { (struct dc_setting *)settings->output_path,             dc_options_set_path, "out",     required_argument, 'o', "OUT",     dc_string_from_string, "out",     dc_string_from_config, NULL             },
        { (struct dc_setting *)settings->dump_path,               dc_options_set_path, "dump",    required_argument, 'd', "DUMP",    dc_string_from_string, "dump",    dc_string_from_config, NULL             },
    };

    // note the trick here - we use calloc and add 1 to ensure the last line is all 0/NULL
    settings->opts.opts_count = (sizeof(opts) / sizeof(struct options)) + 1;
    settings->opts.opts_size  = sizeof(struct options);
    settings->opts.opts       = dc_calloc(env, err, settings->opts.opts_count, settings->opts.opts_size);
    memcpy(settings->opts.opts, opts, sizeof(opts));
    settings->opts.flags = "c:vi:o:d:";
    settings->opts.env_prefix = "DC_DUMP_";

    return (struct dc_application_settings *)settings;
}

static int destroy_settings(const struct dc_posix_env *env, __attribute__ ((unused)) struct dc_error *err, struct dc_application_settings **psettings)
{
    struct application_settings *app_settings;

    DC_TRACE(env);
    app_settings = (struct application_settings *)*psettings;
    dc_setting_bool_destroy(env, &app_settings->verbose);
    dc_setting_path_destroy(env, &app_settings->input_path);
    dc_setting_path_destroy(env, &app_settings->output_path);
    dc_setting_path_destroy(env, &app_settings->dump_path);
    dc_free(env, app_settings->opts.opts, app_settings->opts.opts_count);
    dc_free(env, *psettings, sizeof(struct application_settings));

    if(env->null_free)
    {
        *psettings = NULL;
    }

    return 0;
}

static int run(const struct dc_posix_env *env, struct dc_error *err, struct dc_application_settings *settings)
{
    struct application_settings *app_settings;
    int                          ret_val;
    off_t                        max_position;
    int                          fd_dump;
    int                          fd_out;
    struct dc_stream_copy_info  *copy_info;
    struct dc_dump_info         *info;

    DC_TRACE(env);
    app_settings = (struct application_settings *)settings;
    ret_val      = 0;
    max_position = link_stdin(env, app_settings->input_path);

    if(max_position < 0)
    {
        fprintf(stderr, "Can't open file %s\n", dc_setting_path_get(env, app_settings->input_path));
        return -1;
    }

    fd_dump = link_stdout(env, app_settings->dump_path);

    if(fd_dump < 0)
    {
        fprintf(stderr, "Can't open file %s\n", dc_setting_path_get(env, app_settings->dump_path));
        return -1;
    }

    fd_out = open_out(env, app_settings->output_path);

    if(fd_out < 0)
    {
        fprintf(stderr, "Can't open file %s\n", dc_setting_path_get(env, app_settings->input_path));
        return -1;
    }

    info      = dc_dump_info_create(env, err, STDOUT_FILENO, max_position);
    copy_info = dc_stream_copy_info_create(env, err, NULL, dc_dump_dumper, info, NULL, NULL);
    dc_stream_copy(env, err, STDIN_FILENO, fd_out, 1024, copy_info);
    dc_stream_copy_info_destroy(env, &copy_info);
    dc_dump_info_destroy(env, &info);

    return ret_val;
}

static off_t link_stdin(const struct dc_posix_env *env, struct dc_setting_path *setting)
{
    off_t max_position;

    DC_TRACE(env);

    if(dc_setting_is_set(env, (struct dc_setting *)setting))
    {
        const char *path;
        int         fd;
        struct stat file_info;

        path = dc_setting_path_get(env, setting);
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
        max_position = dc_max_off_t(env);
    }

    return max_position;
}


static int link_stdout(const struct dc_posix_env *env, struct dc_setting_path *setting)
{
    int ret_val;

    DC_TRACE(env);
    ret_val = 0;

    if(dc_setting_is_set(env, (struct dc_setting *)setting))
    {
        const char *path;

        path = dc_setting_path_get(env, setting);
        ret_val = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);

        if(ret_val > 0)
        {
            dup2(ret_val, STDOUT_FILENO);
        }
    }

    return ret_val;
}

static int open_out(const struct dc_posix_env *env, struct dc_setting_path *setting)
{
    const char *path;
    int         fd;

    DC_TRACE(env);

    if(dc_setting_is_set(env, (struct dc_setting *)setting))
    {
        path = dc_setting_path_get(env, setting);
    }
    else
    {
        path = "/dev/null";
    }

    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);

    return fd;
}

static void error_reporter(__attribute__ ((unused)) const struct dc_posix_env *env, const struct dc_error *err)
{
    fprintf(stderr, "ERROR: %s : %s : @ %zu : %d\n", err->file_name, err->function_name, err->line_number, 0);
}

__attribute__ ((unused)) static void trace(__attribute__ ((unused)) const struct dc_posix_env *env, const char *file_name, const char *function_name, size_t line_number)
{
    fprintf(stdout, "TRACE: %s : %s : @ %zu\n", file_name, function_name, line_number);
}
