#include "config_file.h"

config *config_init() {
	/* init config */
	config *conf = malloc(sizeof(config));
	
	/* load default configs */
	config_load_default (conf);
	
	return conf;
}

int config_load_default(config *conf) {
	/* default configuration */
	
	conf->max_workers = 1;
	
	conf->max_pending = 128000;
	conf->max_clients = 128000;
	
	conf->child_stack_size = 128000;
	
	conf->listen_port = 25;
	conf->server_name = "Comfirm DMARC Server";
	conf->daemonize = 1;
	
	conf->read_buffer_size = 4046;
	conf->write_buffer_size = 131072;
	conf->data_buffer_size = 40000;
	
	conf->tcp_nodelay = 1;

	conf->chroot = "";
	
	conf->queue_file = "/tmp/rest-queue";
	conf->queue_size = 100000;
	
	return 0;
}

int config_copy(config *dest, config *source) {
	/* copy a config to another */
	
	memcpy (dest, source, sizeof(config));
	
	/* copy all pointer values */
	config_save_value ("server-name", source->server_name, dest);
	config_save_value ("chroot", source->chroot, dest);
	config_save_value ("queue-file", source->queue_file, dest);
	
	return 0;
}

int config_load(char *filename, config *conf) {
	/* load configuration from file */
	
	FILE *file = NULL;
	char line[4048];
	int len, i;
 	
	if ((file = fopen(filename, "r")) == NULL) {
		perror ("ERROR opening config file");
		return -1;
	}
     
     	int is_key, is_value, next;
     	
     	char key[512];
     	char value[4096];
     	
     	int key_len = 0;
     	int value_len = 0;
     	
	while (fgets(line, sizeof(line), file) != NULL) {  
		if ((len = strlen(line)) > 1) {
			is_key = 1;
			is_value = 0;
			next = 0;
			
			key_len = 0;
			value_len = 0;
		
			for (i = 0; i < len; ++i) {
				switch (line[i]) {
					case '#':
						/* comment */
						next = 1;	
					break;	
					
					case '=':
						if (is_key) {
							key[key_len] = '\0';
							is_key = 0;
							is_value = 1;
						}
					break;
					
					case '\t':
					case '\r':
					case '\n':
					break;
					
					default:
						if (is_key) {
							if (line[i] != ' ') {
								key[key_len] = line[i];
								++key_len;
							}
						}
						else if (is_value) {
							if (line[i] != ' ' || value_len != 0) {
								value[value_len] = line[i];
								++value_len;
							}
						}
					break;
				}
				
				if (next) {
					break;
				}
			}
			
			value[value_len] = '\0';
		
			if (key_len > 0 && value_len > 0) {
				/* save the value */
				config_save_value (&key, &value, conf);
			}
		}
	}

	fclose (file);

	return 0;
}

int config_save_value(char *key, char *value, config *conf) {
	/* save config value in 'config' struct */
	
	if (strcmp(key, "listen-port") == 0) {
		conf->listen_port = atoi(value);
	}
	else if (strcmp(key, "max-workers") == 0) {
		conf->max_workers = atoi(value);
	}
	else if (strcmp(key, "child-stack-size") == 0) {
		conf->child_stack_size = atoi(value);
	}
	else if (strcmp(key, "max-pending") == 0) {
		conf->max_pending = atoi(value);
	}
	else if (strcmp(key, "max-clients") == 0) {
		conf->max_clients = atoi(value);
	}
	else if (strcmp(key, "server-name") == 0) {
		conf->server_name = malloc(strlen(value)+1);
		memcpy (conf->server_name, value, strlen(value)+1);
	}
	else if (strcmp(key, "daemonize") == 0) {
		conf->daemonize = atoi(value);
	}
	else if (strcmp(key, "read-buffer-size") == 0) {
		conf->read_buffer_size = atoi(value);
	}
	else if (strcmp(key, "write-buffer-size") == 0) {
		conf->write_buffer_size = atoi(value);
	}
	else if (strcmp(key, "data-buffer-size") == 0) {
		conf->data_buffer_size = atoi(value);
	}
	else if (strcmp(key, "tcp-nodelay") == 0) {
		conf->tcp_nodelay = atoi(value);
	}
	else if (strcmp(key, "chroot") == 0) {
		conf->chroot = malloc(strlen(value)+1);
		memcpy (conf->chroot, value, strlen(value)+1);
	}
	else if (strcmp(key, "queue-file") == 0) {
		conf->queue_file = malloc(strlen(value)+1);
		memcpy (conf->queue_file, value, strlen(value)+1);
	}
	else if (strcmp(key, "queue-size") == 0) {
		conf->queue_size = atoi(value);
	}
	else {
		/* key not found */
		printf ("Config key '%s' is uknown.\n", key);
	}
	
	return 0;
}

