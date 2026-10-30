/*
 * Smart Student Performance & Result Management System
 * C HTTP backend  (serves the dashboard + REST API)
 *
 * - Records are saved in students.dat:
 *       int32 count, then `count` records of 96 bytes each (max 200)
 *   Subjects (in order): Artificial Intelligence, Computer Network, Computer Aptitude,
 *   Python, Web Programming, ADBMS, PL/SQL.
 *   NOTE: the older 5-subject students.dat (88-byte records) is NOT compatible.
 *   If such a file is found it is renamed to students.dat.old and a new one is started.
 *
 * Files served: index.html, style.css, app.js (keep them in the same folder)
 *
 * Build (Windows, MinGW):   gcc server.c -o server.exe -lws2_32
 * Build (Linux / macOS):    gcc server.c -o server
 * Run:                      server            (default port 8080)
 *                           server 9000       (custom port)
 * Open:                     http://localhost:8080
 *
 * REST API
 *   GET    /api/students            list all students
 *   GET    /api/students/<roll>     one student
 *   POST   /api/students            add     {"roll":101,"name":"Asha","marks":[90,80,70,60,50]}
 *   PUT    /api/students/<roll>     update  {"name":"Asha K","marks":[..5 numbers..]}
 *   DELETE /api/students/<roll>     delete
 *   GET    /api/topper              student(s) with the highest total
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define CLOSESOCK closesocket
  #define BAD_SOCK INVALID_SOCKET
#else
  #include <unistd.h>
  #include <signal.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  typedef int sock_t;
  #define CLOSESOCK close
  #define BAD_SOCK (-1)
#endif

#define MAX_STUDENTS 200
#define NUM_SUBJECTS 7
#define DATA_FILE    "students.dat"
#define REQ_MAX      (64 * 1024)

/* 7 subjects. Record size = 4 + 50 + 2 (padding) + 28 + 4 + 4 + 4 = 96 bytes */
typedef struct {
    int   roll;
    char  name[50];
    float marks[NUM_SUBJECTS];
    float total;
    float percentage;
    char  grade[4];
} Student;

typedef char size_check[(sizeof(Student) == 96) ? 1 : -1];

static Student db[MAX_STUDENTS];
static int     count = 0;

/* ------------------------------------------------------------------ */
/* Business logic (same rules as the dashboard)                        */
/* ------------------------------------------------------------------ */
static void assign_grade(Student *s) {
    float p = s->percentage;
    const char *g = p >= 90 ? "A+" : p >= 80 ? "A" : p >= 70 ? "B" :
                    p >= 60 ? "C"  : p >= 50 ? "D" : p >= 40 ? "E" : "F";
    strcpy(s->grade, g);
}

static void calculate(Student *s) {
    s->total = 0;
    for (int i = 0; i < NUM_SUBJECTS; i++) s->total += s->marks[i];
    s->percentage = s->total / NUM_SUBJECTS;
    assign_grade(s);
}

static int find_index(int roll) {
    for (int i = 0; i < count; i++) if (db[i].roll == roll) return i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* File storage                                                        */
/* ------------------------------------------------------------------ */
static void save_to_file(void) {
    FILE *f = fopen(DATA_FILE, "wb");
    if (!f) { fprintf(stderr, "Error: could not save data.\n"); return; }
    fwrite(&count, sizeof(int), 1, f);
    fwrite(db, sizeof(Student), count, f);
    fclose(f);
}

static void load_from_file(void) {
    FILE *f = fopen(DATA_FILE, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    int n = 0;
    if (fread(&n, sizeof(int), 1, f) != 1 || n < 0 || n > MAX_STUDENTS ||
        size != (long)(sizeof(int) + (size_t)n * sizeof(Student))) {
        fclose(f);
        /* wrong format (for example the old 5-subject file): keep it safe, start fresh */
        remove(DATA_FILE ".old");
        if (rename(DATA_FILE, DATA_FILE ".old") == 0)
            printf(" Old/incompatible %s found: renamed to %s.old, starting with an empty list.\n", DATA_FILE, DATA_FILE);
        return;
    }
    if (n > 0 && fread(db, sizeof(Student), n, f) != (size_t)n) n = 0;
    count = n;
    fclose(f);
    for (int i = 0; i < count; i++) {          /* be safe with old data */
        db[i].name[sizeof(db[i].name) - 1] = '\0';
        calculate(&db[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Growable string buffer for building JSON                            */
/* ------------------------------------------------------------------ */
typedef struct { char *p; size_t len, cap; } Buf;

static void buf_add(Buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2 + 256;
        b->p = (char *)realloc(b->p, b->cap);
        if (!b->p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void buf_str(Buf *b, const char *s) { buf_add(b, s, strlen(s)); }
static void buf_fmt(Buf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#include <stdarg.h>
static void buf_fmt(Buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) buf_add(b, tmp, (size_t)(n < (int)sizeof tmp ? n : (int)sizeof tmp - 1));
}

static void json_escape(Buf *b, const char *s) {
    buf_str(b, "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"')       buf_str(b, "\\\"");
        else if (c == '\\') buf_str(b, "\\\\");
        else if (c < 0x20)  buf_fmt(b, "\\u%04x", c);
        else                buf_add(b, (const char *)&c, 1);
    }
    buf_str(b, "\"");
}

static void student_json(Buf *b, const Student *s) {
    buf_fmt(b, "{\"roll\":%d,\"name\":", s->roll);
    json_escape(b, s->name);
    buf_str(b, ",\"marks\":[");
    for (int i = 0; i < NUM_SUBJECTS; i++)
        buf_fmt(b, "%s%.2f", i ? "," : "", s->marks[i]);
    buf_fmt(b, "],\"total\":%.2f,\"percentage\":%.2f,\"grade\":\"%s\",\"status\":\"%s\"}",
            s->total, s->percentage, s->grade, s->percentage >= 40 ? "PASS" : "FAIL");
}

/* ------------------------------------------------------------------ */
/* Tiny JSON reader (enough for our request bodies)                    */
/* ------------------------------------------------------------------ */
static const char *find_key(const char *json, const char *key) {
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ':') return NULL;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int json_get_number(const char *json, const char *key, double *out) {
    const char *p = find_key(json, key);
    if (!p) return 0;
    char *end;
    double v = strtod(p, &end);
    if (end == p) return 0;
    *out = v;
    return 1;
}

static void put_utf8(char *dst, size_t *n, size_t max, unsigned cp) {
    char t[4]; int k = 0;
    if (cp < 0x80) t[k++] = (char)cp;
    else if (cp < 0x800) { t[k++] = (char)(0xC0 | (cp >> 6)); t[k++] = (char)(0x80 | (cp & 0x3F)); }
    else { t[k++] = (char)(0xE0 | (cp >> 12)); t[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[k++] = (char)(0x80 | (cp & 0x3F)); }
    if (*n + k < max) { memcpy(dst + *n, t, k); *n += k; }
}

static int json_get_string(const char *json, const char *key, char *out, size_t max) {
    const char *p = find_key(json, key);
    if (!p || *p != '"') return 0;
    p++;
    size_t n = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': case 'r': case 't': if (n + 1 < max) out[n++] = ' '; p++; break;
                case 'u': {
                    unsigned cp = 0;
                    if (sscanf(p + 1, "%4x", &cp) == 1) { put_utf8(out, &n, max, cp); p += 5; }
                    else p++;
                    break;
                }
                default: if (n + 1 < max) out[n++] = *p; p++; break;
            }
        } else {
            if (n + 1 < max) out[n++] = *p;
            p++;
        }
    }
    /* don't cut a UTF-8 sequence in half */
    while (n > 0 && ((unsigned char)out[n - 1] & 0xC0) == 0x80) n--;
    if (n > 0 && ((unsigned char)out[n - 1] & 0x80)) {
        size_t i = n - 1;
        while (i > 0 && ((unsigned char)out[i] & 0xC0) == 0x80) i--;
        unsigned char lead = (unsigned char)out[i];
        size_t need = lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        if (n - i < need) n = i;
    }
    out[n] = '\0';
    return 1;
}

static int json_get_marks(const char *json, float out[NUM_SUBJECTS]) {
    const char *p = find_key(json, "marks");
    if (!p || *p != '[') return 0;
    p++;
    for (int i = 0; i < NUM_SUBJECTS; i++) {
        char *end;
        double v = strtod(p, &end);
        if (end == p) return 0;
        if (v < 0 || v > 100) return -1;           /* out of range */
        out[i] = (float)v;
        p = end;
        while (*p && isspace((unsigned char)*p)) p++;
        if (i < NUM_SUBJECTS - 1) { if (*p != ',') return 0; p++; }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* HTTP helpers                                                        */
/* ------------------------------------------------------------------ */
static void send_all(sock_t s, const char *data, size_t len) {
    while (len > 0) {
        int n = send(s, data, (int)(len > 65536 ? 65536 : len), 0);
        if (n <= 0) return;
        data += n; len -= (size_t)n;
    }
}

static void send_response(sock_t s, int code, const char *status, const char *ctype,
                          const char *body, size_t blen) {
    char head[512];
    int n = snprintf(head, sizeof head,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n",
        code, status, ctype, blen);
    send_all(s, head, (size_t)n);
    if (blen) send_all(s, body, blen);
}

static void send_json(sock_t s, int code, const char *status, Buf *b) {
    send_response(s, code, status, "application/json; charset=utf-8", b->p ? b->p : "", b->len);
}

static void send_error(sock_t s, int code, const char *status, const char *msg) {
    Buf b = {0};
    buf_str(&b, "{\"error\":");
    json_escape(&b, msg);
    buf_str(&b, "}");
    send_json(s, code, status, &b);
    free(b.p);
}

static void serve_file(sock_t s, const char *file, const char *ctype) {
    FILE *f = fopen(file, "rb");
    if (!f) {
        const char *m = "File not found. Keep index.html, style.css and app.js in the same folder as the server.";
        send_response(s, 404, "Not Found", "text/plain; charset=utf-8", m, strlen(m));
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = (char *)malloc((size_t)sz + 1);
    size_t got = fread(data, 1, (size_t)sz, f);
    fclose(f);
    send_response(s, 200, "OK", ctype, data, got);
    free(data);
}

/* ------------------------------------------------------------------ */
/* API handlers                                                        */
/* ------------------------------------------------------------------ */
static void api_list(sock_t s) {
    Buf b = {0};
    buf_str(&b, "[");
    for (int i = 0; i < count; i++) { if (i) buf_str(&b, ","); student_json(&b, &db[i]); }
    buf_str(&b, "]");
    send_json(s, 200, "OK", &b);
    free(b.p);
}

static void api_get(sock_t s, int roll) {
    int i = find_index(roll);
    if (i < 0) { send_error(s, 404, "Not Found", "Student not found."); return; }
    Buf b = {0};
    student_json(&b, &db[i]);
    send_json(s, 200, "OK", &b);
    free(b.p);
}

static void api_topper(sock_t s) {
    Buf b = {0};
    if (count == 0) { buf_str(&b, "[]"); send_json(s, 200, "OK", &b); free(b.p); return; }
    float best = db[0].total;
    for (int i = 1; i < count; i++) if (db[i].total > best) best = db[i].total;
    buf_str(&b, "[");
    int first = 1;
    for (int i = 0; i < count; i++) {
        if (db[i].total == best) { if (!first) buf_str(&b, ","); student_json(&b, &db[i]); first = 0; }
    }
    buf_str(&b, "]");
    send_json(s, 200, "OK", &b);
    free(b.p);
}

static void api_add(sock_t s, const char *body) {
    double roll_d;
    char name[50];
    float marks[NUM_SUBJECTS];
    if (!json_get_number(body, "roll", &roll_d) || roll_d < 1 || roll_d != (int)roll_d)
        { send_error(s, 400, "Bad Request", "Enter a valid positive roll number."); return; }
    if (!json_get_string(body, "name", name, sizeof name) || name[0] == '\0')
        { send_error(s, 400, "Bad Request", "Enter the student's name."); return; }
    int m = json_get_marks(body, marks);
    if (m != 1) { send_error(s, 400, "Bad Request", "Each subject mark must be a number between 0 and 100."); return; }
    int roll = (int)roll_d;
    if (find_index(roll) >= 0) { send_error(s, 409, "Conflict", "That roll number already exists."); return; }
    if (count >= MAX_STUDENTS) { send_error(s, 507, "Insufficient Storage", "Database full."); return; }

    Student *st = &db[count];
    memset(st, 0, sizeof *st);
    st->roll = roll;
    strncpy(st->name, name, sizeof st->name - 1);
    memcpy(st->marks, marks, sizeof marks);
    calculate(st);
    count++;
    save_to_file();

    Buf b = {0};
    student_json(&b, st);
    send_json(s, 201, "Created", &b);
    free(b.p);
}

static void api_update(sock_t s, int roll, const char *body) {
    int i = find_index(roll);
    if (i < 0) { send_error(s, 404, "Not Found", "Student not found."); return; }
    char name[50];
    float marks[NUM_SUBJECTS];
    int has_name = json_get_string(body, "name", name, sizeof name);
    int has_marks = json_get_marks(body, marks);
    if (has_name && name[0] == '\0') { send_error(s, 400, "Bad Request", "Student name cannot be empty."); return; }
    if (has_marks == -1) { send_error(s, 400, "Bad Request", "Each subject mark must be between 0 and 100."); return; }
    if (has_name) { memset(db[i].name, 0, sizeof db[i].name); strncpy(db[i].name, name, sizeof db[i].name - 1); }
    if (has_marks == 1) memcpy(db[i].marks, marks, sizeof marks);
    calculate(&db[i]);
    save_to_file();

    Buf b = {0};
    student_json(&b, &db[i]);
    send_json(s, 200, "OK", &b);
    free(b.p);
}

static void api_delete(sock_t s, int roll) {
    int i = find_index(roll);
    if (i < 0) { send_error(s, 404, "Not Found", "Student not found."); return; }
    for (int j = i; j < count - 1; j++) db[j] = db[j + 1];
    count--;
    save_to_file();
    Buf b = {0};
    buf_str(&b, "{\"deleted\":true}");
    send_json(s, 200, "OK", &b);
    free(b.p);
}

/* ------------------------------------------------------------------ */
/* Request handling                                                    */
/* ------------------------------------------------------------------ */
static void handle_client(sock_t c) {
    char *req = (char *)malloc(REQ_MAX + 1);
    if (!req) return;
    size_t got = 0;
    char *body = NULL;
    size_t content_len = 0;

    /* read until headers are complete, then read the body */
    while (got < REQ_MAX) {
        int n = recv(c, req + got, (int)(REQ_MAX - got), 0);
        if (n <= 0) break;
        got += (size_t)n;
        req[got] = '\0';
        body = strstr(req, "\r\n\r\n");
        if (body) {
            body += 4;
            const char *cl = strstr(req, "Content-Length:");
            if (!cl) cl = strstr(req, "content-length:");
            content_len = cl ? (size_t)atol(cl + 15) : 0;
            if (got - (size_t)(body - req) >= content_len) break;
        }
    }
    if (!body) { free(req); return; }
    req[got] = '\0';

    char method[16] = {0}, path[256] = {0};
    if (sscanf(req, "%15s %255s", method, path) != 2) { free(req); return; }
    char *q = strchr(path, '?');
    if (q) *q = '\0';

    printf("%s %s\n", method, path);

    if (strcmp(method, "OPTIONS") == 0) {
        send_response(c, 204, "No Content", "text/plain", "", 0);
    }
    else if (strcmp(method, "GET") == 0 && (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)) {
        serve_file(c, "index.html", "text/html; charset=utf-8");
    }
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/style.css") == 0) {
        serve_file(c, "style.css", "text/css; charset=utf-8");
    }
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/app.js") == 0) {
        serve_file(c, "app.js", "application/javascript; charset=utf-8");
    }
    else if (strncmp(path, "/api/", 5) == 0) {
        if (strcmp(path, "/api/students") == 0) {
            if (strcmp(method, "GET") == 0)       api_list(c);
            else if (strcmp(method, "POST") == 0) api_add(c, body);
            else send_error(c, 405, "Method Not Allowed", "Method not allowed.");
        }
        else if (strncmp(path, "/api/students/", 14) == 0) {
            int roll = atoi(path + 14);
            if (strcmp(method, "GET") == 0)         api_get(c, roll);
            else if (strcmp(method, "PUT") == 0)    api_update(c, roll, body);
            else if (strcmp(method, "DELETE") == 0) api_delete(c, roll);
            else send_error(c, 405, "Method Not Allowed", "Method not allowed.");
        }
        else if (strcmp(path, "/api/topper") == 0 && strcmp(method, "GET") == 0) {
            api_topper(c);
        }
        else send_error(c, 404, "Not Found", "Unknown API endpoint.");
    }
    else {
        const char *m = "Not found";
        send_response(c, 404, "Not Found", "text/plain; charset=utf-8", m, strlen(m));
    }
    free(req);
}

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 8080;
    if (port <= 0 || port > 65535) port = 8080;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { fprintf(stderr, "WSAStartup failed\n"); return 1; }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    load_from_file();

    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == BAD_SOCK) { fprintf(stderr, "Could not create socket\n"); return 1; }
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* this computer only */
    addr.sin_port = htons((unsigned short)port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) != 0) {
        fprintf(stderr, "Could not bind to port %d (is it already in use? try: server 9000)\n", port);
        return 1;
    }
    listen(srv, 16);

    printf("=====================================================\n");
    printf(" Smart Student Performance & Result Management System\n");
    printf("=====================================================\n");
    printf(" Loaded %d student record(s) from %s\n", count, DATA_FILE);
    printf(" Open in your browser:  http://localhost:%d\n", port);
    printf(" Press Ctrl+C to stop.\n\n");

    for (;;) {
        sock_t c = accept(srv, NULL, NULL);
        if (c == BAD_SOCK) continue;
        handle_client(c);
        CLOSESOCK(c);
    }
}
