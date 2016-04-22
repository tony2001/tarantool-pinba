#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#include "uthash.h"

#include "pinba.pb-c.h"

#define PINBA_STR_BUFFER_SIZE 257
#define PINBA_WORD_SIZE 256
#define PINBA_SERVER_SIZE 256
#define PINBA_PORT_SIZE 8

#define PINBA_RESOLVE_FREQ 60 //seconds

typedef struct {
	int fd;
	struct sockaddr_storage sockaddr;
	int sockaddr_len;
	time_t last_resolve_time;
} pinba_socket_t;

typedef struct {
	char key[PINBA_SERVER_SIZE + PINBA_PORT_SIZE + 1];
	char server_name[PINBA_SERVER_SIZE];
	char port[PINBA_PORT_SIZE];
	pinba_socket_t sock;
	UT_hash_handle hh;
} pinba_hash_sock_t;

pinba_hash_sock_t *g_sock_hash = NULL;
char g_hostname[PINBA_STR_BUFFER_SIZE];

#define memcpy_static(buf, data, data_len, result_len)	\
	do {												\
		size_t tmp_len = (data_len);					\
		if (tmp_len >= sizeof(buf)) {					\
			tmp_len = sizeof(buf) - 1;					\
		}												\
		memcpy((buf), (data), tmp_len);					\
		(buf)[tmp_len] = '\0';							\
		(result_len) = tmp_len;							\
	} while(0);

#define memcpy_static_nl(buf, data, data_len)			\
	do {												\
		size_t tmp_len = (data_len);					\
		if (tmp_len >= sizeof(buf)) {					\
			tmp_len = sizeof(buf) - 1;					\
		}												\
		memcpy((buf), (data), tmp_len);					\
		(buf)[tmp_len] = '\0';							\
	} while(0);

#define COPY_STR(attr, var, default_str) \
	if (var && var[0] != '\0') { \
		attr = strdup(var); \
	} else { \
		attr = strdup(default_str); \
	}

static int pinba_resolve_and_open_socket(const char *host, int port, pinba_socket_t **sock, char *errbuf, int errbuf_size) /* {{{ */
{
	char port_str[64];
	char tmp_key[PINBA_SERVER_SIZE + PINBA_PORT_SIZE + 1];
	struct addrinfo *ai_list, *ai_ptr, ai_hints;
	int status, tmp_key_len, port_str_len;
	pinba_hash_sock_t *element;

	*sock = NULL;

	port_str_len = snprintf(port_str, sizeof(port_str), "%d", port);

	tmp_key_len = snprintf(tmp_key, sizeof(tmp_key), "%s:%d", host, port);
	if (tmp_key_len < 0) {
		return -1;
	}

	HASH_FIND_STR(g_sock_hash, tmp_key, element);
	if (!element) {
		element = calloc(1, sizeof(pinba_hash_sock_t));
		if (!element) {
			snprintf(errbuf, errbuf_size, "calloc() failed");
			return -1;
		}

		memcpy_static_nl(element->key, tmp_key, tmp_key_len);
		memcpy_static_nl(element->server_name, host, strlen(host));
		memcpy_static_nl(element->port, port_str, port_str_len);
		HASH_ADD_STR(g_sock_hash, key, element);
	} else {
		*sock = &element->sock;

		if (time(NULL) < (element->sock.last_resolve_time + PINBA_RESOLVE_FREQ)) {
			return (element->sock.fd < 0) ? -1 : 0;
		}

		if (element->sock.fd >= 0) {
			close(element->sock.fd);
		}
	}

	/* (re-)resolve */
	element->sock.fd = -1;

	/* reset time rightaway to prevent repeated DNS requests in case of failure */
	element->sock.last_resolve_time = time(NULL);

	memset(&ai_hints, 0, sizeof(ai_hints));
	ai_hints.ai_flags = 0;
#ifdef AI_ADDRCONFIG
	ai_hints.ai_flags |= AI_ADDRCONFIG;
#endif
	ai_hints.ai_family = AF_UNSPEC;
	ai_hints.ai_socktype  = SOCK_DGRAM;
	ai_hints.ai_addr = NULL;
	ai_hints.ai_canonname = NULL;
	ai_hints.ai_next = NULL;

	ai_list = NULL;
	status = getaddrinfo(element->server_name, element->port, &ai_hints, &ai_list);
	if (status != 0) {
		snprintf(errbuf, errbuf_size, "getaddrinfo(\"%s:%s\") failed: %s", element->server_name, element->port, gai_strerror(status));
		return -1;
	}

	for (ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
		element->sock.fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
		if (element->sock.fd >= 0) {
			memcpy(&(element->sock.sockaddr), ai_ptr->ai_addr, ai_ptr->ai_addrlen);
			element->sock.sockaddr_len = ai_ptr->ai_addrlen;
			break;
		}
	}

	if (element->sock.fd < 0) {
		snprintf(errbuf, errbuf_size, "socket() failed: %s", strerror(errno));
		freeaddrinfo(ai_list);
		return -1;
	}
	freeaddrinfo(ai_list);

	*sock = &element->sock;
	return 0;
}
/* }}} */

static int pinba_send_data(pinba_socket_t *sock, Pinba__Request *request, size_t packed_size, char *errbuf, int errbuf_size) /* {{{ */
{
	uint8_t *buf;
	uint8_t buf_static[1024];
	int res, ret = 0;

	if (packed_size == 0) {
		packed_size = pinba__request__get_packed_size(request);
	}

	if (packed_size > sizeof(buf_static)) {
		buf = calloc(1, packed_size);
	} else {
		buf = buf_static;
	}
	pinba__request__pack(request, buf);

	res = sendto(sock->fd, buf, packed_size, 0, (struct sockaddr *) &sock->sockaddr, sock->sockaddr_len);
	if (res < 0) {
		snprintf(errbuf, errbuf_size, "sendto() failed: %s", strerror(res));
		ret = -1;
	}

	if (buf != buf_static) {
		free(buf);
	}
	return ret;
}
/* }}} */

static int lbox_pinba_send(struct lua_State *L) /* {{{ */
{
	const char *server_host;
	int server_port, res;
	char errbuf[512];
	pinba_socket_t *sock;

	server_host = luaL_optstring(L, 1, "");
	if (!server_host || server_host[0] == '\0') {
		return luaL_error(L, "Pinba server host cannot be empty");
	}
	
	server_port = luaL_optint(L, 2, 0);
	if (server_port <= 0) {
		return luaL_error(L, "Pinba server port must be greater than 0");
	}

	res = pinba_resolve_and_open_socket(server_host, server_port, &sock, errbuf, sizeof(errbuf));
	if (res < 0) {
		return luaL_error(L, "failed to open socket to Pinba server: %s", errbuf);
	}

	{
		char hostname_tmp[PINBA_STR_BUFFER_SIZE] = {0};
		Pinba__Request *request;
		const char *str_value;
		long long_value;
		double double_value;

		request = malloc(sizeof(Pinba__Request));
		pinba__request__init(request);

		if (g_hostname[0] == '\0') {
			if (gethostname(hostname_tmp, sizeof(hostname_tmp)) == 0) {
				memcpy(g_hostname, hostname_tmp, PINBA_STR_BUFFER_SIZE);
			} else {
				memcpy(g_hostname, "unknown", sizeof("unknown"));
			}
		}

		lua_getfield(L, 3, "hostname");
		str_value = luaL_checkstring(L, -1);
		COPY_STR(request->hostname, str_value, g_hostname);
		lua_pop(L, 1);

		lua_getfield(L, 3, "server_name");
		str_value = luaL_checkstring(L, -1);
		COPY_STR(request->server_name, str_value, "");
		lua_pop(L, 1);

		lua_getfield(L, 3, "script_name");
		str_value = luaL_checkstring(L, -1);
		COPY_STR(request->script_name, str_value, "");
		lua_pop(L, 1);

		lua_getfield(L, 3, "request_count");
		long_value = luaL_checklong(L, -1);

		if (long_value > 0 && long_value < UINT_MAX) {
			request->request_count = long_value;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "document_size");
		long_value = luaL_checklong(L, -1);
		if (long_value > 0 && long_value < UINT_MAX) {
			request->document_size = long_value;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "memory_peak");
		long_value = luaL_checklong(L, -1);
		if (long_value > 0 && long_value < UINT_MAX) {
			request->memory_peak = long_value;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "memory_footprint");
		long_value = luaL_checklong(L, -1);
		if (long_value > 0 && long_value < UINT_MAX) {
			request->memory_footprint = long_value;
			request->has_memory_footprint = 1;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "request_time");
		double_value = luaL_checknumber(L, -1);
		if (double_value > 0) {
			request->request_time = double_value;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "ru_utime");
		double_value = luaL_checknumber(L, -1);
		if (double_value > 0) {
			request->ru_utime = double_value;
		}
		lua_pop(L, 1);
		
		lua_getfield(L, 3, "ru_stime");
		double_value = luaL_checknumber(L, -1);
		if (double_value > 0) {
			request->ru_stime = double_value;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "status");
		long_value = luaL_checklong(L, -1);
		if (long_value > 0 && long_value < UINT_MAX) {
			request->status = long_value;
			request->has_status = 1;
		}
		lua_pop(L, 1);

		lua_getfield(L, 3, "schema");
		str_value = luaL_checkstring(L, -1);
		if (str_value && str_value[0] != '\0') {
			request->schema = strdup(str_value);
			printf("schema: %s\n", str_value);
		}
		lua_pop(L, 1);

		res = pinba_send_data(sock, request, 0, errbuf, sizeof(errbuf));
		pinba__request__free_unpacked(request, NULL);
		if (res) {
			return luaL_error(L, "failed to send data to Pinba server: %s", errbuf);
		}
	}

	return 0;
}
/* }}} */

LUA_API int luaopen_pinba_lib(lua_State *L) /* {{{ */
{
	static const struct luaL_reg reg[] = {
		{"send", lbox_pinba_send},
		{NULL, NULL}
	};

	luaL_register(L, "box._lib", reg);
	return 1;
}
/* }}} */

