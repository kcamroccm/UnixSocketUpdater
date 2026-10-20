#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "common.h"

static volatile int keep_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    keep_running = 0;
}

int main(void)
{
    struct sockaddr_un server_addr;
    struct sockaddr_un client_addr;
    socklen_t client_addr_len;
    int sockfd;
    ssize_t n;
    update_msg_t msg;
    entity_t entities[MAX_ENTITIES];
    int entity_count = 8;
    int i;
    int have_client = 0;

    /* Install signal handler for clean shutdown */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Create UNIX domain datagram socket */
    sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("server: socket");
        return EXIT_FAILURE;
    }

    /* Bind to our well-known socket path */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SERVER_SOCK_PATH,
            sizeof(server_addr.sun_path) - 1);

    unlink(SERVER_SOCK_PATH);
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("server: bind");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("server: listening on %s\n", SERVER_SOCK_PATH);

    /* ------------------------------------------------------------------
     * Phase 1: wait for a client to subscribe.
     * Because SOCK_DGRAM is connectionless, the server must learn the
     * client's address before it can send updates to that specific client.
     * ------------------------------------------------------------------ */
    printf("server: waiting for client to subscribe...\n");
    client_addr_len = sizeof(client_addr);
    n = recvfrom(sockfd, &msg, sizeof(msg), 0,
                 (struct sockaddr *)&client_addr, &client_addr_len);
    if (n < 0)
    {
        perror("server: recvfrom");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if ((uint32_t)n < sizeof(msg_type_t) || msg.msg_type != MSG_SUBSCRIBE)
    {
        fprintf(stderr, "server: expected SUBSCRIBE, got msg_type=%u\n",
                msg.msg_type);
        close(sockfd);
        return EXIT_FAILURE;
    }

    have_client = 1;
    printf("server: client subscribed, sending updates to %s\n",
           client_addr.sun_path);

    /* ------------------------------------------------------------------
     * Phase 2: initialize a simple entity simulation.
     * Entities are placed in a line and given constant velocities so
     * their positions change each update.
     * ------------------------------------------------------------------ */
    for (i = 0; i < entity_count; i++)
    {
        entities[i].id = i;
        entities[i].position.x = (float)i * 5.0f;
        entities[i].position.y = 0.0f;
        entities[i].position.z = 0.0f;
        entities[i].orientation.x = 0.0f;
        entities[i].orientation.y = 0.0f;
        entities[i].orientation.z = 0.0f;
        entities[i].orientation.w = 1.0f;
        entities[i].velocity.x = 1.0f + (float)i * 0.5f;
        entities[i].velocity.y = 0.0f;
        entities[i].velocity.z = 0.0f;
    }

    /* ------------------------------------------------------------------
     * Phase 3: main simulation loop.
     * Update entity state, pack into a datagram, and send to the client.
     * ------------------------------------------------------------------ */
    printf("server: streaming updates (~10 Hz). Press Ctrl+C to stop.\n");
    while (keep_running)
    {
        /* Advance simulation (simple Euler integration, dt = 0.1 s) */
        for (i = 0; i < entity_count; i++)
        {
            entities[i].position.x += entities[i].velocity.x * 0.1f;
            entities[i].position.y += entities[i].velocity.y * 0.1f;
            entities[i].position.z += entities[i].velocity.z * 0.1f;
        }

        /* Build and send the update message */
        memset(&msg, 0, sizeof(msg));
        msg.msg_type = MSG_ENTITY_UPDATE;
        msg.entity_count = (uint32_t)entity_count;
        memcpy(msg.entities, entities, sizeof(entity_t) * (size_t)entity_count);

        if (have_client)
        {
            ssize_t sent = sendto(sockfd, &msg, sizeof(msg), 0,
                                  (struct sockaddr *)&client_addr,
                                  client_addr_len);
            if (sent < 0)
            {
                perror("server: sendto");
                break;
            }
        }

        usleep(100000); /* 100 ms -> 10 updates per second */
    }

    printf("\nserver: shutting down.\n");
    close(sockfd);
    unlink(SERVER_SOCK_PATH);
    return EXIT_SUCCESS;
}
