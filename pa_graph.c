#include<stdio.h>

#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include <glib.h>
#include <gmodule.h>

#include<unistd.h>

#include<graphviz/gvc.h>
//	gcc -Wall pa_graph.c -lpulse `pkg-config --cflags --libs glib-2.0`

enum states_t {
	STATE_INIT,
	STATE_GET_SOURCES,
	STATE_GET_SINKS,
	STATE_GET_SOURCE_OUTPUTS,
	STATE_GET_SINK_INPUTS,
	STATE_GET_MODULES,
	STATE_GET_CLIENTS,
	STATE_FINISHED,
}states_t;

struct thread_data_t{
	pa_context *ctx;
	pa_mainloop *loop;
};

enum element_t {
	SOURCE,
	SINK,
	SOURCE_OUTPUT,
	SINK_INPUT,
	MODULE,
	CLIENT,
}element_t;

struct pa_element_t
{
	enum element_t type;
	char *name;
	int index;
	int owner_id;
	int client;
	int source_id;
	int sink_id;
	int mute;
	Agnode_t *node;
};

GSList *elements = NULL;

static int end = 0;

Agraph_t *graph;
GVC_t *gvc;

char *get_broken_string(const char *str)
{
	char *f_str = NULL;
	size_t len = 0, i = 0;
	int dist = 0;

	len = strlen(str);
	f_str = malloc(len);
	if(!f_str)
	{
		printf("malloc() failed\n");
		return NULL;
	}
	for(i = 0; i < len; i++)
	{
		if((str[i] == '.') && (dist >= 10) && ((len - i) > 10))
		{
			f_str[i] = '\n';
			dist = 0;
		}
		else
		{
			f_str[i] = str[i];
			dist++;
		}
	}
	return f_str;
}

char *get_element_type_name(enum element_t type)
{
	switch(type)
	{
		case SINK:
		return "SINK";

		case SOURCE:
		return "SOURCE";

		case SINK_INPUT:
		return "SINK INPUT";

		case SOURCE_OUTPUT:
		return "SOURCE OUTPUT";

		case MODULE:
		return "MODULE";

		case CLIENT:
		return "CLIENT";
	
		default:
		return "unknown";
	}
}

struct pa_element_t *get_node_by_id(int id, enum element_t type)
{
	struct pa_element_t *element;
	for(int i = 0; i < g_slist_length(elements); i++)
	{
		element = (struct pa_element_t *)g_slist_nth(elements, i)->data;
		printf("   SEARCH SOURCE %d <--> %d\n", id, element->source_id);
		if((element->index == id) && (element->type == type))
		{
			printf("   found source id: %s of type %s\n", element->name, get_element_type_name(type));
			return element;
		}
	}
	return NULL;
}

void add_element(Agnode_t *node, enum element_t type, const char *name, int index, int owner_id, int client, int source_id, int sink_id, int mute)
{
	struct pa_element_t *pa_element = (void *)malloc(sizeof(struct pa_element_t));

	if(pa_element == NULL)
	{
		printf("malloc() failed\n");
		return;
	}
	pa_element->name = get_broken_string(name);
	pa_element->type = type;
	pa_element->index = index;
	pa_element->client = client;
	pa_element->source_id = source_id;
	pa_element->sink_id = sink_id;
	pa_element->owner_id = owner_id;
	pa_element->mute = mute;
	pa_element->node = node;

	elements = g_slist_append(elements, pa_element);
}

void client_cb(pa_context *ctx, const pa_client_info *info, int eol, void *data)
{
	if(eol)
	{
		return;
	}
	printf("   CLIENT    %80s[%d], eol=%d\n", info->name, info->index, eol);
	add_element(NULL, CLIENT, info->name, info->index, info->owner_module, -1, -1, -1, -1);
}

void module_cb(pa_context *ctx, const pa_module_info *info, int eol, void *data)
{
	if(eol)
	{
		return;
	}
	printf("   MODULE  %80s[%d], eol=%d\n", info->name, info->index, eol);
	add_element(NULL, MODULE, info->name, info->index, -1, -1, -1, -1, -1);
}

void sink_cb(pa_context *ctx, const pa_sink_info *info, int eol, void *data)
{
	if(eol)
	{
		return;
	}
	printf("   SINK %80s[%d], eol=%d\n", info->name, info->index, eol);
	Agnode_t *s = agnode(graph, get_broken_string(info->name), 1);
	agsafeset(s, "color", "green", "green");
	add_element(s, SINK, info->name, info->index, info->owner_module, -1, -1, -1, info->mute);
}

void source_cb(pa_context *ctx, const pa_source_info *info, int eol, void *data)
{
	if(eol)
	{
		return;
	}
	printf("   SRC  %80s[%d], eol=%d\n", info->name, info->index, eol);
	Agnode_t *s = agnode(graph, get_broken_string(info->name), 1);
	agsafeset(s, "color", "red", "red");
	//agsafeset(s, "shape", "cds", "cds");
	add_element(s, SOURCE, info->name, info->index, info->owner_module, -1, -1, -1, info->mute);
}

struct pa_element_t *create_node(void *i)
{
	struct pa_element_t *src = NULL;
	pa_sink_input_info *info = (pa_sink_input_info *)i;
	if(info->client != -1)
	{
		src = get_node_by_id(info->client, CLIENT);
		src->node = agnode(graph, strdup(src->name), 1);
	}
	else
	{
		src = get_node_by_id(info->owner_module, MODULE);
		if(src->node == NULL)
		{
			src->node = agnode(graph, get_broken_string(src->name), 1);
		}

	}
	agsafeset(src->node, "color", "blue", "blue");
	return src;
}

void sink_input_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *data)
{
	struct pa_element_t *sink = NULL, *src = NULL;
	if(eol)
	{
		printf(" ------------ END GATHERING ------------ \n\n");
		end = 1;
		return;
	}
	printf("   SINK INPUT %80s[%d] --> sink %d\n", info->name, info->index, info->sink);
	src = create_node((void *)info);	
	sink = get_node_by_id(info->sink, SINK);

	agedge(graph, src->node, sink->node, "TEST", 1);

	add_element(NULL, SINK_INPUT, info->name, info->index, info->owner_module, info->client, -1, info->sink, info->mute);
}

void source_output_cb(pa_context *ctx, const pa_source_output_info *info, int eol, void *data)
{
	struct pa_element_t *sink = NULL, *src = NULL;
	if(eol)
	{
		return;
	}
	printf("   SRC OUT  %80s[%d], SOURCE = %d, client = %d\n", info->name, info->index, info->source, info->client);
	sink = create_node((void *)info);	
	src = get_node_by_id(info->source, SOURCE);

	agedge(graph, src->node, sink->node, "TEST", 1);

	add_element(NULL, SOURCE_OUTPUT, info->name, info->index, info->owner_module, info->client, info->source, -1, info->mute);
}

void *worker_thread(void *data)
{
	enum states_t states = STATE_INIT;
	struct thread_data_t *w_data = NULL;

	if(!data)
	{
		printf("worker: no args\n");
		return NULL;
	}

	w_data = (struct thread_data_t *)data;

	while(!end)
	{
		switch(states)
		{
			case STATE_INIT:
				if(pa_context_get_state(w_data->ctx) == PA_CONTEXT_READY)
					states = STATE_GET_SOURCES;
		
				printf("WORKER waiting, pa state = %d\n", pa_context_get_state(w_data->ctx));
			break;

			case STATE_GET_SOURCES:
			printf("STATE: %s", "request SOURCES\n");
			pa_context_get_source_info_list(w_data->ctx, source_cb, w_data);

			states = STATE_GET_SINKS; 
			break;

			case STATE_GET_SINKS:
			printf("STATE: %s", "request SINKS\n");
			pa_context_get_sink_info_list(w_data->ctx, sink_cb, w_data);
			
			states = STATE_GET_MODULES; 
			break;
			
			case STATE_GET_MODULES:
			printf("STATE: %s", "request MODULES INPUTS\n");
			pa_context_get_module_info_list(w_data->ctx, module_cb, w_data);
			
			states = STATE_GET_CLIENTS;
			break;
			
			case STATE_GET_CLIENTS:
			printf("STATE: %s", "request CLIENTS INPUTS\n");
			pa_context_get_client_info_list(w_data->ctx, client_cb, w_data);
			
			states = STATE_GET_SOURCE_OUTPUTS;
			break;
			
			case STATE_GET_SOURCE_OUTPUTS:
			printf("STATE: %s", "request SOURC OUTPUTS\n");
			pa_context_get_source_output_info_list(w_data->ctx, source_output_cb, w_data);
			
			states = STATE_GET_SINK_INPUTS; 
			break;
			
			case STATE_GET_SINK_INPUTS:
			printf("STATE: %s", "request SINK INPUTS\n");
			pa_context_get_sink_input_info_list(w_data->ctx, sink_input_cb, w_data);
			
			states = STATE_FINISHED;
			break;
			
			case STATE_FINISHED:
			{
			}
			break;
		}
		sleep(0.2);
	}

	printf("FOUND %d elements\n", g_slist_length(elements));
	for(int i = 0; i < g_slist_length(elements); i++)
	{
		struct pa_element_t *element = NULL;
		element = (struct pa_element_t *)g_slist_nth(elements, i)->data;
		if(!element)
		{
			printf("cannot get %dth element\n\n", i);
			return 0;
		}
		printf("%d -> %15s: %80s\n", element->type, get_element_type_name(element->type), element->name);
		switch(element->type)
		{
			case SOURCE:
			{
			}
			break;

			case SINK:
			{
			}
			break;

			case SOURCE_OUTPUT:
			{
			}
			break;

			case SINK_INPUT:
			{
			}
			break;

			case MODULE:
			{
			}
			break;

			case CLIENT:
			{
			}
			break;
		}
	}

	printf("--> LOOP DONE. exit\n\n");
	agwrite(graph, stdout);
	//gvLayout(gvc, graph, "neato");
	gvLayout(gvc, graph, "dot");
	gvRenderFilename(gvc, graph, "png", "/tmp/test.png");
	
	pa_mainloop_quit(w_data->loop, 0);
	return NULL;
}

void ctx_cb(pa_context *ctx, void *data)
{
	//printf("ctx_cb here state = %d\n", pa_context_get_state(ctx));

}

int main(int argc, char **argv)
{
	pthread_t worker;
	struct thread_data_t w_data;

	pa_mainloop_api *api;
	pa_context *ctx;
	int ret = 0;

	gvc = gvContext();
	graph = agopen("PA GRAPH", Agdirected, NULL);

	agattr(graph, AGNODE, "shape", "box");
	agattr(graph, AGNODE, "rankdir", "LR");
	agattr(graph, AGNODE, "fixedsize", "shape");
	agattr(graph, AGNODE, "fontsize", "10.");
	agattr(graph, AGNODE, "width", "3.0");
	agattr(graph, AGNODE, "height", "0.7");

	w_data.loop = pa_mainloop_new();
	if(w_data.loop == NULL)
	{
		printf("pa_mainloop_new() failed\n");
		return -1;
	}
	api = pa_mainloop_get_api(w_data.loop);
	if(api == NULL)
	{
		printf("pa_get api() failed\n");
		return -1;
	}
	ctx = pa_context_new(api, "pa graph");
	if(ctx == NULL)
	{
		printf("pa_context_new() failed\n");
		return -1;
	}

	pa_context_set_state_callback(ctx, ctx_cb, NULL);
	if(pa_context_connect(ctx, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0)
	{
		printf("connect() failed\n");
		return -1;
	}

	w_data.ctx = ctx;
	
	pthread_create(&worker, NULL, worker_thread, &w_data);
	pa_mainloop_run(w_data.loop, &ret);

	pthread_join(worker, NULL);


	return 0;
}
