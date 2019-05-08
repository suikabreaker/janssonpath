#include <stddef.h>
#include <malloc.h>
#include <string.h>
//functions for parsing

const char* jassonpath_match_string(const char* in,const char* end){
  if(*in=='\"')in++;//in should be left quotation mark to match
  else return in;
  for(;in!=end&&*in;in++){
    if(in[0]=='\\'&&in[1]!='\0')in+=2;
    else if(in[0]=='\"'){ in++;break;}
  }
  return in;
}

const char *jassonpath_next_delima(const char *in, const char *end) {
  for (;in != end && *in;in++){
    if(in[0]=='\"') in = jassonpath_match_string(in,end);
    else if(in[0]=='['||in[0]==']'||in[0]=='.')break;
  }
  return in;
}

const char* jassonpath_next_matched_bracket(const char* in,const char* end, char left, char right){
  if(*in==left)in++;//in should be left bracket to match
  else return in;
  for(;in!=end&&*in;in++){
    if(in[0]=='\"') in=jassonpath_match_string(in,end);
    else if(in[0]==right)break;
  }
  return in;
}

const char *jassonpath_next_seprator(const char *in, const char *end, char sep) {
  for (;in != end && *in;in++){
    if(in[0]=='\"') in = jassonpath_match_string(in,end);
    else if(in[0]==sep)break;
  }
  return in;
}

const char *jassonpath_next_punctors_outside_para(const char *in, const char *end, char* sep) {
  for (;in != end && *in;in++){
    if(in[0]=='\"') in = jassonpath_match_string(in,end);
    if(in[0]=='(') in = jassonpath_next_matched_bracket(in,end,'(',')');
    else if(strchr(sep,in[0]))break;
  }
  return in;
}

const char *jassonpath_next_punctor_outside_para(const char *in, const char *end, char sep) {
  for (;in != end && *in;in++){
    if(in[0]=='\"') in = jassonpath_match_string(in,end);
    if(in[0]=='(') in = jassonpath_next_matched_bracket(in,end,'(',')');
    else if(in[0]==sep)break;
  }
  return in;
}


const char *jassonpath_strdup_no_terminal(const char *start, const char *end) {
  size_t len = end - start;
  char *ret = (char *)malloc(sizeof(char) * (len + 1));
  memcpy(ret, start, len);
  ret[len] = '\0';
  return ret;
}