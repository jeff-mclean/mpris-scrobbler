/**
 * @author Marius Orcsik <marius@habarnam.ro>
 */
#ifndef MPRIS_SCROBBLER_AUDIOSCROBBLER_API_H
#define MPRIS_SCROBBLER_AUDIOSCROBBLER_API_H

#ifndef LASTFM_API_KEY
#include "credentials_lastfm.h"
#endif
#ifndef LIBREFM_API_KEY
#include "credentials_librefm.h"
#endif

#define API_TOKEN_NODE_NAME         "token"
#define API_SESSION_NODE_NAME       "session"
#define API_NAME_NODE_NAME          "name"
#define API_KEY_NODE_NAME           "key"
#define API_SUBSCRIBER_NODE_NAME    "subscriber"
#define API_NOWPLAYING_NODE_NAME    "nowplaying"
#define API_SCROBBLES_NODE_NAME     "scrobbles"
#define API_SCROBBLE_NODE_NAME      "scrobble"
#define API_TIMESTAMP_NODE_NAME     "timestamp"
#define API_TRACK_NODE_NAME         "track"
#define API_ARTIST_NODE_NAME        "artist"
#define API_ALBUM_NODE_NAME         "album"
#define API_ALBUMARTIST_NODE_NAME   "albumArtist"
#define API_IGNORED_NODE_NAME       "ignoredMessage"
#define API_STATUS_ATTR_NAME        "status"
#define API_STATUS_VALUE_OK         "ok"
#define API_STATUS_VALUE_FAILED     "failed"
#define API_ERROR_NODE_NAME         "error"
#define API_ERROR_MESSAGE_NAME      "message"
#define API_ERROR_CODE_ATTR_NAME    "code"

#define API_METHOD_NOW_PLAYING      "track.updateNowPlaying"
#define API_METHOD_SCROBBLE         "track.scrobble"
#define API_METHOD_GET_TOKEN        "auth.getToken"
#define API_METHOD_GET_SESSION      "auth.getSession"

enum api_node_types {
    api_node_type_unknown,
    // all
    api_node_type_document,
    api_node_type_root,
    api_node_type_error,
    // auth.getToken
    api_node_type_token,
    // track.nowPlaying
    api_node_type_now_playing,
    api_node_type_track,
    api_node_type_artist,
    api_node_type_album,
    api_node_type_album_artist,
    api_node_type_ignored_message,
    // track.scrobble
    api_node_type_scrobbles,
    api_node_type_scrobble,
    api_node_type_timestamp,
    // auth.getSession
    api_node_type_session,
    api_node_type_session_name,
    api_node_type_session_key,
    api_node_type_session_subscriber,
} api_node_type;

enum api_return_code {
    unavaliable             = 1, //The service is temporarily unavailable, please try again.
    invalid_service         = 2, //Invalid service - This service does not exist
    invalid_method          = 3, //Invalid Method - No method with that name in this package
    authentication_failed   = 4, //Authentication Failed - You do not have permissions to access the service
    invalid_format          = 5, //Invalid format - This service doesn't exist in that format
    invalid_parameters      = 6, //Invalid parameters - Your request is missing a required parameter
    invalid_resource        = 7, //Invalid resource specified
    operation_failed        = 8, //Operation failed - Something else went wrong
    invalid_session_key     = 9, //Invalid session key - Please re-authenticate
    invalid_apy_key         = 10, //Invalid API key - You must be granted a valid key by last.fm
    service_offline         = 11, //Service Offline - This service is temporarily offline. Try again later.
    invalid_signature       = 13, //Invalid method signature supplied
    temporary_error         = 16, //There was a temporary error processing your request. Please try again
    suspended_api_key       = 26, //Suspended API key - Access for your account has been suspended, please contact Last.fm
    rate_limit_exceeded     = 29, //Rate limit exceeded - Your IP has made too many requests in a short period
};

struct api_error {
    enum api_return_code code;
    char *message;
};

void api_response_get_session_key_json(const char *buffer, const size_t length, char **session_key, char **name)
{
    if (NULL == buffer) { return; }
    if (length == 0) { return; }
    if (NULL == *session_key) { return; }
    if (NULL == *name) { return; }

    struct json_tokener *tokener = json_tokener_new();
    if (NULL == tokener) { return; }
    json_object *root = json_tokener_parse_ex(tokener, buffer, length);
    if (NULL == root) {
        _warn("json::invalid_json_message");
        goto _exit;
    }
    if (json_object_object_length(root) < 1) {
        _warn("json::no_root_object");
        goto _exit;
    }
    json_object *sess_object = NULL;
    if (!json_object_object_get_ex(root, API_SESSION_NODE_NAME, &sess_object) || NULL == sess_object) {
        _warn("json:missing_session_object");
        goto _exit;
    }
    if(!json_object_is_type(sess_object, json_type_object)) {
        _warn("json::session_is_not_object");
        goto _exit;
    }
    json_object *key_object = NULL;
    if (!json_object_object_get_ex(sess_object, API_KEY_NODE_NAME, &key_object) || NULL == key_object) {
        _warn("json:missing_key");
        goto _exit;
    }
    if(!json_object_is_type(key_object, json_type_string)) {
        _warn("json::key_is_not_string");
        goto _exit;
    }
    const char *sess_value = json_object_get_string(key_object);
    strncpy(*session_key, sess_value, strlen(sess_value));
    _info("json::loaded_session_key: %s", *session_key);

    json_object *name_object = NULL;
    if (!json_object_object_get_ex(sess_object, API_NAME_NODE_NAME, &name_object) || NULL == name_object) {
        goto _exit;
    }
    if (!json_object_is_type(name_object, json_type_string)) {
        goto _exit;
    }
    const char *name_value = json_object_get_string(name_object);
    strncpy(*name, name_value, strlen(name_value));
    _info("json::loaded_session_user: %s", *name);

_exit:
    if (NULL != root) { json_object_put(root); }
    json_tokener_free(tokener);
}

void api_response_get_token_json(const char *buffer, const size_t length, char **token)
{
    // {"token":"NQH5C24A6RbIOx1xWUcty1N6yOHcKcRk"}
    if (NULL == buffer) { return; }
    if (length == 0) { return; }
    if (NULL == *token) { return; }

    struct json_tokener *tokener = json_tokener_new();
    json_object *root = json_tokener_parse_ex(tokener, buffer, length);
    if (NULL == root) {
        _warn("json::invalid_json_message");
        goto _exit;
    }
    if (json_object_object_length(root) < 1) {
        _warn("json::no_root_object");
        goto _exit;
    }
    json_object *tok_object = NULL;
    if (!json_object_object_get_ex(root, API_TOKEN_NODE_NAME, &tok_object) || NULL == tok_object) {
        _warn("json:missing_token_key");
        goto _exit;
    }
    if (!json_object_is_type(tok_object, json_type_string)) {
        _warn("json::token_is_not_string");
        goto _exit;
    }
    const char *value = json_object_get_string(tok_object);
    strncpy(*token, value, strlen(value));
    _info("json::loaded_token: %s", *token);

_exit:
    if (NULL != root) { json_object_put(root); }
    json_tokener_free(tokener);
}

bool json_document_is_error(const char *buffer, const size_t length)
{
    // {"error":14,"message":"This token has not yet been authorised"}

    bool result = false;

    if (NULL == buffer) { return result; }
    if (length == 0) { return result; }

    json_object *err_object = NULL;
    json_object *msg_object = NULL;

    struct json_tokener *tokener = json_tokener_new();
    if (NULL == tokener) { return result; }
    json_object *root = json_tokener_parse_ex(tokener, buffer, length);

    if (NULL == root || json_object_object_length(root) < 1) {
        goto _exit;
    }
    json_object_object_get_ex(root, API_ERROR_NODE_NAME, &err_object);
    json_object_object_get_ex(root, API_ERROR_MESSAGE_NAME, &msg_object);
    if (NULL == err_object || !json_object_is_type(err_object, json_type_string)) {
        goto _exit;
    }
    if (NULL == msg_object || !json_object_is_type(msg_object, json_type_int)) {
        goto _exit;
    }
    result = true;

_exit:
    if (NULL != root) { json_object_put(root); }
    json_tokener_free(tokener);
    return result;
}

static bool audioscrobbler_valid_credentials(const struct api_credentials *auth)
{
    bool status = false;
    if (NULL == auth) { return status; }
    if (auth->end_point != lastfm && auth->end_point != librefm) { return status; }

    status = true;
    return status;
}

char *api_get_url(struct api_endpoint*);
struct api_endpoint *api_endpoint_new(enum api_type);
char *api_get_signature(const char*, const char*);
struct http_request *http_request_new(void);
/*
 * api_key (Required) : A Last.fm API key.
 * api_sig (Required) : A Last.fm method signature. See [authentication](https://www.last.fm/api/authentication) for more information.
*/
struct http_request *audioscrobbler_api_build_request_get_token(CURL *handle, const struct api_credentials *auth)
{
    if (NULL == handle) { return NULL; }
    if (!audioscrobbler_valid_credentials(auth)) { return NULL; }

    const char *api_key = auth->api_key;
    const char *secret = auth->secret;

    struct http_request *request = http_request_new();

    char *sig_base = get_zero_string(MAX_BODY_SIZE);
    char *query = get_zero_string(MAX_BODY_SIZE);

    char *escaped_api_key = curl_easy_escape(handle, api_key, strlen(api_key));
    size_t escaped_key_len = strlen(escaped_api_key);
    strncat(query, "api_key=", 8);
    strncat(query, escaped_api_key, escaped_key_len);
    strncat(query, "&", 1);

    strncat(sig_base, "api_key", 7);
    strncat(sig_base, escaped_api_key, escaped_key_len);
    free(escaped_api_key);

    const char *method = API_METHOD_GET_TOKEN;
    size_t method_len = strlen(method);
    strncat(query, "method=", 7);
    strncat(query, method, method_len);
    strncat(query, "&",1);

    strncat(sig_base, "method", 6);
    strncat(sig_base, method, method_len);

    char *sig = (char*)api_get_signature(sig_base, secret);
    strncat(query, "api_sig=", 8);
    strncat(query, sig, strlen(sig));
    strncat(query, "&",1);
    free(sig);
    free(sig_base);

    strncat(query, "format=json", 11);

    request->query = query;
    request->end_point = api_endpoint_new(auth->end_point);
    request->url = api_get_url(request->end_point);
    return request;
}

/*
 * token (Required) : A 32-character ASCII hexadecimal MD5 hash returned by step 1 of the authentication process (following the granting of permissions to the application by the user)
 * api_key (Required) : A Last.fm API key.
 * api_sig (Required) : A Last.fm method signature. See authentication for more information.
 * Return codes
 *  2 : Invalid service - This service does not exist
 *  3 : Invalid Method - No method with that name in this package
 *  4 : Authentication Failed - You do not have permissions to access the service
 *  4 : Invalid authentication token supplied
 *  5 : Invalid format - This service doesn't exist in that format
 *  6 : Invalid parameters - Your request is missing a required parameter
 *  7 : Invalid resource specified
 *  8 : Operation failed - Something else went wrong
 *  9 : Invalid session key - Please re-authenticate
 * 10 : Invalid API key - You must be granted a valid key by last.fm
 * 11 : Service Offline - This service is temporarily offline. Try again later.
 * 13 : Invalid method signature supplied
 * 14 : This token has not been authorized
 * 15 : This token has expired
 * 16 : There was a temporary error processing your request. Please try again
 * 26 : Suspended API key - Access for your account has been suspended, please contact Last.fm
 * 29 : Rate limit exceeded - Your IP has made too many requests in a short period*
 */
struct http_request *audioscrobbler_api_build_request_get_session(CURL *handle, const struct api_credentials *auth)
{

    if (NULL == handle) { return NULL; }
    if (!audioscrobbler_valid_credentials(auth)) { return NULL; }

    const char *api_key = auth->api_key;
    const char *secret = auth->secret;
    const char *token = auth->token;

    struct http_request *request = http_request_new();

    const char *method = API_METHOD_GET_SESSION;

    char *sig_base = get_zero_string(MAX_BODY_SIZE);
    char *query = get_zero_string(MAX_BODY_SIZE);

    char *escaped_api_key = curl_easy_escape(handle, api_key, strlen(api_key));
    size_t escaped_key_len = strlen(escaped_api_key);
    strncat(query, "api_key=", 8);
    strncat(query, escaped_api_key, escaped_key_len);
    strncat(query, "&", 1);

    strncat(sig_base, "api_key", 7);
    strncat(sig_base, escaped_api_key, escaped_key_len);
    free(escaped_api_key);

    size_t method_len = strlen(method);
    strncat(query, "method=", 7);
    strncat(query, method, method_len);
    strncat(query, "&", 1);

    strncat(sig_base, "method", 6);
    strncat(sig_base, method, method_len);

    size_t token_len = strlen(token);
    strncat(query, "token=", 6);
    strncat(query, token, token_len);
    strncat(query, "&", 1);

    strncat(sig_base, "token", 5);
    strncat(sig_base, token, token_len);

    char *sig = (char*)api_get_signature(sig_base, secret);
    strncat(query, "api_sig=", 8);
    strncat(query, sig, strlen(sig));
    strncat(query, "&", 1);
    free(sig);
    free(sig_base);

    strncat(query, "format=json", 11);

    request->query = query;
    request->end_point = api_endpoint_new(auth->end_point);
    request->url = api_get_url(request->end_point);
    return request;
}

/*
 * artist (Required) : The artist name.
 * track (Required) : The track name.
 * album (Optional) : The album name.
 * trackNumber (Optional) : The track number of the track on the album.
 * context (Optional) : Sub-client version (not public, only enabled for certain API keys)
 * mbid (Optional) : The MusicBrainz Track ID.
 * duration (Optional) : The length of the track in seconds.
 * albumArtist (Optional) : The album artist - if this differs from the track artist.
 * api_key (Required) : A Last.fm API key.
 * api_sig (Required) : A Last.fm method signature. See authentication for more information.
 * sk (Required) : A session key generated by authenticating a user via the authentication protocol.
 */
struct http_request *audioscrobbler_api_build_request_now_playing(const struct scrobble *track, CURL *handle, const struct api_credentials *auth)
{
    if (NULL == handle) { return NULL; }
    if (!audioscrobbler_valid_credentials(auth)) { return NULL; }

    const char *api_key = auth->api_key;
    const char *secret = auth->secret;
    const char *sk = auth->session_key;

    struct http_request *request = http_request_new();

    char *sig_base = get_zero_string(MAX_BODY_SIZE);
    char *body = get_zero_string(MAX_BODY_SIZE);

    size_t album_len = strlen(track->album);
    char *esc_album = curl_easy_escape(handle, track->album, album_len);
    size_t esc_album_len = strlen(esc_album);
    strncat(body, "album=", 6);
    strncat(body, esc_album, esc_album_len);
    strncat(body, "&", 1);

    strncat(sig_base, "album", 5);
    strncat(sig_base, track->album, album_len);
    free(esc_album);

    size_t api_key_len = strlen(api_key);
    char *esc_api_key = curl_easy_escape(handle, api_key, api_key_len);
    size_t esc_api_key_len = strlen(esc_api_key);
    strncat(body, "api_key=", 8);
    strncat(body, esc_api_key, esc_api_key_len);
    strncat(body, "&", 1);

    strncat(sig_base, "api_key", 7);
    strncat(sig_base, api_key, api_key_len);
    free(esc_api_key);

    size_t artist_len = strlen(track->artist);
    char *esc_artist = curl_easy_escape(handle, track->artist, artist_len);
    size_t esc_artist_len = strlen(esc_artist);
    strncat(body, "artist=", 7);
    strncat(body, esc_artist, esc_artist_len);
    strncat(body, "&", 1);

    strncat(sig_base, "artist", 6);
    strncat(sig_base, track->artist, artist_len);
    free(esc_artist);

    const char *method = API_METHOD_NOW_PLAYING;

    size_t method_len = strlen(method);
    strncat(body, "method=", 7);
    strncat(body, method, method_len);
    strncat(body, "&", 1);

    strncat(sig_base, "method", 6);
    strncat(sig_base, method, method_len);

    strncat(body, "sk=", 3);
    size_t sk_len = strlen(sk);
    strncat(body, sk, sk_len);
    strncat(body, "&", 1);

    strncat(sig_base, "sk", 2);
    strncat(sig_base, sk, sk_len);

    size_t title_len = strlen(track->title);
    char *esc_title = curl_easy_escape(handle, track->title, title_len);
    size_t esc_title_len = strlen(esc_title);
    strncat(body, "track=", 6);
    strncat(body, esc_title, esc_title_len);
    strncat(body, "&", 1);

    strncat(sig_base, "track", 5);
    strncat(sig_base, track->title, title_len);
    free(esc_title);

    char *sig = (char*)api_get_signature(sig_base, secret);
    strncat(body, "api_sig=", 8);
    strncat(body, sig, strlen(sig));
    free(sig_base);
    free(sig);

    char *query = get_zero_string(MAX_BODY_SIZE);
    strncat(query, "format=json", 11);

    request->query = query;
    request->body = body;
    request->body_length = strlen(body);
    request->end_point = api_endpoint_new(auth->end_point);
    request->url = api_get_url(request->end_point);
    return request;
}

struct http_request *audioscrobbler_api_build_request_scrobble(const struct scrobble *tracks[], size_t track_count, CURL *handle, const struct api_credentials *auth)
{
    if (NULL == handle) { return NULL; }
    if (!audioscrobbler_valid_credentials(auth)) { return NULL; }

    const char *api_key = auth->api_key;
    const char *secret = auth->secret;
    const char *sk = auth->session_key;

    struct http_request *request = http_request_new();

    const char *method = API_METHOD_SCROBBLE;

    char *sig_base = get_zero_string(MAX_BODY_SIZE);
    char *body = get_zero_string(MAX_BODY_SIZE);

    for (size_t i = 0; i < track_count; i++) {
        const struct scrobble *track = tracks[i];

        size_t album_len = strlen(track->album);
        char *esc_album = curl_easy_escape(handle, track->album, album_len);
        size_t esc_album_len = strlen(esc_album);
        strncat(body, "album[]=", 8);
        strncat(body, esc_album, esc_album_len);
        strncat(body, "&", 1);

        strncat(sig_base, "album[]", 7);
        strncat(sig_base, track->album, album_len);
        free(esc_album);
    }

    size_t api_key_len = strlen(api_key);
    char *esc_api_key = curl_easy_escape(handle, api_key, api_key_len);
    size_t esc_api_key_len = strlen(esc_api_key);
    strncat(body, "api_key=", 8);
    strncat(body, esc_api_key, esc_api_key_len);
    strncat(body, "&", 1);

    strncat(sig_base, "api_key", 7);
    strncat(sig_base, api_key, api_key_len);
    free(esc_api_key);

    for (size_t i = 0; i < track_count; i++) {
        const struct scrobble *track = tracks[i];

        size_t artist_len = strlen(track->artist);
        char *esc_artist = curl_easy_escape(handle, track->artist, artist_len);
        size_t esc_artist_len = strlen(esc_artist);
        strncat(body, "artist[]=", 9);
        strncat(body, esc_artist, esc_artist_len);
        strncat(body, "&", 1);

        strncat(sig_base, "artist[]", 8);
        strncat(sig_base, track->artist, artist_len);
        free(esc_artist);
    }

    size_t method_len = strlen(method);
    strncat(body, "method=", 7);
    strncat(body, method, method_len);
    strncat(body, "&", 1);

    strncat(sig_base, "method", 6);
    strncat(sig_base, method, method_len);

    size_t sk_len = strlen(sk);
    strncat(body, "sk=", 3);
    strncat(body, sk, sk_len);
    strncat(body, "&", 1);

    strncat(sig_base, "sk", 2);
    strncat(sig_base, sk, sk_len);

    for (size_t i = 0; i < track_count; i++) {
        const struct scrobble *track = tracks[i];

        char *timestamp = get_zero_string(32);
        snprintf(timestamp, 32, "%ld", track->start_time);
        strncat(body, "timestamp[]=", 12);
        strncat(body, timestamp, strlen(timestamp));
        strncat(body, "&", 1);

        strncat(sig_base, "timestamp[]", 11);
        strncat(sig_base, timestamp, strlen(timestamp));
        free(timestamp);

        size_t title_len = strlen(track->title);
        char *esc_title = curl_easy_escape(handle, track->title, title_len);
        size_t esc_title_len = strlen(esc_title);
        strncat(body, "track[]=", 8);
        strncat(body, esc_title, esc_title_len);
        strncat(body, "&", 1);

        strncat(sig_base, "track[]", 7);
        strncat(sig_base, track->title, title_len);
        free(esc_title);
    }

    char *sig = (char*)api_get_signature(sig_base, secret);
    strncat(body, "api_sig=", 8);
    strncat(body, sig, strlen(sig));
    free(sig);
    free(sig_base);

    char *query = get_zero_string(MAX_BODY_SIZE);
    strncat(query, "format=json", 11);

    request->query = query;
    request->body = body;
    request->body_length = strlen(body);
    request->end_point = api_endpoint_new(auth->end_point);
    request->url = api_get_url(request->end_point);

    return request;
}

#endif // MPRIS_SCROBBLER_AUDIOSCROBBLER_API_H