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
    struct sockaddr_un client_addr;
    struct sockaddr_un server_addr;
    int sockfd;
    ssize_t n;
    update_msg_t msg;
    uint32_t i;

    /* Install signal handler for clean shutdown */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Create UNIX domain datagram socket */
    sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("client: socket");
        return EXIT_FAILURE;
    }

    /* Bind to our own socket path so the server can reply to us */
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    strncpy(client_addr.sun_path, CLIENT_SOCK_PATH,
            sizeof(client_addr.sun_path) - 1);

    unlink(CLIENT_SOCK_PATH);
    if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("client: bind");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("client: bound to %s\n", CLIENT_SOCK_PATH);

    /* Send a SUBSCRIBE message to the server so it knows where to send updates */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SERVER_SOCK_PATH,
            sizeof(server_addr.sun_path) - 1);

    msg.msg_type = MSG_SUBSCRIBE;
    msg.entity_count = 0;

    if (sendto(sockfd, &msg, sizeof(msg), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("client: sendto (subscribe)");
        close(sockfd);
        unlink(CLIENT_SOCK_PATH);
        return EXIT_FAILURE;
    }

    printf("client: subscribed to server at %s\n", SERVER_SOCK_PATH);
    printf("client: waiting for entity updates. Press Ctrl+C to stop.\n\n");

    /* Receive and print updates.
     *
     * Because SOCK_DGRAM queues messages in the socket receive buffer,
     * a slow client can fall behind.  To avoid processing stale state,
     * we drain all pending datagrams and act only on the most recent one.
     */
    while (keep_running)
    {
        /* Block until at least one message is available */
        n = recvfrom(sockfd, &msg, sizeof(msg), 0, NULL, NULL);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("client: recvfrom");
            break;
        }

        /* Drain any additional pending datagrams; keep only the latest */
        for (;;)
        {
            ssize_t m = recvfrom(sockfd, &msg, sizeof(msg), MSG_DONTWAIT,
                                 NULL, NULL);
            if (m < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                if (errno == EINTR)
                {
                    continue;
                }
                perror("client: recvfrom drain");
                break;
            }
            /* msg now holds the newest datagram; keep draining */
        }

        if (msg.msg_type != MSG_ENTITY_UPDATE)
        {
            fprintf(stderr, "client: unexpected msg_type=%u\n", msg.msg_type);
            continue;
        }

        printf("client: update received (%u entities)\n", msg.entity_count);
        for (i = 0; i < msg.entity_count && i < MAX_ENTITIES; i++)
        {
            printf("  id=%-3d pos=(%6.2f,%6.2f,%6.2f) "
                   "ori=(%6.2f,%6.2f,%6.2f,%6.2f) "
                   "vel=(%6.2f,%6.2f,%6.2f)\n",
                   msg.entities[i].id,
                   msg.entities[i].position.x,
                   msg.entities[i].position.y,
                   msg.entities[i].position.z,
                   msg.entities[i].orientation.x,
                   msg.entities[i].orientation.y,
                   msg.entities[i].orientation.z,
                   msg.entities[i].orientation.w,
                   msg.entities[i].velocity.x,
                   msg.entities[i].velocity.y,
                   msg.entities[i].velocity.z);
        }
        printf("\n");
        fflush(stdout);
    }

    printf("\nclient: shutting down.\n");
    close(sockfd);
    unlink(CLIENT_SOCK_PATH);
    return EXIT_SUCCESS;
}
