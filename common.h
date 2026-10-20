#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/un.h>
#include <sys/socket.h>

/* UNIX domain socket paths */
#define SERVER_SOCK_PATH "/tmp/unix_dgram_server.sock"
#define CLIENT_SOCK_PATH "/tmp/unix_dgram_client.sock"

/* Maximum number of entities the server simulates */
#define MAX_ENTITIES 32

/* Message types exchanged between server and client */
typedef enum
{
    MSG_SUBSCRIBE = 1,   /* client -> server: register to receive updates */
    MSG_ACK = 2,         /* server -> client: acknowledgement */
    MSG_ENTITY_UPDATE = 3 /* server -> client: periodic entity state update */
} msg_type_t;

typedef struct
{
    float x, y, z;
} vec3_t;

typedef struct
{
    float x, y, z, w;
} quat_t;

typedef struct
{
    int32_t id;
    vec3_t position;   /* world position */
    quat_t orientation;/* orientation (quaternion) */
    vec3_t velocity;   /* linear velocity */
} entity_t;

typedef struct
{
    uint32_t msg_type;
    uint32_t entity_count;
    entity_t entities[MAX_ENTITIES];
} update_msg_t;

#endif /* COMMON_H */
