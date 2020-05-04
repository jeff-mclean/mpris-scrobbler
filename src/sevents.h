/**
 * @author Marius Orcsik <marius@habarnam.ro>
 */
#ifndef MPRIS_SCROBBLER_SEVENTS_H
#define MPRIS_SCROBBLER_SEVENTS_H

#include <pthread.h>
#include <event2/thread.h>

static void send_now_playing(evutil_socket_t, short, void *);
void events_free(struct events *ev)
{
    if (NULL == ev) { return; }
    _trace2("mem::free::event(%p):SIGINT", ev->sigint);
    event_free(ev->sigint);
    _trace2("mem::free::event(%p):SIGTERM", ev->sigterm);
    event_free(ev->sigterm);
    _trace2("mem::free::event(%p):SIGHUP", ev->sighup);
    event_free(ev->sighup);
}

struct events *events_new(void)
{
    struct events *result = calloc(1, sizeof(struct events));
    return result;
}

void log_event(int severity, const char *msg)
{
    _trace2("libevent[%d]: %s", severity, msg);
}

void events_init(struct events *ev, struct state *s)
{
    if (NULL == ev) { return; }

#if 1
    // as curl uses different threads, it's better to initialize support
    // for it in libevent2
    int maybe_threads = evthread_use_pthreads();
    if (maybe_threads < 0) {
        _error("events::unable_to_setup_multithreading");
    }
#ifdef DEBUG
    event_enable_debug_mode();
    event_enable_debug_logging(EVENT_DBG_ALL);
    event_set_log_callback(log_event);
#endif
#endif

    ev->base = event_base_new();
    if (NULL == ev->base) {
        _error("mem::init_libevent: failure");
        return;
    }

    _trace2("mem::inited_libevent(%p)", ev->base);
    ev->sigint = evsignal_new(ev->base, SIGINT, sighandler, s);
    if (NULL == ev->sigint || event_add(ev->sigint, NULL) < 0) {
        _error("mem::add_event(SIGINT): failed");
        return;
    }
    ev->sigterm = evsignal_new(ev->base, SIGTERM, sighandler, s);
    if (NULL == ev->sigterm || event_add(ev->sigterm, NULL) < 0) {
        _error("mem::add_event(SIGTERM): failed");
        return;
    }
    ev->sighup = evsignal_new(ev->base, SIGHUP, sighandler, s);
    if (NULL == ev->sighup || event_add(ev->sighup, NULL) < 0) {
        _error("mem::add_event(SIGHUP): failed");
        return;
    }
}

static void send_now_playing(evutil_socket_t fd, short event, void *data)
{
    assert(data);

    struct event_payload *state = data;

    struct mpris_player *player = state->parent;
    assert(player);

    struct scrobbler *scrobbler = player->scrobbler;

    struct scrobble *track = &state->scrobble;

    assert(!scrobble_is_empty(track));

    if (track->position + NOW_PLAYING_DELAY > (double)track->length) {
        event_del(state->event);
        return;
    }

    if (now_playing_is_valid(track)) {
        const struct scrobble **tracks = NULL;
        arrput(tracks, track);
        _info("scrobbler::now_playing: %s//%s//%s", track->title, track->artist[0], track->album);
        api_request_do(scrobbler, tracks, api_build_request_now_playing);
    } else {
        print_scrobble_valid_check(track, log_tracing2);
    }

    if (track->position + NOW_PLAYING_DELAY < (double)track->length) {
        _debug("events::triggered(%p//%p):now_playing ellapsed %2.3f seconds", state->event, track, track->position);
        track->position += NOW_PLAYING_DELAY;
        add_event_now_playing(player, track, NOW_PLAYING_DELAY);
    }
}

static bool add_event_now_playing(struct mpris_player *player, struct scrobble *track, time_t delay)
{
    assert (NULL != player);
    assert (NULL != track);
    assert (mpris_player_is_valid(player));
    assert(!scrobble_is_empty(track));

    if (player->ignored) {
        _trace("events::add_event:now_playing: skipping, player %s is ignored", player->name);
        return false;
    }

    struct timeval now_playing_tv = { .tv_sec = delay };

    struct event_payload *payload = &player->payload;
    scrobble_copy(&payload->scrobble, track);

    if (NULL != payload->event) {
        event_free(payload->event);
    }
    payload->event = event_new(player->evbase, -1, EV_TIMEOUT, send_now_playing, payload);
    if (NULL == payload->event) {
        _warn("events::add_event_failed(%p):now_playing", payload->event);
        return false;
    }

    // Initalize timed event for now_playing
    _debug("events::add_event:now_playing[%s](%p//%p) in %2.3f seconds", player->name, payload->event, payload->scrobble, (double)(now_playing_tv.tv_sec + now_playing_tv.tv_usec));
    event_add(payload->event, &now_playing_tv);

    return true;
}

static void queue(evutil_socket_t fd, short event, void *data)
{
    if (NULL == data) {
        _warn("events::triggered::queue[%d:%d]: missing data", fd, event);
        return;
    }
    struct event_payload *state = data;
    struct scrobbler *scrobbler = state->parent;
    if (NULL == scrobbler) {
        _warn("events::triggered::queue[%d:%d]: missing scrobbler", fd, event);
        return;
    }
    struct scrobble *scrobble = &state->scrobble;
    if (NULL == scrobble) {
        _warn("events::triggered::queue[%d:%d]: missing track", fd, event);
        return;
    }

    _debug("events::triggered(%p:%p):queue", state, scrobbler->queue);
    if (scrobble_is_valid(scrobble)) {
        scrobbles_append(scrobbler, scrobble);

        int queue_count = arrlen(scrobbler->queue);
        _debug("events::new_queue_length: %zu", queue_count);
    } else {
        print_scrobble_valid_check(scrobble, log_tracing2);
    }

    event_del(state->event);
}

static void send_scrobble(evutil_socket_t fd, short event, void *data)
{
    if (NULL == data) {
        _warn("events::triggered::scrobble[%d:%d]: missing data", fd, event);
        return;
    }
    struct event_payload *payload = data;
    struct scrobbler *scrobbler = payload->parent;

    int queue_count = 0;
    if (NULL == payload->parent) {
        _warn("events::triggered::scrobble[%p]: missing scrobbler info", payload);
        return;
    }
    if (NULL == scrobbler->queue) {
        _warn("events::triggered::scrobble[%p]: nil queue", payload);
        return;
    }
    queue_count = arrlen(scrobbler->queue);
    if (queue_count > 0) {
        _trace("events::triggered(%p:%p):scrobble", payload->event, scrobbler->queue);
        scrobbles_consume_queue(scrobbler, scrobbler->queue);

        scrobbles_free(&scrobbler->queue, false);
        queue_count = arrlen(scrobbler->queue);
        _debug("events::new_queue_length: %zu", queue_count);
    }
}

static bool add_event_queue(struct scrobbler *scrobbler, struct scrobble *track, struct event_base *base)
{
    assert (NULL != scrobbler);
    assert(NULL != track);

    struct event_payload *payload = &scrobbler->payload;
    scrobble_copy(&payload->scrobble, track);

    assert(!scrobble_is_empty(track));

    if (NULL == payload->event) {
        payload->event = event_new(base, -1, EV_TIMEOUT, queue, payload);
        if (NULL == payload->event) {
            _warn("events::add_event_failed(%p):queue", payload->event);
        }
    }

    // This is the event that adds a scrobble to the queue after the correct amount of time
    // round to the second
    struct timeval timer = {
        .tv_sec = min_scrobble_seconds(track),
    };

    _debug("events::add_event:queue(%p:%p) in %2.3f seconds", payload, payload->scrobble, (double)(timer.tv_sec + timer.tv_usec));
    event_add(payload->event, &timer);

    return true;
}

#if 0
static bool add_event_scrobble(struct mpris_player *scrobbler)
{
    if (NULL == scrobbler) { return false; }

    struct timeval scrobble_tv = {.tv_sec = 0 };
    struct player_events *ev = &player->events;
    struct scrobble_payload *payload = scrobble_payload_new(player->scrobbler);

    // Initalize timed event for scrobbling
    // TODO(marius): Split scrobbling into two events:
    //  1. Actually add the current track to the top of the queue in length / 2 or 4 minutes, whichever comes first
    //  2. Process the queue and call APIs with the current queue

    if (event_assign(payload->event, ev->base, -1, EV_PERSIST, send_scrobble, payload) == 0) {
        // round to the second
        scrobble_tv.tv_sec = min_scrobble_seconds(track);
        _debug("events::add_event(%p):scrobble in %2.3f seconds", payload->event, (double)(scrobble_tv.tv_sec + scrobble_tv.tv_usec));
        event_add(payload->event, &scrobble_tv);
    } else {
        _warn("events::add_event_failed(%p):scrobble", payload->event);
    }

    return true;
}
#endif

static inline bool mpris_event_changed_playback_status(const struct mpris_event *ev)
{
    return ev->loaded_state & mpris_load_property_playback_status;
}
static inline bool mpris_event_changed_track(const struct mpris_event *ev)
{
    return ev->loaded_state > mpris_load_property_position;
}
static inline bool mpris_event_changed_volume(const struct mpris_event *ev)
{
    return ev->loaded_state & mpris_load_property_volume;
}
static inline bool mpris_event_changed_position(const struct mpris_event *ev)
{
    return ev->loaded_state & mpris_load_property_position;
}

static inline void mpris_event_clear(struct mpris_event *ev)
{
    ev->playback_status_changed = false;
    ev->volume_changed = false;
    ev->track_changed = false;
    ev->position_changed = false;
    ev->loaded_state = 0;
    _trace2("mem::zeroed::mpris_event");
}

bool state_dbus_is_valid(struct dbus *bus)
{
    return (NULL != bus->conn);

}

bool state_player_is_valid(struct mpris_player *player)
{
    return strlen(player->mpris_name) > 0;
}

bool state_is_valid(struct state *state) {
    return (
        (NULL != state) &&
        (state->player_count > 0)/* && state_player_is_valid(state->player)*/ &&
        (NULL != state->dbus) && state_dbus_is_valid(state->dbus)
    );
}

void resend_now_playing (struct state *state)
{
    if (!state_is_valid(state)) {
        _error("events::invalid_state");
        return;
    }
    for (int i = 0; i < state->player_count; i++) {
        struct mpris_player *player = &state->players[i];
        load_player_mpris_properties(state->dbus->conn, player);
        struct scrobble scrobble = {0};
        if (load_scrobble(&scrobble, &player->properties) && now_playing_is_valid(&scrobble)) {
            add_event_now_playing(player, &scrobble, 0);
        }
    }
}

#endif // MPRIS_SCROBBLER_SEVENTS_H
