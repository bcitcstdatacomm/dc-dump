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
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <fcntl.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#include <libconfig.h>
#pragma GCC diagnostic pop
#include <dc_posix/posix_env.h>
#include <dc_posix/stdlib.h>
#include <dc_util/streams.h>
#include <dc_util/types.h>
#include <dc_application/application.h>
#include <dc_application/settings.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"
#define __USE_POSIX 1
#pragma GCC diagnostic pop
#include <string.h>


struct application_settings
{
    struct dc_application_settings parent;
    struct dc_setting_bool *verbose;
    struct dc_setting_path *input_path;
    struct dc_setting_path *output_path;
    struct dc_setting_path *dump_path;
};


static int parse_command_line(struct dc_application_settings *settings, int argc, char *argv[]);
static int read_env_vars(struct dc_application_settings *settings, char **env);
static void set_from_env(struct application_settings *settings,
                         size_t prefix_len,
                         const char *key,
                         const char *value);
static int load_config(struct dc_application_settings *settings);
static int set_defaults(__attribute__((unused)) struct dc_application_settings *settings);
static void set_path(const struct dc_posix_env *env, struct dc_setting_path *setting, const char *path, dc_setting_type type);
static int run(struct dc_application_settings *settings);
static int cleanup(struct dc_application_settings *settings);
static struct dc_application_lifecycle *create_lifecycle(const struct dc_posix_env *env);
static struct dc_application_settings *create_settings(const struct dc_posix_env *env);
static int destroy_settings(struct dc_application_settings **psettings);


static void error_reporter(const char *file_name, const char *function_name, size_t line_number, int err)
{
    fprintf(stderr, "ERROR: %s : %s : @ %zu : %d\n", file_name, function_name, line_number, err);
}

static void trace(const char *file_name, const char *function_name, size_t line_number)
{
    fprintf(stderr, "TRACE: %s : %s : @ %zu\n", file_name, function_name, line_number);
}


int main(int argc, char *argv[])
{
    struct dc_posix_env         env;
    struct dc_application_info *info;
    int                         ret_val;

    dc_posix_env_init(&env, error_reporter);
    env.tracer = trace;
    info      = dc_application_info_create("Test Application", stdout, &env);
    ret_val   = dc_application_run(info, create_lifecycle, "~/.dcdump.conf", argc, argv);
    dc_application_info_destroy(&info);

    return ret_val;
}


static struct dc_application_lifecycle *create_lifecycle(const struct dc_posix_env *env)
{
    struct dc_application_lifecycle *lifecycle;

    lifecycle = dc_application_lifecycle_create(env, create_settings, destroy_settings, run);
    dc_application_lifecycle_set_parse_command_line(lifecycle, parse_command_line);
    dc_application_lifecycle_set_read_env_vars(lifecycle, read_env_vars);
    dc_application_lifecycle_set_read_config(lifecycle, load_config);
    dc_application_lifecycle_set_set_defaults(lifecycle, set_defaults);
    dc_application_lifecycle_set_cleanup(lifecycle, cleanup);

    return lifecycle;
}

static struct dc_application_settings *create_settings(const struct dc_posix_env *env)
{
    struct application_settings *settings;

    settings = calloc(1, sizeof(struct application_settings));
    settings->parent.config_path = dc_setting_path_create();
    settings->parent.env         = env;
    settings->input_path         = dc_setting_path_create();
    settings->verbose            = dc_setting_bool_create();
    settings->output_path        = dc_setting_path_create();
    settings->dump_path          = dc_setting_path_create();

//    return NULL;
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
    memset(*psettings, 0, sizeof(struct application_settings));
    free(*psettings);
    *psettings = NULL;

    return 0;
}

static int parse_command_line(struct dc_application_settings *settings, int argc, char *argv[])
{
    int verbose_flag;
    struct option long_options[] =
            {
                    { "verbose",  no_argument,       &verbose_flag, 1   },
                    { "in",       required_argument, 0,             'i' },
                    { "out",      required_argument, 0,             'o' },
                    { "dump-out", required_argument, 0,             'd' },
                    { 0,          0,                 0,             0   }
            };

    struct application_settings *app_settings;

    app_settings = (struct application_settings *)settings;
    verbose_flag = 0;

    while(1)
    {
        int c;
        int option_index;

        option_index = 0;
        c = getopt_long(argc, (char **)argv, "vi:o:d:",
                        long_options, &option_index);

        if(c == -1)
        {
            break;
        }

        switch(c)
        {
            case 0:
            {
                if(verbose_flag)
                {
                    dc_setting_bool_set(app_settings->verbose, true, DC_SETTING_COMMAND_LINE);
                }

                break;
            }
            case 'i':
            {
                set_path(settings->env, app_settings->input_path, optarg, DC_SETTING_COMMAND_LINE);
                break;
            }
            case 'o':
            {
                set_path(settings->env, app_settings->output_path, optarg, DC_SETTING_COMMAND_LINE);
                break;
            }
            case 'd':
            {
                set_path(settings->env, app_settings->dump_path, optarg, DC_SETTING_COMMAND_LINE);
                break;
            }
            default:
            {
                fprintf(stderr, "OOPS %d!\n", c);
            }
        }
    }

    return 0;
}

static int read_env_vars(struct dc_application_settings *settings, char **env)
{
    struct application_settings *app_settings;
    const char                  *prefix;
    size_t                       prefix_len;

    app_settings = (struct application_settings *)settings;
    prefix       = "DC_DUMP_";
    prefix_len   = strlen(prefix);

    while(*env)
    {
        if(strncmp(*env, prefix, prefix_len) == 0)
        {
            char   *env_var;
            size_t  length;
            char   *rest;
            char   *key;
            char   *value;
            int     err;

            length  = strlen(*env) + 1;
            env_var = dc_malloc(settings->env, &err, length * sizeof(char));

            if(env_var == NULL)
            {
            }

            strcpy(env_var, *env);
            rest  = NULL;
            key   = strtok_r(env_var, "=", &rest);
            value = strtok_r(NULL, "=", &rest);
            set_from_env(app_settings, prefix_len, key, value);
            free(env_var);
        }

        env++;
    }

    return 0;
}


static void set_from_env(struct application_settings *settings,
                         size_t prefix_len,
                         const char *key,
                         const char *value)
{
    const char *sub_key;

    sub_key = &key[prefix_len];

    if(strcmp(sub_key, "IN_PATH") == 0)
    {
        set_path(settings->parent.env, settings->input_path, value, DC_SETTING_COMMAND_LINE);
    }
    else if(strcmp(sub_key, "OUT_PATH") == 0)
    {
        set_path(settings->parent.env, settings->output_path, value, DC_SETTING_COMMAND_LINE);
    }
    else if(strcmp(sub_key, "DUMP_PATH") == 0)
    {
        set_path(settings->parent.env, settings->dump_path, value, DC_SETTING_COMMAND_LINE);
    }
}

static int load_config(struct dc_application_settings *settings)
{
    const char                  *config_path;
    struct application_settings *app_settings;
    config_t                     cfg;

    config_path  = dc_setting_path_get(settings->config_path);
    app_settings = (struct application_settings *)settings;

    config_init(&cfg);

    if(!(config_read_file(&cfg, config_path)))
    {
        // if the config file was passed in on the command line or set as an env var
        if(dc_setting_is_set((struct dc_setting *)settings->config_path))
        {
            fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
                    config_error_line(&cfg), config_error_text(&cfg));
            config_destroy(&cfg);

            return -1;
        }
    }
    else
    {
        const char *path;

        if(config_lookup_string(&cfg, "IN_PATH", &path))
        {
            set_path(settings->env, app_settings->input_path, optarg, DC_SETTING_COMMAND_LINE);
        }

        if(config_lookup_string(&cfg, "OUT_PATH", &path))
        {
            set_path(settings->env, app_settings->output_path, optarg, DC_SETTING_COMMAND_LINE);
        }

        if(config_lookup_string(&cfg, "DUMP_PATH", &path))
        {
            set_path(settings->env, app_settings->dump_path, optarg, DC_SETTING_COMMAND_LINE);
        }
    }

    config_destroy(&cfg);

    return 0;
}


static int set_defaults(__attribute__((unused)) struct dc_application_settings *settings)
{
    return 0;
}


static void set_path(const struct dc_posix_env *env, struct dc_setting_path *setting, const char *path, dc_setting_type type)
{
    dc_setting_path_set(env, setting, path, type);
}


static int run(struct dc_application_settings *settings)
{
    struct application_settings *app_settings;
    const char                  *out_path;
    int                          fd_out;
    off_t                        max_position;
    struct dc_stream_copy_info  *copy_info;
    struct dc_dump_info         *info;
    int                          ret_val;

    app_settings = (struct application_settings *)settings;
    ret_val      = 0;

    if(dc_setting_is_set((struct dc_setting *)app_settings->input_path))
    {
        const char *path;
        int         fd;
        struct stat file_info;

        path = dc_setting_path_get(app_settings->input_path);
        fd   = open(path, O_RDONLY);

        if(fd < 0)
        {
            // todo: what to do?
            fprintf(stderr, "Can't open file %s\n", path);
            ret_val = -1;
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

    if(ret_val >= 0)
    {
        if(dc_setting_is_set((struct dc_setting *)app_settings->output_path))
        {
            out_path = dc_setting_path_get(app_settings->input_path);
        }
        else
        {
            out_path = "/dev/null";
        }

        fd_out = open(out_path, O_WRONLY);
        info = dc_dump_dump_info_create(settings->env, STDOUT_FILENO, max_position);
        copy_info = dc_stream_copy_info_create(settings->env, NULL, dc_dumper, info, NULL, NULL);
        dc_stream_copy(settings->env, STDIN_FILENO, fd_out, 1024, copy_info);
        dc_stream_copy_info_destroy(settings->env, &copy_info);
        dc_dump_dump_info_destroy(&info);
    }

//    return 1;
    return ret_val;
}

static int cleanup(__attribute__((unused)) struct dc_application_settings *settings)
{
    return 0;
}
