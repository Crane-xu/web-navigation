#include "cJSON.h"

cJSON *open_sites();
void close_sites(cJSON *root);
void modify_site(cJSON *root, const char *old_url, const char *new_url,
                 const char *new_img, const char *new_name);
int delete_site(cJSON *root, const char *url);
void add_site(cJSON *root, const char *url, const char *img, const char *name);
void find_site(cJSON *root, const char *url);