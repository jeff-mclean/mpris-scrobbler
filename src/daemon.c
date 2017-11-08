/**
 * @author Marius Orcsik <marius@habarnam.ro>
 */

#include <dbus/dbus.h>
#include <time.h>
#include "structs.h"
#include "ini.c"
#include "utils.h"
#include "configuration.h"
#include "api.h"
#include "smpris.h"
#include "scrobble.h"
#include "sdbus.h"
#include "sevents.h"
#include "version.h"

#if 0
#define ARG_PID             "-p"
#endif
#define HELP_MESSAGE        "MPRIS scrobbler daemon, version %s\n" \
"Usage:\n  %s\t\tstart daemon\n" \
HELP_OPTIONS \
""
#if 0
"\t" ARG_PID " PATH\t\tWrite PID to this path\n" \
""
#endif

const char* get_version(void)
{
#ifndef VERSION_HASH
#define VERSION_HASH "(unknown)"
#endif
    return VERSION_HASH;
}

enum log_levels _log_level = debug | info | warning | error;
struct configuration global_config = { .name = NULL, .credentials = {NULL, NULL}, .credentials_length = 0};

void print_help(char* name)
{
    const char* help_msg;
    const char* version = get_version();

    help_msg = HELP_MESSAGE;

    fprintf(stdout, help_msg, version, name);
}

/**
 * TODO list
 *  1. Build our own last.fm API functionality
 *  2. Add support for libre.fm in the API
 *  3. Add support for credentials on multiple accounts
 *  4. Add support to blacklist players (I don't want to submit videos from vlc, for example)
 */
int main (int argc, char** argv)
{
    char *name = argv[0];
    char *command = NULL;
    bool have_pid = false;
    if (argc > 0) { command = argv[1]; }

    for (int i = 0 ; i < argc; i++) {
        command = argv[i];
        if (strcmp(command, ARG_HELP) == 0) {
            print_help(name);
            return EXIT_SUCCESS;
        }
        if (strncmp(command, ARG_QUIET, strlen(ARG_QUIET)) == 0) {
            _log_level = error;
        }
        if (strncmp(command, ARG_VERBOSE1, strlen(ARG_VERBOSE1)) == 0) {
            _log_level = info | warning | error;
        }
        if (strncmp(command, ARG_VERBOSE2, strlen(ARG_VERBOSE2)) == 0) {
            _log_level = debug | info | warning | error;
        }
        if (strncmp(command, ARG_VERBOSE3, strlen(ARG_VERBOSE3)) == 0) {
#ifdef DEBUG
            _log_level = tracing | debug | info | warning | error;
#else
            _warn("main::debug: extra verbose output is disabled");
#endif
        }
#if 0
        if (strncmp(command, ARG_PID, strlen(ARG_PID)) == 0) {
            if (NULL == argv[i+1] || argv[i+1][0] == '-') {
                _error("main::argument_missing: " ARG_PID " requires a writeable PID path");
                return EXIT_FAILURE;
            }
            pid_path = argv[i+1];
            i += 1;
        }
#endif
    }
    // TODO(marius): make this asynchronous to be requested when submitting stuff
    load_configuration(&global_config);
    if (global_config.credentials_length == 0) {
        _warn("main::load_credentials: no credentials were loaded");
    }

    struct state *state = state_new();
    if (NULL == state) { return EXIT_FAILURE; }

    char *full_pid_path = get_pid_file(&global_config);
    _trace("main::writing_pid: %s", full_pid_path);
    have_pid = write_pid(full_pid_path);

    for(size_t i = 0; i < state->scrobbler->credentials_length; i++) {
        struct api_credentials *cur = state->scrobbler->credentials[i];
        if (NULL == cur) { continue; }
        if (cur->enabled && NULL == cur->session_key && NULL != cur->token) {
            CURL *curl = curl_easy_init();
            struct http_request *req = api_build_request_get_session(curl, cur->end_point, cur->token);
            struct http_response *res = http_response_new();
            // TODO: do something with the response to see if the api call was successful
            enum api_return_statuses ok = api_get_request(curl, req, res);
            //enum api_return_statuses ok = status_ok;

            http_request_free(req);
            if (ok == status_ok) {
                cur->authenticated = true;
                cur->user_name = api_response_get_name(res->doc);
                cur->session_key = api_response_get_key(res->doc);
                _info("api::get_session%s] %s", get_api_type_label(cur->end_point), "ok");
            } else {
                cur->authenticated = false;
                cur->enabled = false;
                _error("api::get_session[%s] %s - disabling", get_api_type_label(cur->end_point), "nok");
            }
            http_response_free(res);
            curl_easy_cleanup(curl);
        }
    }

    event_base_dispatch(state->events->base);
    state_free(state);
    if (have_pid) {
        _debug("main::cleanup_pid: %s", full_pid_path);
        cleanup_pid(full_pid_path);
        free(full_pid_path);
    }
    free_configuration(&global_config);

    return EXIT_SUCCESS;
}
