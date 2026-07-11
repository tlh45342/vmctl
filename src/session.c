/* session.c - local vmctl session state and variables. */
#include "session.h"
#include <stdlib.h>
#include <string.h>
static char *dupstr(const char *s){size_t n=strlen(s?s:"");char *p=malloc(n+1);if(!p)return NULL;memcpy(p,s?s:"",n+1);return p;}
static int find(vmctl_session_t *s,const char *k){for(size_t i=0;i<s->vars.count;i++)if(strcmp(s->vars.items[i].key,k)==0)return (int)i;return -1;}
void session_init(vmctl_session_t *s){memset(s,0,sizeof(*s));}
void session_shutdown(vmctl_session_t *s){for(size_t i=0;i<s->vars.count;i++){free(s->vars.items[i].key);free(s->vars.items[i].value);}free(s->vars.items);}
bool session_set(vmctl_session_t *s,const char *k,const char *v){int i=find(s,k);char *nv=dupstr(v);if(!nv)return false;if(i>=0){free(s->vars.items[i].value);s->vars.items[i].value=nv;return true;}if(s->vars.count==s->vars.capacity){size_t nc=s->vars.capacity?s->vars.capacity*2:16;vmctl_var_t *p=realloc(s->vars.items,nc*sizeof(*p));if(!p){free(nv);return false;}s->vars.items=p;s->vars.capacity=nc;}s->vars.items[s->vars.count].key=dupstr(k);s->vars.items[s->vars.count].value=nv;if(!s->vars.items[s->vars.count].key){free(nv);return false;}s->vars.count++;return true;}
const char *session_get(vmctl_session_t *s,const char *k){int i=find(s,k);return i<0?NULL:s->vars.items[i].value;}
bool session_unset(vmctl_session_t *s,const char *k){int i=find(s,k);if(i<0)return false;free(s->vars.items[i].key);free(s->vars.items[i].value);size_t last=s->vars.count-1;if((size_t)i!=last)s->vars.items[i]=s->vars.items[last];s->vars.count--;return true;}
void session_dump(vmctl_session_t *s){for(size_t i=0;i<s->vars.count;i++)printf("%s=%s\n",s->vars.items[i].key,s->vars.items[i].value);}
