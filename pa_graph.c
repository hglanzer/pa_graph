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
	int cb_done;
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
	int client_id;
	int source_id;
	int sink_id;
	int mute;
	int monitor_source;
	Agnode_t *node;
};

GSList *elements = NULL;

Agraph_t *graph;
GVC_t *gvc;

struct pa_element_t *get_element_by_type(enum element_t type)
{
	struct pa_element_t *element;
	for(int i = 0; i < g_slist_length(elements); i++)
	{
		element = (struct pa_element_t *)g_slist_nth(elements, i)->data;
		if(element->index == type)
		{
			return element;
		}
	}
	return NULL;
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

struct pa_element_t *get_element_by_type_and_id(int id, enum element_t type)
{
	struct pa_element_t *element;
	for(int i = 0; i < g_slist_length(elements); i++)
	{
		element = (struct pa_element_t *)g_slist_nth(elements, i)->data;
		if((element->index == id) && (element->type == type))
		{
			//printf("   found source id: %s of type %s\n", element->name, get_element_type_name(type));
			return element;
		}
	}
	return NULL;
}

void link_monitors()
{
	struct pa_element_t *element = NULL, *monitor = NULL;
	for(int i = 0; i < g_slist_length(elements); i++)
	{
		element = (struct pa_element_t *)g_slist_nth(elements, i)->data;
		if(element->type == SINK)
		{
			monitor = get_element_by_type_and_id(element->monitor_source, SOURCE);
			if(monitor)
			{
				printf("Sink %s monitor: %s[%d]\n", element->name, monitor->name, element->monitor_source);
				Agedge_t *e = agedge(graph, element->node, monitor->node, "TEST", 1);
			}
			else
				printf("*** ERROR ***: no monitor for sink %s found", element->name);
		}
	}
}

void link_source_outputs()
{
	struct pa_element_t *element = NULL, *src = NULL, *client = NULL, *module = NULL;
	for(int i = 0; i < g_slist_length(elements); i++)
	{
		element = (struct pa_element_t *)g_slist_nth(elements, i)->data;
		if(element->type == SOURCE_OUTPUT)
		{
			src = get_element_by_type_and_id(element->source_id, SOURCE);
			client = get_element_by_type_and_id(element->client_id, CLIENT);
			if(!src)
			{
				printf("NO SRC for source output...?? ****\n");
				continue;
			}
			else if(src && client)
			{
				if(!src->node)
				{
					continue;
					printf("SINK %d has no node\n", element->sink_id);
				}
				if(!src->node)
				{
					printf("CLIENT %d has no node\n", element->client_id);
					continue;
				}
				agedge(graph, src->node, client->node, "TEST", 1);
			}
			else
			// loopback modules: no client (N/A): create link from loopback module
			{
				module = get_element_by_type_and_id(element->owner_id, MODULE);
				if(!src->node)
				{
					continue;
					printf("SRC %d has no node\n", element->sink_id);
				}
				if(!module->node)
				{
					printf("MODULE %d has no node\n", element->owner_id);
					module->node = agnode(graph, module->name, 1);
					agsafeset(module->node, "color", "black", "black");
					agsafeset(module->node, "shape", "ellipse", "ellipse");
					agsafeset(module->node, "height", "0.3", "0.3");
					agsafeset(module->node, "width", "1.2", "1.2");
				}
				printf("*** MODULE %d found for sink input %s\n", element->owner_id, element->name);
				agedge(graph, src->node, module->node, "module", 1);
			}
		}
	}
}

void link_sink_inputs()
{
	struct pa_element_t *element = NULL, *sink = NULL, *client = NULL, *module = NULL;
	for(int i = 0; i < g_slist_length(elements); i++)
	{
		element = (struct pa_element_t *)g_slist_nth(elements, i)->data;
		if(element->type == SINK_INPUT)
		{
			sink = get_element_by_type_and_id(element->sink_id, SINK);
			client = get_element_by_type_and_id(element->client_id, CLIENT);
			if(!sink)
			{
				printf("NO SINK for sinkinput...?? ****\n");
				continue;
			}
			else if(sink && client)
			{
				if(!sink->node)
				{
					continue;
					printf("SINK %d has no node\n", element->sink_id);
				}
				if(!client->node)
				{
					printf("CLIENT %d has no node\n", element->client_id);
					continue;
				}
				agedge(graph, client->node, sink->node, "TEST", 1);
			}
			else
			// loopback modules: no client (N/A): create link from loopback module
			{
				module = get_element_by_type_and_id(element->owner_id, MODULE);
				if(!sink->node)
				{
					continue;
					printf("SINK %d has no node\n", element->sink_id);
				}
				if(!module->node)
				{
					printf("MODULE %d has no node\n", element->owner_id);
					module->node = agnode(graph, module->name, 1);
					agsafeset(module->node, "color", "black", "black");
					agsafeset(module->node, "shape", "ellipse", "ellipse");
					agsafeset(module->node, "height", "0.3", "0.3");
					agsafeset(module->node, "width", "1.2", "1.2");
				}
				printf("*** MODULE %d found for sink input %s\n", element->owner_id, element->name);
				agedge(graph, module->node, sink->node, "module", 1);
			}
		}
	}
}

char *get_broken_string(const char *str)
{
	char *f_str = NULL;
	size_t len = 0, i = 0;
	int dist = 0;

	len = strlen(str);
	f_str = strdup(str);
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
			dist++;
	}
	return f_str;
}

void add_element(Agnode_t *node, enum element_t type, const char *name, int index, int owner_id, int client_id, int source_id, int sink_id, int mute, int monitor_source)
{
	struct pa_element_t *pa_element = (void *)malloc(sizeof(struct pa_element_t));

	if(pa_element == NULL)
	{
		printf("malloc() failed\n");
		return;
	}
	pa_element->name = strdup(name);
	pa_element->type = type;
	pa_element->index = index;
	pa_element->client_id = client_id;
	pa_element->source_id = source_id;
	pa_element->sink_id = sink_id;
	pa_element->owner_id = owner_id;
	pa_element->mute = mute;
	pa_element->node = node;
	pa_element->monitor_source = monitor_source;

	elements = g_slist_append(elements, pa_element);
}

void client_cb(pa_context *ctx, const pa_client_info *info, int eol, void *data)
{
	struct thread_data_t *w_data = (struct thread_data_t *)data;
	char name[64];
	if(eol)
	{
		w_data->cb_done = 1;
		return;
	}
	snprintf(name, sizeof(name), "Client %d", info->index);
	printf("%20s: %80s[%2d]\n", "CLIENT", name, info->index);

	Agnode_t *s = agnode(graph, get_broken_string(info->name), 1);
	agsafeset(s, "color", "yellow", "yellow");
	add_element(s, CLIENT, name, info->index, info->owner_module, -1, -1, -1, -1, -1);
}

void module_cb(pa_context *ctx, const pa_module_info *info, int eol, void *data)
{
	struct thread_data_t *w_data = (struct thread_data_t *)data;
	if(eol)
	{
		w_data->cb_done = 1;
		return;
	}
	printf("%20s: %80s[%2d]\n", "MODULE", info->name, info->index);
	add_element(NULL, MODULE, info->name, info->index, -1, -1, -1, -1, -1, -1);
}

void sink_cb(pa_context *ctx, const pa_sink_info *info, int eol, void *data)
{
	struct thread_data_t *w_data = (struct thread_data_t *)data;
	if(eol)
	{
		w_data->cb_done = 1;
		return;
	}
	printf("%20s: %80s[%2d]\n", "SINK", info->name, info->index);
	Agnode_t *s = agnode(graph, get_broken_string(info->name), 1);
	agsafeset(s, "color", "green", "green");
	add_element(s, SINK, info->name, info->index, info->owner_module, -1, -1, -1, info->mute, info->monitor_source);
}

void source_cb(pa_context *ctx, const pa_source_info *info, int eol, void *data)
{
	struct thread_data_t *w_data = (struct thread_data_t *)data;
	if(eol)
	{
		w_data->cb_done = 1;
		return;
	}
	printf("%20s: %80s[%2d]\n", "SOURCE", info->name, info->index);
	Agnode_t *s = agnode(graph, get_broken_string(info->name), 1);
	agsafeset(s, "color", "red", "red");
	//agsafeset(s, "shape", "cds", "cds");
	add_element(s, SOURCE, info->name, info->index, info->owner_module, -1, -1, -1, info->mute, -1);
}

struct pa_element_t *create_node(void *i)
{
	struct pa_element_t *src = NULL;
	pa_sink_input_info *info = (pa_sink_input_info *)i;
	if(info->client != -1)
	{
		src = get_element_by_type_and_id(info->client, CLIENT);
		src->node = agnode(graph, strdup(src->name), 1);
	}
	else
	{
		src = get_element_by_type_and_id(info->owner_module, MODULE);
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
	struct thread_data_t *w_data = (struct thread_data_t *)data;
	char name[64];
	if(eol)
	{
		w_data->cb_done = 1;
		return;
	}
	snprintf(name, sizeof(name), "Sink Input %d", info->index);
	printf("%20s: %80s[%2d]\n", "SINK INPUT", name, info->index);
	add_element(NULL, SINK_INPUT, name, info->index, info->owner_module, info->client, -1, info->sink, info->mute, -1);
	/*
	src = create_node((void *)info);	
	sink = get_element_by_id(info->sink, SINK);

	agedge(graph, src->node, sink->node, "TEST", 1);

	*/
}

void source_output_cb(pa_context *ctx, const pa_source_output_info *info, int eol, void *data)
{
	struct thread_data_t *w_data = (struct thread_data_t *)data;
	char name[64];
	if(eol)
	{
		w_data->cb_done = 1;
		return;
	}
	snprintf(name, sizeof(name), "Source Output %d", info->index);
	printf("%20s: %80s[%2d]\n", "SOURCE OUTPUT", name, info->index);
	add_element(NULL, SOURCE_OUTPUT, name, info->index, info->owner_module, info->client, info->source, -1, info->mute, -1);
	/*
	sink = create_node((void *)info);	
	src = get_element_by_id(info->source, SOURCE);

	agedge(graph, src->node, sink->node, "TEST", 1);

	*/
}

void *worker_thread(void *data)
{
	enum states_t state = STATE_INIT;
	struct thread_data_t *w_data = NULL;
	int done = 0;

	if(!data)
	{
		printf("worker: no args\n");
		return NULL;
	}

	w_data = (struct thread_data_t *)data;

	while(!done)
	{
		while(1)
		{
			sleep(1);
			if(w_data->cb_done)
			{
				w_data->cb_done = 0;
				state++;
				break;
			}
			else
				printf(".");
		}

		switch(state)
		{
			case STATE_INIT:
				if(pa_context_get_state(w_data->ctx) == PA_CONTEXT_READY)
					state = STATE_GET_SOURCES;
		
				printf("WORKER waiting, pa state = %d\n", pa_context_get_state(w_data->ctx));
			break;

			case STATE_GET_SOURCES:
			pa_context_get_source_info_list(w_data->ctx, source_cb, w_data);
			break;

			case STATE_GET_SINKS:
			pa_context_get_sink_info_list(w_data->ctx, sink_cb, w_data);
			
			break;
			
			case STATE_GET_MODULES:
			pa_context_get_module_info_list(w_data->ctx, module_cb, w_data);
			
			break;
			
			case STATE_GET_CLIENTS:
			pa_context_get_client_info_list(w_data->ctx, client_cb, w_data);
			
			break;
			
			case STATE_GET_SOURCE_OUTPUTS:
			pa_context_get_source_output_info_list(w_data->ctx, source_output_cb, w_data);
			
			break;
			
			case STATE_GET_SINK_INPUTS:
			pa_context_get_sink_input_info_list(w_data->ctx, sink_input_cb, w_data);
			
			break;
			
			case STATE_FINISHED:
			{
				printf("--- STATE FINISHED ---\n\n");
				done = 1;
			}
			break;
		}
	}

	printf("FOUND %d elements\n", g_slist_length(elements));

	link_monitors();
	link_sink_inputs();
	link_source_outputs();

	agwrite(graph, stdout);
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

	// this just rotates the graph / not what we want...
	//agattr(graph, AGRAPH, "orientation", "landscape");

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
