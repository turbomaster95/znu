#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tomlc17.h"

typedef struct { unsigned char *p; size_t n, cap; } buf;
static int reserve(buf *b,size_t add) { size_t need,cap;unsigned char *p;if(add>SIZE_MAX-b->n)return 0;need=b->n+add;if(need<=b->cap)return 1;cap=b->cap?b->cap:256;while(cap<need){if(cap>SIZE_MAX/2){cap=need;break;}cap*=2;}p=realloc(b->p,cap);if(!p)return 0;b->p=p;b->cap=cap;return 1; }
static int put(buf *b,const void *p,size_t n) { if(!reserve(b,n))return 0;memcpy(b->p+b->n,p,n);b->n+=n;return 1; }
static int ch(buf *b,unsigned char c) { return put(b,&c,1); }
static int atom(buf *b,const void *p,size_t n,const char *hint) { char tmp[64];int z;if(hint){z=snprintf(tmp,sizeof(tmp),"[%zu:%s]",strlen(hint),hint);if(z<0||!put(b,tmp,(size_t)z))return 0;}z=snprintf(tmp,sizeof(tmp),"%zu:",n);return z>0&&put(b,tmp,(size_t)z)&&put(b,p,n); }
static int list_begin(buf *b) { return ch(b,'('); }
static int list_end(buf *b) { return ch(b,')'); }
static int integer_atom(buf *b,uint64_t v,const char *hint,size_t width) { unsigned char x[8];size_t i;for(i=0;i<width;i++)x[i]=(unsigned char)(v>>(i*8));return atom(b,x,width,hint); }
static int pair(buf *b,const char *key,size_t keylen,int (*emit)(buf *,const toml_datum_t *),const toml_datum_t *v) { return list_begin(b)&&atom(b,key,keylen,NULL)&&emit(b,v)&&list_end(b); }
static int emit_datum(buf *b,const toml_datum_t *d);
static int emit_table(buf *b,const toml_datum_t *d) { int i;if(!list_begin(b))return 0;for(i=0;i<d->u.tab.size;i++)if(!pair(b,d->u.tab.key[i],(size_t)d->u.tab.len[i],emit_datum,&d->u.tab.value[i]))return 0;return list_end(b); }
static int emit_array(buf *b,const toml_datum_t *d) { int i;if(!list_begin(b))return 0;for(i=0;i<d->u.arr.size;i++)if(!emit_datum(b,&d->u.arr.elem[i]))return 0;return list_end(b); }
static int emit_datetime(buf *b,const toml_datum_t *d) { char s[96];int n;const char *hint="iso8601";if(d->type==TOML_DATE)n=snprintf(s,sizeof(s),"%04d-%02d-%02d",d->u.ts.year,d->u.ts.month,d->u.ts.day);else if(d->type==TOML_TIME)n=snprintf(s,sizeof(s),"%02d:%02d:%02d.%06d",d->u.ts.hour,d->u.ts.minute,d->u.ts.second,d->u.ts.usec);else if(d->type==TOML_DATETIME)n=snprintf(s,sizeof(s),"%04d-%02d-%02dT%02d:%02d:%02d.%06d",d->u.ts.year,d->u.ts.month,d->u.ts.day,d->u.ts.hour,d->u.ts.minute,d->u.ts.second,d->u.ts.usec);else { int tz=d->u.ts.tz;char sign=tz<0?'-':'+';if(tz<0)tz=-tz;n=snprintf(s,sizeof(s),"%04d-%02d-%02dT%02d:%02d:%02d.%06d%c%02d:%02d",d->u.ts.year,d->u.ts.month,d->u.ts.day,d->u.ts.hour,d->u.ts.minute,d->u.ts.second,d->u.ts.usec,sign,tz/60,tz%60); }return n>0&&(size_t)n<sizeof(s)&&atom(b,s,(size_t)n,hint); }
static int emit_datum(buf *b,const toml_datum_t *d) { uint64_t bits;switch(d->type){case TOML_STRING:return atom(b,d->u.str.ptr,(size_t)d->u.str.len,NULL);case TOML_INT64:bits=(uint64_t)d->u.int64;return integer_atom(b,bits,"i64",8);case TOML_FP64:memcpy(&bits,&d->u.fp64,8);return integer_atom(b,bits,"f64",8);case TOML_BOOLEAN:return integer_atom(b,d->u.boolean?1:0,"bool",1);case TOML_DATE:case TOML_TIME:case TOML_DATETIME:case TOML_DATETIMETZ:return emit_datetime(b,d);case TOML_ARRAY:return emit_array(b,d);case TOML_TABLE:return emit_table(b,d);default:fprintf(stderr,"zcfc: unsupported TOML datum type %d\n",(int)d->type);return 0;} }
static int readfile(const char *path,buf *b) { FILE *f=fopen(path,"rb");unsigned char tmp[8192];size_t n;if(!f){perror(path);return 0;}while((n=fread(tmp,1,sizeof(tmp),f))>0)if(!put(b,tmp,n)){fclose(f);return 0;}if(ferror(f)){perror(path);fclose(f);return 0;}fclose(f);return 1; }
static int writefile(const char *path,const buf *b) { FILE *f=fopen(path,"wb");if(!f){perror(path);return 0;}if(b->n&&fwrite(b->p,1,b->n,f)!=b->n){perror(path);fclose(f);return 0;}if(fclose(f)){perror(path);return 0;}return 1; }
static int dump_atom(FILE *f,const unsigned char *s,size_t n,const unsigned char *h,size_t hn) { size_t i;if(hn){fprintf(f,"[%.*s] ",(int)hn,h);}fputc('"',f);for(i=0;i<n;i++){unsigned char c=s[i];if(c=='"'||c=='\\'){fputc('\\',f);fputc(c,f);}else if(c=='\n')fputs("\\n",f);else if(c=='\r')fputs("\\r",f);else if(c=='\t')fputs("\\t",f);else if(c<32||c>=127)fprintf(f,"\\x%02x",c);else fputc(c,f);}fputc('"',f);return !ferror(f); }
static int dump_node(FILE *f,const unsigned char *s,size_t len,size_t *p,unsigned depth) { size_t n=0,i,hn=0;unsigned char *hint=NULL;if(depth>1024||*p>=len)return 0;if(s[*p]=='('){(*p)++;fputc('(',f);while(*p<len&&s[*p]!=')'){if(!dump_node(f,s,len,p,depth+1))return 0;if(*p<len&&s[*p]!=')')fputc(' ',f);}if(*p>=len)return 0;(*p)++;fputc(')',f);return !ferror(f);}if(s[*p]=='['){size_t hp=++*p;while(*p<len&&s[*p]>='0'&&s[*p]<='9'){if(n>(SIZE_MAX-(s[*p]-'0'))/10)return 0;n=n*10+(s[(*p)++]-'0');}if(*p>=len||s[(*p)++]!=':'||n>len-*p)return 0;hint=(unsigned char *)(s+*p);hn=n;*p+=n;if(*p>=len||s[(*p)++]!=']')return 0;(void)hp;}n=0;while(*p<len&&s[*p]>='0'&&s[*p]<='9'){if(n>(SIZE_MAX-(s[*p]-'0'))/10)return 0;n=n*10+(s[(*p)++]-'0');}if(*p>=len||s[(*p)++]!=':'||n>len-*p)return 0;i=*p;*p+=n;return dump_atom(f,s+i,n,hint,hn); }
static int cmd_build(const char *in,const char *out) { buf src={0},dst={0};toml_result_t r;int ok=0;if(!readfile(in,&src))goto done;if(src.n>INT_MAX){fprintf(stderr,"zcfc: input too large for tomlc17\n");goto done;}r=toml_parse_named((const char *)src.p,(int)src.n,in);if(!r.ok){fprintf(stderr,"zcfc: TOML error: %s\n",r.errmsg);toml_free(r);goto done;}if(!emit_datum(&dst,&r.toptab)){fprintf(stderr,"zcfc: conversion failed\n");toml_free(r);goto done;}toml_free(r);if(!writefile(out,&dst))goto done;ok=1;done:free(src.p);free(dst.p);return ok; }
static int cmd_dump(const char *in) { buf src={0};size_t p=0;int ok=0;if(!readfile(in,&src))goto done;if(!dump_node(stdout,src.p,src.n,&p,0)||p!=src.n){fprintf(stderr,"\nzcfc: invalid CSEXPs input at byte %zu\n",p);goto done;}fputc('\n',stdout);ok=!ferror(stdout);done:free(src.p);return ok; }
static void usage(FILE *f) { fputs("usage:\n  zcfc build input.toml -o output.ccb\n  zcfc dump input.ccb\n  zcfc fmt input.rtm\n\nThis build implements TOML 1.1 -> canonical CSEXPs and CSEXPs -> RTM dump.\n",f); }
int main(int argc,char **argv) { if(argc==5&&!strcmp(argv[1],"build")&&!strcmp(argv[3],"-o"))return cmd_build(argv[2],argv[4])?0:1;if(argc==3&&!strcmp(argv[1],"dump"))return cmd_dump(argv[2])?0:1;if(argc==3&&!strcmp(argv[1],"fmt")){fprintf(stderr,"zcfc: RTM formatter is not yet implemented in this build\n");return 2;}usage(stderr);return 2; }
