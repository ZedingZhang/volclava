/* Exercise the real initializer and XDR codecs without starting a cluster. */
#include <assert.h>

#ifndef MBD_SERV_SOURCE
#define MBD_SERV_SOURCE "../lsbatch/daemons/mbd.serv.c"
#endif
#include MBD_SERV_SOURCE

/* Only the allocation-failure daemon shutdown path is outside this harness. */
void *
my_malloc(int size, const char *caller)
{
    void *allocation = malloc(size);
    (void) caller;
    assert(allocation != NULL);
    return allocation;
}

static char *hosts[] = { "host-a", "host-b", "host-c" };

static void
source_request(struct submitReq *request, int count)
{
    memset(request, 0, sizeof(*request));
    request->options = count ? SUB_HOST : 0;
    request->numAskedHosts = count;
    request->askedHosts = count ? hosts : NULL;
    request->numProcessors = request->maxNumProcessors = 1;
    request->queue = "normal";
    request->command = "true";
    request->fromHost = "localhost";
    request->jobFile = request->inFile = request->outFile = "";
    request->errFile = request->hostSpec = request->chkpntDir = "";
    request->subHomeDir = request->cwd = "";
    request->inFileSpool = request->commandSpool = "";
}

static unsigned int
encode_request(char *buffer, unsigned int capacity, int count, int modify)
{
    struct modifyReq source = {0};
    struct LSFHeader header = {0};
    XDR stream;
    unsigned int length;

    source_request(&source.submitReq, count);
    source.jobId = 42;
    source.jobIdStr = "42";
    xdrmem_create(&stream, buffer, capacity, XDR_ENCODE);
    if (modify)
        assert(xdr_modifyReq(&stream, &source, &header));
    else
        assert(xdr_submitReq(&stream, &source.submitReq, &header));
    length = xdr_getpos(&stream);
    xdr_destroy(&stream);
    return length;
}

static bool_t
decode_request(char *buffer, unsigned int length, struct modifyReq *request,
               int modify)
{
    struct LSFHeader header = {0};
    XDR stream;
    bool_t success;

    xdrmem_create(&stream, buffer, length, XDR_DECODE);
    if (modify)
        success = xdr_modifyReq(&stream, request, &header);
    else
        success = xdr_submitReq(&stream, &request->submitReq, &header);
    xdr_destroy(&stream);
    return success;
}

static void
check_hosts(struct submitReq *request, int count)
{
    int i;
    assert(request->numAskedHosts == count);
    if (count == 0)
        assert(request->askedHosts == NULL);
    for (i = 0; i < count; i++)
        assert(strcmp(request->askedHosts[i], hosts[i]) == 0);
}

static void
exercise_requests(int modify)
{
    struct modifyReq request = {0};
    struct submitMbdReply reply = {0};
    struct LSFHeader header = {0};
    static const int counts[] = {1, 3, 0, 2, 1, 0};
    char buffer[8192];
    unsigned int length;
    int first = TRUE;
    int i;
    XDR stream;

    for (i = 0; i < 1000; i++) {
        int count = counts[i % (sizeof(counts) / sizeof(counts[0]))];
        length = encode_request(buffer, sizeof(buffer), count, modify);
        initSubmit(&first, &request.submitReq, &reply);
        assert(decode_request(buffer, length, &request, modify));
        check_hosts(&request.submitReq, count);
        if (modify) {
            assert(request.jobId == 42);
            assert(strcmp(request.jobIdStr, "42") == 0);
        }

        if (i % 10 == 0) {
            /* A late decode error after the host list was allocated. */
            length = encode_request(buffer, sizeof(buffer), 3, modify);
            initSubmit(&first, &request.submitReq, &reply);
            assert(!decode_request(buffer, length - (modify ? 20 : 4),
                                   &request, modify));
            check_hosts(&request.submitReq, 0);

            /* An early error, before the nested submit codec in bmod. */
            length = encode_request(buffer, sizeof(buffer), 1, modify);
            initSubmit(&first, &request.submitReq, &reply);
            assert(decode_request(buffer, length, &request, modify));
            initSubmit(&first, &request.submitReq, &reply);
            assert(!decode_request(buffer, 0, &request, modify));
        }
    }

    /* Free the nested request directly; XDR_FREE needs no wire framing. */
    xdrmem_create(&stream, NULL, 0, XDR_FREE);
    assert(xdr_submitReq(&stream, &request.submitReq, &header));
    xdr_destroy(&stream);
    free(request.jobIdStr);
    free(reply.badJobName);
    free(reply.pendLimitReason);
}

int
main(void)
{
    exercise_requests(FALSE);
    exercise_requests(TRUE);
    puts("PASS: 1000 submit and 1000 modify requests, plus decode-error recovery");
    return 0;
}
